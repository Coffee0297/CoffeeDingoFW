// Host-side self-check for the pure function logic (no ChibiOS, no board). tests/host/port.h stands in for
// the board's port.h. From the repo root:
//   g++ -std=c++20 -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/timer.cpp -o build/host_selftest && build/host_selftest
// Each feature adds its own section below, picked with -D<NAME>_ONLY, and its build line here.
//   g++ -std=c++20 -Wall -DCURVE_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/curve.cpp -o build/host_curve && build/host_curve
#include <cassert>
#include <cstdio>
#include "port.h"

float *pVarMap[VAR_MAP_SIZE];
uint32_t nHostTimeMs = 0;

#if 0 // feature sections: #elif defined(<NAME>_ONLY)
#elif defined(CURVE_ONLY)
#include <cmath>
#include "curve.h"

int main()
{
    float in = 0;
    pVarMap[0] = &in;
    // fan duty against temperature: 0 % to 80 C, 30 % at 80, 100 % at 100
    Config_Curve c{};
    c.bEnabled = true; c.nInput = 0; c.nPoints = 3;
    c.fX[0] = 80; c.fY[0] = 30;
    c.fX[1] = 90; c.fY[1] = 50;
    c.fX[2] = 100; c.fY[2] = 100;
    Curve k; k.SetConfig(&c);
    auto at = [&](float x) { in = x; k.Update(); return k.fVal; };
    auto near = [](float a, float b) { return std::fabs(a - b) < 1e-4f; };

    assert(near(at(20), 30));                       // below the first point: held
    assert(near(at(80), 30));
    assert(near(at(85), 40));                       // interpolated
    assert(near(at(95), 75));
    assert(near(at(100), 100));
    assert(near(at(150), 100));                     // above the last point: held

    c.fX[1] = 70;                                   // not ascending: point skipped, 80 -> 100 straight
    assert(near(at(90), 65));
    c.fX[1] = 80;                                   // equal X: skipped too, no divide by zero
    assert(near(at(90), 65));
    c.fX[1] = 90;

    c.nPoints = 12;                                 // 12 points: the unused ones (X = 0) are skipped
    assert(near(at(95), 75) && near(at(200), 100));
    c.nPoints = 255;                                // never read past the arrays
    assert(near(at(95), 75));

    c.bEnabled = false;
    assert(at(95) == 0);

    std::puts("host_selftest CURVE OK");
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
