// Host-side self-check for the pure function logic (no ChibiOS, no board). tests/host/port.h stands in for
// the board's port.h. From the repo root:
//   g++ -std=c++20 -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/timer.cpp -o build/host_selftest && build/host_selftest
//   g++ -std=c++20 -DMATH_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/math_block.cpp -o build/host_math && build/host_math
//   g++ -std=c++20 -DPID_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/pid.cpp -o build/host_pid && build/host_pid
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
#elif defined(PID_ONLY)
#include <cmath>
#include "pid.h"

static float zero = 0, one = 1, sp, pv, act, ff;
static Config_Pid c;
static Pid p;

// Defaults as in PID_PARAMS, enabled, 10 ms period
static void Reset()
{
    c = Config_Pid{};
    c.bEnabled = true; c.nSetpointInput = 2; c.nProcessInput = 3; c.nActivateInput = 4; c.nFeedFwdInput = 5;
    c.fKp = 1; c.fIMin = -100; c.fIMax = 100; c.fOutMin = 0; c.fOutMax = 100; c.nPeriodMs = 10;
    sp = pv = ff = 0; act = 1;
    c.bEnabled = false; p.SetConfig(&c); p.Update(); c.bEnabled = true; // clear state
}

// First-order plant pv' = (u - pv) / 1 s, stepped with the PID every 10 ms for ms
static void Run(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 10)
    {
        nHostTimeMs += 10;
        p.Update();
        pv += 0.01f * (p.fVal - pv);
    }
}

int main()
{
    float *vars[] = {&zero, &one, &sp, &pv, &act, &ff};
    for (int i = 0; i < 6; i++) pVarMap[i] = vars[i];

    // Proportional only: output = Kp * error, and a steady-state error remains
    Reset(); c.fKp = 2; sp = 40;
    p.Update(); assert(p.fVal == 80 && p.fP == 80 && p.fI == 0 && p.fD == 0);
    Run(20000); assert(std::fabs(pv - 80.0f / 3) < 0.5f);

    // Integral removes it
    Reset(); c.fKi = 2; sp = 40;
    Run(20000); assert(std::fabs(pv - 40) < 0.2f);

    // Update period: nothing changes between periods
    Reset(); c.nPeriodMs = 100; sp = 10; p.Update(); sp = 50; nHostTimeMs += 50; p.Update(); assert(p.fVal == 10);
    nHostTimeMs += 50; p.Update(); assert(p.fVal == 50);

    // Output clamp + anti-windup: held at 30 for 20 s the integral doesn't grow, so it leaves saturation at once
    Reset(); c.fKi = 2; c.fOutMax = 30; sp = 100;
    Run(20000); assert(p.fVal == 30 && p.fI == 0);
    sp = 10; Run(10); assert(p.fVal < 30);
    // Integral clamp
    Reset(); c.fKp = 0; c.fKi = 10; c.fIMax = 5; sp = 40; Run(5000); assert(p.fI == 5 && p.fVal == 5);

    // Invert: pv above sp drives the output up (cooling)
    Reset(); c.bInvert = true; sp = 80; pv = 90; p.Update(); assert(p.fVal == 10);
    pv = 70; nHostTimeMs += 10; p.Update(); assert(p.fVal == 0);

    // Deadband
    Reset(); c.fDeadband = 2; sp = 41; pv = 40; p.Update(); assert(p.fVal == 0);
    pv = 38; nHostTimeMs += 10; p.Update(); assert(p.fVal == 3);

    // Derivative on measurement: a setpoint step doesn't kick, a pv change does
    Reset(); c.fKp = 0; c.fKd = 1; p.Update(); sp = 100; Run(10); assert(p.fD == 0);
    pv = 1; nHostTimeMs += 10; p.Update(); assert(std::fabs(p.fD + 100) < 0.01f);

    // Setpoint slew: 10 units/s from the measured value
    Reset(); c.fSpRate = 10; p.Update(); sp = 100; nHostTimeMs += 1000; p.Update(); assert(std::fabs(p.fVal - 10) < 0.01f);

    // Activate off: output = feed-forward (constant + var), P/I/D 0, integral reset
    Reset(); c.fKi = 1; sp = 40; Run(1000); assert(p.fI > 0);
    act = 0; c.fFeedForward = 10; ff = 5; nHostTimeMs += 10; p.Update();
    assert(p.fVal == 15 && p.fP == 0 && p.fI == 0 && p.fD == 0);
    act = 1; nHostTimeMs += 10; p.Update(); assert(p.fI < 1 && p.fVal == 15 + p.fP + p.fI); // integral restarts from 0

    // Sensor fault: pv outside the window, NaN or inf -> fallback, integral frozen
    Reset(); c.fKi = 1; c.fPvMin = 1; c.fPvMax = 200; c.fFallback = 75; sp = 40; pv = 20; Run(1000);
    const float fI = p.fI;
    pv = 0; nHostTimeMs += 10; p.Update(); assert(p.fVal == 75 && p.fI == fI && p.fP == 0);
    pv = 201; nHostTimeMs += 10; p.Update(); assert(p.fVal == 75 && p.fI == fI);
    pv = NAN; nHostTimeMs += 10; p.Update(); assert(p.fVal == 75 && p.fI == fI);
    c.fPvMin = c.fPvMax = 0; pv = INFINITY; nHostTimeMs += 10; p.Update(); assert(p.fVal == 75);
    sp = NAN; pv = 20; nHostTimeMs += 10; p.Update(); assert(p.fVal == 75);
    sp = 40; nHostTimeMs += 10; p.Update(); assert(p.fVal > 20 && p.fVal < 100); // back in control, no D kick
    c.fFallback = 500; pv = NAN; nHostTimeMs += 10; p.Update(); assert(p.fVal == 100); // fallback clamped too

    // Bad numbers never reach the output
    Reset(); c.fKp = NAN; sp = 40; p.Update(); assert(p.fVal == 0);
    Reset(); c.fOutMin = NAN; sp = 40; p.Update(); assert(p.fVal == 0); // the param range check lets a NaN through
    Reset(); c.fKp = 1e6; sp = 1e6; pv = -1e6; p.Update(); assert(p.fVal == 100);
    Reset(); act = NAN; c.fFeedForward = 20; p.Update(); assert(p.fVal == 20); // NaN activate = off

    // Crossed limits = disabled, output 0
    Reset(); c.fOutMin = 50; c.fOutMax = 40; sp = 40; p.Update(); assert(p.fVal == 0);
    Reset(); c.fIMin = 1; c.fIMax = 0; sp = 40; p.Update(); assert(p.fVal == 0);
    Reset(); c.fPvMin = 10; c.fPvMax = 5; sp = 40; p.Update(); assert(p.fVal == 0);
    Reset(); c.bEnabled = false; sp = 40; p.Update(); assert(p.fVal == 0);

    std::puts("host_selftest PID OK");
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
