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
        // copy the main module; hold the last state while the link is down
        fLinkOk = (nTimeNow - nLastRx) < IGN_LINK_TIMEOUT_MS ? 1 : 0;
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
    const bool start = On(pStart);
    const bool press = start && !bLastStart;
    bLastStart = start;
    if (!start) bNeedRelease = false;
    const bool allow = pAllow ? On(pAllow) : true;
    const bool running = On(pRunning);
    const uint16_t maxCrank = pConfig->nMaxCrankMs < 500 ? 500 : pConfig->nMaxCrankMs;
    bool ign = eState != IgnState::Off;

    switch (pConfig->eMode)
    {
    case IgnMode::SwitchButton:
    case IgnMode::Key:
        ign = On(pIgn);
        break;
    case IgnMode::ButtonOnly:
        if (press && eState == IgnState::Cranking) { eState = IgnState::On; bNeedRelease = true; }   // abort
        else if (press && (eState == IgnState::Running || running)) ign = false;                     // power down
        else if (press && eState == IgnState::On && !allow) ign = false;                              // off again
        else if (press && eState == IgnState::Off) ign = true;                                        // on (and maybe crank, below)
        break;
    }

    if (!ign)
    {
        eState = IgnState::Off;
    }
    else if (eState == IgnState::Cranking)
    {
        const bool held = pConfig->eMode == IgnMode::ButtonOnly ? true : start;
        const bool keep = pConfig->bCrankWhileHeld || pConfig->eMode == IgnMode::Key ? held : true;
        if (running) eState = IgnState::Running;
        else if (!keep) eState = IgnState::On;
        else if ((nTimeNow - nCrankStart) >= maxCrank) { eState = IgnState::On; bNeedRelease = true; }
    }
    else
    {
        if (running) eState = IgnState::Running;
        else if (eState == IgnState::Running) eState = IgnState::On;   // stalled
        if (eState == IgnState::Off) eState = IgnState::On;

        // crank request: never while running, only with the crank-allow held, after a release
        const bool want = pConfig->eMode == IgnMode::Key || pConfig->bCrankWhileHeld ? start : press;
        if (eState == IgnState::On && want && allow && !bNeedRelease)
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
    fReady = eState == IgnState::On ? 1 : 0;   // ignition on, engine not running, not cranking: start light
    if (fIgnition) bIgnWas = true;
    else if (bIgnWas) { bIgnWas = false; nIgnOffTime = nTimeNow; }
    fDash = (fIgnition || (nIgnOffTime && (nTimeNow - nIgnOffTime) < pConfig->nDashHoldMs)) ? 1 : 0;
}

bool Ignition::CheckMsg(uint16_t nSid, const uint8_t *data, uint8_t nDlc, uint32_t nTimeNow)
{
    if (!pConfig || !pConfig->bEnabled || pConfig->eRole != IgnRole::Follower) return false;
    if (nSid != pConfig->nMainStateId || nDlc < 3 || data[0] != 'I') return false;
    nRxFlags = data[2];
    nLastRx = nTimeNow;
    return true;
}

bool Ignition::StateFrame(uint32_t nTimeNow, uint8_t *data)
{
    if (!Main() || (nTimeNow - nLastTx) < 100) return false;
    nLastTx = nTimeNow;
    data[0] = 'I';
    data[1] = static_cast<uint8_t>(eState);
    data[2] = (fIgnition ? 0x01 : 0) | (fCranking ? 0x02 : 0) | (fRunning ? 0x04 : 0) | (fReady ? 0x08 : 0) | (fDash ? 0x10 : 0);
    for (uint8_t i = 3; i < 8; i++) data[i] = 0;
    return true;
}
