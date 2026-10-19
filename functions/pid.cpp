#include "pid.h"
#include <cmath>

// NaN lands on the lower limit, inf on a limit: a bad input can never reach a duty
static float Clamp(float f, float fMin, float fMax)
{
    if (!(f > fMin))
        f = fMin;
    if (f > fMax)
        f = fMax;
    return std::isfinite(f) ? f : 0.0f;
}

void Pid::Update()
{
    // Crossed limits are treated as disabled rather than guessing which one was meant
    if (!pConfig->bEnabled ||
        (pConfig->fOutMin > pConfig->fOutMax) ||
        (pConfig->fIMin > pConfig->fIMax) ||
        (pConfig->fPvMin > pConfig->fPvMax))
    {
        bInit = false;
        bPrevValid = false;
        fVal = fP = fI = fD = 0.0f;
        return;
    }

    const uint32_t nNow = SYS_TIME;
    if (bInit && ((nNow - nLastTime) < pConfig->nPeriodMs))
        return;

    const float fDt = bInit ? (nNow - nLastTime) * 0.001f : 0.0f;
    nLastTime = nNow;
    bInit = true;

    const float fSp = *pSetpoint;
    const float fPv = *pProcess;
    const float fFf = pConfig->fFeedForward + *pFeedFwd;

    if (!(*pActivate > 0.5f))
    {
        bPrevValid = false;
        fP = fI = fD = 0.0f;
        fVal = Clamp(fFf, pConfig->fOutMin, pConfig->fOutMax);
        return;
    }

    const bool bWindow = (pConfig->fPvMin != 0.0f) || (pConfig->fPvMax != 0.0f);
    if (!std::isfinite(fSp) || !std::isfinite(fPv) || !std::isfinite(fFf) ||
        (bWindow && ((fPv < pConfig->fPvMin) || (fPv > pConfig->fPvMax))))
    {
        // Integral frozen, so control picks up where it left off when the sensor is back
        bPrevValid = false;
        fP = fD = 0.0f;
        fVal = Clamp(pConfig->fFallback, pConfig->fOutMin, pConfig->fOutMax);
        return;
    }

    // Bumpless (re)start: the setpoint ramps from the measured value, no derivative step
    if (!bPrevValid)
    {
        bPrevValid = true;
        fSpRamp = fPv;
        fPrevPv = fPv;
    }

    if (pConfig->fSpRate > 0.0f)
        fSpRamp = Clamp(fSp, fSpRamp - (pConfig->fSpRate * fDt), fSpRamp + (pConfig->fSpRate * fDt));
    else
        fSpRamp = fSp;

    float fErr = pConfig->bInvert ? (fPv - fSpRamp) : (fSpRamp - fPv);
    if (std::fabs(fErr) < pConfig->fDeadband)
        fErr = 0.0f;

    fP = pConfig->fKp * fErr;

    // Derivative on measurement, so a setpoint change doesn't kick the output
    const float fDPv = fPv - fPrevPv;
    fPrevPv = fPv;
    fD = (fDt > 0.0f) ? (pConfig->fKd * (pConfig->bInvert ? fDPv : -fDPv) / fDt) : 0.0f;

    // Anti-windup: the integral is clamped and never pushed further into a saturated output
    const float fINew = Clamp(fI + (pConfig->fKi * fErr * fDt), pConfig->fIMin, pConfig->fIMax);
    const float fOut = fFf + fP + fINew + fD;
    if (!(((fOut > pConfig->fOutMax) && (fINew > fI)) || ((fOut < pConfig->fOutMin) && (fINew < fI))))
        fI = fINew;

    fVal = Clamp(fFf + fP + fI + fD, pConfig->fOutMin, pConfig->fOutMax);
}
