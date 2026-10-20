// Host-side self-check for the pure function logic (no ChibiOS, no board). tests/host/port.h stands in for
// the board's port.h. From the repo root:
//   g++ -std=c++20 -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/timer.cpp -o build/host_selftest && build/host_selftest
//   g++ -std=c++20 -Wall -DIGNITION_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/ignition.cpp -o build/host_ignition && build/host_ignition
//   g++ -std=c++20 -Wall -DIGN_STATUS_ID_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/ignition.cpp -o build/host_ign_status_id && build/host_ign_status_id
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
    auto step = [&](uint32_t dt = 10) { t += dt; g.Update(t, 0x680); };

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
    c.nSyncId = 32; assert(Ignition::SyncId(c, 0x680) == IGN_NO_SYNC);   // base+32 = its own Msg 30
    c.nSyncId = 33; assert(Ignition::SyncId(c, 0x680) == 0x6A1);
    c.nSyncId = 0x200; assert(Ignition::SyncId(c, 0x680) == IGN_NO_SYNC);   // 0x880 > 0x7FF
    c.nSyncId = 0x30; g.Update(t, 0x680); assert(g.nSyncSid == 0x6B0);
    c.eSyncMode = IgnSyncMode::Absolute; c.nSyncId = 0x681;              // absolute, on its own Msg -1: refused
    g.Update(t, 0x680); assert(g.nSyncSid == IGN_NO_SYNC && !g.SyncFrame(frame, t + 1000));
    g.StatusFrame(frame); assert(frame[3] == 3);                          // status: sync id invalid
    c.nSyncId = 0x6F1; g.Update(t, 0x680);
    g.StatusFrame(frame); assert((frame[0] & 0x0F) == 1 && (frame[0] >> 4) == 0 && frame[3] == 0 && (frame[6] & 0x21) == 0x21);
    c.eRole = IgnRole::Standalone; g.Update(t, 0x680); assert(!g.SyncFrame(frame, t + 5000) && g.eState == IgnState::On);
    c.eRole = IgnRole::Main;

    // follower: std id, DLC >= 2, state byte; offset mode resolves against the main module's base (nSyncBase)
    Config_Ignition f{}; f.bEnabled = true; f.eRole = IgnRole::Follower; f.nSyncId = 0x30;
    f.eSyncMode = IgnSyncMode::BaseOffset; f.nSyncBase = 0x680;
    fol.SetConfig(&f);
    auto rx = [&](uint32_t sid, uint8_t st, uint8_t dlc = 2, uint8_t ide = CAN_IDE_STD) {
        CANRxFrame r{}; r.SID = sid; r.EID = sid; r.IDE = ide; r.DLC = dlc; r.data8[0] = st; fol.CheckMsg(r, t); };
    fol.Update(t, 0x6C0); assert(fol.nSyncSid == 0x6B0 && !fol.SyncFrame(frame, t));   // followers don't send
    assert(fol.fLinkOk == 0 && fol.fIgnition == 0);                    // nothing heard yet: no link
    fol.StatusFrame(frame); assert(frame[3] == 2 && (frame[0] >> 4) == 1);
    rx(0x6B1, 1); fol.Update(t, 0x6C0); assert(fol.fLinkOk == 0);       // not the sync id
    rx(0x6B0, 1, 1); fol.Update(t, 0x6C0); assert(fol.fLinkOk == 0);    // too short
    rx(0x6B0, 1, 2, CAN_IDE_EXT); fol.Update(t, 0x6C0); assert(fol.fLinkOk == 0);   // extended
    rx(0x6B0, 1); fol.Update(t, 0x6C0);
    assert(fol.fIgnition == 1 && fol.fLinkOk == 1 && fol.eState == IgnState::On && fol.fReady == 1 && fol.fAccessory == 1);
    fol.StatusFrame(frame); assert(frame[3] == 1);
    fol.Update(t + 1000, 0x6C0); assert(fol.fIgnition == 1 && fol.fLinkOk == 0);   // link lost: holds the last state
    // ... but not cranking: the starter drops when the main module goes quiet
    t += 2000; rx(0x6B0, 2); fol.Update(t, 0x6C0); assert(fol.fCranking == 1 && fol.fAccessory == 0);
    fol.Update(t + 600, 0x6C0); assert(fol.fLinkOk == 0 && fol.fCranking == 0 && fol.fIgnition == 1 && fol.fAccessory == 1);
    rx(0x6B0, 9); fol.Update(t, 0x6C0); assert(fol.eState == IgnState::Off);   // unknown state reads Off
    f.eSyncMode = IgnSyncMode::Absolute; f.nSyncId = 0x6F1; fol.Update(t, 0x6C0); assert(fol.nSyncSid == 0x6F1);
    rx(0x6F1, 3); fol.Update(t, 0x6C0); assert(fol.eState == IgnState::Running && fol.fRunning == 1);
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

    c.bEnabled = false; step(); assert(g.fIgnition == 0 && !g.SyncFrame(frame, t + 9999));
    std::puts("host_ignition OK");
    return 0;
}
#elif defined(IGN_STATUS_ID_ONLY)
#include "ignition.h"

int main()
{
    // PDM at 0x680: own frames 0x680..0x6A0, status slot base+32 = 0x6A0, sync 0x6F1
    Config_Ignition c{}; c.bEnabled = true; c.eRole = IgnRole::Main; c.nSyncId = 0x6F1;
    assert(Ignition::StatusId(c, 0x680, 0x6A0) == 0x6A0);                 // 0: its own slot
    c.nStatusId = 0x7A0; assert(Ignition::StatusId(c, 0x680, 0x6A0) == 0x7A0);
    c.nStatusId = 0x6A0; assert(Ignition::StatusId(c, 0x680, 0x6A0) == 0x6A0);   // = its slot: fine
    c.nStatusId = 0x685; assert(Ignition::StatusId(c, 0x680, 0x6A0) == IGN_NO_SYNC);   // its own Msg 3
    c.nStatusId = 0x680; assert(Ignition::StatusId(c, 0x680, 0x6A0) == IGN_NO_SYNC);   // its base
    c.nStatusId = 0x6A1; assert(Ignition::StatusId(c, 0x680, 0x6A0) == 0x6A1);   // just past its frames
    c.nStatusId = 0x67F; assert(Ignition::StatusId(c, 0x680, 0x6A0) == 0x67F);   // just below its base
    c.nStatusId = 0x800; assert(Ignition::StatusId(c, 0x680, 0x6A0) == IGN_NO_SYNC);
    c.nStatusId = 0x6F1; assert(Ignition::StatusId(c, 0x680, 0x6A0) == IGN_NO_SYNC);   // its sync frame
    c.eRole = IgnRole::Follower; assert(Ignition::StatusId(c, 0x680, 0x6A0) == IGN_NO_SYNC);   // the id it follows
    c.eRole = IgnRole::Standalone; assert(Ignition::StatusId(c, 0x680, 0x6A0) == 0x6F1);   // sends no sync
    // offset sync: 0x680 + 0x40 = 0x6C0
    c.eRole = IgnRole::Main; c.eSyncMode = IgnSyncMode::BaseOffset; c.nSyncId = 0x40;
    c.nStatusId = 0x6C0; assert(Ignition::StatusId(c, 0x680, 0x6A0) == IGN_NO_SYNC);
    c.nStatusId = 0x6C1; assert(Ignition::StatusId(c, 0x680, 0x6A0) == 0x6C1);

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
