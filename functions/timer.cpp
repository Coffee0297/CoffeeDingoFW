#include "timer.h"

// OnDelay  (TON): output on once the input has been active for the preset; off as soon as it isn't.
// OffDelay (TOF): output on with the input; stays on for the preset after the input goes inactive.
// Pulse    (TP) : one output pulse of the preset length each time the input becomes active
//                 (a new activation restarts it; releasing the input doesn't cut it short).
void Timer::Update(uint32_t nTimeNow)
{
    if (!pConfig->bEnabled)
    {
        fVal = 0;
        bTiming = false;
        return;
    }

    const bool bActive = (pConfig->eEdge == InputEdge::Falling) ? (*pInput == 0.0f) : (*pInput != 0.0f);
    const bool bRise = bActive && !bLastActive;
    const bool bFall = !bActive && bLastActive;
    bLastActive = bActive;

    switch (pConfig->eMode)
    {
    case TimerMode::OnDelay:
        if (bRise) { bTiming = true; nStartTime = nTimeNow; }
        if (!bActive) bTiming = false;
        fVal = (bTiming && (nTimeNow - nStartTime) >= pConfig->nPreset) ? 1.0f : 0.0f;
        break;

    case TimerMode::OffDelay:
        if (bFall) { bTiming = true; nStartTime = nTimeNow; }
        if (bActive) bTiming = false;
        if (bTiming && (nTimeNow - nStartTime) >= pConfig->nPreset) bTiming = false;
        fVal = (bActive || bTiming) ? 1.0f : 0.0f;
        break;

    case TimerMode::Pulse:
        if (bRise) { bTiming = true; nStartTime = nTimeNow; }
        if (bTiming && (nTimeNow - nStartTime) >= pConfig->nPreset) bTiming = false;
        fVal = bTiming ? 1.0f : 0.0f;
        break;

    default:
        fVal = 0;
        break;
    }
}
