#include "pwm.h"
#include "dbc.h"

#if (NUM_OUTPUTS > 0) || HAS_DIG_PWM
void Pwm::Update()
{
    bChannelEnabled = (bool)(m_pwm->enabled & (1 << 0));

    if (!pConfig->bEnabled || !bChannelEnabled) {
        bLastChannelEnabled = bChannelEnabled;
        bSoftStartComplete = false;
        nDutyCycle = 0;
    }

    if (!pConfig->bSoftStart) {
        nDutyCycle = GetTargetDutyCycle();
    }
    else
    {
        // Initialize soft start
        if (bChannelEnabled != bLastChannelEnabled) {
            InitSoftStart();
        }
        
        // Update soft start
        if (!bSoftStartComplete) {
            UpdateSoftStart();
        }
        // After soft start completes, track the variable-duty source.
        else if (pConfig->bVariableDutyCycle) {
            uint8_t target = GetTargetDutyCycle();
            if (pConfig->bRampDutyChanges && pConfig->nSoftStartRampTime > 0) {
                // Slew toward the new target at the soft-start rate (a full 0..100% change
                // takes nSoftStartRampTime), so duty changes ramp instead of jumping.
                float step = 100.0f * PWM_UPDATE_TIME / (float)pConfig->nSoftStartRampTime;
                if (fDutyActual + step < target)      fDutyActual += step;
                else if (fDutyActual - step > target) fDutyActual -= step;
                else                                  fDutyActual = target;
                nDutyCycle = (uint16_t)(fDutyActual + 0.5f);
            } else {
                nDutyCycle = target;
                fDutyActual = target;
            }
        }
    }

    UpdateFrequency();
    
    bLastChannelEnabled = bChannelEnabled;
}

uint8_t Pwm::GetTargetDutyCycle() {
    if (pConfig->bVariableDutyCycle && pConfig->nDutyCycleInputDenom > 0) {
        uint8_t dc = (uint8_t)((*pInput) / pConfig->nDutyCycleInputDenom);
        if (dc < pConfig->nMinDutyCycle)
            dc = pConfig->nMinDutyCycle;
        return dc;
    }
    return pConfig->nFixedDutyCycle;
}

void Pwm::InitSoftStart() {
    fSoftStartStep = GetTargetDutyCycle() / (float)pConfig->nSoftStartRampTime;
    bSoftStartComplete = false;
    nSoftStartTime = SYS_TIME;
    fDutyActual = pConfig->nMinDutyCycle;   // slew starts from the min-duty floor
}

void Pwm::UpdateSoftStart() {
    uint8_t targetDuty = GetTargetDutyCycle();
    nDutyCycle = (uint8_t)((fSoftStartStep * (SYS_TIME - nSoftStartTime)) + pConfig->nMinDutyCycle);
    
    if (nDutyCycle >= targetDuty) {
        nDutyCycle = targetDuty;
        bSoftStartComplete = true;
        fDutyActual = nDutyCycle;   // hand off to the slew tracker at the reached duty
    }
}

// Effective PWM frequency: fixed nFreq, or — when bVariableFreq — derived from a signal
// (Hz = signal / denom), clamped to the valid 15..400 Hz window.
uint16_t Pwm::GetTargetFreq()
{
    if (pConfig->bVariableFreq && pConfig->nFreqInputDenom > 0 && pFreqInput != nullptr)
    {
        int32_t f = (int32_t)((*pFreqInput) / pConfig->nFreqInputDenom);
        if (f < 15) f = 15;
        if (f > 400) f = 400;
        return (uint16_t)f;
    }
    return pConfig->nFreq;
}

void Pwm::UpdateFrequency()
{
    const uint16_t nFreq = GetTargetFreq();

    // Frequency within range and
    // (Frequency setting has changed or
    // PWM driver frequency != Frequency setting)
    if (((nFreq <= 400) && (nFreq > 0)) &&
        ((nFreq != nLastFreq) ||
        (m_pwm->period != (m_pwmCfg->frequency / nFreq))))
    {
        pwmChangePeriod(m_pwm, m_pwmCfg->frequency / nFreq);
    }

    nLastFreq = nFreq;
}

msg_t Pwm::Init()
{
    return pwmStart(m_pwm, m_pwmCfg);
}

void Pwm::EnsureStarted()
{
    if (m_pwm->state != PWM_READY)
        Init();
}

void Pwm::OverrideFrequency(uint16_t nFreq)
{
    if (nFreq > 0 && nFreq <= 400 &&
        m_pwm->period != (m_pwmCfg->frequency / nFreq))
    {
        pwmChangePeriod(m_pwm, m_pwmCfg->frequency / nFreq);
    }
}

void Pwm::On()
{
    // PWM duty cycle is 0-10000
    //  100% = 10000
    //  50% = 5000
    //  0% = 0
    pwmEnableChannel(m_pwm, static_cast<uint8_t>(m_pwmCh), PWM_PERCENTAGE_TO_WIDTH(m_pwm, nDutyCycle * 100));
    pwmEnablePeriodicNotification(m_pwm);
    pwmEnableChannelNotification(m_pwm, static_cast<uint8_t>(m_pwmCh));
}

void Pwm::Off()
{
    pwmDisablePeriodicNotification(m_pwm);
    pwmDisableChannel(m_pwm, static_cast<uint8_t>(m_pwmCh));
}
#endif