#include "ignition.h"

static inline bool On(const float *p) { return p && *p != 0.0f; }

void Ignition::Update(uint32_t nTimeNow)
{
    if (!pConfig || !pConfig->bEnabled)
    {
        eState = IgnState::Off;
        fIgnition = fAccessory = fCranking = fRunning = fReady = fDash = fLinkOk = 0;
        return;
    }

    if (pConfig->eRole == IgnRole::Follower)
    {
        // Hold the last state while the link is down, but never keep cranking
        fLinkOk = (bRxSeen && (nTimeNow - nLastRx) < IGN_LINK_TIMEOUT_MS) ? 1 : 0;
        if (!fLinkOk)
            nRxFlags &= ~0x02;
        fIgnition = (nRxFlags & 0x01) ? 1 : 0;
        fCranking = (nRxFlags & 0x02) ? 1 : 0;
        fRunning = (nRxFlags & 0x04) ? 1 : 0;
        fReady = (nRxFlags & 0x08) ? 1 : 0;
        fDash = (nRxFlags & 0x10) ? 1 : 0;
        fAccessory = (fIgnition && !fCranking) ? 1 : 0;
        eState = fCranking ? IgnState::Cranking : fRunning ? IgnState::Running : fIgnition ? IgnState::On : IgnState::Off;
        return;
    }

    fLinkOk = 1;
    const bool bStart = On(pStart);
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

void Ignition::CheckMsg(uint16_t nSid, const uint8_t *data, uint8_t nDlc, uint32_t nTimeNow)
{
    if (!pConfig || !pConfig->bEnabled || pConfig->eRole != IgnRole::Follower)
        return;
    if (nSid != pConfig->nMainStateId || nDlc < 3 || data[0] != IGN_FRAME_MARKER)
        return;
    nRxFlags = data[2];
    nLastRx = nTimeNow;
    bRxSeen = true;
}

bool Ignition::StateFrame(uint8_t *data)
{
    if (!pConfig || !pConfig->bEnabled || pConfig->eRole != IgnRole::Main)
        return false;
    data[0] = IGN_FRAME_MARKER;
    data[1] = static_cast<uint8_t>(eState);
    data[2] = (fIgnition ? 0x01 : 0) | (fCranking ? 0x02 : 0) | (fRunning ? 0x04 : 0) | (fReady ? 0x08 : 0) | (fDash ? 0x10 : 0);
    for (uint8_t i = 3; i < 8; i++)
        data[i] = 0;
    return true;
}
