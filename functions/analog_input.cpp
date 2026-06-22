#include "analog_input.h"

void Analog_Input::Update()
{
    if(!pConfig->bEnabled)
    {
        fVal = 0;
        return;
    }

    fVal = (float)GetAdcRaw(m_channel);
    fValMillivolts = GetAdcVolts(m_channel) * 1000.0;

    RotaryUpdate();
    SwitchUpdate();
}

void Analog_Input::RotaryUpdate()
{
    if(!pConfig->stRotary.bEnabled)
    {
        fRotaryPos = 0;
        return;
    }

    // Calibrated points: each position is a measured centre voltage. A position registers only when
    // the reading is within its window — capped at the tolerance AND at the midpoint to each
    // neighbour, so windows never overlap and a far-apart 2-position switch gets a narrow band, not
    // half the rail. Outside every window the reading is in a dead zone -> report ROTARY_NO_POS.
    if (pConfig->stRotary.bUsePoints)
    {
        const int n   = pConfig->stRotary.nNumPos;
        const int tol  = pConfig->stRotary.nTolerance;
        const int mv  = static_cast<int>(fValMillivolts);
        int found = ROTARY_NO_POS;
        for (int k = 0; k < n; k++)
        {
            const int c     = pConfig->stRotary.nPoint[k];
            const int gapLo = (k == 0)     ? tol * 2 : c - pConfig->stRotary.nPoint[k - 1];
            const int gapHi = (k == n - 1) ? tol * 2 : pConfig->stRotary.nPoint[k + 1] - c;
            const int halfLo = (gapLo / 2 < tol) ? gapLo / 2 : tol;
            const int halfHi = (gapHi / 2 < tol) ? gapHi / 2 : tol;
            if (mv >= c - halfLo && mv <= c + halfHi) { found = k; break; }
        }
        if (found != ROTARY_NO_POS && pConfig->stRotary.bInvert)
            found = (n - 1) - found;
        fRotaryPos = found;
        return;
    }

    if (fValMillivolts < pConfig->stRotary.fOffset)
    {
        fRotaryPos = pConfig->stRotary.bInvert ? pConfig->stRotary.fMaxPos : 0;
    }
    else
    {
        float stepsAboveOffset = (fValMillivolts - pConfig->stRotary.fOffset) / pConfig->stRotary.fStep;
        int pos = static_cast<int>(stepsAboveOffset);
        if (pos > pConfig->stRotary.fMaxPos)
            pos = pConfig->stRotary.fMaxPos;

        fRotaryPos = pConfig->stRotary.bInvert ? (pConfig->stRotary.fMaxPos - pos) : pos;
    }
}

void Analog_Input::SwitchUpdate()
{
    if(!pConfig->stSwitch.bEnabled)
    {
        fSwitchVal = 0;
        return;
    }

    fSwitchVal = input.Check(pConfig->stSwitch.eMode, pConfig->stSwitch.bInvert, fValMillivolts > static_cast<float>(pConfig->stSwitch.nThreshold));
}