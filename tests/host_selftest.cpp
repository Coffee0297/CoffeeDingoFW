// Host-side self-check for the pure function logic (no ChibiOS, no board). tests/host/port.h stands in for
// the board's port.h. From the repo root:
//   g++ -std=c++20 -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/timer.cpp -o build/host_selftest && build/host_selftest
//   g++ -std=c++20 -DCONDITION_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/condition.cpp -o build/host_condition && build/host_condition
//   g++ -std=c++20 -DANALOG_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/input.cpp -o build/host_analog && build/host_analog
//   g++ -std=c++20 -DCONDITION_HYST_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/condition.cpp -o build/host_condition_hyst && build/host_condition_hyst
//   g++ -std=c++20 -Wall -DCONDITION_DELAY_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/condition.cpp -o build/host_condition_delay && build/host_condition_delay
//   g++ -std=c++20 -DCAN_INPUT_TIMEOUT_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/can_input.cpp functions/input.cpp utils/dbc.cpp -o build/host_can_input_timeout && build/host_can_input_timeout
//   g++ -std=c++20 -DWIRE_LOAD_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp -o build/host_wire_load && build/host_wire_load
//   g++ -std=c++20 -DMATH_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/math_block.cpp -o build/host_math && build/host_math
//   g++ -std=c++20 -DCAN_MSG_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/can_msg.cpp -o build/host_can_msg && build/host_can_msg
//   g++ -std=c++20 -DCAN_MSG_CHECKSUM_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/can_msg.cpp utils/dbc.cpp -o build/host_can_msg_cs && build/host_can_msg_cs
//   g++ -std=c++20 -Wall -DLATCH_GATE_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/latch_gate.cpp -o build/host_latch_gate && build/host_latch_gate
//   g++ -std=c++20 -DPID_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/pid.cpp -o build/host_pid && build/host_pid
// Each feature adds its own section below, picked with -D<NAME>_ONLY, and its build line here.
//   g++ -std=c++20 -Wall -DPWM_METER_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp -o build/host_pwm_meter && build/host_pwm_meter
//   g++ -std=c++20 -Wall -DANALOG_SCALE_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/input.cpp -o build/host_analog_scale && build/host_analog_scale
//   g++ -std=c++20 -Wall -DROTARY_CAL_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/input.cpp -o build/host_rotary_cal && build/host_rotary_cal
//   g++ -std=c++20 -Wall -DCHECKSUM_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp utils/dbc.cpp -o build/host_checksum && build/host_checksum
//   g++ -std=c++20 -Wall -DTABLE_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/table.cpp -o build/host_table && build/host_table
//   g++ -std=c++20 -Wall -DCURVE_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/curve.cpp -o build/host_curve && build/host_curve
#include <cassert>
#include <cstdio>
#include "port.h"

float *pVarMap[VAR_MAP_SIZE];
uint32_t nHostTimeMs = 0;

#if 0 // feature sections: #elif defined(<NAME>_ONLY)
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
#elif defined(ANALOG_ONLY)
enum class AnalogChannel { Ai1 };     // board port.h stand-ins, ahead of analog_input.h
float fHostMv = 0;
uint16_t GetAdcRaw(AnalogChannel) { return static_cast<uint16_t>(fHostMv); }
float GetAdcVolts(AnalogChannel) { return fHostMv / 1000.0f; }
#include "analog_input.cpp"         // same unit: it needs the stand-ins above

