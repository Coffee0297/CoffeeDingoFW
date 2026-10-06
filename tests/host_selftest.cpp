// Host-side self-check for the pure function logic (no ChibiOS needed). From the repo root:
//   g++ -std=c++20 -DDINGO_HOST_TEST -I functions -I core tests/host_selftest.cpp functions/table.cpp functions/timer.cpp -o build/host_selftest && build/host_selftest
#include <cassert>
#include <cmath>
#include <cstdio>
#include "table.h"
#include "timer.h"

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
}

int main()
{
    TestTable();
    TestTimer();
    std::puts("host_selftest OK");
    return 0;
}
