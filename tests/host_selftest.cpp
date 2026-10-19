// Host-side self-check for the pure function logic (no ChibiOS, no board). tests/host/port.h stands in for
// the board's port.h. From the repo root:
//   g++ -std=c++20 -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/timer.cpp -o build/host_selftest && build/host_selftest
//   g++ -std=c++20 -DMATH_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/math_block.cpp -o build/host_math && build/host_math
//   g++ -std=c++20 -DCONDITION_HYST_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/condition.cpp -o build/host_condition_hyst && build/host_condition_hyst
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
