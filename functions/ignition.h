#pragma once

#include <cstdint>
#include "enums.h"
#ifdef DINGO_HOST_TEST
#ifndef VAR_MAP_SIZE
#define VAR_MAP_SIZE 1
#endif
#else
#include "port.h"
#endif

extern float *pVarMap[VAR_MAP_SIZE];

// Ignition & start button (5.6, upstream dingoFW#67-#74). One module (Main) reads the switch / button and runs the
// state machine; it broadcasts the state at BaseId + IGN_STATE_OFFSET every 100 ms (the first id after the board's
// own range: +30 on a PDM, +14 on a CANBoard; port.h). Followers copy the main
// module's state and keep their last state if the link drops. The state shows as var-map signals, so an output
// gets a role by following one of them: Ignition, Accessory (off while cranking), Starter (Cranking), Dash, or
// the start-button light (Ready).
#ifndef IGN_STATE_OFFSET
#define IGN_STATE_OFFSET 30   // host tests; each board sets its own in port.h
#endif
#define IGN_LINK_TIMEOUT_MS 500

struct Config_Ignition{
  bool bEnabled;
  IgnMode eMode;            // SwitchButton / ButtonOnly / Key
  IgnRole eRole;            // Main / Follower
  uint16_t nIgnInput;       // ignition switch, or key at ON (not used by ButtonOnly)
  uint16_t nStartInput;     // start button, or key at START
  uint16_t nCrankAllowInput;// clutch / brake that must be held to crank (0 = none)
  uint16_t nRunningInput;   // engine running (0 = none: cranking ends on release or max time)
  uint16_t nMaxCrankMs;     // 500..30000
  bool bCrankWhileHeld;     // true: cranks while held; false: one press cranks until running or the max time
  uint16_t nDashHoldMs;     // the dash stays on this long after the ignition goes off (power-down grace)
  uint16_t nMainStateId;    // Follower: the CAN id of the main module's state frame (its base + its IGN_STATE_OFFSET)
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
    // Follower: a frame from the main module (true when it was one)
    bool CheckMsg(uint16_t nSid, const uint8_t *data, uint8_t nDlc, uint32_t nTimeNow);
    // Main: the state frame to broadcast now, or false
    bool StateFrame(uint32_t nTimeNow, uint8_t *data);

    bool Main() const { return pConfig && pConfig->bEnabled && pConfig->eRole == IgnRole::Main; }

    IgnState eState = IgnState::Off;
    // var-map signals (1/0)
    float fIgnition = 0, fAccessory = 0, fCranking = 0, fRunning = 0, fReady = 0, fDash = 0, fLinkOk = 0;

private:
    void Publish(uint32_t nTimeNow);

    Config_Ignition* pConfig = nullptr;
    float *pIgn = nullptr, *pStart = nullptr, *pAllow = nullptr, *pRunning = nullptr;
    bool bLastStart = false;
    bool bNeedRelease = false;   // max crank time hit / aborted: the button must be released first
    uint32_t nCrankStart = 0;
    uint32_t nIgnOffTime = 0;
    bool bIgnWas = false;
    uint32_t nLastRx = 0, nLastTx = 0;
    uint8_t nRxFlags = 0;
};
