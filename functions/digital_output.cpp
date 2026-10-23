#include "digital_output.h"

void Digital_Output::Update()
{
    if (bTest && (!pConfig->bEnabled || ((int32_t)(nTestEnd - SYS_TIME) <= 0)))
        bTest = false;

    bool bOn = pConfig->bEnabled && ((*pInput != 0) || bTest);

    // PWM: the timer's period/compare callbacks drive the line (port_pwm.h), same as the Profet outputs.
    // A bench test is solid on.
    if (bOn && !bTest && pwm.IsEnabled())
    {
        pwm.EnsureStarted();
        pwm.On();
    }
    else
    {
        pwm.Off();
        palWriteLine(m_line, bOn ? PAL_HIGH : PAL_LOW);
    }
    pwm.Update(false);
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