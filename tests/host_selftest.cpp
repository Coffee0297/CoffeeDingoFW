// Host-side self-check for the pure function logic (no ChibiOS, no board). tests/host/port.h stands in for
// the board's port.h. From the repo root:
//   g++ -std=c++20 -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/timer.cpp -o build/host_selftest && build/host_selftest
// Each feature adds its own section below, picked with -D<NAME>_ONLY, and its build line here.
//   g++ -std=c++20 -Wall -DFORCE_SLEEP_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp -o build/host_force_sleep && build/host_force_sleep
#include <cassert>
#include <cstdio>
#include "port.h"

float *pVarMap[VAR_MAP_SIZE];
uint32_t nHostTimeMs = 0;

#if 0 // feature sections: #elif defined(<NAME>_ONLY)
#elif defined(FORCE_SLEEP_ONLY)
#include "force_sleep.h"

int main()
{
    assert(!ForceSleepDue(0, 0));                 // forced while the others still talk: wait
    assert(!ForceSleepDue(999, 5000));            // a goodbye 999 ms ago: wait for the full quiet
    assert(ForceSleepDue(1000, 1200));            // 1 s quiet: sleep
    assert(ForceSleepDue(0, 10000));              // still talking after 10 s: sleep anyway

    std::puts("host_force_sleep OK");
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
