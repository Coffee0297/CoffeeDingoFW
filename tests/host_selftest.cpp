// Host-side self-check for the pure function logic (no ChibiOS needed). From the repo root:
//   g++ -std=c++20 -DDINGO_HOST_TEST -I functions -I core tests/host_selftest.cpp functions/table.cpp functions/timer.cpp -o build/host_selftest && build/host_selftest
// Counter (it needs a bigger var map than the others, so it builds on its own):
//   g++ -std=c++20 -DDINGO_HOST_TEST -DCOUNTER_ONLY -I functions -I core tests/host_selftest.cpp functions/counter.cpp -o build/host_counter && build/host_counter
// Virtual input NOR and Condition AND/NAND (5.6.6):
//   g++ -std=c++20 -DDINGO_HOST_TEST -DLOGIC_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/virtual_input.cpp functions/input.cpp functions/condition.cpp -o build/host_logic && build/host_logic
#include <cassert>
#include <cmath>
#include <cstdio>
#ifdef LOGIC_ONLY
#include "virtual_input.h"
#include "condition.h"
float *pVarMap[VAR_MAP_SIZE];
uint32_t nHostTimeMs = 0;

int main()
{
    float zero = 0, a = 0, b = 0;
    pVarMap[0] = &zero; pVarMap[1] = &a; pVarMap[2] = &b;
    Config_VirtualInput c{};
    c.bEnabled = true; c.nVar0 = 1; c.nVar1 = 2; c.eCond0 = BoolOperator::Nor; c.eMode = InputMode::Momentary;
    VirtualInput v; v.SetConfig(&c);
    for (int i = 0; i < 4; i++) { a = i & 1; b = i >> 1; v.Update(); assert(v.fVal == (i == 0 ? 1 : 0)); }   // on only when neither

    Config_Condition k{};
    k.bEnabled = true; k.nInput = 1; k.eOperator = Operator::BitwiseAnd; k.fArg = 4; k.fArgOff = 4;
    Condition cd; cd.SetConfig(&k);
    a = 6; cd.Update(); assert(cd.fVal == 1);   // was 4 (the masked number)
    a = 3; cd.Update(); assert(cd.fVal == 0);
    k.eOperator = Operator::BitwiseNand;
    a = 3; cd.Update(); assert(cd.fVal == 1);
    a = 6; cd.Update(); assert(cd.fVal == 0);   // was ~4, non-zero
    std::puts("host_logic OK");
    return 0;
}
#elif defined(COUNTER_ONLY)
#include "counter.h"
float *pVarMap[VAR_MAP_SIZE];
uint32_t nHostTimeMs = 0;

// A falling-edge reset (e.g. "off when the ignition goes off") clears the count once; counting works again
// right after. It used to return before remembering the inputs, so the reset re-fired on every cycle.
int main()
{
    float zero = 0, inc = 0, rst = 1;
    pVarMap[0] = &zero; pVarMap[1] = &inc; pVarMap[2] = &rst;
    Config_Counter c{};
    c.bEnabled = true; c.nIncInput = 1; c.nDecInput = 0; c.nResetInput = 2;
    c.nMinCount = 0; c.nMaxCount = 1; c.bWrapAround = true;
    c.eIncEdge = InputEdge::Rising; c.eDecEdge = InputEdge::Rising; c.eResetEdge = InputEdge::Falling;
    Counter k; k.SetConfig(&c); k.fVal = 0;
    auto step = [&] { k.Update(); nHostTimeMs += 10; };
    auto press = [&] { inc = 1; step(); inc = 0; step(); };
    step();
    press(); assert(k.fVal == 1);
    press(); assert(k.fVal == 0);     // toggles (wrap 1 -> 0)
    press(); assert(k.fVal == 1);
    rst = 0; step(); assert(k.fVal == 0);   // reset input falls: cleared
    step(); step();
    press(); assert(k.fVal == 1);     // and it counts again with the reset input still low
    std::puts("host_counter OK");
    return 0;
}
#else
#include "table.h"
#include "timer.h"
#include "pwm_meter.h"

