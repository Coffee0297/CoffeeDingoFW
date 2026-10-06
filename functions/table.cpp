#include "table.h"

// Find x on an ascending axis of n points: lower index i (0..n-2) and the fraction t in [0,1]
// between axis[i] and axis[i+1]. Clamps outside the range; a 1-point axis pins i=0, t=0.
static void Locate(const float* axis, uint8_t n, float x, uint8_t& i, float& t)
{
    if (n <= 1 || x <= axis[0]) { i = 0; t = 0.0f; return; }
    if (x >= axis[n - 1]) { i = n - 2; t = 1.0f; return; }
    i = 0;
    while (i < n - 2 && x > axis[i + 1]) i++;
    const float span = axis[i + 1] - axis[i];
    t = (span > 0.0f) ? (x - axis[i]) / span : 0.0f;
}

float Table::Interpolate(const Config_Table& c, float x, float y)
{
    uint8_t nx = c.nXSize, ny = c.nYSize;
    if (nx < 1) nx = 1;
    if (nx > TABLE_AXIS_MAX) nx = TABLE_AXIS_MAX;
    if (ny < 1) ny = 1;
    if (ny > TABLE_AXIS_MAX) ny = TABLE_AXIS_MAX;

    uint8_t ix, iy;
    float tx, ty;
    Locate(c.fXAxis, nx, x, ix, tx);
    Locate(c.fYAxis, ny, y, iy, ty);
    const uint8_t ix1 = (nx > 1) ? ix + 1 : ix;
    const uint8_t iy1 = (ny > 1) ? iy + 1 : iy;

    const float c00 = c.fCell[iy][ix],  c10 = c.fCell[iy][ix1];
    const float c01 = c.fCell[iy1][ix], c11 = c.fCell[iy1][ix1];
    const float r0 = c00 + (c10 - c00) * tx;
    const float r1 = c01 + (c11 - c01) * tx;
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