int main()
{
    Config_AnalogInput k{};
    k.stSwitch.bEnabled = true; k.stSwitch.nThreshold = 1000;
    Analog_Input a(AnalogChannel::Ai1); a.SetConfig(&k);
    fHostMv = 2500; a.Update();
    assert(a.fVal == 2500 && a.fValMillivolts > 2499 && a.fValMillivolts < 2501);  // disabled: raw still live (was 0)
    assert(a.fSwitchVal == 0);                                                     // decoders stay off
    k.bEnabled = true; a.Update(); assert(a.fSwitchVal == 1);
    k.bEnabled = false; fHostMv = 500; a.Update();
    assert(a.fValMillivolts < 501 && a.fSwitchVal == 0);                            // was frozen at 2500 / 1
    std::puts("host_analog OK");
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
#elif defined(CAN_INPUT_TIMEOUT_ONLY)
#include "can_input.h"

int main()
{
    // coolant temp in byte 0; output on above 100 (a fan rule); timeout 500 ms
    Config_CanInput c{};
    c.bEnabled = true; c.bTimeoutEnabled = true; c.nTimeout = 500; c.nID = 0x100;
    c.nBitLength = 8; c.fFactor = 1; c.eOperator = Operator::GreaterThan; c.fOperand = 100;
    c.fTimeoutValue = 120;
    CanInput k; k.SetConfig(&c);
    CANRxFrame rx{}; rx.SID = 0x100;
    auto frame = [&](uint32_t ms, uint8_t v) { nHostTimeMs = ms; rx.data8[0] = v; k.CheckMsg(rx); };
    auto idle = [&](uint32_t ms) { nHostTimeMs = ms; k.CheckTimeout(); };

    frame(0, 90); idle(400); assert(k.fVal == 90 && k.fOutput == 0);
    idle(501); assert(k.fVal == 0 && k.fOutput == 0);          // flag off: as before, 0 and off

    c.bTimeoutUseValue = true;
    frame(1000, 90); idle(1501); assert(k.fVal == 120 && k.fOutput == 1);   // sender lost: reads 120, fan on
    frame(1600, 90); assert(k.fVal == 90 && k.fOutput == 0);                // sender back: live value again
    c.fTimeoutValue = 50; idle(2101); assert(k.fVal == 50 && k.fOutput == 0);

    c.eMode = InputMode::Latching; c.fTimeoutValue = 120;     // not through the latch: no toggle on timeout
    frame(3000, 110); assert(k.fOutput == 1); frame(3010, 90); assert(k.fOutput == 1);
    idle(3511); assert(k.fOutput == 1); idle(3600); assert(k.fOutput == 1);
    c.fTimeoutValue = 0; idle(3700); assert(k.fOutput == 0); idle(3800); assert(k.fOutput == 0);

    c.bTimeoutEnabled = false; c.fTimeoutValue = 120; idle(9000); assert(k.fVal == 0);   // no timeout: holds
    c.bTimeoutEnabled = true; c.bEnabled = false; idle(9000); assert(k.fVal == 0 && k.fOutput == 0);   // disabled input never uses the value

    std::puts("host_selftest CAN_INPUT_TIMEOUT OK");
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
#elif defined(PWM_METER_ONLY)
#include <cmath>
#include "pwm_meter.h"

static bool near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

int main()
{
    const uint32_t clk = 180000000;                       // PDM HCLK: 100 Hz = 1.8 M cycles
    PwmMeter m{}; uint32_t t = 0xFFF00000u;               // start near the 32-bit wrap
    auto pulse = [&](uint32_t hi, uint32_t period) { m.Edge(true, t); m.Edge(false, t + hi); t += period; };
    m.Edge(false, t - 5);                                 // a fall before the first rise is ignored
    for (int i = 0; i < 5; i++) pulse(450000, 1800000);   // 25 % at 100 Hz
    m.Edge(true, t);                                      // closes the 5th period
    PwmMeter::Sample s = m.Take();
    assert(s.periods == 5);
    assert(near(PwmMeter::Duty(s), 25.0f));
    assert(std::fabs(PwmMeter::Freq(s, clk) - 100.0f) < 0.01f);
    assert(m.Take().periods == 0);                        // Take() empties the sums

    m.Edge(false, t + 1620000); t += 1800000;             // one 90 % period
    m.Edge(true, t);
    s = m.Take();
    assert(s.periods == 1 && near(PwmMeter::Duty(s), 90.0f));
    assert(s.lastPeriod == 1800000);

    m.Reset(); m.Edge(true, 0); m.Edge(true, 1000);       // rise, rise without a fall: no period
    assert(m.Take().periods == 0 && PwmMeter::Duty(m.Take()) == 0.0f);

    // glitch filter 20 us = 3600 cycles: spikes in either phase are dropped
    m = PwmMeter{}; m.nMinCycles = 3600; m.Reset();
    assert(m.nMinCycles == 3600);                         // Reset keeps the filter
    t = 1000;
    for (int i = 0; i < 4; i++)
    {
        m.Edge(true, t);
        m.Edge(false, t + 200000); m.Edge(true, t + 200900);      // 5 us spike low in the high phase
        m.Edge(false, t + 450000);
        m.Edge(true, t + 1000000); m.Edge(false, t + 1001800);    // 10 us spike high in the low phase
        t += 1800000;
    }
    m.Edge(true, t); m.Edge(false, t + 450000);           // closes the last period
    s = m.Take();
    assert(s.periods == 4);
    assert(near(PwmMeter::Duty(s), 25.0f));
    assert(std::fabs(PwmMeter::Freq(s, clk) - 100.0f) < 0.01f);

    // a real pulse narrower than the filter is dropped: no edge passes, the input times out to its level
    m.Reset(); t = 0;
    bool bAny = false;
    for (int i = 0; i < 10; i++) { bAny |= m.Edge(true, t); bAny |= m.Edge(false, t + 1800); t += 180000; }
    assert(m.Take().periods == 0 && !bAny);

    // filter off: every edge counts
    PwmMeter n{}; n.Edge(true, 0); n.Edge(false, 100); n.Edge(true, 200); n.Edge(false, 1000); n.Edge(true, 2000);
    assert(n.Take().periods == 2);

    std::puts("host_pwm_meter OK");
    return 0;
}
#elif defined(ANALOG_SCALE_ONLY)
#include <cmath>
enum class AnalogChannel { AnIn1 };
static float fHostVolts;
uint16_t GetAdcRaw(AnalogChannel) { return (uint16_t)(fHostVolts * 4095 / 3.3f); }
float GetAdcVolts(AnalogChannel) { return fHostVolts; }
#include "analog_input.cpp"

int main()
{
    Config_AnalogInput c{};
    c.bEnabled = true; c.fScaleGain = 0.05f; c.fScaleOffset = -25.0f;   // 0.5 V = 0, 4.5 V = 200
    Analog_Input a(AnalogChannel::AnIn1); a.SetConfig(&c);
    auto at = [&](float v) { fHostVolts = v; a.Update(); return a.fScaledVal; };

    assert(at(2.5f) == 0);                                // scaling off: 0
    c.bScale = true;
    assert(std::fabs(at(0.5f) - 0) < 1e-3f);
    assert(std::fabs(at(4.5f) - 200) < 1e-3f);
    assert(std::fabs(at(2.5f) - 100) < 1e-3f);
    assert(std::fabs(at(0.0f) + 25) < 1e-3f);             // below the range: extrapolated, no clamp
    c.bEnabled = false; assert(at(2.5f) == 0);            // input off: 0

    std::puts("host_analog_scale OK");
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
#elif defined(CHECKSUM_ONLY)
#include "dbc.h"

int main()
{
    const uint8_t check[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    assert(Dbc::Checksum(CanOutputKind::Crc8SaeJ1850, check, 9, 0xFF) == 0x4B);   // published check values
    assert(Dbc::Checksum(CanOutputKind::Crc8Autosar, check, 9, 0xFF) == 0xDF);

    uint8_t f[8] = {0x01, 0x02, 0xAA, 0x04, 0x80, 0x80, 0x00, 0x00};
    assert(Dbc::Checksum(CanOutputKind::ChecksumXor, f, 8, 2) == (0x01 ^ 0x02 ^ 0x04));       // skips byte 2
    assert(Dbc::Checksum(CanOutputKind::ChecksumSum, f, 8, 2) == ((0x01 + 0x02 + 0x04 + 0x100) & 0xFF)); // mod 256
    assert(Dbc::Checksum(CanOutputKind::Crc8SaeJ1850, f, 6, 2) != Dbc::Checksum(CanOutputKind::Crc8SaeJ1850, f, 6, 0xFF));

    // rolling counter: the output encodes nCount++ with EncodeInt, which keeps the low bits = wrap at the length
    uint8_t d[8] = {0xFF, 0xFF, 0, 0, 0, 0, 0, 0};
    Dbc::EncodeInt(d, 17, 4, 4);
    assert(d[0] == 0x1F && d[1] == 0xFF);           // 17 & 0xF = 1 in bits 4-7, neighbours untouched

    std::puts("host_selftest CHECKSUM OK");
    return 0;
}
#elif defined(CAN_MSG_CHECKSUM_ONLY)
#include <cstring>
#include "can_msg.h"
#include "dbc.h"

int main()
{
    const uint8_t s[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', 0};    // published check values
    assert(Dbc::Checksum(CanOutputKind::Crc8SaeJ1850, s, 10, 9) == 0x4B);
    assert(Dbc::Checksum(CanOutputKind::Crc8Autosar, s, 10, 9) == 0xDF);

    float in = 1;
    pVarMap[3] = &in;
    Config_CanMsg c{};
    c.bEnabled = true; c.nInput = 3; c.nID = 0x123; c.nDLC = 8; c.nInterval = 100;
    for (uint8_t i = 0; i < 8; i++) c.nData[i] = 0x10 * i + 1;
    c.nCounterByte = 0xFF;
    static CanMsg k; k.SetConfig(&c);   // static: zeroed like the firmware's globals
    uint8_t d[8];

    k.Fill(d); assert(std::memcmp(d, c.nData, 8) == 0);            // no counter, no checksum: the data as set

    c.nCounterByte = 1; c.nData[1] = 0xA7;                          // counter in the low nibble, high nibble kept
    for (int n = 0; n < 17; n++) { k.Fill(d); assert(d[1] == (0xA0 | (n & 0x0F))); }

    c.eChecksum = CanOutputKind::ChecksumXor; c.nChecksumByte = 7;
    k.Fill(d);                                                      // counter first, checksum over it
    uint8_t x = 0; for (int i = 0; i < 7; i++) x ^= d[i];
    assert(d[1] == 0xA1 && d[7] == x);
    c.eChecksum = CanOutputKind::ChecksumSum; k.Fill(d);
    x = 0; for (int i = 0; i < 7; i++) x += d[i];
    assert(d[7] == x);
    c.eChecksum = CanOutputKind::Crc8SaeJ1850; c.nChecksumByte = 0; k.Fill(d);
    assert(d[0] == Dbc::Checksum(CanOutputKind::Crc8SaeJ1850, d, 8, 0));

    c.nDLC = 4; c.nChecksumByte = 5; c.nCounterByte = 6; k.Fill(d);   // past the DLC: off
    assert(d[5] == c.nData[5] && d[6] == c.nData[6]);
    c.eChecksum = CanOutputKind::RollingCounter; c.nChecksumByte = 3; k.Fill(d);   // 1 is no checksum kind
    assert(d[3] == c.nData[3]);

    std::puts("host_selftest CAN_MSG_CHECKSUM OK");
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
