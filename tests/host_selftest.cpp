// Host-side self-check for the pure function logic (no ChibiOS, no board). tests/host/port.h stands in for
// the board's port.h. From the repo root:
//   g++ -std=c++20 -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/timer.cpp -o build/host_selftest && build/host_selftest
//   g++ -std=c++20 -DMATH_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/math_block.cpp -o build/host_math && build/host_math
//   g++ -std=c++20 -DCONDITION_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/condition.cpp -o build/host_condition && build/host_condition
//   g++ -std=c++20 -DWIRE_LOAD_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp -o build/host_wire_load && build/host_wire_load
//   g++ -std=c++20 -DOVL_ONLY -DNUM_OUTPUTS=2 -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/overload_log.cpp -o build/host_ovl && build/host_ovl
//   g++ -std=c++20 -DCONDITION_HYST_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/condition.cpp -o build/host_condition_hyst && build/host_condition_hyst
//   g++ -std=c++20 -Wall -DCONDITION_DELAY_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/condition.cpp -o build/host_condition_delay && build/host_condition_delay
//   g++ -std=c++20 -Wall -DCOMFORT_BLINK_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/comfort_blink.cpp -o build/host_comfort_blink && build/host_comfort_blink
//   g++ -std=c++20 -Wall -DCURVE_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/curve.cpp -o build/host_curve && build/host_curve
//   g++ -std=c++20 -Wall -DLATCH_GATE_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/latch_gate.cpp -o build/host_latch_gate && build/host_latch_gate
//   g++ -std=c++20 -Wall -DRETAIN_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/input.cpp -o build/host_retain && build/host_retain
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
#elif defined(CONDITION_ONLY)
#include "condition.h"

int main()
{
    float a = 0;
    pVarMap[0] = &a;
    Config_Condition k{};
    k.bEnabled = true; k.nInput = 0; k.eOperator = Operator::BitwiseAnd; k.fArg = 4;
    Condition c; c.SetConfig(&k);
    a = 6; c.Update(); assert(c.fVal == 1);    // was 4: the CAN status bit (fVal & 1) read it as off
    a = 3; c.Update(); assert(c.fVal == 0);
    k.eOperator = Operator::BitwiseNand;
    a = 3; c.Update(); assert(c.fVal == 1);
    a = 6; c.Update(); assert(c.fVal == 0);
    std::puts("host_condition OK");
    return 0;
}
#elif defined(WIRE_LOAD_ONLY)
#include "wire_load.h"

int main()
{
    // 10 A step, tau 10 s, 2 ms steps: 63.2 % of I^2 at one tau, > 99 % at five
    float fI2 = 0;
    for (uint32_t t = 0; t < 10000; t += 2)
        fI2 = FilterI2(fI2, 10.0f, 2, 10);
    std::printf("after 1 tau: %.1f %% of I^2\n", fI2);
    assert(fI2 > 62.5f && fI2 < 64.0f);
    for (uint32_t t = 0; t < 40000; t += 2)
        fI2 = FilterI2(fI2, 10.0f, 2, 10);
    assert(fI2 > 99.0f && fI2 <= 100.0f);

    // Current off: decays the same way
    for (uint32_t t = 0; t < 10000; t += 2)
        fI2 = FilterI2(fI2, 0.0f, 2, 10);
    assert(fI2 > 36.0f && fI2 < 37.5f);

    // A step longer than tau (first call after boot) can't overshoot
    assert(FilterI2(0.0f, 5.0f, 60000, 10) == 25.0f);

    std::puts("host_selftest wire load OK");
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
#elif defined(RETAIN_ONLY)
#include "input.h"

int main()
{
    // A restored latch is not toggled by an input that is already on at boot, the next press toggles it
    Input in;
    in.Restore(true);
    assert(in.Check(InputMode::Latching, false, true) == true);
    assert(in.Check(InputMode::Latching, false, false) == true);
    assert(in.Check(InputMode::Latching, false, true) == false);
    in.Restore(true);
    assert(in.Check(InputMode::Latching, false, false) == true);   // input off at boot: kept too
    in.Restore(true);
    assert(in.Check(InputMode::Momentary, false, false) == false); // momentary follows its input

    std::puts("host_retain OK");
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

    // Constant setpoint when no setpoint input is set, read live
    Reset(); c.fKi = 2; c.nSetpointInput = 0; c.fSetpoint = 40; sp = 99; p.SetConfig(&c);
    Run(20000); assert(std::fabs(pv - 40) < 0.2f);
    c.fSetpoint = 20; Run(20000); assert(std::fabs(pv - 20) < 0.2f);

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
