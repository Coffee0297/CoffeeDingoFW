#include "can_input.h"
#include "dbc.h"
#include "can_frame.h"

bool CanInput::CheckMsg(const CANRxFrame &rx, uint8_t nBus)
{
    if (!pConfig->bEnabled)
        return false;
    if (CanInputBus(*pConfig) != nBus)
        return false;
    if ((CanIsExt(rx) != (pConfig->nIDE == 1)) ||
        (pConfig->nID != CanId(rx)))
        return false;
    if (pConfig->nBitLength == 0)
        return false;

    nLastRxTime = SYS_TIME;

    fVal = Dbc::DecodeFloat(rx.data8, pConfig->nStartBit, pConfig->nBitLength,
                            pConfig->fFactor, pConfig->fOffset,
                            pConfig->eByteOrder, pConfig->bSigned);

    // Use Input to enable momentary/latching
    switch (pConfig->eOperator)
    {
    case Operator::Equal:
        fOutput = input.Check(pConfig->eMode, false, fVal == pConfig->fOperand);
        break;

    case Operator::NotEqual:
        fOutput = input.Check(pConfig->eMode, false, fVal != pConfig->fOperand);
        break;

    case Operator::GreaterThan:
        fOutput = input.Check(pConfig->eMode, false, fVal > pConfig->fOperand);
        break;

    case Operator::LessThan:
        fOutput = input.Check(pConfig->eMode, false, fVal < pConfig->fOperand);
        break;

    case Operator::GreaterThanOrEqual:
        fOutput = input.Check(pConfig->eMode, false, fVal >= pConfig->fOperand);
        break;

    case Operator::LessThanOrEqual:
        fOutput = input.Check(pConfig->eMode, false, fVal <= pConfig->fOperand);
        break;

    case Operator::BitwiseAnd:
        fOutput = input.Check(pConfig->eMode, false, ((uint32_t)fVal & (uint32_t)pConfig->fOperand) > 0);
        break;

    case Operator::BitwiseNand:
        fOutput = input.Check(pConfig->eMode, false, !(((uint32_t)fVal & (uint32_t)pConfig->fOperand) > 0));
        break;
    }

    return true;
}

void CanInput::CheckTimeout()
{
    if (!pConfig->bTimeoutEnabled)
        return;

    if (SYS_TIME - nLastRxTime > pConfig->nTimeout)
    {
        fVal = 0;
        fOutput = 0;
    }
}