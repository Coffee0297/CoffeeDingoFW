#pragma once

#include <cstdint>
#include "hal.h"
#include "port.h"
#include "enums.h"

extern float *pVarMap[VAR_MAP_SIZE];

// Ignition and start button. The main module reads the switch/button and runs the state machine, and broadcasts
// its state on the sync id: [state, flags], every 100 ms and at once on a change (dingoFW #72's frame).
// Followers take the state from it and hold it if it stops, except cranking: the starter never stays engaged
// without the main module.
#define IGN_LINK_TIMEOUT_MS 500
#define IGN_SYNC_INTERVAL_MS 100
#define IGN_NO_SYNC 0xFFFF

struct Config_Ignition{
  bool bEnabled;
  IgnMode eMode;
  IgnRole eRole;
  uint16_t nIgnInput;        // Ignition switch, or key at ON (not used by ButtonOnly)
  uint16_t nStartInput;      // Start button, or key at START
  uint16_t nCrankAllowInput; // Clutch/brake that must be held to crank (0 = none)
  uint16_t nRunningInput;    // Engine running (0 = none: cranking ends on release or max time)
  uint16_t nMaxCrankMs;
  bool bCrankWhileHeld;      // false: one press cranks until running or the max time
  uint16_t nDashHoldMs;      // Dash stays on this long after the ignition goes off
  uint16_t nSyncId;          // Sync frame: the std id, or the offset from the main module's base id
  IgnSyncMode eSyncMode;
  uint16_t nSyncBase;        // Follower, BaseOffset: the main module's base id
};

class Ignition
{
public:
    Ignition() {
    };

    static const uint16_t nBaseIndex = 0x1E00;

    void SetConfig(Config_Ignition* config)
    {
        pConfig = config;
        pIgn = pVarMap[config->nIgnInput];
        pStart = pVarMap[config->nStartInput];
        pAllow = config->nCrankAllowInput ? pVarMap[config->nCrankAllowInput] : nullptr;
        pRunning = config->nRunningInput ? pVarMap[config->nRunningInput] : nullptr;
    }

    // nOwnBase: this module's base id, for a BaseOffset sync id
    void Update(uint32_t nTimeNow, uint16_t nOwnBase);

    // Follower: a frame that may be the main module's sync frame
    void CheckMsg(const CANRxFrame &rx, uint32_t nTimeNow);

    // The sync id: a follower's resolves against nSyncBase, a sender's against its own base and must stay
    // clear of its own frames (base .. base + CYCLIC_TX_OFFSET + NUM_TX_MSGS - 1). IGN_NO_SYNC when invalid.
    static uint16_t SyncId(const Config_Ignition &c, uint16_t nOwnBase);

    // Main: true when the sync frame [state, flags] is due (every 100 ms, at once on a change); nSyncSid is its id
    bool SyncFrame(uint8_t *data, uint32_t nTimeNow);
    uint16_t nSyncSid = IGN_NO_SYNC;

    // Status frame bytes 0-6 (byte 7, the wake source, is the caller's)
    void StatusFrame(uint8_t *data);

    IgnState eState = IgnState::Off;
    float fIgnition, fAccessory, fCranking, fRunning, fReady, fDash, fLinkOk;

private:
    void Publish(uint32_t nTimeNow);

    Config_Ignition* pConfig = nullptr;
    float *pIgn, *pStart, *pAllow, *pRunning;
    bool bLastStart;
    bool bNeedRelease; // Max crank time hit or aborted: the button must be released first
    uint32_t nCrankStart;
    bool bDashHold;
    uint32_t nIgnOnTime; // Last time the ignition was on, for the dash hold
    bool bRxSeen;
    uint32_t nLastRx;
    IgnState eMainState; // Follower: the state in the last sync frame
    uint32_t nSyncTime;
    IgnState eSyncState;
};
