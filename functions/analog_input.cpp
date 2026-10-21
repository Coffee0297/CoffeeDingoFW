#include "analog_input.h"

void Analog_Input::Update()
{
    // Raw pin value is always live (CAN status, var map); only the decoders depend on bEnabled
    fVal = (float)GetAdcRaw(m_channel);
    fValMillivolts = GetAdcVolts(m_channel) * 1000.0;

    if(!pConfig->bEnabled)
    {
        fRotaryPos = 0;
        fSwitchVal = 0;
        fScaledVal = 0;
        return;
    }

    RotaryUpdate();
    SwitchUpdate();

    fScaledVal = pConfig->bScale ? (pConfig->fScaleGain * fValMillivolts + pConfig->fScaleOffset) : 0;
}

void Analog_Input::RotaryUpdate()
{
    if(!pConfig->stRotary.bEnabled)
    {
        fRotaryPos = 0;
        return;
    }

    if (pConfig->stRotary.eMode == RotaryMode::Calibrated)
    {
        // A position counts only inside its window: the tolerance, capped at half the gap to each
        // neighbour so windows never overlap. Anywhere else reads ROTARY_NO_POS.
        const Config_RotarySwitch &r = pConfig->stRotary;
        auto point = [&r](int k) { return (int)((r.nPointPair[k / 2] >> ((k & 1) * 16)) & 0xFFFF); };
        int n = (r.nNumPos > ROTARY_MAX_POS) ? ROTARY_MAX_POS : r.nNumPos;
        int mv = static_cast<int>(fValMillivolts);
        int pos = ROTARY_NO_POS;
        for (int k = 0; k < n; k++)
        {
            int c = point(k);
            int lo = (k > 0 && (c - point(k - 1)) / 2 < r.nTolerance) ? (c - point(k - 1)) / 2 : r.nTolerance;
            int hi = (k < n - 1 && (point(k + 1) - c) / 2 < r.nTolerance) ? (point(k + 1) - c) / 2 : r.nTolerance;
            if (mv >= c - lo && mv <= c + hi)
            {
                pos = r.bInvert ? (n - 1 - k) : k;
                break;
            }
        }
        fRotaryPos = pos;
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