// Host-side self-check for the pure function logic (no ChibiOS, no board). tests/host/port.h stands in for
// the board's port.h. From the repo root:
//   g++ -std=c++20 -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/timer.cpp -o build/host_selftest && build/host_selftest
//   g++ -std=c++20 -DMATH_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/math_block.cpp -o build/host_math && build/host_math
//   g++ -std=c++20 -Wall -DROTARY_CAL_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/input.cpp -o build/host_rotary_cal && build/host_rotary_cal
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
#elif defined(ROTARY_CAL_ONLY)
enum class AnalogChannel { AnIn1 };
static float fHostVolts;
uint16_t GetAdcRaw(AnalogChannel) { return 0; }
float GetAdcVolts(AnalogChannel) { return fHostVolts; }
#include "analog_input.cpp"

int main()
{
    Config_AnalogInput c{};
    c.bEnabled = true; c.stRotary.bEnabled = true;
    c.stRotary.fOffset = 0; c.stRotary.fStep = 1000; c.stRotary.fMaxPos = 4;
    Analog_Input a(AnalogChannel::AnIn1); a.SetConfig(&c);
    auto pos = [&](int mv) { fHostVolts = mv / 1000.0f; a.Update(); return (int)a.fRotaryPos; };

    assert(pos(2500) == 2 && pos(9000) == 4);             // uniform mode (default) as before

    auto pair = [](uint16_t even, uint16_t odd) { return (uint32_t)even | ((uint32_t)odd << 16); };
    c.stRotary.eMode = RotaryMode::Calibrated;
    c.stRotary.nNumPos = 4; c.stRotary.nTolerance = 200;
    c.stRotary.nPointPair[0] = pair(500, 1500); c.stRotary.nPointPair[1] = pair(2500, 3500);
    assert(pos(500) == 0 && pos(300) == 0 && pos(700) == 0);
    assert(pos(299) == ROTARY_NO_POS && pos(701) == ROTARY_NO_POS && pos(1000) == ROTARY_NO_POS);
    assert(pos(1600) == 1 && pos(2400) == 2 && pos(3700) == 3);
    assert(pos(3701) == ROTARY_NO_POS && pos(0) == ROTARY_NO_POS);
    c.stRotary.bInvert = true;
    assert(pos(500) == 3 && pos(3500) == 0 && pos(1000) == ROTARY_NO_POS);
    c.stRotary.bInvert = false;

    // tolerance wider than half the gap: the windows meet in the middle, never overlap
    c.stRotary.nNumPos = 2; c.stRotary.nTolerance = 300;
    c.stRotary.nPointPair[0] = pair(1000, 1200);
    assert(pos(700) == 0 && pos(1100) == 0 && pos(1101) == 1 && pos(1500) == 1);
    assert(pos(699) == ROTARY_NO_POS && pos(1501) == ROTARY_NO_POS);

    // all ROTARY_MAX_POS positions; a count above it is held to it
    c.stRotary.nNumPos = 99; c.stRotary.nTolerance = 100;
    for (int k = 0; k < ROTARY_MAX_POS / 2; k++) c.stRotary.nPointPair[k] = pair(400 * (2 * k + 1), 400 * (2 * k + 2));
    for (int k = 0; k < ROTARY_MAX_POS; k++) assert(pos(400 * (k + 1)) == k);
    assert(pos(400 * (ROTARY_MAX_POS + 1)) == ROTARY_NO_POS);

    std::puts("host_rotary_cal OK");
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
