// Host-side self-check for the pure function logic (no ChibiOS, no board). tests/host/port.h stands in for
// the board's port.h. From the repo root:
//   g++ -std=c++20 -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/timer.cpp -o build/host_selftest && build/host_selftest
//   g++ -std=c++20 -DMATH_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/math_block.cpp -o build/host_math && build/host_math
//   g++ -std=c++20 -DCONDITION_HYST_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/condition.cpp -o build/host_condition_hyst && build/host_condition_hyst
//   g++ -std=c++20 -Wall -DCONDITION_DELAY_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/condition.cpp -o build/host_condition_delay && build/host_condition_delay
//   g++ -std=c++20 -Wall -DCOMFORT_BLINK_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/comfort_blink.cpp -o build/host_comfort_blink && build/host_comfort_blink
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
#elif defined(CONDITION_HYST_ONLY)
#include "condition.h"

int main()
{
    float in = 0;
    pVarMap[0] = &in;
    Config_Condition c{};
    c.bEnabled = true; c.nInput = 0; c.fArg = 100;
    Condition k; k.SetConfig(&c);
    auto at = [&](float v) { in = v; k.Update(); return k.fVal; };

    c.eOperator = Operator::GreaterThan;            // fHyst 0: exactly the plain compare
    assert(at(101) == 1); assert(at(100) == 0); assert(at(101) == 1); assert(at(99) == 0);
    c.eOperator = Operator::GreaterThanOrEqual;
    assert(at(100) == 1); assert(at(99.9f) == 0);

    c.fHyst = 5;
    c.eOperator = Operator::GreaterThan;            // on above 100, off at 95
    assert(at(100) == 0); assert(at(96) == 0);      // off: the band doesn't turn it on
    assert(at(101) == 1); assert(at(96) == 1); assert(at(95) == 0); assert(at(99) == 0);
    c.eOperator = Operator::GreaterThanOrEqual;     // on at 100, off below 95
    assert(at(100) == 1); assert(at(95) == 1); assert(at(94.9f) == 0);
    c.eOperator = Operator::LessThan;               // on below 100, off at 105
    assert(at(104) == 0); assert(at(99) == 1); assert(at(104) == 1); assert(at(105) == 0);
    c.eOperator = Operator::LessThanOrEqual;        // on at 100, off above 105
    assert(at(100) == 1); assert(at(105) == 1); assert(at(105.1f) == 0);
    c.eOperator = Operator::Equal;                  // ignored by ==
    assert(at(100) == 1); assert(at(101) == 0);

    c.eOperator = Operator::GreaterThan;
    c.fHyst = __builtin_nanf("");                   // NaN band: plain compare, no flipping every update
    assert(at(101) == 1); assert(at(101) == 1); assert(at(100) == 0);
    c.fHyst = 5;

    c.bEnabled = false; assert(at(200) == 0);      // disabled: off, and the band doesn't hold it
    c.bEnabled = true; c.eOperator = Operator::GreaterThan; assert(at(96) == 0);

    std::puts("host_selftest CONDITION_HYST OK");
    return 0;
}
#elif defined(CONDITION_DELAY_ONLY)
#include "condition.h"

int main()
{
    float in = 0;
    pVarMap[0] = &in;
    Config_Condition c{};
    c.bEnabled = true; c.nInput = 0; c.eOperator = Operator::GreaterThan; c.fArg = 10;
    Condition k; k.SetConfig(&c);
    // Run the 1 ms loop up to and including ms; the input set before a call starts at the next step
    auto to = [&](uint32_t ms) { while (nHostTimeMs != ms) { nHostTimeMs++; k.Update(); } };

    in = 20; to(1); assert(k.fVal == 1);            // no delays: unchanged, on at once
    in = 0; to(2); assert(k.fVal == 0);

    c.nDelayOn = 100; c.nDelayOff = 50;
    in = 20; to(101); assert(k.fVal == 0);          // true from 3 on
    to(102); assert(k.fVal == 1);                   // on after 100 ms true
    in = 0; to(130);
    in = 20; to(131); in = 0; to(180); assert(k.fVal == 1);  // a true blip restarts the off delay
    to(181); assert(k.fVal == 0);                   // off after 50 ms false
    in = 20; to(190); in = 0; to(300); assert(k.fVal == 0);  // a short pulse never turns it on

    nHostTimeMs = 0xFFFFFF00; k.SetConfig(&c);      // SYS_TIME wraps during a pending on delay
    to(0xFFFFFFF0);
    in = 20; to(0x00000053); assert(k.fVal == 0);
    to(0x00000054); assert(k.fVal == 1);

    c.bEnabled = false; to(0x60); assert(k.fVal == 0);
    c.bEnabled = true; to(0xC3); assert(k.fVal == 0);  // re-enabled: the on delay runs again
    to(0xC4); assert(k.fVal == 1);

    std::puts("host_selftest CONDITION_DELAY OK");
    return 0;
}
#elif defined(COMFORT_BLINK_ONLY)
#include "comfort_blink.h"

