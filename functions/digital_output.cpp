#include "digital_output.h"

void Digital_Output::Update()
{
    if(!pConfig->bEnabled)
    {
        fVal = 0;
        palWriteLine(m_line, 0);
        bTest = false;
        return;
    }

    if (bTest && ((int32_t)(nTestEnd - SYS_TIME) <= 0))
        bTest = false;

    bool bOn = (*pInput != 0) || bTest;
    palWriteLine(m_line, bOn ? PAL_HIGH : PAL_LOW);
    fVal = bOn;
}

bool Digital_Output::SetTest(uint8_t nMode, uint8_t nHoldSec)
{
    if (nMode == 0)
    {
        bTest = false;
        return true;
    }

    if (!pConfig->bEnabled || nMode != 1 || nHoldSec < 1 || nHoldSec > 30)
        return false;

    nTestEnd = SYS_TIME + (uint32_t)nHoldSec * 1000;
    bTest = true;
    return true;
}