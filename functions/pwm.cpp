#include "pwm.h"
#include "dbc.h"

#if NUM_OUTPUTS > 0
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
        // After soft start completes, continue updating duty cycle if variable
        else if (pConfig->bVariableDutyCycle) {
            if (pConfig->bRampDutyChanges)
                UpdateDutyRamp();
            else
                nDutyCycle = GetTargetDutyCycle();
        }
    }

    UpdateFrequency();
    
    bLastChannelEnabled = bChannelEnabled;
}

uint8_t Pwm::GetTargetDutyCycle() {
    if (pConfig->bVariableDutyCycle && pConfig->nDutyCycleInputDenom > 0) {
        // Clamp duty cycle to 100% and minimum duty cycle
        float fDc = (*pInput) / pConfig->nDutyCycleInputDenom;
        if (fDc > 100.0f)
            fDc = 100.0f;
        if (!(fDc >= pConfig->nMinDutyCycle))
            fDc = pConfig->nMinDutyCycle;
        return (uint8_t)fDc;
    }
    return pConfig->nFixedDutyCycle;
}

void Pwm::InitSoftStart() {
    nSoftStartTime = SYS_TIME;

    // No ramp time configured - go straight to target instead of dividing by zero
    if (pConfig->nSoftStartRampTime == 0) {
        nDutyCycle = GetTargetDutyCycle();
        bSoftStartComplete = true;
        return;
    }

    fSoftStartStep = GetTargetDutyCycle() / (float)pConfig->nSoftStartRampTime;
    bSoftStartComplete = false;
}

void Pwm::UpdateSoftStart() {
    uint8_t targetDuty = GetTargetDutyCycle();
    // Compare in float - the ramp can overshoot 255 in one step on short ramps
    float fDuty = (fSoftStartStep * (SYS_TIME - nSoftStartTime)) + pConfig->nMinDutyCycle;

    if (fDuty >= targetDuty) {
        nDutyCycle = targetDuty;
        bSoftStartComplete = true;
        fDutyRamp = targetDuty;
        nSoftStartTime = SYS_TIME;
    }
    else {
        nDutyCycle = (uint8_t)fDuty;
    }
}

void Pwm::UpdateDutyRamp() {
    // Slew at the soft-start rate: a full 0-100% change takes nSoftStartRampTime
    // nSoftStartTime = last ramp update once the soft start is complete
    uint8_t targetDuty = GetTargetDutyCycle();
    float fStep = 100.0f;
    if (pConfig->nSoftStartRampTime > 0)
        fStep = 100.0f * (SYS_TIME - nSoftStartTime) / pConfig->nSoftStartRampTime;
    nSoftStartTime = SYS_TIME;

    if (fDutyRamp + fStep < targetDuty)
        fDutyRamp += fStep;
    else if (fDutyRamp - fStep > targetDuty)
        fDutyRamp -= fStep;
    else
        fDutyRamp = targetDuty;

    nDutyCycle = (uint8_t)fDutyRamp;
}

uint16_t Pwm::GetTargetFreq()
{
    if (!pConfig->bVariableFreq || pConfig->nFreqInputDenom == 0)
        return (pConfig->nFreq > 0 && pConfig->nFreq < PWM_MIN_FREQ) ? PWM_MIN_FREQ : pConfig->nFreq;

    // 16 Hz = slowest period that fits the 16 bit timers at 1 MHz
    float fFreq = (*pFreqInput) / pConfig->nFreqInputDenom;
    if (fFreq > 400.0f)
        fFreq = 400.0f;
    if (!(fFreq >= PWM_MIN_FREQ))
        fFreq = PWM_MIN_FREQ;
    return (uint16_t)fFreq;
}

void Pwm::UpdateFrequency()
{
    uint16_t nFreq = GetTargetFreq();

    // Frequency within range and
    // (Frequency setting has changed or
    // PWM driver frequency != Frequency setting)
    if (((nFreq <= 400) && (nFreq > 0)) &&
        ((nFreq != nLastFreq) ||
        (m_pwm->period != (m_pwmCfg->frequency / nFreq))))
    {
        pwmChangePeriod(m_pwm, m_pwmCfg->frequency / nFreq);

        // Width was set for the old period, rescale it so the duty holds
        if (bChannelEnabled)
            On();
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
    if (nFreq > 0 && nFreq < PWM_MIN_FREQ)
        nFreq = PWM_MIN_FREQ;

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