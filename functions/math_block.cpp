#include "math_block.h"

float MathEval(MathOp op, float a, float b)
{
    switch (op)
    {
    case MathOp::Add:     return a + b;
    case MathOp::Sub:     return a - b;
    case MathOp::Mul:     return a * b;
    case MathOp::Div:     return b == 0.0f ? 0.0f : a / b;
    case MathOp::Min:     return a < b ? a : b;
    case MathOp::Max:     return a > b ? a : b;
    case MathOp::Average: return (a + b) * 0.5f;
    }
    return 0.0f;
}

void MathBlock::Update()
{
    if (!pConfig->bEnabled || !pA)
    {
        fVal = 0;
        return;
    }
    const float b = pB ? *pB : pConfig->fConstB;
    fVal = MathEval(pConfig->eOp, *pA, b) * pConfig->fFactor + pConfig->fOffset;
}
