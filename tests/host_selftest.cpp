// Host-side self-check for the pure function logic (no ChibiOS, no board). tests/host/port.h stands in for
// the board's port.h. From the repo root:
//   g++ -std=c++20 -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/timer.cpp -o build/host_selftest && build/host_selftest
// Each feature adds its own section below, picked with -D<NAME>_ONLY, and its build line here.
#include <cassert>
#include <cstdio>
#include "port.h"

float *pVarMap[VAR_MAP_SIZE];
uint32_t nHostTimeMs = 0;

#if 0 // feature sections: #elif defined(<NAME>_ONLY)
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
