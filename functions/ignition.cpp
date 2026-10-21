#include "ignition.h"
#include "device_config.h"

static inline bool On(const float *p) { return p && *p != 0.0f; }

void Ignition::Update(uint32_t nTimeNow, uint16_t nOwnBase, bool bHoldAwake)
{
    if (!pConfig || !pConfig->bEnabled)
    {
        eState = IgnState::Off;
        eDash = IgnDash::Off;
        fIgnition = fAccessory = fCranking = fRunning = fReady = fDash = fLinkOk = fDashShutdown = 0;
        bRxSeen = false;
        bLastDoor = bDoorOn = false;
        bInit = false;
        bMainSleep = false;
        ePhase = Phase::Awake;
        eSleep = IgnSleep::Disabled;
        return;
    }

    nSyncSid = SyncId(*pConfig, nOwnBase);
    bHold = bHoldAwake;
    nNow = nTimeNow;
    // First pass after enable: the sleep delay starts now, and the start input is only sampled (below)
    const bool bFirst = !bInit;
    bInit = true;
    if (bFirst)
        nIdleSince = nTimeNow;

    if (pConfig->eRole == IgnRole::Follower)
    {
        // Hold the last state while the link is down, but never keep cranking
        fLinkOk = (bRxSeen && (nTimeNow - nLastRx) < IGN_LINK_TIMEOUT_MS) ? 1 : 0;
        if (fLinkOk)
            eState = eMainState;
        else if (!bRxSeen)
            eState = IgnState::Off; // nothing heard yet, e.g. just switched from Main
        else if (eState == IgnState::Cranking)
            eState = IgnState::On;
        Publish(nTimeNow);
        return;
    }

    fLinkOk = 1;
    const bool bStart = StartIn(nTimeNow);
    // A button or key in START held at power-up (or when enabled) must be released before it can crank
    if (bFirst)
    {
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


    // Door: timed from the rising edge, so a door left open doesn't keep the dash on. Open at power-up counts
    // (opening it is often what woke the module).
    const bool bDoor = On(pDoor);
    if (bDoor && !bLastDoor)
    {
        bDoorOn = true;
        nDoorTime = nTimeNow;
    }
    bLastDoor = bDoor;
    if (bDoorOn && (nTimeNow - nDoorTime) >= pConfig->nDoorOnMs)
        bDoorOn = false;

    // Dash power down: grace, then (with an off delay) halting with DashShutdown on, then 2 s unpowered
    const bool bWant = fIgnition || bDoorOn;
    if (eDash == IgnDash::Off && bWant)
        eDash = IgnDash::On;
    if (eDash == IgnDash::On && !bWant)
    {
        eDash = IgnDash::Grace;
        nDashTime = nTimeNow;
    }
    if (eDash == IgnDash::Grace)
    {
        if (bWant)
            eDash = IgnDash::On;
        else if ((nTimeNow - nDashTime) >= pConfig->nDashHoldMs)
        {
            eDash = pConfig->nDashOffDelayMs ? IgnDash::Halting : IgnDash::Off;
            nDashTime = nTimeNow;
        }
    }
    if (eDash == IgnDash::Halting && (nTimeNow - nDashTime) >= pConfig->nDashOffDelayMs)
    {
        eDash = IgnDash::Restart;
        nDashTime = nTimeNow;
    }
    if (eDash == IgnDash::Restart && (nTimeNow - nDashTime) >= IGN_RESTART_OFF_MS)
        eDash = IgnDash::Off;
    fDash = (eDash == IgnDash::On || eDash == IgnDash::Grace || eDash == IgnDash::Halting) ? 1 : 0;
    fDashShutdown = eDash == IgnDash::Halting ? 1 : 0;

    UpdateSleep(nTimeNow);
}

void Ignition::UpdateSleep(uint32_t nTimeNow)
{
    const bool bFollower = pConfig->eRole == IgnRole::Follower;
    if (fIgnition || bDoorOn)
    {
        ePhase = Phase::Awake;
        eSleep = IgnSleep::Awake;
        nIdleSince = nTimeNow;
        return;
    }

    bool bTimeUp;
    if (bFollower && (fLinkOk || bMainSleep))
        bTimeUp = bMainSleep; // The main module decides while it is there
    else if (!pConfig->nSleepDelayMs)
    {
        ePhase = Phase::Awake;
        eSleep = IgnSleep::Disabled;
        return;
    }
    else // The quiet time is part of the delay: asleep nSleepDelayMs after the ignition went off
        bTimeUp = (nTimeNow - nIdleSince) >= (pConfig->nSleepDelayMs > IGN_QUIET_MS ? pConfig->nSleepDelayMs - IGN_QUIET_MS : 0);

    switch (ePhase)
    {
    case Phase::Awake:
        if (!bTimeUp)
            eSleep = (bFollower && fLinkOk) ? IgnSleep::Following : IgnSleep::Counting;
        else if (fDash)
            eSleep = IgnSleep::WaitingDash;
        else if (bHold)
            eSleep = IgnSleep::Held;
        else
        {
            ePhase = (pConfig->eRole == IgnRole::Main) ? Phase::Announce : Phase::Quiet;
            nPhaseTime = nTimeNow;
        }
        break;
    case Phase::Announce:
        if ((nTimeNow - nPhaseTime) >= IGN_ANNOUNCE_MS)
        {
            ePhase = Phase::Quiet;
            nPhaseTime = nTimeNow;
        }
        break;
    case Phase::Quiet:
        if (bHold || !bTimeUp) // USB, a config session, or the main module called it off
            ePhase = Phase::Awake;
        else if ((nTimeNow - nPhaseTime) >= IGN_QUIET_MS)
            ePhase = Phase::Sleep;
        break;
    case Phase::Sleep:
        break;
    }
    if (ePhase == Phase::Announce || ePhase == Phase::Quiet)
        eSleep = IgnSleep::Quiet;
    else if (ePhase == Phase::Sleep)
        eSleep = IgnSleep::Sleep;
}

bool Ignition::StartIn(uint32_t nTimeNow) const
{
    if (!pConfig->nButtonMask)
        return On(pStart);
    // A sender that goes quiet must not leave the button pressed
    return bButton && (!pConfig->nButtonTimeoutMs || (nTimeNow - nButtonRx) < pConfig->nButtonTimeoutMs);
}

void Ignition::CheckMsg(const CANRxFrame &rx, uint32_t nTimeNow)
{
    if (!pConfig || !pConfig->bEnabled)
        return;
    if (pConfig->eRole != IgnRole::Follower)
    {
        const bool bExt = pConfig->nButtonIde == 1;
        if (pConfig->nButtonMask && rx.IDE == (bExt ? CAN_IDE_EXT : CAN_IDE_STD) &&
            (bExt ? rx.EID : rx.SID) == pConfig->nButtonId && rx.DLC > pConfig->nButtonByte)
        {
            bButton = (rx.data8[pConfig->nButtonByte] & pConfig->nButtonMask) != 0;
            nButtonRx = nTimeNow;
        }
        return;
    }
    if (rx.IDE != CAN_IDE_STD || rx.SID != nSyncSid || rx.DLC < 2)
        return;
    eMainState = (rx.data8[0] <= static_cast<uint8_t>(IgnState::Running)) ? static_cast<IgnState>(rx.data8[0]) : IgnState::Off;
    bMainSleep = rx.data8[1] & 0x01;
    nLastRx = nTimeNow;
    bRxSeen = true;
}

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

uint16_t Ignition::StatusId(const Config_Ignition &c, uint16_t nOwnBase, uint16_t nDefault)
{
    if (c.nStatusId == 0 || c.nStatusId == nDefault)
        return nDefault;
    if (c.nStatusId > 0x7FF || static_cast<uint32_t>(c.nStatusId - nOwnBase) < static_cast<uint32_t>(CYCLIC_TX_OFFSET + NUM_TX_MSGS))
        return IGN_NO_SYNC;
    if (c.eRole != IgnRole::Standalone && c.nStatusId == SyncId(c, nOwnBase))
        return IGN_NO_SYNC;
    return c.nStatusId;
}

bool Ignition::SyncFrame(uint8_t *data, uint32_t nTimeNow)
{
    if (!pConfig || !pConfig->bEnabled || pConfig->eRole != IgnRole::Main || nSyncSid == IGN_NO_SYNC)
        return false;
    // Changes go out at once, the repeat is what followers time the link on
    const bool bSleep = ePhase != Phase::Awake;
    if (eState == eSyncState && bSleep == bSyncSleep && (nTimeNow - nSyncTime) < IGN_SYNC_INTERVAL_MS)
        return false;
    nSyncTime = nTimeNow;
    eSyncState = eState;
    bSyncSleep = bSleep;
    data[0] = static_cast<uint8_t>(eState);
    data[1] = bSleep ? 0x01 : 0; // flags: bit 0 sleep (the followers go quiet with the main module)
    return true;
}

float *Ignition::RoleVar(IgnOutputRole eRole)
{
    switch (eRole)
    {
    case IgnOutputRole::Ignition: return &fIgnition;
    case IgnOutputRole::Accessory: return &fAccessory;
    case IgnOutputRole::Dash: return &fDash;
    case IgnOutputRole::Starter: return &fCranking;
    default: return nullptr;
    }
}

void Ignition::StatusFrame(uint8_t *data)
{
    const bool bFollower = pConfig->eRole == IgnRole::Follower;
    data[0] = static_cast<uint8_t>(eState) | (static_cast<uint8_t>(pConfig->eRole) << 4);
    data[1] = static_cast<uint8_t>(eDash);
    data[2] = static_cast<uint8_t>(eSleep);
    // Link: 0 not a follower, 1 ok, 2 lost, 3 sync id invalid (a Main can't send, a follower can't listen)
    data[3] = (nSyncSid == IGN_NO_SYNC && pConfig->eRole != IgnRole::Standalone) ? 3 : bFollower ? (fLinkOk ? 1 : 2) : 0;
    // Seconds to sleep while counting, rounded up
    const uint32_t nIdle = nNow - nIdleSince;
    const uint32_t nLeft = (eSleep == IgnSleep::Counting && nIdle < pConfig->nSleepDelayMs) ? (pConfig->nSleepDelayMs - nIdle + 999) / 1000 : 0;
    data[4] = nLeft & 0xFF;
    data[5] = (nLeft >> 8) & 0xFF;
    data[6] = (fIgnition ? 0x01 : 0) | (fAccessory ? 0x02 : 0) | (fDash ? 0x04 : 0) | (fCranking ? 0x08 : 0) |
              (fRunning ? 0x10 : 0) | (fReady ? 0x20 : 0) | (fDashShutdown ? 0x40 : 0);
}