float *pVarMap[VAR_MAP_SIZE];
static bool near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

static void TestTable()
{
    Config_Table c{};
    c.bEnabled = true; c.nXSize = 3; c.nYSize = 2;
    const float xs[] = {0, 10, 20}, ys[] = {0, 100};
    for (int i = 0; i < 3; i++) c.fXAxis[i] = xs[i];
    for (int i = 0; i < 2; i++) c.fYAxis[i] = ys[i];
    // row y=0: 0 10 20 ; row y=100: 100 110 120
    for (int x = 0; x < 3; x++) { c.fCell[0][x] = xs[x]; c.fCell[1][x] = 100 + xs[x]; }

    assert(near(Table::Interpolate(c, 0, 0), 0));
    assert(near(Table::Interpolate(c, 5, 0), 5));        // linear along x
    assert(near(Table::Interpolate(c, 15, 0), 15));
    assert(near(Table::Interpolate(c, 10, 50), 60));     // linear along y
    assert(near(Table::Interpolate(c, 5, 50), 55));      // bilinear
    assert(near(Table::Interpolate(c, -99, 0), 0));      // clamp low
    assert(near(Table::Interpolate(c, 999, 100), 120));  // clamp high on both axes
    c.nYSize = 1;                                          // 1-D curve ignores y
    assert(near(Table::Interpolate(c, 15, 12345), 15));
    c.nXSize = 0; c.nYSize = 0;                            // degenerate sizes -> cell[0][0]
    assert(near(Table::Interpolate(c, 7, 7), 0));
}

static void TestTimer()
{
    float in = 0; pVarMap[0] = &in;
    Config_Timer cfg{}; cfg.bEnabled = true; cfg.nInput = 0; cfg.eEdge = InputEdge::Rising; cfg.nPreset = 100;
    Timer t; t.SetConfig(&cfg);

    // TON: on only after 100 ms of active input, off immediately on release
    cfg.eMode = TimerMode::OnDelay;
    t.Update(0);  assert(t.fVal == 0);
    in = 1; t.Update(10); assert(t.fVal == 0);
    t.Update(109); assert(t.fVal == 0);
    t.Update(110); assert(t.fVal == 1);
    in = 0; t.Update(120); assert(t.fVal == 0);

    // TOF: follows the input on, holds 100 ms after release
    cfg.eMode = TimerMode::OffDelay; t = Timer(); t.SetConfig(&cfg); in = 0; t.Update(200);
    in = 1; t.Update(210); assert(t.fVal == 1);
    in = 0; t.Update(220); assert(t.fVal == 1);
    t.Update(319); assert(t.fVal == 1);
    t.Update(320); assert(t.fVal == 0);

    // TP: a 100 ms pulse per activation, release doesn't shorten it, re-activation retriggers
    cfg.eMode = TimerMode::Pulse; t = Timer(); t.SetConfig(&cfg); in = 0; t.Update(400);
    in = 1; t.Update(410); assert(t.fVal == 1);
    in = 0; t.Update(450); assert(t.fVal == 1);
    t.Update(510); assert(t.fVal == 0);
    in = 1; t.Update(520); assert(t.fVal == 1);
    in = 0; t.Update(530); in = 1; t.Update(600); assert(t.fVal == 1);   // retrigger at 600
    t.Update(690); assert(t.fVal == 1);
    t.Update(700); assert(t.fVal == 0);

    // Falling edge = active while the input is false (e.g. "ignition off for 100 ms")
    cfg.eMode = TimerMode::OnDelay; cfg.eEdge = InputEdge::Falling; t = Timer(); t.SetConfig(&cfg);
    in = 1; t.Update(800); assert(t.fVal == 0);
    in = 0; t.Update(810); t.Update(910); assert(t.fVal == 1);
    in = 1; t.Update(920); assert(t.fVal == 0);

    // disabled -> always 0
    cfg.bEnabled = false; in = 0; t.Update(1000); t.Update(1200); assert(t.fVal == 0);

    // a pulse from an input that is already on (Always On: a prime at power-up) also fires when the timer is
    // enabled later, even if that slot ran before (it used to remember the old input and see no new edge)
    cfg.eMode = TimerMode::Pulse; cfg.eEdge = InputEdge::Rising; cfg.bEnabled = true; t = Timer(); t.SetConfig(&cfg);
    in = 1; t.Update(2000); assert(t.fVal == 1); t.Update(2100); assert(t.fVal == 0);   // ran once
    cfg.bEnabled = false; t.Update(2200);                                                // slot freed
    cfg.bEnabled = true; t.Update(2300); assert(t.fVal == 1);                          // reused: pulses again
}

