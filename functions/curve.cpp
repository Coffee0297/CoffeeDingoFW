#include "curve.h"

void Curve::Update()
{
    if (!pConfig->bEnabled)
    {
        fVal = 0;
        return;
    }

    const float *fX = pConfig->fX;
    const float *fY = pConfig->fY;
    uint8_t nPoints = pConfig->nPoints > CURVE_MAX_POINTS ? CURVE_MAX_POINTS : pConfig->nPoints;
    float fIn = *pInput;

    // Ends are held. A point whose X is not above the last used X is skipped, so a
    // half-edited curve never divides by zero or runs backwards.
    fVal = fY[0];
    if (fIn <= fX[0])
        return;

    uint8_t nPrev = 0;
    for (uint8_t i = 1; i < nPoints; i++)
    {
        if (fX[i] <= fX[nPrev])
            continue;

        if (fIn <= fX[i])
        {
            fVal = fY[nPrev] + (fIn - fX[nPrev]) * (fY[i] - fY[nPrev]) / (fX[i] - fX[nPrev]);
            return;
        }

        nPrev = i;
    }

    fVal = fY[nPrev];
}
