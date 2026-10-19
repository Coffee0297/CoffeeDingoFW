// Host-side self-check for the pure function logic (no ChibiOS, no board). tests/host/port.h stands in for
// the board's port.h. From the repo root:
//   g++ -std=c++20 -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/timer.cpp -o build/host_selftest && build/host_selftest
//   g++ -std=c++20 -Wall -DLATCH_GATE_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/latch_gate.cpp -o build/host_latch_gate && build/host_latch_gate
// Each feature adds its own section below, picked with -D<NAME>_ONLY, and its build line here.
#include <cassert>
#include <cstdio>
#include "port.h"

float *pVarMap[VAR_MAP_SIZE];
uint32_t nHostTimeMs = 0;

#if 0 // feature sections: #elif defined(<NAME>_ONLY)
#elif defined(LATCH_GATE_ONLY)
#include "latch_gate.h"

int main()
{
    float in = 0, ctl = 0;
    pVarMap[0] = &in; pVarMap[1] = &ctl;
    Config_LatchGate c{};
    c.bEnabled = true; c.eType = LatchGateType::Latch; c.nInput = 0; c.nControlInput = 1; c.nTime = 0;
    LatchGate g; g.SetConfig(&c);
    auto at = [&](uint32_t ms) { nHostTimeMs = ms; g.Update(); };

    in = 1; at(0); at(10); assert(g.fVal == 0);     // input already on when enabled: no edge
    in = 0; at(20); in = 1; at(30); assert(g.fVal == 1);   // rising edge sets
    in = 0; at(40); at(10000); assert(g.fVal == 1); // stays set, no auto-cancel
    ctl = 1; at(10010); assert(g.fVal == 0);        // reset edge clears
    in = 1; at(10020); assert(g.fVal == 1);         // set edge while reset held: set (reset is edge-only)
    ctl = 0; at(10030); in = 0; at(10040);
    in = 1; ctl = 1; at(10050); assert(g.fVal == 0); // set and reset in the same cycle: reset wins

    c.nTime = 100; in = 0; ctl = 0; at(10060);
    in = 1; at(10070); assert(g.fVal == 1);
    at(10169); assert(g.fVal == 1);
    at(10170); assert(g.fVal == 0);                 // auto-cancel after 100 ms
    at(10300); assert(g.fVal == 0);                 // input still on: no new edge

    c.eType = LatchGateType::Gate; c.bEnabled = false; at(10400); assert(g.fVal == 0);
    c.bEnabled = true; at(10410);
    in = 42; ctl = 1; at(10420); assert(g.fVal == 42); // follows while enabled
    in = 7; at(10430); assert(g.fVal == 7);
    ctl = 0; in = 99; at(10440); assert(g.fVal == 7);  // holds when disabled
    ctl = 1; at(10450); assert(g.fVal == 99);

    std::puts("host_selftest LATCH_GATE OK");
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
