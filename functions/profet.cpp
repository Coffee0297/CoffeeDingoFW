#include "profet.h"
#include "dbc.h"

#if NUM_OUTPUTS > 0
void Profet::Update(bool bOutEnabled)
{
    eLastState = eState;

    if (!pConfig->bEnabled)
    {
        pwm.Off();
        palClearLine(m_in);
        nOcCount = 0;
        eState = ProfetState::Off;
        fCurrent = 0;
        fOutput = 0;
        fWarning = 0;
        fOpenLoad = 0;
        fUndercurrent = 0;
        fI2 = fLoad = 0;
        nTestMode = 0;
        return;
    }

    // DSEL for dual-channel devices is set by the caller, see SelectDsel()

    // Follower device — mirror primary
    if (pPrimary != nullptr)
    {
        FollowerUpdate();
        return;
    }

    // Test ends on timeout or trip; drop to Off for a cycle so PWM and inrush re-arm from the input
    if (nTestMode && (((int32_t)(nTestEnd - SYS_TIME) <= 0) || (eState != ProfetState::On && eState != ProfetState::Off)))
    {
        nTestMode = 0;
        if (eState == ProfetState::On)
        {
            pwm.Off();
            palClearLine(m_in);
            eState = ProfetState::Off;
        }
    }

    if (((*pInput) || nTestMode) && bOutEnabled)
        eReqState = ProfetState::On;
    else
        eReqState = ProfetState::Off;

    MeasureCurrent();

    // Sum follower current into primary's reported current
    if (pFollower != nullptr && pFollower->eState != ProfetState::Fault)
        fCurrent += pFollower->fCurrent;

    // Check for fault (device overcurrent/overtemp/short)
    // Raw ADC current reading will be very high
    if (nIS > 30000)
    {
        eState = ProfetState::Fault;
    }

    // Follower hardware fault is permanent — bring down the primary too
    if (pFollower != nullptr && pFollower->eState == ProfetState::Fault)
        eState = ProfetState::Fault;

    bInRushActive = (SYS_TIME - nInRushOnTime) < pConfig->nInrushTime;

    // Current-limited soft start: the ramp holds above the inrush limit instead of tripping,
    // and the output trips if the ramp hasn't finished within the timeout
    bool bLimitedStart = pConfig->nSoftStartTimeout > 0 && pwm.IsSoftStarting();
    bool bHoldSoftStart = false;

    switch (eState)
    {
    case ProfetState::Off:
        pwm.Off();

        palClearLine(m_in);

        nOcCount = 0;

        // Check for turn on
        if (eReqState == ProfetState::On)
        {
            nInRushOnTime = SYS_TIME;
            eState = ProfetState::On;
        }
        break;

    case ProfetState::On:
        if (nTestMode == 2)
        {
            pwm.EnsureStarted();
            pwm.OverrideFrequency(nTestFreq);
            pwm.SetDutyCycle(nTestDuty);
            pwm.On();
        }
        else if (PwmRunning())
            pwm.On();
        else
        {
            pwm.Off();
            palSetLine(m_in);
        }

        // Check for turn off
        if (eReqState == ProfetState::Off)
        {
            // Soft stop ramps a PWM output's duty down first, but a disabled output (starter disable) cuts at once
            if (pwm.IsEnabled() && pConfig->nSoftStopTime > 0 && bOutEnabled)
            {
                if (!bSoftStopping)
                {
                    bSoftStopping = true;
                    nSoftStopStart = SYS_TIME;
                    nSoftStopDuty = pwm.GetDutyCycle();
                }

                if ((SYS_TIME - nSoftStopStart) >= pConfig->nSoftStopTime)
                    eState = ProfetState::Off;
            }
            else
                eState = ProfetState::Off;
        }
        else if (bSoftStopping)
        {
            // Back on during the soft stop - return to the duty it had
            bSoftStopping = false;
            pwm.SetDutyCycle(nSoftStopDuty);
        }

        // Overcurrent
        if (fCurrent > pConfig->fCurrentLimit && !bInRushActive)
        {
            nOcTriggerTime = SYS_TIME;
            nOcCount++;
            eState = ProfetState::Overcurrent;
        }

        // Inrush overcurrent
        if (fCurrent > pConfig->fInrushLimit && bInRushActive && !bLimitedStart)
        {
            nOcTriggerTime = SYS_TIME;
            nOcCount++;
            eState = ProfetState::Overcurrent;
        }

        if (bLimitedStart)
        {
            bHoldSoftStart = fCurrent > pConfig->fInrushLimit;

            if ((SYS_TIME - nInRushOnTime) > pConfig->nSoftStartTimeout)
            {
                nOcTriggerTime = SYS_TIME;
                nOcCount++;
                eState = ProfetState::Overcurrent;
            }
}

        // Hard short - straight to Fault, inrush window or not, no retry
        if (pConfig->fHardShortLimit > 0 && fCurrent > pConfig->fHardShortLimit)
        {
            nOcCount++;
            eState = ProfetState::Fault;
        }

        break;

    case ProfetState::Overcurrent:
        pwm.Off();

        palClearLine(m_in);

        // No reset, or overcurrent count exceeded - latch fault
        // Checked first so nothing below can override it
        if ((pConfig->eResetMode == ProfetResetMode::None) ||
            (pConfig->eResetMode == ProfetResetMode::Count && nOcCount >= pConfig->nResetLimit))
        {
            eState = ProfetState::Fault;
        }
        // Check for turn off
        else if (eReqState == ProfetState::Off)
        {
            eState = ProfetState::Off;
        }
        // Overcurrent reset time exceeded
        // ResetEndless or ResetCount
        else if ((SYS_TIME - nOcTriggerTime) > pConfig->nResetTime)
        {
            nInRushOnTime = SYS_TIME;
            eState = ProfetState::On;
        }
        break;

    case ProfetState::Fault:
        pwm.Off();

        palClearLine(m_in);
        // Fault requires power cycle, a retry-all or (if enabled) the input going off
        if (pConfig->bClearOnReassert && !(*pInput))
            ClearFault();
        break;

    default: // Warning/OpenLoad/Undercurrent are overlays, never in eState
        break;
    }

    // The test owns duty and frequency while it runs
    if (!nTestMode)
        pwm.Update(bHoldSoftStart);

    // Past inrush: Warning above fWarnLimit, OpenLoad below fOpenLoadLimit for nOpenLoadTime,
    // Undercurrent below fUnderLimit for nUnderTime (report only, or a trip to Fault with bUnderTrip)
    bool bReportOn = (eState == ProfetState::On) && ((SYS_TIME - nInRushOnTime) >= pConfig->nInrushTime);
    bool bOpenLoad = bReportOn && (pConfig->fOpenLoadLimit > 0) && (fCurrent < pConfig->fOpenLoadLimit);
    bool bUnder = bReportOn && (pConfig->fUnderLimit > 0) && (fCurrent < pConfig->fUnderLimit);
    if (!bOpenLoad)
        nOpenLoadStart = SYS_TIME;
    if (!bUnder)
        nUnderStart = SYS_TIME;
    bOpenLoad = bOpenLoad && ((SYS_TIME - nOpenLoadStart) >= pConfig->nOpenLoadTime);
    bUnder = bUnder && ((SYS_TIME - nUnderStart) >= pConfig->nUnderTime);

    if (bUnder && pConfig->bUnderTrip)
    {
        pwm.Off();
        palClearLine(m_in);
        eState = ProfetState::Fault;
    }

    if (bOpenLoad)
        eOverlay = ProfetState::OpenLoad;
    else if (bUnder)
        eOverlay = ProfetState::Undercurrent;
    else if (bReportOn && (pConfig->fWarnLimit > 0) && (fCurrent > pConfig->fWarnLimit))
        eOverlay = ProfetState::Warning;
    else
        eOverlay = ProfetState::On;

    // After pwm.Update() so the ramp overrides the duty it sets
    if (bSoftStopping && eState == ProfetState::On)
    {
        uint32_t nElapsed = SYS_TIME - nSoftStopStart;
        if (nElapsed < pConfig->nSoftStopTime)
            pwm.SetDutyCycle(nSoftStopDuty * (pConfig->nSoftStopTime - nElapsed) / pConfig->nSoftStopTime);
    }
    else
        bSoftStopping = false;

    // Set var map values
    fOutput = eState == ProfetState::On ? 1 : 0;
    fOvercurrent = eState == ProfetState::Overcurrent ? 1 : 0;
    fFault = eState == ProfetState::Fault ? 1 : 0;
    fWarning = GetReportedState() == ProfetState::Warning ? 1 : 0;
    fOpenLoad = GetReportedState() == ProfetState::OpenLoad ? 1 : 0;
    fUndercurrent = GetReportedState() == ProfetState::Undercurrent ? 1 : 0;

    palClearLine(LINE_E2);
}

