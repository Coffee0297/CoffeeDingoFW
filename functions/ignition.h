#pragma once

#include <cstdint>
#include "port.h"
#include "enums.h"

extern float *pVarMap[VAR_MAP_SIZE];

// Ignition and start button. The main module reads the switch/button and runs the state machine, its state
// goes out in a cyclic frame (ignition state msg). Followers copy that frame and hold the last state if it
// stops, except cranking: the starter never stays engaged without the main module.
#define IGN_LINK_TIMEOUT_MS 500
#define IGN_FRAME_MARKER 'I'

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
  uint16_t nMainStateId;     // Follower: CAN id of the main module's ignition state frame
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

    void Update(uint32_t nTimeNow);

    // Follower: a frame that may be the main module's state
    void CheckMsg(uint16_t nSid, const uint8_t *data, uint8_t nDlc, uint32_t nTimeNow);

    // Main: fills the state frame, false when this module doesn't send one
    bool StateFrame(uint8_t *data);

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
    uint8_t nRxFlags;
};
