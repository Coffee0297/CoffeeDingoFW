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

    switch(pConfig->eOperator)
    {
        case Operator::Equal:
            fVal = *pInput == pConfig->fArg;
            break;

        case Operator::NotEqual:
            fVal = *pInput != pConfig->fArg;
            break;

        case Operator::GreaterThan:
            fVal = *pInput > pConfig->fArg;
            break;

        case Operator::LessThan:
            fVal = *pInput < pConfig->fArg;
            break;

        case Operator::GreaterThanOrEqual:
            fVal = *pInput >= pConfig->fArg;
            break;

        case Operator::LessThanOrEqual:
            fVal = *pInput <= pConfig->fArg;
            break;

        case Operator::BitwiseAnd:
            fVal = (uint16_t)(*pInput) & (uint16_t)(pConfig->fArg);
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