// Back to Off with the retry count cleared, so the output turns on again if requested
void Profet::ClearFault()
{
    if (eState == ProfetState::Overcurrent || eState == ProfetState::Fault)
    {
        eState = ProfetState::Off;
        nOcCount = 0;
    }

    // A follower latches Fault when it mirrors its primary's, and would hold the primary in Fault
    if (pFollower != nullptr && pFollower->eState == ProfetState::Fault)
        pFollower->eState = ProfetState::Off;
}

void Profet::FollowerUpdate()
{
    MeasureCurrent();

    // Hardware fault — independent, permanent
    if (nIS > 30000)
        eState = ProfetState::Fault;

    if (eState == ProfetState::Fault)
    {
        pwm.Off();
        palClearLine(m_in);
        fOutput = 0; 
        fOvercurrent = 0; 
        fFault = 1;
        return;
    }

    // Mirror primary state
    if (pPrimary->eState == ProfetState::On)
    {
        if (pPrimary->IsPwmEnabled())
        {
            pwm.EnsureStarted();
            pwm.OverrideFrequency(pPrimary->GetFrequency());
            pwm.SetDutyCycle(pPrimary->GetDutyCycle());
            // Align follower timer phase to primary on first enable so both switches
            // transition simultaneously. Both timers share the same APB clock so
            // there is no ongoing drift — only the initial phase offset needs fixing.
            if (eLastState != ProfetState::On)
                m_pwmDriver->tim->CNT = pPrimary->m_pwmDriver->tim->CNT;
            pwm.On();
        }
        else
        {
            pwm.Off();
            palSetLine(m_in);
        }
        eState = ProfetState::On;
    }
    else
    {
        pwm.Off();
        palClearLine(m_in);
        eState = pPrimary->eState;
    }

    fOutput = eState == ProfetState::On ? 1 : 0;
    fOvercurrent = eState == ProfetState::Overcurrent ? 1 : 0;
    fFault = 0;
}

