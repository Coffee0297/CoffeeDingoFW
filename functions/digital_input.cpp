#include "digital_input.h"

// No edge for 3 measured periods reads as a steady 0 % or 100 %,
// clamped to 50 ms .. 3 s. ponytail: one EXTI interrupt per edge, fine to a few kHz; a timer
// input-capture channel per pin if faster signals are ever needed.
#define PWM_MIN_TIMEOUT_MS 50U
#define PWM_MAX_TIMEOUT_MS 3000U

static void PwmEdgeCb(void *arg)
{
    static_cast<Digital_Input *>(arg)->OnEdge();
}

// The EXTI line is also the sleep wake source, so sleep stops the PWM measurement first
void Digital_Input::StopPwm()
{
    if (!bPwmActive)
        return;
    palDisableLineEvent(m_line);
    bPwmActive = false;
}

void Digital_Input::OnEdge()
{
    rtcnt_t now = chSysGetRealtimeCounterX();
    chSysLockFromISR();
    if (meter.Edge(palReadLine(m_line), now))
        nLastEdge = chVTGetSystemTimeX(); // filtered-out spikes don't keep a dead signal alive
    chSysUnlockFromISR();
}

void Digital_Input::UpdatePwm()
{
    chSysLock();
    PwmMeter::Sample s = meter.Take();
    systime_t nLast = nLastEdge;
    chSysUnlock();

    uint32_t nTimeout = 3U * (s.lastPeriod / (STM32_HCLK / 1000U));
    if (nTimeout < PWM_MIN_TIMEOUT_MS) nTimeout = PWM_MIN_TIMEOUT_MS;
    if (nTimeout > PWM_MAX_TIMEOUT_MS) nTimeout = PWM_MAX_TIMEOUT_MS;

    if (chTimeDiffX(nLast, chVTGetSystemTimeX()) > TIME_MS2I(nTimeout))
    {
        fDuty = (palReadLine(m_line) != pConfig->bInvert) ? 100.0f : 0.0f;
        fFreq = 0;
        fVal = 0;
        return;
    }

    if (s.periods > 0)
    {
        fDuty = PwmMeter::Duty(s);
        fFreq = PwmMeter::Freq(s, STM32_HCLK);
    }
    fVal = 1;
}

void Digital_Input::SetConfig(Config_DigInput *config)
{
    StopPwm();
    pConfig = config;

    SetPull(config->ePull);

    fDuty = 0;
    fFreq = 0;
    if (config->bEnabled && config->bPwm)
    {
        chSysLock();
        meter.nMinCycles = (uint32_t)config->nPwmMinPulseUs * (STM32_HCLK / 1000000U);
        meter.Reset();
        nLastEdge = chVTGetSystemTimeX();
        palSetLineCallbackI(m_line, PwmEdgeCb, this);
        palEnableLineEventI(m_line, PAL_EVENT_MODE_BOTH_EDGES);
        bPwmActive = true;
        chSysUnlock();
    }
}

void Digital_Input::Update()
{
    if(!pConfig->bEnabled)
    {
        fVal = 0;
        fDuty = 0;
        fFreq = 0;
        return;
    }

    if (pConfig->bPwm)
    {
        UpdatePwm();
        return;
    }

    bool bIn;

    bIn = palReadLine(m_line);

    // Debounce input
    if (bIn != bLast)
    {
        nLastTrigTime = SYS_TIME;
        bCheck = true;
    }

    bLast = bIn;

    if ((bCheck && ((SYS_TIME - nLastTrigTime) > pConfig->nDebounceTime)) || (!bInit))
    {
        bCheck = false;
        fVal = input.Check(pConfig->eMode, pConfig->bInvert, bIn);
    }

    bInit = true;
}

void Digital_Input::SetPull(InputPull pull)
{
    switch (pull)
    {
    case InputPull::None:
        palSetLineMode(m_line, PAL_MODE_INPUT);
        break;
    case InputPull::Up:
        palSetLineMode(m_line, PAL_MODE_INPUT_PULLUP);
        break;
    case InputPull::Down:
        palSetLineMode(m_line, PAL_MODE_INPUT_PULLDOWN);
        break;
    }
}