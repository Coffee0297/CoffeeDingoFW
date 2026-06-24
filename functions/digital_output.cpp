#include "digital_output.h"

void Digital_Output::Update()
{
    if(!pConfig->bEnabled)
    {
        fVal = 0;
#if HAS_DIG_PWM
        pwm.Off();
#endif
        palWriteLine(m_line, 0);
        return;
    }

#if HAS_DIG_PWM
    // PWM mode: the timer's period/compare ISR callbacks toggle the GPIO line
    // (see port_pwm.h). Mirrors the Profet PWM drive sequence: On() then Update().
    if (pwm.IsEnabled())
    {
        bool bOn = (bool)(*pInput);
        if (bOn)
        {
            pwm.EnsureStarted();
            pwm.On();
        }
        else
        {
            pwm.Off();
            palClearLine(m_line);   // ISR no longer drives the line; force it low
        }
        pwm.Update();
        fVal = bOn ? 1.0f : 0.0f;
        return;
    }
#endif

    palWriteLine(m_line, *pInput);
    fVal = *pInput;
}