bool Profet::SetTest(uint8_t nMode, uint8_t nDuty, uint16_t nFreq, uint8_t nHoldSec)
{
    if (nMode == 0)
    {
        if (nTestMode)
            nTestEnd = SYS_TIME; // ends next cycle through the normal path
        return true;
    }

    // Paired outputs share the load, testing one half would overload the other
    if (!pConfig->bEnabled || pPrimary != nullptr || pFollower != nullptr ||
        nMode > 2 || nHoldSec < 1 || nHoldSec > 30 ||
        (nMode == 2 && (nDuty > 100 || nFreq < 16 || nFreq > 400)) ||
        eState == ProfetState::Overcurrent || eState == ProfetState::Fault)
        return false;

    nTestDuty = nDuty;
    nTestFreq = nFreq;
    nTestEnd = SYS_TIME + (uint32_t)nHoldSec * 1000;
    nTestMode = nMode;
    return true;
}

void Profet::SelectDsel()
{
    // Select the IS channel on dual-channel devices
    // Caller waits DSEL_SETTLE_US before measuring, once for all outputs in the pass
    if (m_model == ProfetModel::BTS7008_2EPA_CH1)
        palClearLine(m_dsel);
    else if (m_model == ProfetModel::BTS7008_2EPA_CH2)
        palSetLine(m_dsel);
}

void Profet::MeasureCurrent()
{
    if (PwmRunning() && eState == ProfetState::On)
    {
        // Assign to local vars to prevent CNT rolling over and slipping past check
        // Example:
        // CCR = 2500
        // When checking read delay CNT = 2499
        // Before getting to the CNT < CCR check the CNT has rolled over to 0
        // This will cause an incorrect reading
        // Copying to local var freezes the CNT value
        uint32_t nCCR = m_pwmDriver->tim->CCR[static_cast<uint8_t>(m_pwmChannel)];
        uint32_t nCNT = m_pwmDriver->tim->CNT;

        // Delay for the first 1/3 of the PWM period to allow current to stabilize after switching, or a fixed min delay if the period is very short. 
        // This ensures we don't read during the switching transient
        uint32_t nDelay = (nCCR / 3 > nPwmReadDelay) ? nCCR / 3 : nPwmReadDelay;

        if ((nCCR > nDelay) &&
            (nCNT > nDelay) &&
            (nCNT < nCCR))
        {
            //==============================================================================
            //Trigger E2 on channel 1 for debug purposes to verify timing
            if (m_num == 1)
                palSetLine(LINE_E2);
            //==============================================================================

            nIS = GetAdcRaw(m_ain);
            nLastIS = nIS;
        }
        else
        {
            nIS = nLastIS;
        }
    }
    else
    {
        // Also the PWM fallback: at turn-on a low duty has no sample window yet, so it must not
        // reuse the last on-time reading (a held soft start would never start again after a trip)
        nIS = GetAdcRaw(m_ain);
        nLastIS = nIS;
    }

    CalculateCurrent();

    // Wire load from this output's own current, before a follower's is added
    if (pConfig->nThermalTau > 0 && pConfig->fCurrentLimit > 0)
    {
        fI2 = FilterI2(fI2, fCurrent, SYS_TIME - nLoadTime, pConfig->nThermalTau);
        fLoad = 100.0f * fI2 / (pConfig->fCurrentLimit * pConfig->fCurrentLimit);
    }
    else
        fI2 = fLoad = 0;
    nLoadTime = SYS_TIME;
}

void Profet::CalculateCurrent()
{
    // Current = (rawVal * (VDDA / 4095)) / 1.2k) * kILIS
    fCurrent = (((float)nIS * (GetVDDA() / 4095)) / 1200) * fKILIS;

    // Noise floor — ignore current below measurable threshold
    switch (m_model)
    {
    case ProfetModel::BTS7002_1EPP:
        if (fCurrent <= 0.5f)
            fCurrent = 0;
        break;
    case ProfetModel::BTS7008_2EPA_CH1:
    case ProfetModel::BTS7008_2EPA_CH2:
        if (fCurrent <= 0.2f)
            fCurrent = 0;
        break;
    case ProfetModel::BTS70012_1ESP:
        if (fCurrent <= 1.0f)
            fCurrent = 0;
        break;
    }
}

#endif
