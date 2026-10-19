#include "digital_output.h"

void Digital_Output::Update()
{
    bool bOn = pConfig->bEnabled && (*pInput != 0);

    // PWM: the timer's period/compare callbacks drive the line (port_pwm.h), same as the Profet outputs
    if (bOn && pwm.IsEnabled())
    {
        pwm.EnsureStarted();
        pwm.On();
    }
    else
    {
        pwm.Off();
        palWriteLine(m_line, bOn ? PAL_HIGH : PAL_LOW);
    }
    pwm.Update();

    fVal = bOn;
}