static float v[VAR_MAP_SIZE];
enum { L = 1, R = 2, CLK = 3 };
static ComfortBlink cb;
static uint32_t t = 0;
static int blinksL = 0, blinksR = 0;
static bool lastL = false, lastR = false;
// Run for ms in 10 ms steps, clock 400 on / 400 off from t = 0; counts lit phases per side (lamp = side && clock)
static void run(uint32_t ms)
{
    for (uint32_t e = 0; e < ms; e += 10, t += 10)
    {
        v[CLK] = ((t / 400) % 2 == 0) ? 1 : 0;
        cb.Update(t);
        bool l = cb.fLeft && v[CLK], r = cb.fRight && v[CLK];
        if (l && !lastL) blinksL++;
        if (r && !lastR) blinksR++;
        lastL = l; lastR = r;
    }
}
static void reset() { v[L] = v[R] = 0; run(2000); blinksL = blinksR = 0; }

int main()
{
    for (int i = 0; i < VAR_MAP_SIZE; i++) pVarMap[i] = &v[i];
    Config_ComfortBlink c{}; c.bEnabled = true; c.nLeftInput = L; c.nRightInput = R; c.nClockInput = CLK; c.nTapMs = 1000; c.nBlinks = 3;
    cb.SetConfig(&c);

    // a tap (200 ms) blinks 3 times, then stops
    reset(); t = 4000;
    v[L] = 1; run(200); v[L] = 0; run(5000);
    assert(blinksL == 3 && blinksR == 0 && cb.fLeft == 0);

    // held past 1 s: blinks while held, stops at release
    reset(); t = 12000;
    v[R] = 1; run(4000); assert(cb.fRight == 1); v[R] = 0; run(10); assert(cb.fRight == 0);
    assert(blinksR == 5);                   // 4 s at 0.8 s per blink

    // a tap the other way during a comfort blink cancels it, and does not blink itself
    reset(); t = 24000;
    v[L] = 1; run(200); v[L] = 0; run(600);
    assert(cb.fLeft == 1);
    v[R] = 1; run(200); v[R] = 0; run(3000);
    assert(cb.fLeft == 0 && cb.fRight == 0 && blinksL == 1 && blinksR == 0);

    // ... held past 1 s, that cancelling press becomes a normal blink the other way
    reset(); t = 36000;
    v[L] = 1; run(200); v[L] = 0; run(600);
    v[R] = 1; run(1500); assert(cb.fLeft == 0 && cb.fRight == 1); v[R] = 0; run(10); assert(cb.fRight == 0);

    // tapped while the clock is lit: that blink counts, still 3 in all; 4 blinks when set to 4
    reset(); t = 48200;
    v[L] = 1; run(100); v[L] = 0; run(5000); assert(blinksL == 3);
    c.nBlinks = 4; reset(); t = 60000;
    v[L] = 1; run(200); v[L] = 0; run(6000); assert(blinksL == 4);

    // tap time measured across the 32-bit ms wrap
    c.nBlinks = 3; reset(); t = 0xFFFFFFA0;   // the tap starts 96 ms before the wrap
    v[L] = 1; run(200); v[L] = 0; run(5000); assert(blinksL == 3 && cb.fLeft == 0);

    // off: nothing
    c.bEnabled = false; v[L] = 1; run(100); assert(cb.fLeft == 0);
    std::puts("host_comfort_blink OK");
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
