#include "timer.h"

void Timer::Update()
{
    if (!pConfig->bEnabled)
    {
        bInit = false;
        fRunning = 0.0f;
        bOut = false;
        fVal = 0.0f;
        return;
    }

    const bool bIn = *pInput > 0.5f;
    const bool bReset = *pResetInput > 0.5f;
    const uint32_t nNow = SYS_TIME;

    // First pass after enable only samples the input. Without this a timer
    // whose input is already high at power up sees a rising edge that never
    // happened and fires a pulse on its own.
    if (!bInit)
    {
        bInit = true;
        bLastIn = bIn;
        fRunning = 0.0f;
        bOut = false;
        fVal = 0.0f;
        return;
    }

    // Reset is level sensitive, not edge: while it is held the timer stays
    // cleared, so it can be used to cancel a running delay and keep it cancelled
    if (bReset)
    {
        fRunning = 0.0f;
        bOut = false;
        bLastIn = bIn;
        fVal = 0.0f;
        return;
    }

    const bool bRising = bIn && !bLastIn;
    const bool bFalling = !bIn && bLastIn;
    const bool bElapsed = (fRunning != 0.0f) && ((nNow - nStartTime) >= pConfig->nTime);

    switch (pConfig->eMode)
    {
    case TimerMode::OnDelay:
        // Level triggered, not edge: an input already on at power up or
        // after a reset is released still starts the delay
        if (!bIn)
        {
            fRunning = 0.0f;
            bOut = false;
        }
        else if ((fRunning == 0.0f) && !bOut)
        {
            fRunning = 1.0f;
            nStartTime = nNow;
        }
        else if (bElapsed)
        {
            fRunning = 0.0f;
            bOut = true;
        }
        break;

    case TimerMode::OffDelay:
        if (bIn)
        {
            fRunning = 0.0f;
            bOut = true;
        }
        else
        {
            if (bFalling)
            {
                fRunning = 1.0f;
                nStartTime = nNow;
            }

            if (bElapsed)
            {
                fRunning = 0.0f;
                bOut = false;
            }
        }
        break;

    case TimerMode::PulseRetrig:
        if (bRising)
        {
            fRunning = 1.0f;
            bOut = true;
            nStartTime = nNow;
        }
        else if (bElapsed)
        {
            fRunning = 0.0f;
            bOut = false;
        }
        break;

    case TimerMode::PulseOneShot:
        if (bRising && (fRunning == 0.0f))
        {
            fRunning = 1.0f;
            bOut = true;
            nStartTime = nNow;
        }
        else if (bElapsed)
        {
            fRunning = 0.0f;
            bOut = false;
        }
        break;
    }

    bLastIn = bIn;
    fVal = bOut ? 1.0f : 0.0f;
}
