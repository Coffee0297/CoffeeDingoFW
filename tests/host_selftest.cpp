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
//   g++ -std=c++20 -DCAN_MSG_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/can_msg.cpp utils/dbc.cpp -o build/host_can_msg && build/host_can_msg
//   g++ -std=c++20 -DCAN_MSG_CHECKSUM_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/can_msg.cpp utils/dbc.cpp -o build/host_can_msg_cs && build/host_can_msg_cs
//   g++ -std=c++20 -Wall -DLATCH_GATE_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/latch_gate.cpp -o build/host_latch_gate && build/host_latch_gate
//   g++ -std=c++20 -DPID_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/pid.cpp -o build/host_pid && build/host_pid
//   g++ -std=c++20 -Wall -DRETAIN_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/input.cpp -o build/host_retain && build/host_retain
//   g++ -std=c++20 -DOVL_ONLY -DNUM_OUTPUTS=2 -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/overload_log.cpp -o build/host_ovl && build/host_ovl
//   g++ -std=c++20 -Wall -DCOMFORT_BLINK_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/comfort_blink.cpp -o build/host_comfort_blink && build/host_comfort_blink
//   g++ -std=c++20 -Wall -DIGNITION_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/ignition.cpp -o build/host_ignition && build/host_ignition
//   g++ -std=c++20 -Wall -DIGN_STATUS_ID_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/ignition.cpp -o build/host_ign_status_id && build/host_ign_status_id
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

    // the first pass only samples: an input already on at power-up sends no Once frame
    static CanMsg k2; c.bEnabled = true; c.eMode = CanMsgMode::Once; in = 1; k2.SetConfig(&c);
    nHostTimeMs = 9000; assert(!k2.Due(false));
    in = 0; nHostTimeMs = 9010; k2.Due(false); in = 1; nHostTimeMs = 9020; assert(k2.Due(false));

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
#elif defined(IGNITION_ONLY)
#include "ignition.h"

static float v[VAR_MAP_SIZE];
static Ignition g, fol, fol2;   // static: zeroed like the firmware's globals

