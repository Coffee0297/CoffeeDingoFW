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

bool CanInput::Compare(float fIn)
{
    switch (pConfig->eOperator)
    {
    case Operator::Equal:
        return fIn == pConfig->fOperand;

    case Operator::NotEqual:
        return fIn != pConfig->fOperand;

    case Operator::GreaterThan:
        return fIn > pConfig->fOperand;

    case Operator::LessThan:
        return fIn < pConfig->fOperand;

    case Operator::GreaterThanOrEqual:
        return fIn >= pConfig->fOperand;

    case Operator::LessThanOrEqual:
        return fIn <= pConfig->fOperand;

    case Operator::BitwiseAnd:
        return ((uint32_t)fIn & (uint32_t)pConfig->fOperand) > 0;

    case Operator::BitwiseNand:
        return !(((uint32_t)fIn & (uint32_t)pConfig->fOperand) > 0);
    }

    return false;
}

void CanInput::CheckTimeout()
{
    if (!pConfig->bTimeoutEnabled)
        return;

    if (SYS_TIME - nLastRxTime > pConfig->nTimeout)
    {
        // Sender lost: read 0 and off, or act as if fTimeoutValue had arrived (e.g. coolant temp 120 so a fan
        // rule turns on). The rule is applied directly, not through the latch, so a latching input can't toggle.
        bool bUseValue = pConfig->bEnabled && pConfig->bTimeoutUseValue;
        fVal = bUseValue ? pConfig->fTimeoutValue : 0;
        fOutput = bUseValue && Compare(fVal);
    }
}