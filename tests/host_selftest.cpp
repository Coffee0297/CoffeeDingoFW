// Host-side self-check for the pure function logic (no ChibiOS, no board). tests/host/port.h stands in for
// the board's port.h. From the repo root:
//   g++ -std=c++20 -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/timer.cpp -o build/host_selftest && build/host_selftest
//   g++ -std=c++20 -DCONDITION_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/condition.cpp -o build/host_condition && build/host_condition
//   g++ -std=c++20 -DANALOG_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/input.cpp -o build/host_analog && build/host_analog
//   g++ -std=c++20 -DCONDITION_HYST_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/condition.cpp -o build/host_condition_hyst && build/host_condition_hyst
//   g++ -std=c++20 -Wall -DCONDITION_DELAY_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/condition.cpp -o build/host_condition_delay && build/host_condition_delay
//   g++ -std=c++20 -DCAN_INPUT_TIMEOUT_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/can_input.cpp functions/input.cpp utils/dbc.cpp -o build/host_can_input_timeout && build/host_can_input_timeout
//   g++ -std=c++20 -DWIRE_LOAD_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp -o build/host_wire_load && build/host_wire_load
// Each feature adds its own section below, picked with -D<NAME>_ONLY, and its build line here.
//   g++ -std=c++20 -Wall -DPWM_METER_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp -o build/host_pwm_meter && build/host_pwm_meter
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
