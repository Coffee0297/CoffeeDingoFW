// Host-side self-check for the pure function logic (no ChibiOS, no board). tests/host/port.h stands in for
// the board's port.h. From the repo root:
//   g++ -std=c++20 -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/timer.cpp -o build/host_selftest && build/host_selftest
//   g++ -std=c++20 -DCAN_MSG_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/can_msg.cpp -o build/host_can_msg && build/host_can_msg
// Each feature adds its own section below, picked with -D<NAME>_ONLY, and its build line here.
#include <cassert>
#include <cstdio>
#include "port.h"

float *pVarMap[VAR_MAP_SIZE];
uint32_t nHostTimeMs = 0;

#if 0 // feature sections: #elif defined(<NAME>_ONLY)
#elif defined(CAN_MSG_ONLY)
#include "can_msg.h"

int main()
{
    float in = 0;
    pVarMap[3] = &in;
    Config_CanMsg c{};
    c.bEnabled = true; c.nInput = 3; c.nID = 0x123; c.nDLC = 8; c.eMode = CanMsgMode::Repeat; c.nInterval = 100;
    CanMsg k; k.SetConfig(&c);
    auto due = [&](uint32_t ms, bool bMuted = false) { nHostTimeMs = ms; return k.Due(bMuted); };

    assert(!due(0));                                    // input off: nothing
    in = 1;
    assert(due(10));                                    // turns on: at once
    assert(!due(50)); assert(!due(109));
    assert(due(110));                                   // then every interval
    assert(!due(210, true)); assert(!due(400, true));   // muted: held back
    assert(due(410));                                   // unmuted: at once, then the interval again
    assert(!due(500)); assert(due(510));
    in = 0; assert(!due(700));

    c.eMode = CanMsgMode::Once;
    in = 1; assert(due(800));                           // one frame each time it turns on
    assert(!due(1000)); assert(!due(5000));
    in = 0; due(5010); in = 1; assert(due(5020, true));  // Once is an event: not muted

    c.eMode = CanMsgMode::Repeat; in = 0; due(6000); in = 1;
    c.nID = 0x800; assert(!due(6010)); assert(!due(7000));   // standard id > 0x7FF: never
    c.nIDE = 1; assert(due(7010));                           // as an extended id it's fine
    c.bEnabled = false; assert(!due(8000));

    std::puts("host_selftest CAN_MSG OK");
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
