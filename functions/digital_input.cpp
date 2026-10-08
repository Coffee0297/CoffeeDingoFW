#include "digital_input.h"

// PWM mode: no edge for 3 periods (configured, or the last measured one) reads as a steady level, 0 % or
// 100 %; clamped to 50 ms .. 3 s, so auto-detect covers ~1 Hz upward. ponytail: edge IRQ per transition,
// fine to a few kHz; a timer input-capture channel per pin if faster signals are ever needed.
#define PWM_MAX_PERIOD_MS 1000U
#define PWM_MIN_TIMEOUT_MS 50U

static void PwmEdgeCb(void *arg)
{
    static_cast<Digital_Input *>(arg)->OnEdge();
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
        meter.Reset();
        nLastEdgeMs = SYS_TIME;
        palSetLineCallbackI(m_line, PwmEdgeCb, this);
        palEnableLineEventI(m_line, PAL_EVENT_MODE_BOTH_EDGES);
        bPwmActive = true;
        chSysUnlock();
    }
}

void Digital_Input::StopPwm()
{
    if (!bPwmActive)
        return;
    palDisableLineEvent(m_line);
    bPwmActive = false;
}

// Both edges, from the EXTI ISR (cycle-counter timestamps).
void Digital_Input::OnEdge()
{
    rtcnt_t now = chSysGetRealtimeCounterX();
    chSysLockFromISR();
    meter.Edge(palReadLine(m_line), now);
    nLastEdgeMs = SYS_TIME;
    chSysUnlockFromISR();
}

void Digital_Input::UpdatePwm()
{
    chSysLock();
    PwmMeter::Sample s = meter.Take();
    uint32_t lastEdge = nLastEdgeMs;
    chSysUnlock();

    const uint32_t clk = STM32_HCLK;
    uint32_t timeout;
    if (pConfig->nPwmFreq > 0)
        timeout = 3000U / pConfig->nPwmFreq;   // 3 periods of the configured frequency
    else
        timeout = 3U * (uint32_t)(((uint64_t)s.lastPeriod * 1000U) / clk);
    if (timeout < PWM_MIN_TIMEOUT_MS) timeout = PWM_MIN_TIMEOUT_MS;
    if (timeout > 3U * PWM_MAX_PERIOD_MS) timeout = 3U * PWM_MAX_PERIOD_MS;

    bool level = palReadLine(m_line) != pConfig->bInvert;
    if ((SYS_TIME - lastEdge) > timeout)
    {
        // no signal: a steady level is 0 % or 100 %
        fDuty = level ? 100.0f : 0.0f;
        fFreq = 0;
        fVal = 0;
        return;
    }

    if (s.periods > 0)
    {
        float duty = PwmMeter::Duty(s);
        fDuty = pConfig->bInvert ? 100.0f - duty : duty;
        fFreq = (pConfig->nPwmFreq > 0) ? (float)pConfig->nPwmFreq : PwmMeter::Freq(s, clk);
    }
    fVal = 1;
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