int main()
{
    for (int i = 0; i < VAR_MAP_SIZE; i++) pVarMap[i] = &v[i];
    // 4 ignition switch, 5 start button, 6 clutch, 7 running
    Config_Ignition c{}; c.bEnabled = true; c.eMode = IgnMode::SwitchButton; c.eRole = IgnRole::Main;
    c.nIgnInput = 4; c.nStartInput = 5; c.nCrankAllowInput = 6; c.nRunningInput = 7;
    c.nMaxCrankMs = 2000; c.bCrankWhileHeld = true; c.nDashHoldMs = 1000;
    g.SetConfig(&c);
    uint32_t t = 0xFFFFF000;                                          // crosses the 32-bit ms wrap below
    auto step = [&](uint32_t dt = 10) { t += dt; g.Update(t, 0x680, false); };

    // switch + button: ignition with the switch; cranks only with the clutch, while held
    step(); assert(g.eState == IgnState::Off && g.fDash == 0);
    v[4] = 1; step(); assert(g.eState == IgnState::On && g.fIgnition == 1 && g.fReady == 1 && g.fAccessory == 1);
    v[5] = 1; step(); assert(g.eState == IgnState::On);             // no clutch: no crank
    v[5] = 0; step(); v[6] = 1; v[5] = 1; step();
    assert(g.eState == IgnState::Cranking && g.fCranking == 1 && g.fAccessory == 0 && g.fReady == 0);
    v[5] = 0; step(); assert(g.eState == IgnState::On);             // released: stops
    v[5] = 1; step(); assert(g.eState == IgnState::Cranking);
    v[7] = 1; step(); assert(g.eState == IgnState::Running && g.fRunning == 1 && g.fReady == 0);
    v[5] = 0; step(); v[5] = 1; step(); assert(g.eState == IgnState::Running);   // no crank while running
    v[5] = 0; v[7] = 0; step(); assert(g.eState == IgnState::On);   // stalled
    // max crank time (across the wrap), then a release is needed
    v[5] = 1; step(); assert(g.eState == IgnState::Cranking);
    step(1980); assert(g.eState == IgnState::Cranking);
    step(30); assert(g.eState == IgnState::On);
    step(); assert(g.eState == IgnState::On);
    v[5] = 0; step(); v[5] = 1; step(); assert(g.eState == IgnState::Cranking);
    v[5] = 0; v[6] = 0;
    // switch off: dash holds for 1 s
    v[4] = 0; step(); assert(g.eState == IgnState::Off && g.fIgnition == 0 && g.fDash == 1);
    step(500); assert(g.fDash == 1);
    step(600); assert(g.fDash == 0);

    // button only: press = on; clutch + press = crank; press while running = off; press while cranking = abort
    c.eMode = IgnMode::ButtonOnly; c.bCrankWhileHeld = false; v[4] = 0; v[7] = 0;
    auto press = [&] { v[5] = 1; step(); v[5] = 0; step(); };
    press(); assert(g.eState == IgnState::On);
    press(); assert(g.eState == IgnState::Off);
    v[6] = 1; press(); assert(g.eState == IgnState::Cranking);      // from off, clutch held: on and crank at once
    press(); assert(g.eState == IgnState::On);                        // pressed during cranking: aborts
    press(); assert(g.eState == IgnState::Cranking);
    step(2000); assert(g.eState == IgnState::On); step();            // one press still stops at the max time
    press(); assert(g.eState == IgnState::Cranking);
    v[7] = 1; step(); assert(g.eState == IgnState::Running);
    v[6] = 0; press(); assert(g.eState == IgnState::Off);             // running + press: everything off
    v[7] = 0;

    // key: ON held is the ignition, START cranks while held
    c.eMode = IgnMode::Key; v[6] = 1;
    v[4] = 1; step(); assert(g.eState == IgnState::On);
    v[5] = 1; step(); assert(g.eState == IgnState::Cranking);
    v[5] = 0; step(); assert(g.eState == IgnState::On);
    v[4] = 0; step(); assert(g.eState == IgnState::Off);

    // sync frame on 0x6F1: [state, flags], at once on a change, then every 100 ms
    v[4] = 1; c.eMode = IgnMode::SwitchButton; c.nSyncId = 0x6F1; step(200);
    uint8_t frame[8] = {};
    assert(g.nSyncSid == 0x6F1 && g.SyncFrame(frame, t) && frame[0] == (uint8_t)IgnState::On && frame[1] == 0);
    assert(!g.SyncFrame(frame, t + 50) && g.SyncFrame(frame, t + 100));
    v[4] = 0; step(); assert(g.SyncFrame(frame, t) && frame[0] == (uint8_t)IgnState::Off);   // change: at once
    v[4] = 1; step();
    // offset mode: base 0x680 + 0x30; inside its own frames (base .. base+2+NUM_TX_MSGS-1) or past 0x7FF: no sync
    c.eSyncMode = IgnSyncMode::BaseOffset; c.nSyncId = 0x30;
    assert(Ignition::SyncId(c, 0x680) == 0x6B0);
    c.nSyncId = 36; assert(Ignition::SyncId(c, 0x680) == IGN_NO_SYNC);   // base+36 = its own Msg 34
    c.nSyncId = 37; assert(Ignition::SyncId(c, 0x680) == 0x6A5);
    c.nSyncId = 0x200; assert(Ignition::SyncId(c, 0x680) == IGN_NO_SYNC);   // 0x880 > 0x7FF
    c.nSyncId = 0x30; g.Update(t, 0x680, false); assert(g.nSyncSid == 0x6B0);
    c.eSyncMode = IgnSyncMode::Absolute; c.nSyncId = 0x681;              // absolute, on its own Msg -1: refused
    g.Update(t, 0x680, false); assert(g.nSyncSid == IGN_NO_SYNC && !g.SyncFrame(frame, t + 1000));
    g.StatusFrame(frame); assert(frame[3] == 3);                          // status: sync id invalid
    c.nSyncId = 0x6F1; g.Update(t, 0x680, false);
    g.StatusFrame(frame); assert((frame[0] & 0x0F) == 1 && (frame[0] >> 4) == 0 && frame[3] == 0 && (frame[6] & 0x21) == 0x21);
    c.eRole = IgnRole::Standalone; g.Update(t, 0x680, false); assert(!g.SyncFrame(frame, t + 5000) && g.eState == IgnState::On);
    c.eRole = IgnRole::Main;

    // follower: std id, DLC >= 2, state byte; offset mode resolves against the main module's base (nSyncBase)
    Config_Ignition f{}; f.bEnabled = true; f.eRole = IgnRole::Follower; f.nSyncId = 0x30;
    f.eSyncMode = IgnSyncMode::BaseOffset; f.nSyncBase = 0x680;
    fol.SetConfig(&f);
    auto rx = [&](uint32_t sid, uint8_t st, uint8_t dlc = 2, uint8_t ide = CAN_IDE_STD) {
        CANRxFrame r{}; r.SID = sid; r.EID = sid; r.IDE = ide; r.DLC = dlc; r.data8[0] = st; fol.CheckMsg(r, t); };
    fol.Update(t, 0x6C0, false); assert(fol.nSyncSid == 0x6B0 && !fol.SyncFrame(frame, t));   // followers don't send
    assert(fol.fLinkOk == 0 && fol.fIgnition == 0);                    // nothing heard yet: no link
    fol.StatusFrame(frame); assert(frame[3] == 2 && (frame[0] >> 4) == 1);
    rx(0x6B1, 1); fol.Update(t, 0x6C0, false); assert(fol.fLinkOk == 0);       // not the sync id
    rx(0x6B0, 1, 1); fol.Update(t, 0x6C0, false); assert(fol.fLinkOk == 0);    // too short
    rx(0x6B0, 1, 2, CAN_IDE_EXT); fol.Update(t, 0x6C0, false); assert(fol.fLinkOk == 0);   // extended
    rx(0x6B0, 1); fol.Update(t, 0x6C0, false);
    assert(fol.fIgnition == 1 && fol.fLinkOk == 1 && fol.eState == IgnState::On && fol.fReady == 1 && fol.fAccessory == 1);
    fol.StatusFrame(frame); assert(frame[3] == 1);
    fol.Update(t + 1000, 0x6C0, false); assert(fol.fIgnition == 1 && fol.fLinkOk == 0);   // link lost: holds the last state
    // ... but not cranking: the starter drops when the main module goes quiet
    t += 2000; rx(0x6B0, 2); fol.Update(t, 0x6C0, false); assert(fol.fCranking == 1 && fol.fAccessory == 0);
    fol.Update(t + 600, 0x6C0, false); assert(fol.fLinkOk == 0 && fol.fCranking == 0 && fol.fIgnition == 1 && fol.fAccessory == 1);
    rx(0x6B0, 9); fol.Update(t, 0x6C0, false); assert(fol.eState == IgnState::Off);   // unknown state reads Off
    f.eSyncMode = IgnSyncMode::Absolute; f.nSyncId = 0x6F1; fol.Update(t, 0x6C0, false); assert(fol.nSyncSid == 0x6F1);
    rx(0x6F1, 3); fol.Update(t, 0x6C0, false); assert(fol.eState == IgnState::Running && fol.fRunning == 1);
    t -= 2000;

    // power-up interlock: a start held when the block starts must be released first
    c.bEnabled = false; step(); v[5] = 1; c.bEnabled = true; step(); step();
    assert(g.eState == IgnState::On);                                 // switch on, start held, clutch: no crank
    v[5] = 0; step(); v[5] = 1; step(); assert(g.eState == IgnState::Cranking);
    v[5] = 0; step();
    // stop input (var 2): off while it is on, back with the switch after
    c.nStopInput = 2; g.SetConfig(&c); v[2] = 1; step(); assert(g.eState == IgnState::Off);
    v[2] = 0; step(); assert(g.eState == IgnState::On);

    // start button from a CAN frame: ext 0x18FF0001, byte 2, mask 0x04, 500 ms timeout
    c.bCrankWhileHeld = true; c.nButtonMask = 0x04; c.nButtonIde = 1; c.nButtonId = 0x18FF0001; c.nButtonByte = 2; c.nButtonTimeoutMs = 500;
    auto btn = [&](uint8_t ide, uint8_t b2, uint8_t dlc = 8) {
        CANRxFrame r{}; r.IDE = ide; r.EID = 0x18FF0001; r.SID = 0x001; r.DLC = dlc; r.data8[2] = b2; g.CheckMsg(r, t); };
    v[5] = 1; step(); assert(g.eState == IgnState::On);              // the start var doesn't count any more
    v[5] = 0;
    btn(CAN_IDE_STD, 0x04); step(); assert(g.eState == IgnState::On);       // standard frame: not it
    btn(CAN_IDE_EXT, 0x04, 2); step(); assert(g.eState == IgnState::On);    // too short for byte 2
    btn(CAN_IDE_EXT, 0x04); step(); assert(g.eState == IgnState::Cranking);
    step(400); assert(g.eState == IgnState::Cranking);                // within the timeout
    step(200); assert(g.eState == IgnState::On);                      // sender quiet 610 ms: released
    btn(CAN_IDE_EXT, 0x00); step(); btn(CAN_IDE_EXT, 0x04); step(); assert(g.eState == IgnState::Cranking);
    btn(CAN_IDE_EXT, 0x00); step(); assert(g.eState == IgnState::On);
    c.nButtonMask = 0;

    // output roles
    assert(g.RoleVar(IgnOutputRole::Ignition) == &g.fIgnition && g.RoleVar(IgnOutputRole::Starter) == &g.fCranking &&
           g.RoleVar(IgnOutputRole::Accessory) == &g.fAccessory && g.RoleVar(IgnOutputRole::Dash) == &g.fDash &&
           !g.RoleVar(IgnOutputRole::None));

    // dash power down: grace 1000 ms (cancelled by the ignition), halting 1500 ms (not), then 2 s unpowered
    c.nDashOffDelayMs = 1500;
    v[4] = 0; step(); assert(g.eDash == IgnDash::Grace && g.fDash == 1 && g.fDashShutdown == 0);
    v[4] = 1; step(); assert(g.eDash == IgnDash::On);
    v[4] = 0; step(); step(1000); assert(g.eDash == IgnDash::Halting && g.fDash == 1 && g.fDashShutdown == 1);
    g.StatusFrame(frame); assert(frame[1] == (uint8_t)IgnDash::Halting && (frame[6] & 0x44) == 0x44);
    v[4] = 1; step(); assert(g.eDash == IgnDash::Halting);           // halting can't be called off
    step(1500); assert(g.eDash == IgnDash::Restart && g.fDash == 0 && g.fDashShutdown == 0);
    step(1990); assert(g.eDash == IgnDash::Restart);
    step(20); step(); assert(g.eDash == IgnDash::On && g.fDash == 1);   // then on again with the ignition

    // door (var 3): dash on for 3 s from the edge with the ignition off, even if the door stays open
    v[4] = 0; c.nDashOffDelayMs = 0; c.nDoorInput = 3; c.nDoorOnMs = 3000; g.SetConfig(&c);
    step(); step(1100); assert(g.eDash == IgnDash::Off);
    v[3] = 1; step(); assert(g.fDash == 1 && g.fIgnition == 0);
    step(2000); assert(g.fDash == 1);
    step(1000); assert(g.eDash == IgnDash::Grace);
    step(1000); assert(g.fDash == 0);
    v[3] = 0; step(); c.nDoorInput = 0; g.SetConfig(&c);

    // ignition sleep, main: 5 s after off = 3 s counting, sleep flag 300 ms, 2 s quiet; USB/config session holds it
    c.nSleepDelayMs = 5000; c.nDashHoldMs = 0; v[4] = 1; step(); v[4] = 0; step();
    g.StatusFrame(frame); assert(frame[2] == (uint8_t)IgnSleep::Counting && frame[4] == 5 && frame[5] == 0);
    step(2980); assert(g.eSleep == IgnSleep::Counting && !g.TxQuiet());
    t += 20; g.Update(t, 0x680, true); assert(g.eSleep == IgnSleep::Held);
    step(); assert(!g.TxQuiet() && g.SyncFrame(frame, t) && frame[0] == 0 && frame[1] == 0x01);   // sleep flag first
    step(300); assert(g.TxQuiet() && !g.SleepRequest());
    step(1990); assert(!g.SleepRequest());
    step(20); assert(g.SleepRequest() && g.eSleep == IgnSleep::Sleep);
    v[4] = 1; step(); assert(!g.TxQuiet() && g.eSleep == IgnSleep::Awake);   // ignition back: awake
    v[4] = 0; c.nSleepDelayMs = 0; step(); assert(g.eSleep == IgnSleep::Disabled);

    // ignition sleep, follower: quiet with the main module's flag (kept after its frames stop); alone, its own delay
    Config_Ignition c2{}; c2.bEnabled = true; c2.eRole = IgnRole::Follower; c2.nSyncId = 0x6F1; c2.nSleepDelayMs = 4000;
    fol2.SetConfig(&c2);
    uint32_t u = 1000;
    auto sy = [&](uint8_t st, uint8_t fl) {
        CANRxFrame r{}; r.SID = 0x6F1; r.IDE = CAN_IDE_STD; r.DLC = 2; r.data8[0] = st; r.data8[1] = fl; fol2.CheckMsg(r, u); };
    fol2.Update(u, 0x6C0, false); sy(0, 0); fol2.Update(u += 10, 0x6C0, false);
    assert(fol2.eSleep == IgnSleep::Following);
    u += 10000; sy(0, 0); fol2.Update(u, 0x6C0, false); assert(fol2.eSleep == IgnSleep::Following && !fol2.TxQuiet());
    sy(0, 1); fol2.Update(u += 10, 0x6C0, false); assert(fol2.TxQuiet());   // no announce of its own
    fol2.Update(u += 600, 0x6C0, false); assert(fol2.fLinkOk == 0 && fol2.TxQuiet());
    fol2.Update(u += 1400, 0x6C0, false); assert(fol2.SleepRequest());
    c2.bEnabled = false; fol2.Update(u, 0x6C0, false); c2.bEnabled = true;
    fol2.Update(u += 10, 0x6C0, false); sy(1, 0); fol2.Update(u += 10, 0x6C0, false); sy(0, 0); fol2.Update(u += 10, 0x6C0, false);
    fol2.Update(u += 600, 0x6C0, false); assert(fol2.fLinkOk == 0 && fol2.eSleep == IgnSleep::Counting);
    fol2.Update(u += 1500, 0x6C0, false); assert(fol2.TxQuiet());

    c.bEnabled = false; step(); assert(g.fIgnition == 0 && !g.SyncFrame(frame, t + 9999));
    std::puts("host_ignition OK");
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
#elif defined(IGN_STATUS_ID_ONLY)
#include "ignition.h"

int main()
{
    // PDM at 0x680: own frames 0x680..0x6A4, status slot base+36 = 0x6A4, sync 0x6F1
    Config_Ignition c{}; c.bEnabled = true; c.eRole = IgnRole::Main; c.nSyncId = 0x6F1;
    assert(Ignition::StatusId(c, 0x680, 0x6A4) == 0x6A4);                 // 0: its own slot
    c.nStatusId = 0x7A0; assert(Ignition::StatusId(c, 0x680, 0x6A4) == 0x7A0);
    c.nStatusId = 0x6A4; assert(Ignition::StatusId(c, 0x680, 0x6A4) == 0x6A4);   // = its slot: fine
    c.nStatusId = 0x685; assert(Ignition::StatusId(c, 0x680, 0x6A4) == IGN_NO_SYNC);   // its own Msg 3
    c.nStatusId = 0x680; assert(Ignition::StatusId(c, 0x680, 0x6A4) == IGN_NO_SYNC);   // its base
    c.nStatusId = 0x6A5; assert(Ignition::StatusId(c, 0x680, 0x6A4) == 0x6A5);   // just past its frames
    c.nStatusId = 0x67F; assert(Ignition::StatusId(c, 0x680, 0x6A4) == 0x67F);   // just below its base
    c.nStatusId = 0x800; assert(Ignition::StatusId(c, 0x680, 0x6A4) == IGN_NO_SYNC);
    c.nStatusId = 0x6F1; assert(Ignition::StatusId(c, 0x680, 0x6A4) == IGN_NO_SYNC);   // its sync frame
    c.eRole = IgnRole::Follower; assert(Ignition::StatusId(c, 0x680, 0x6A4) == IGN_NO_SYNC);   // the id it follows
    c.eRole = IgnRole::Standalone; assert(Ignition::StatusId(c, 0x680, 0x6A4) == 0x6F1);   // sends no sync
    // offset sync: 0x680 + 0x40 = 0x6C0
    c.eRole = IgnRole::Main; c.eSyncMode = IgnSyncMode::BaseOffset; c.nSyncId = 0x40;
    c.nStatusId = 0x6C0; assert(Ignition::StatusId(c, 0x680, 0x6A4) == IGN_NO_SYNC);
    c.nStatusId = 0x6C1; assert(Ignition::StatusId(c, 0x680, 0x6A4) == 0x6C1);

    std::puts("host_ign_status_id OK");
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
