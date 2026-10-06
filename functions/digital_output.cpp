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

    // Bench test override: while a test holds, the output ignores its input (mode 1 solid on, mode 2
    // PWM at the requested duty/frequency). The cycle it ends, the line is dropped so the normal path
    // re-arms cleanly next cycle (soft-start, duty, frequency).
    bool bTestNow = TestActive();
    if (!bTestNow)
        nTestMode = 0;
    bool bTestEnd = bTest && !bTestNow;
    bTest = bTestNow;
    bTestPwm = bTest && nTestMode == 2;

    if (bTestEnd)
    {
#if HAS_DIG_PWM
        pwm.Off();
        pwm.Update();   // sees the channel off -> resets soft-start/duty for the re-arm
#endif
        palClearLine(m_line);
        fVal = 0;
        return;
    }

#if HAS_DIG_PWM
    if (bTestPwm)
    {
        // The DO pins are plain GPIO toggled by the timer ISRs (port_pwm.h), so a PWM test also works on
        // an output whose own PWM is off: start the timer, run the test duty/frequency.
        pwm.EnsureStarted();
        pwm.OverrideFrequency(nTestFreq);
        pwm.SetDutyCycle(nTestDuty);
        pwm.On();
        fVal = 1.0f;
        return;
    }
#endif

    if (bTest)
    {
        // Solid on: drive the GPIO directly, whatever this output's PWM config says.
#if HAS_DIG_PWM
        pwm.Off();
#endif
        palSetLine(m_line);
        fVal = 1.0f;
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

bool Digital_Output::TestActive()
{
    return nTestMode != 0 && (int32_t)(nTestEnd - SYS_TIME) > 0;
}

void Digital_Output::SetTest(uint8_t nMode, uint8_t nDuty, uint16_t nFreq, uint8_t nHoldSec)
{
#if !HAS_DIG_PWM
    if (nMode == 2) nMode = 1;   // no PWM on this board: a PWM test is a solid-on test
#endif
    if (nMode == 0 || nMode > 2)
    {
        nTestMode = 0;
        return;
    }

    if (nDuty > 100) nDuty = 100;
#if HAS_DIG_PWM
    if (nFreq == 0) nFreq = pConfig->stPwm.nFreq > 0 ? pConfig->stPwm.nFreq : 100;   // 0 = this output's own frequency
#endif
    if (nFreq < 15) nFreq = 15;                                                      // same window as Pwm::GetTargetFreq
    if (nFreq > 400) nFreq = 400;
    if (nHoldSec == 0) nHoldSec = 1;
    if (nHoldSec > 30) nHoldSec = 30;

    nTestDuty = nDuty;
    nTestFreq = nFreq;
    nTestEnd = SYS_TIME + (uint32_t)nHoldSec * 1000u;
    nTestMode = nMode;   // last — makes the override visible to the control loop
}
