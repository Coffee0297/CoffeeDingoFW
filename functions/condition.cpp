#include "condition.h"
#include "dbc.h"

void Condition::Update()
{
    if (!pConfig->bEnabled)
    {
        fVal = 0;
        bOut = false;
        nLastSame = SYS_TIME;
        return;
    }

    // While on, the relational ops release fHyst further away from fArg (hysteresis); 0 = plain compare.
    // "> 0" also turns a NaN band into a plain compare instead of a condition that flips every cycle.
    const float fBand = (fVal && pConfig->fHyst > 0) ? pConfig->fHyst : 0;

    switch(pConfig->eOperator)
    {
        case Operator::Equal:
            fVal = *pInput == pConfig->fArg;
            break;

        case Operator::NotEqual:
            fVal = *pInput != pConfig->fArg;
            break;

        case Operator::GreaterThan:
            fVal = *pInput > pConfig->fArg - fBand;
            break;

        case Operator::LessThan:
            fVal = *pInput < pConfig->fArg + fBand;
            break;

        case Operator::GreaterThanOrEqual:
            fVal = *pInput >= pConfig->fArg - fBand;
            break;

        case Operator::LessThanOrEqual:
            fVal = *pInput <= pConfig->fArg + fBand;
            break;

        case Operator::BitwiseAnd:
            fVal = ((uint16_t)(*pInput) & (uint16_t)(pConfig->fArg)) > 0;
            break;

        case Operator::BitwiseNand:
            fVal = !((uint16_t)(*pInput) & (uint16_t)(pConfig->fArg));
            break;

        default:
            fVal = 0;
            break;
    }

    // The result follows the compare only once it has differed for the delay
    const bool bCompare = fVal != 0;
    if ((bCompare == bOut) || ((SYS_TIME - nLastSame) >= (bCompare ? pConfig->nDelayOn : pConfig->nDelayOff)))
    {
        bOut = bCompare;
        nLastSame = SYS_TIME;
    }

    if (pConfig->nDelayOn || pConfig->nDelayOff)
        fVal = bOut;
}