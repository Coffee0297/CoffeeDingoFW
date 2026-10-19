// Host-side self-check for the pure function logic (no ChibiOS, no board). tests/host/port.h stands in for
// the board's port.h. From the repo root:
//   g++ -std=c++20 -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/timer.cpp -o build/host_selftest && build/host_selftest
//   g++ -std=c++20 -DMATH_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/math_block.cpp -o build/host_math && build/host_math
//   g++ -std=c++20 -DOVL_ONLY -DNUM_OUTPUTS=2 -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/overload_log.cpp -o build/host_ovl && build/host_ovl
// Each feature adds its own section below, picked with -D<NAME>_ONLY, and its build line here.
#include <cassert>
#include <cstdio>
#include "port.h"

float *pVarMap[VAR_MAP_SIZE];
uint32_t nHostTimeMs = 0;

#if 0 // feature sections: #elif defined(<NAME>_ONLY)
#elif defined(MATH_ONLY)
#include "math_block.h"

int main()
{
    float zero = 0, a = 2, b = 3;
    pVarMap[0] = &zero; pVarMap[1] = &a; pVarMap[2] = &b;
    Config_Math c{};
    c.bEnabled = true; c.nInputA = 1; c.nInputB = 2; c.fFactor = 1;
    MathBlock m;
    auto eval = [&](MathOp op) { c.eOp = op; m.SetConfig(&c); m.Update(); return m.fVal; };

    assert(eval(MathOp::Add) == 5);
    assert(eval(MathOp::Sub) == -1);
    assert(eval(MathOp::Mul) == 6);
    assert(eval(MathOp::Min) == 2);
    assert(eval(MathOp::Max) == 3);
    assert(eval(MathOp::Average) == 2.5f);
    b = 0; assert(eval(MathOp::Div) == 0);          // no inf / NaN into the outputs
    a = 6; b = 3; assert(eval(MathOp::Div) == 2);

    // oil - coolant, then x0.5 + 10; B is the constant when its input is 0
    a = 110; b = 90; c.fFactor = 0.5f; c.fOffset = 10;
    assert(eval(MathOp::Sub) == 20);
    c.nInputB = 0; c.fConstB = 100; assert(eval(MathOp::Sub) == 15);
    b = 50; m.Update(); assert(m.fVal == 15);        // the constant, not the variable

    c.bEnabled = false; m.Update(); assert(m.fVal == 0);

    std::puts("host_selftest MATH OK");
    return 0;
}
#elif defined(OVL_ONLY)
#include "overload_log.h"

int main()
{
    // 1 cycle = 2 ms, one sample per 20 cycles (40 ms). Output 1 runs at 4 A, spikes to 30 A once, then trips.
    auto cycle = [](uint8_t n, float a, ProfetState e, bool s) { OvlLogUpdate(n, a, e, 20.0f, s); };
    for (int k = 0; k < 300 * 20; k++)
    {
        nHostTimeMs += 2;
        float a = (k == 300 * 20 - 15) ? 30.0f : 4.0f; // spike between two samples still shows (peak hold)
        cycle(1, a, ProfetState::On, k % 20 == 19);
        cycle(0, 0, ProfetState::On, k % 20 == 19);
    }
    assert(OvlLogCount() == 0);
    cycle(1, 25.0f, ProfetState::Overcurrent, false); // trip
    uint32_t nTrip = nHostTimeMs;
    for (int k = 0; k < 10 * 20; k++)                // Overcurrent -> Fault is the same trip
        cycle(1, 0, k < 20 ? ProfetState::Overcurrent : ProfetState::Fault, k % 20 == 19);
    assert(OvlLogCount() == 1 && OvlLogTime(0) == nTrip);

    uint8_t nOut, nState; float fPeak, fLimit;
    assert(OvlLogGetHeader(0, nOut, nState, fPeak, fLimit));
    assert(nOut == 1 && nState == (uint8_t)ProfetState::Overcurrent && fPeak == 30.0f && fLimit == 20.0f);
    uint8_t b[OVL_TOTAL_SAMPLES];
    for (uint16_t off = 0; off < OVL_TOTAL_SAMPLES; off += 4)
        OvlLogGetData(0, off, &b[off], 4);
    assert(b[0] == 8 && b[OVL_PRE_SAMPLES - 1] == 60);  // 4 A, then the 30 A spike as the last pre sample
    assert(b[OVL_PRE_SAMPLES] == 50);                   // first post sample holds the 25 A trip current
    assert(b[OVL_PRE_SAMPLES + 1] == 0 && b[OVL_TOTAL_SAMPLES - 1] == 0);
    assert(OvlLogGetData(0, OVL_TOTAL_SAMPLES, b, 4) == 0 && !OvlLogGetHeader(1, nOut, nState, fPeak, fLimit));

    for (int t = 0; t < 4; t++)                         // 4 trips on output 0: 3 kept, newest first
    {
        nHostTimeMs += 1000;
        cycle(0, 0, ProfetState::On, false);
        cycle(0, 10.0f + t, ProfetState::Overcurrent, false);
        for (int k = 0; k < OVL_POST_SAMPLES; k++)
            cycle(0, 0, ProfetState::Overcurrent, true);
    }
    assert(OvlLogCount() == 3 && OvlLogGetHeader(0, nOut, nState, fPeak, fLimit) && nOut == 0 && fPeak == 13.0f);
    assert(OvlLogGetHeader(2, nOut, nState, fPeak, fLimit) && fPeak == 11.0f);

    OvlLogClear();
    assert(OvlLogCount() == 0 && OvlLogTime(0) == 0);
    cycle(0, 0, ProfetState::Overcurrent, true);        // still tripped after a clear: not logged again
    assert(OvlLogCount() == 0);

    std::puts("host_ovl OK");
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
