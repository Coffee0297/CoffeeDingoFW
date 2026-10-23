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

    if (pConfig->stRotary.eMode == RotaryMode::Calibrated)
    {
        // The nearest centre wins if the voltage is within the tolerance of it, anywhere else reads
        // ROTARY_NO_POS. Nearest first, so windows wider than half the gap split halfway between centres.
        const Config_RotarySwitch &r = pConfig->stRotary;
        int n = (r.nNumPos > ROTARY_MAX_POS) ? ROTARY_MAX_POS : r.nNumPos;
        int mv = static_cast<int>(fValMillivolts);
        int pos = ROTARY_NO_POS;
        int bestDist = 0;
        for (int k = 0; k < n; k++)
        {
            int dist = (mv > r.nCenter[k]) ? (mv - r.nCenter[k]) : (r.nCenter[k] - mv);
            if (dist <= r.nTolerance && (pos == ROTARY_NO_POS || dist < bestDist))
            {
                pos = k;
                bestDist = dist;
            }
        }
        fRotaryPos = (pos != ROTARY_NO_POS && r.bInvert) ? (n - 1 - pos) : pos;
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