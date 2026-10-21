#include "math_block.h"

void MathBlock::Update()
{
    if (!pConfig->bEnabled)
    {
        fVal = 0;
        return;
    }

    float a = *pInputA;
    float b = *pInputB;
    float fResult = 0;

    switch (pConfig->eOp)
    {
    case MathOp::Add:
        fResult = a + b;
        break;

    case MathOp::Sub:
        fResult = a - b;
        break;

    case MathOp::Mul:
        fResult = a * b;
        break;

    case MathOp::Div:
        // 0 instead of inf/NaN, so nothing downstream sees a value it can't compare
        fResult = (b == 0) ? 0 : a / b;
        break;

    case MathOp::Min:
        fResult = (a < b) ? a : b;
        break;

    case MathOp::Max:
        fResult = (a > b) ? a : b;
        break;

    case MathOp::Average:
        fResult = (a + b) / 2;
        break;
    }

    fVal = fResult * pConfig->fFactor + pConfig->fOffset;
}
