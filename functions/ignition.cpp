#include "ignition.h"
#include "device_config.h"

static inline bool On(const float *p) { return p && *p != 0.0f; }

void Ignition::Update(uint32_t nTimeNow, uint16_t nOwnBase)
{
    if (!pConfig || !pConfig->bEnabled)
    {
        eState = IgnState::Off;
        fIgnition = fAccessory = fCranking = fRunning = fReady = fDash = fLinkOk = 0;
        bRxSeen = false;
        bInit = false;
        return;
    }

    nSyncSid = SyncId(*pConfig, nOwnBase);

    if (pConfig->eRole == IgnRole::Follower)
    {
        // Hold the last state while the link is down, but never keep cranking
        fLinkOk = (bRxSeen && (nTimeNow - nLastRx) < IGN_LINK_TIMEOUT_MS) ? 1 : 0;
        if (fLinkOk)
            eState = eMainState;
        else if (eState == IgnState::Cranking)
            eState = IgnState::On;
        Publish(nTimeNow);
        return;
    }

    fLinkOk = 1;
    const bool bStart = On(pStart);
    // A button or key in START held at power-up (or when enabled) must be released before it can crank
    if (!bInit)
    {
        bInit = true;
        bLastStart = bStart;
        bNeedRelease = bStart;
    }
    const bool bPress = bStart && !bLastStart;
    bLastStart = bStart;
    if (!bStart)
        bNeedRelease = false;
    const bool bAllow = pAllow ? On(pAllow) : true;
    const bool bRunning = On(pRunning);
    // Param min is 500, guard a bad stored value anyway: this times the starter
    const uint16_t nMaxCrank = pConfig->nMaxCrankMs < 500 ? 500 : pConfig->nMaxCrankMs;
    bool bIgn = eState != IgnState::Off;

    switch (pConfig->eMode)
    {
    case IgnMode::SwitchButton:
    case IgnMode::Key:
        bIgn = On(pIgn);
        break;
    case IgnMode::ButtonOnly:
        if (bPress && eState == IgnState::Cranking) { eState = IgnState::On; bNeedRelease = true; } // Abort
        else if (bPress && (eState == IgnState::Running || bRunning)) bIgn = false;                 // Power down
        else if (bPress && eState == IgnState::On && !bAllow) bIgn = false;                         // Off again
        else if (bPress && eState == IgnState::Off) bIgn = true;                                    // On, maybe crank below
        break;
    }
    if (On(pStop))
        bIgn = false;

    if (!bIgn)
    {
        eState = IgnState::Off;
    }
    else if (eState == IgnState::Cranking)
    {
        const bool bHeld = pConfig->eMode == IgnMode::ButtonOnly ? true : bStart;
        const bool bKeep = (pConfig->bCrankWhileHeld || pConfig->eMode == IgnMode::Key) ? bHeld : true;
        if (bRunning) eState = IgnState::Running;
        else if (!bKeep) eState = IgnState::On;
        else if ((nTimeNow - nCrankStart) >= nMaxCrank) { eState = IgnState::On; bNeedRelease = true; }
    }
    else
    {
        if (bRunning) eState = IgnState::Running;
        else if (eState == IgnState::Running) eState = IgnState::On; // Stalled
        if (eState == IgnState::Off) eState = IgnState::On;

        // Crank: never while running, only with crank-allow held, and only after a release
        const bool bWant = (pConfig->eMode == IgnMode::Key || pConfig->bCrankWhileHeld) ? bStart : bPress;
        if (eState == IgnState::On && bWant && bAllow && !bNeedRelease)
        {
            eState = IgnState::Cranking;
            nCrankStart = nTimeNow;
        }
    }
    Publish(nTimeNow);
}

void Ignition::Publish(uint32_t nTimeNow)
{
    fIgnition = eState != IgnState::Off ? 1 : 0;
    fCranking = eState == IgnState::Cranking ? 1 : 0;
    fRunning = eState == IgnState::Running ? 1 : 0;
    fAccessory = (fIgnition && !fCranking) ? 1 : 0;
    fReady = eState == IgnState::On ? 1 : 0; // Start button light: on, not cranking, not running

    if (fIgnition)
    {
        bDashHold = true;
        nIgnOnTime = nTimeNow;
    }
    else if (bDashHold && (nTimeNow - nIgnOnTime) >= pConfig->nDashHoldMs)
        bDashHold = false;
    fDash = bDashHold ? 1 : 0;
}

#if !defined(STM32G4XX) // FDCAN frames have no SID/IDE; ported with the rest of v8's CAN code
void Ignition::CheckMsg(const CANRxFrame &rx, uint32_t nTimeNow)
{
    if (!pConfig || !pConfig->bEnabled || pConfig->eRole != IgnRole::Follower)
        return;
    if (rx.IDE != CAN_IDE_STD || rx.SID != nSyncSid || rx.DLC < 2)
        return;
    eMainState = (rx.data8[0] <= static_cast<uint8_t>(IgnState::Running)) ? static_cast<IgnState>(rx.data8[0]) : IgnState::Off;
    nLastRx = nTimeNow;
    bRxSeen = true;
}
#endif

uint16_t Ignition::SyncId(const Config_Ignition &c, uint16_t nOwnBase)
{
    const bool bFollower = c.eRole == IgnRole::Follower;
    const uint32_t nId = c.nSyncId + ((c.eSyncMode == IgnSyncMode::BaseOffset) ? (bFollower ? c.nSyncBase : nOwnBase) : 0);
    if (nId > 0x7FF)
        return IGN_NO_SYNC;
    // A sender's sync frame must not land on its own config or status ids
    if (!bFollower && (nId - nOwnBase) < static_cast<uint32_t>(CYCLIC_TX_OFFSET + NUM_TX_MSGS))
        return IGN_NO_SYNC;
    return nId;
}

bool Ignition::SyncFrame(uint8_t *data, uint32_t nTimeNow)
{
    if (!pConfig || !pConfig->bEnabled || pConfig->eRole != IgnRole::Main || nSyncSid == IGN_NO_SYNC)
        return false;
    // Changes go out at once, the repeat is what followers time the link on
    if (eState == eSyncState && (nTimeNow - nSyncTime) < IGN_SYNC_INTERVAL_MS)
        return false;
    nSyncTime = nTimeNow;
    eSyncState = eState;
    data[0] = static_cast<uint8_t>(eState);
    data[1] = 0; // flags
    return true;
}

void Ignition::StatusFrame(uint8_t *data)
{
    const bool bFollower = pConfig->eRole == IgnRole::Follower;
    data[0] = static_cast<uint8_t>(eState) | (static_cast<uint8_t>(pConfig->eRole) << 4);
    data[1] = 0;
    data[2] = 0;
    // Link: 0 not a follower, 1 ok, 2 lost, 3 sync id invalid (a Main can't send, a follower can't listen)
    data[3] = (nSyncSid == IGN_NO_SYNC && pConfig->eRole != IgnRole::Standalone) ? 3 : bFollower ? (fLinkOk ? 1 : 2) : 0;
    data[4] = 0;
    data[5] = 0;
    data[6] = (fIgnition ? 0x01 : 0) | (fAccessory ? 0x02 : 0) | (fDash ? 0x04 : 0) | (fCranking ? 0x08 : 0) |
              (fRunning ? 0x10 : 0) | (fReady ? 0x20 : 0);
}
