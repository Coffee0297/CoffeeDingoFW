// Host-side self-check for the pure function logic (no ChibiOS, no board). tests/host/port.h stands in for
// the board's port.h. From the repo root:
//   g++ -std=c++20 -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/timer.cpp -o build/host_selftest && build/host_selftest
//   g++ -std=c++20 -DMATH_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/math_block.cpp -o build/host_math && build/host_math
//   g++ -std=c++20 -Wall -DTABLE_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/table.cpp -o build/host_table && build/host_table
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
#elif defined(TABLE_ONLY)
#include <cmath>
#include "table.h"

static bool near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

int main()
{
    Config_Table c{};
    c.bEnabled = true; c.nXSize = 3; c.nYSize = 2;
    const float xs[] = {0, 10, 20}, ys[] = {0, 100};
    for (int i = 0; i < 3; i++) c.fXAxis[i] = xs[i];
    for (int i = 0; i < 2; i++) c.fYAxis[i] = ys[i];
    for (int x = 0; x < 3; x++) { c.fCell[0][x] = xs[x]; c.fCell[1][x] = 100 + xs[x]; }   // row y=0: 0 10 20, y=100: 100 110 120

    assert(near(Table::Interpolate(c, 0, 0), 0));
    assert(near(Table::Interpolate(c, 5, 0), 5));         // along x
    assert(near(Table::Interpolate(c, 15, 0), 15));
    assert(near(Table::Interpolate(c, 10, 50), 60));      // along y
    assert(near(Table::Interpolate(c, 5, 50), 55));       // bilinear
    assert(near(Table::Interpolate(c, -99, 0), 0));       // held below
    assert(near(Table::Interpolate(c, 999, 100), 120));   // held above on both axes
    c.nYSize = 1;                                         // 1-D curve ignores y
    assert(near(Table::Interpolate(c, 15, 12345), 15));
    c.nXSize = 0; c.nYSize = 0;                           // degenerate sizes: cell [0][0]
    assert(near(Table::Interpolate(c, 7, 7), 0));
    c.nXSize = 200; c.nYSize = 200;                       // sizes above the max are held to 8x8
    for (int i = 0; i < TABLE_AXIS_MAX; i++) { c.fXAxis[i] = (float)i; c.fYAxis[i] = (float)i; }
    for (int y = 0; y < TABLE_AXIS_MAX; y++) for (int x = 0; x < TABLE_AXIS_MAX; x++) c.fCell[y][x] = (float)(10 * y + x);
    assert(near(Table::Interpolate(c, 6.5f, 7.5f), 76.5f));

    // the block: X and Y from the var map, 0 while disabled
    float x = 5, y = 50;
    pVarMap[0] = &x; pVarMap[1] = &y;
    c.nXSize = 3; c.nYSize = 2;
    for (int i = 0; i < 3; i++) c.fXAxis[i] = xs[i];
    for (int i = 0; i < 2; i++) c.fYAxis[i] = ys[i];
    for (int i = 0; i < 3; i++) { c.fCell[0][i] = xs[i]; c.fCell[1][i] = 100 + xs[i]; }
    c.nXInput = 0; c.nYInput = 1;
    Table t; t.SetConfig(&c); t.Update(); assert(near(t.fVal, 55));
    c.bEnabled = false; t.Update(); assert(t.fVal == 0);

    std::puts("host_table OK");
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
