// Host-side self-check for the pure function logic (no ChibiOS, no board). tests/host/port.h stands in for
// the board's port.h. From the repo root:
//   g++ -std=c++20 -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/timer.cpp -o build/host_selftest && build/host_selftest
// Each feature adds its own section below, picked with -D<NAME>_ONLY, and its build line here.
//   g++ -std=c++20 -Wall -DPWM_METER_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp -o build/host_pwm_meter && build/host_pwm_meter
#include <cassert>
#include <cstdio>
#include "port.h"

float *pVarMap[VAR_MAP_SIZE];
uint32_t nHostTimeMs = 0;

#if 0 // feature sections: #elif defined(<NAME>_ONLY)
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
