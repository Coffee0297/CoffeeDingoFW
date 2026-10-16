// Host-side self-check for the 5.6 blocks (Math, CAN Message, Ignition). From the repo root:
//   g++ -std=c++20 -DDINGO_HOST_TEST -DVAR_MAP_SIZE=16 -I functions -I core tests/host_56.cpp functions/math_block.cpp functions/can_msg.cpp functions/ignition.cpp -o build/host_56 && build/host_56
#include <cassert>
#include <cmath>
#include <cstdio>
#include "math_block.h"
#include "can_msg.h"
#include "ignition.h"

float *pVarMap[VAR_MAP_SIZE];
static float v[VAR_MAP_SIZE];

static void math()
{
    assert(MathEval(MathOp::Add, 2, 3) == 5);
    assert(MathEval(MathOp::Sub, 2, 3) == -1);
    assert(MathEval(MathOp::Mul, 2, 3) == 6);
    assert(MathEval(MathOp::Div, 6, 3) == 2);
    assert(MathEval(MathOp::Div, 6, 0) == 0);          // no inf / NaN into the outputs
    assert(MathEval(MathOp::Min, 2, 3) == 2);
    assert(MathEval(MathOp::Max, 2, 3) == 3);
    assert(MathEval(MathOp::Average, 2, 3) == 2.5f);

    // oil - coolant, then x0.5 + 10; B a constant when nInputB is 0
    v[1] = 110; v[2] = 90;
    Config_Math c{}; c.bEnabled = true; c.eOp = MathOp::Sub; c.nInputA = 1; c.nInputB = 2; c.fFactor = 0.5f; c.fOffset = 10;
    MathBlock m; m.SetConfig(&c); m.Update();
    assert(m.fVal == 20);
    c.nInputB = 0; c.fConstB = 100; m.SetConfig(&c); m.Update();
    assert(m.fVal == 15);
    c.bEnabled = false; m.Update();
    assert(m.fVal == 0);
}

static void canMsg()
{
    v[3] = 0;
    Config_CanMsg c{}; c.bEnabled = true; c.nInput = 3; c.nDLC = 8; c.eMode = CanMsgMode::Repeat; c.nIntervalMs = 100;
    CanMsg k; k.SetConfig(&c);
    assert(!k.Due(0));                                  // off: nothing
    v[3] = 1;
    assert(k.Due(10));                                  // turns true: at once
    assert(!k.Due(50));
    assert(k.Due(110));                                 // then every interval
    assert(k.fVal == 1);
    v[3] = 0; assert(!k.Due(300)); assert(k.fVal == 0);

    c.eMode = CanMsgMode::Once;
    v[3] = 1; assert(k.Due(400));                       // one frame each time it turns true
    assert(!k.Due(600)); assert(!k.Due(900));
    v[3] = 0; k.Due(1000); v[3] = 1; assert(k.Due(1010));
    c.nInput = 0; assert(!k.Due(2000));                 // no trigger signal: never sends
}

static void ignition()
{
    // 4 ignition switch, 5 start button, 6 clutch, 7 running
    v[4] = v[5] = v[6] = v[7] = 0;
    Config_Ignition c{}; c.bEnabled = true; c.eMode = IgnMode::SwitchButton; c.eRole = IgnRole::Main;
    c.nIgnInput = 4; c.nStartInput = 5; c.nCrankAllowInput = 6; c.nRunningInput = 7;
    c.nMaxCrankMs = 2000; c.bCrankWhileHeld = true; c.nDashHoldMs = 1000;
    Ignition g; g.SetConfig(&c);
    uint32_t t = 0;
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
    // max crank time, then a release is needed
    v[5] = 1; step(); assert(g.eState == IgnState::Cranking);
    step(2100); assert(g.eState == IgnState::On);
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
    v[7] = 1; step(); assert(g.eState == IgnState::Running);
    v[6] = 0; press(); assert(g.eState == IgnState::Off);             // running + press: everything off
    v[7] = 0;

    // key: ON held is the ignition, START cranks while held
    c.eMode = IgnMode::Key; v[6] = 1;
    v[4] = 1; step(); assert(g.eState == IgnState::On);
    v[5] = 1; step(); assert(g.eState == IgnState::Cranking);
    v[5] = 0; step(); assert(g.eState == IgnState::On);
    v[4] = 0; step(); assert(g.eState == IgnState::Off);

    // the main module's frame, and a follower that copies it and holds it when the link drops
    v[4] = 1; c.eMode = IgnMode::SwitchButton; step(200);
    uint8_t frame[8];
    assert(g.StateFrame(t, frame) && frame[0] == 'I' && frame[1] == (uint8_t)IgnState::On && (frame[2] & 0x01));
    assert(!g.StateFrame(t + 50, frame));                              // 10 Hz

    Config_Ignition f{}; f.bEnabled = true; f.eRole = IgnRole::Follower; f.nMainStateId = 0x680 + IGN_STATE_OFFSET;
    Ignition fol; fol.SetConfig(&f);
    assert(!fol.CheckMsg(0x681, frame, 8, t));                         // not the main module's state id
    assert(fol.CheckMsg(0x680 + IGN_STATE_OFFSET, frame, 8, t));
    fol.Update(t); assert(fol.fIgnition == 1 && fol.fLinkOk == 1 && fol.eState == IgnState::On);
    fol.Update(t + 1000); assert(fol.fIgnition == 1 && fol.fLinkOk == 0);   // link lost: holds the last state
}

int main()
{
    for (int i = 0; i < VAR_MAP_SIZE; i++) pVarMap[i] = &v[i];
    math();
    canMsg();
    ignition();
    std::puts("host_56 OK");
    return 0;
}
