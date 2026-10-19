#include "latch_gate.h"

void LatchGate::Update()
{
    if (!pConfig->bEnabled)
    {
        bInit = false;
        fVal = 0.0f;
        return;
    }

    const bool bIn = *pInput > 0.5f;
    const bool bControl = *pControlInput > 0.5f;

    // First pass after enable only samples the inputs, so an input already
    // high at power up is not taken as a rising edge
    if (!bInit)
    {
        bInit = true;
        bLastIn = bIn;
        bLastControl = bControl;
        fVal = 0.0f;
        return;
    }

    switch (pConfig->eType)
    {
    case LatchGateType::Latch:
        // Reset wins over a set in the same cycle
        if (bControl && !bLastControl)
            fVal = 0.0f;
        else if (bIn && !bLastIn)
        {
            fVal = 1.0f;
            nSetTime = SYS_TIME;
        }
        else if ((fVal > 0.5f) && (pConfig->nTime > 0) && ((SYS_TIME - nSetTime) >= pConfig->nTime))
            fVal = 0.0f;
        break;

    case LatchGateType::Gate:
        if (bControl)
            fVal = *pInput;
        break;
    }

    bLastIn = bIn;
    bLastControl = bControl;
}
