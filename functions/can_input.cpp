#include "can_input.h"
#include "dbc.h"

bool CanInput::CheckMsg(CANRxFrame rx)
{
    if (!pConfig->bEnabled)
        return false;
    if (pConfig->nIDE &&
        (pConfig->nID != rx.EID))
        return false;
    if (!pConfig->nIDE &&
        (pConfig->nID != rx.SID))
        return false;
    if (pConfig->nBitLength == 0)
        return false;

    nLastRxTime = SYS_TIME;

    fVal = Dbc::DecodeFloat(rx.data8, pConfig->nStartBit, pConfig->nBitLength,
                            pConfig->fFactor, pConfig->fOffset,
                            pConfig->eByteOrder, pConfig->bSigned);

    // Use Input to enable momentary/latching
    fOutput = input.Check(pConfig->eMode, false, Compare(fVal));

    return true;
}

bool CanInput::Compare(float v) const
{
    switch (pConfig->eOperator)
    {
    case Operator::Equal:              return v == pConfig->fOperand;
    case Operator::NotEqual:           return v != pConfig->fOperand;
    case Operator::GreaterThan:        return v > pConfig->fOperand;
    case Operator::LessThan:           return v < pConfig->fOperand;
    case Operator::GreaterThanOrEqual: return v >= pConfig->fOperand;
    case Operator::LessThanOrEqual:    return v <= pConfig->fOperand;
    case Operator::BitwiseAnd:         return ((uint32_t)v & (uint32_t)pConfig->fOperand) > 0;
    case Operator::BitwiseNand:        return !(((uint32_t)v & (uint32_t)pConfig->fOperand) > 0);
    }
    return false;
}

void CanInput::CheckTimeout()
{
    if (!pConfig->bTimeoutEnabled)
        return;

    // The sender stopped: act as if fTimeoutValue had arrived (5.6; before, the input read 0 and was off).
    // The rule is applied directly, not through the latch: a latching input must not toggle on a lost sender.
    if (SYS_TIME - nLastRxTime > pConfig->nTimeout)
    {
        fVal = pConfig->fTimeoutValue;
        fOutput = Compare(fVal) ? 1 : 0;
    }
}