// Host-side self-check for the pure function logic (no ChibiOS, no board). tests/host/port.h stands in for
// the board's port.h. From the repo root:
//   g++ -std=c++20 -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/timer.cpp -o build/host_selftest && build/host_selftest
//   g++ -std=c++20 -DCAN_INPUT_TIMEOUT_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/can_input.cpp functions/input.cpp utils/dbc.cpp -o build/host_can_input_timeout && build/host_can_input_timeout
// Each feature adds its own section below, picked with -D<NAME>_ONLY, and its build line here.
#include <cassert>
#include <cstdio>
#include "port.h"

float *pVarMap[VAR_MAP_SIZE];
uint32_t nHostTimeMs = 0;

#if 0 // feature sections: #elif defined(<NAME>_ONLY)
#elif defined(CAN_INPUT_TIMEOUT_ONLY)
#include "can_input.h"

int main()
{
    // coolant temp in byte 0; output on above 100 (a fan rule); timeout 500 ms
    Config_CanInput c{};
    c.bEnabled = true; c.bTimeoutEnabled = true; c.nTimeout = 500; c.nID = 0x100;
    c.nBitLength = 8; c.fFactor = 1; c.eOperator = Operator::GreaterThan; c.fOperand = 100;
    c.fTimeoutValue = 120;
    CanInput k; k.SetConfig(&c);
    CANRxFrame rx{}; rx.SID = 0x100;
    auto frame = [&](uint32_t ms, uint8_t v) { nHostTimeMs = ms; rx.data8[0] = v; k.CheckMsg(rx); };
    auto idle = [&](uint32_t ms) { nHostTimeMs = ms; k.CheckTimeout(); };

    frame(0, 90); idle(400); assert(k.fVal == 90 && k.fOutput == 0);
    idle(501); assert(k.fVal == 0 && k.fOutput == 0);          // flag off: as before, 0 and off

    c.bTimeoutUseValue = true;
    frame(1000, 90); idle(1501); assert(k.fVal == 120 && k.fOutput == 1);   // sender lost: reads 120, fan on
    frame(1600, 90); assert(k.fVal == 90 && k.fOutput == 0);                // sender back: live value again
    c.fTimeoutValue = 50; idle(2101); assert(k.fVal == 50 && k.fOutput == 0);

    c.eMode = InputMode::Latching; c.fTimeoutValue = 120;     // not through the latch: no toggle on timeout
    frame(3000, 110); assert(k.fOutput == 1); frame(3010, 90); assert(k.fOutput == 1);
    idle(3511); assert(k.fOutput == 1); idle(3600); assert(k.fOutput == 1);
    c.fTimeoutValue = 0; idle(3700); assert(k.fOutput == 0); idle(3800); assert(k.fOutput == 0);

    c.bTimeoutEnabled = false; c.fTimeoutValue = 120; idle(9000); assert(k.fVal == 0);   // no timeout: holds
    c.bTimeoutEnabled = true; c.bEnabled = false; idle(9000); assert(k.fVal == 0 && k.fOutput == 0);   // disabled input never uses the value

    std::puts("host_selftest CAN_INPUT_TIMEOUT OK");
    return 0;
}
#else
#include "timer.h"

int main()
{
    float in = 0, rst = 0;
    pVarMap[0] = &in; pVarMap[1] = &rst;
    Config_Timer c{};
    c.bEnabled = true; c.nInput = 0; c.nResetInput = 1; c.eMode = TimerMode::OnDelay; c.nTime = 100;
    Timer t; t.SetConfig(&c);
    auto at = [&](uint32_t ms) { nHostTimeMs = ms; t.Update(); };

    at(0);                                          // first pass only samples the input
    in = 1; at(10); assert(t.fVal == 0);
    at(109); assert(t.fVal == 0);
    at(110); assert(t.fVal == 1);                   // on after 100 ms
    in = 0; at(120); assert(t.fVal == 0);           // off at once

    rst = 1; in = 1; at(130); at(500); assert(t.fVal == 0);   // reset held: stays cleared
    rst = 0; at(510); at(610); assert(t.fVal == 1);          // released with the input on: delay starts again

    std::puts("host_selftest OK");
    return 0;
}
#endif
