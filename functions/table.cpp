#include "table.h"

// Lower index i (0..n-2) and the fraction t (0..1) of x between axis[i] and axis[i+1], held at the ends
static void Locate(const float* axis, uint8_t n, float x, uint8_t& i, float& t)
{
    if (n <= 1 || x <= axis[0]) { i = 0; t = 0.0f; return; }
    if (x >= axis[n - 1]) { i = n - 2; t = 1.0f; return; }
    i = 0;
    while (i < n - 2 && x > axis[i + 1]) i++;
    float fSpan = axis[i + 1] - axis[i];
    t = (fSpan > 0.0f) ? (x - axis[i]) / fSpan : 0.0f;
}

float Table::Interpolate(const Config_Table& c, float x, float y)
{
    uint8_t nx = (c.nXSize > TABLE_AXIS_MAX) ? TABLE_AXIS_MAX : c.nXSize;
    uint8_t ny = (c.nYSize > TABLE_AXIS_MAX) ? TABLE_AXIS_MAX : c.nYSize;

    uint8_t ix, iy;
    float tx, ty;
    Locate(c.fXAxis, nx, x, ix, tx);
    Locate(c.fYAxis, ny, y, iy, ty);
    uint8_t ix1 = (nx > 1) ? ix + 1 : ix;
    uint8_t iy1 = (ny > 1) ? iy + 1 : iy;

    float r0 = c.fCell[iy][ix] + (c.fCell[iy][ix1] - c.fCell[iy][ix]) * tx;
    float r1 = c.fCell[iy1][ix] + (c.fCell[iy1][ix1] - c.fCell[iy1][ix]) * tx;
    return r0 + (r1 - r0) * ty;
}

void Table::Update()
{
    if (!pConfig->bEnabled)
    {
        fVal = 0;
        return;
    }

    fVal = Interpolate(*pConfig, *pX, *pY);
}
