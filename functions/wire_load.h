#pragma once

#include <cstdint>

// Wire heating: first-order filter of I^2 with time constant nTauS, step dt = nDtMs
inline float FilterI2(float fI2, float fCurrent, uint32_t nDtMs, uint16_t nTauS)
{
    float fK = nDtMs / (nTauS * 1000.0f);
    if (fK > 1.0f)
        fK = 1.0f;
    return fI2 + (fCurrent * fCurrent - fI2) * fK;
}