static void TestPwmMeter()
{
    const uint32_t clk = 180000000;                  // PDM HCLK: 100 Hz = 1.8 M cycles
    PwmMeter m{}; uint32_t t = 0xFFF00000u;          // start near the 32-bit wrap
    auto pulse = [&](uint32_t hi, uint32_t period) { m.Edge(true, t); m.Edge(false, t + hi); t += period; };
    m.Edge(false, t - 5);                            // a fall before the first rise is ignored
    for (int i = 0; i < 5; i++) pulse(450000, 1800000);   // 25 % at 100 Hz
    m.Edge(true, t);                                 // closes the 5th period
    PwmMeter::Sample s = m.Take();
    assert(s.periods == 5);
    assert(near(PwmMeter::Duty(s), 25.0f));
    assert(std::fabs(PwmMeter::Freq(s, clk) - 100.0f) < 0.01f);
    assert(m.Take().periods == 0);                   // Take() empties the sums

    m.Edge(false, t + 1620000); t += 1800000;        // one 90 % period (rise already seen above)
    m.Edge(true, t);
    s = m.Take();
    assert(s.periods == 1 && near(PwmMeter::Duty(s), 90.0f));
    assert(s.lastPeriod == 1800000);

    m.Reset(); m.Edge(true, 0); m.Edge(true, 1000);  // rise, rise without a fall: no period
    assert(m.Take().periods == 0 && PwmMeter::Duty(m.Take()) == 0.0f);
}

static void TestPwmGlitchFilter()
{
    // 25 % at 100 Hz (1.8 M cycles @ 180 MHz), filter 20 us = 3600 cycles
    PwmMeter m{}; m.nMinCycles = 3600; m.Reset();
    assert(m.nMinCycles == 3600);                    // Reset keeps the filter
    uint32_t t = 1000;
    auto edge = [&](bool h, uint32_t at) { m.Edge(h, at); };
    for (int i = 0; i < 4; i++)
    {
        edge(true, t);
        edge(false, t + 200000); edge(true, t + 200900);          // 5 us spike low during the high phase
        edge(false, t + 450000);
        edge(true, t + 1000000); edge(false, t + 1001800);        // 10 us spike high during the low phase
        t += 1800000;
    }
    edge(true, t); edge(false, t + 450000);                       // closes the last period (held edge committed)
    PwmMeter::Sample s = m.Take();
    assert(s.periods == 4);
    assert(near(PwmMeter::Duty(s), 25.0f));
    assert(std::fabs(PwmMeter::Freq(s, 180000000) - 100.0f) < 0.01f);

    // a real pulse narrower than the filter is dropped: 10 us high every 1 ms reads as no signal
    m.Reset(); t = 0;
    bool any = false;
    for (int i = 0; i < 10; i++) { any |= m.Edge(true, t); any |= m.Edge(false, t + 1800); t += 180000; }
    assert(m.Take().periods == 0);
    assert(!any);                                    // no edge passed: the input times out to its steady level

    // filter off: every edge counts (spike shortens the high time)
    PwmMeter n{}; n.Edge(true, 0); n.Edge(false, 100); n.Edge(true, 200); n.Edge(false, 1000); n.Edge(true, 2000);
    assert(n.Take().periods == 2);
}

int main()
{
    TestTable();
    TestTimer();
    TestPwmMeter();
    TestPwmGlitchFilter();
    std::puts("host_selftest OK");
    return 0;
}
#endif
