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
#elif defined(IGNITION_ONLY)
#include "ignition.h"

static float v[VAR_MAP_SIZE];
static Ignition g, fol;   // static: zeroed like the firmware's globals

int main()
{
    for (int i = 0; i < VAR_MAP_SIZE; i++) pVarMap[i] = &v[i];
    // 4 ignition switch, 5 start button, 6 clutch, 7 running
    Config_Ignition c{}; c.bEnabled = true; c.eMode = IgnMode::SwitchButton; c.eRole = IgnRole::Main;
    c.nIgnInput = 4; c.nStartInput = 5; c.nCrankAllowInput = 6; c.nRunningInput = 7;
    c.nMaxCrankMs = 2000; c.bCrankWhileHeld = true; c.nDashHoldMs = 1000;
    g.SetConfig(&c);
    uint32_t t = 0xFFFFF000;                                          // crosses the 32-bit ms wrap below
    auto step = [&](uint32_t dt = 10) { t += dt; g.Update(t); };

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

    // the main module's frame, and a follower that copies it
    v[4] = 1; c.eMode = IgnMode::SwitchButton; step(200);
    uint8_t frame[8];
    assert(g.StateFrame(frame) && frame[0] == 'I' && frame[1] == (uint8_t)IgnState::On && (frame[2] & 0x01));

    const uint16_t id = 0x680 + 32;                                   // a PDM's ignition state msg
    Config_Ignition f{}; f.bEnabled = true; f.eRole = IgnRole::Follower; f.nMainStateId = id;
    fol.SetConfig(&f);
    assert(!fol.StateFrame(frame));                                   // followers don't send
    fol.Update(t); assert(fol.fLinkOk == 0 && fol.fIgnition == 0);   // nothing heard yet: no link
    fol.CheckMsg(0x681, frame, 8, t); fol.Update(t); assert(fol.fLinkOk == 0);   // not the main module's id
    frame[0] = 'X'; fol.CheckMsg(id, frame, 8, t); fol.Update(t); assert(fol.fLinkOk == 0);   // no marker
    frame[0] = 'I'; fol.CheckMsg(id, frame, 8, t);
    fol.Update(t); assert(fol.fIgnition == 1 && fol.fLinkOk == 1 && fol.eState == IgnState::On);
    fol.Update(t + 1000); assert(fol.fIgnition == 1 && fol.fLinkOk == 0);   // link lost: holds the last state
    // ... but not cranking: the starter drops when the main module goes quiet
    frame[2] = 0x03; fol.CheckMsg(id, frame, 8, t + 2000);
    fol.Update(t + 2000); assert(fol.fCranking == 1 && fol.fAccessory == 0);
    fol.Update(t + 2600); assert(fol.fLinkOk == 0 && fol.fCranking == 0 && fol.fIgnition == 1 && fol.fAccessory == 1);

    c.bEnabled = false; step(); assert(g.fIgnition == 0 && !g.StateFrame(frame));
    std::puts("host_ignition OK");
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
