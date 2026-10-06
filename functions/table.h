#pragma once

#include <cstdint>
#ifdef DINGO_HOST_TEST
#define VAR_MAP_SIZE 1
#else
#include "port.h"
#endif

// 2-axis lookup table (dingoConfig #58): X and Y come from the var map, the output is the
// bilinear interpolation between the four surrounding cells. 8x8 = 64 cells max; a table
// with nYSize = 1 is a plain 1-D curve along X. Outside the axis range the edge value holds.
#define TABLE_AXIS_MAX 8

extern float *pVarMap[VAR_MAP_SIZE];

struct Config_Table{
  bool bEnabled;
  uint16_t nXInput;
  uint16_t nYInput;
  uint8_t nXSize;   // breakpoints used on X, 1..TABLE_AXIS_MAX
  uint8_t nYSize;   // breakpoints used on Y, 1..TABLE_AXIS_MAX (1 = 1-D curve)
  float fXAxis[TABLE_AXIS_MAX];                  // ascending
  float fYAxis[TABLE_AXIS_MAX];                  // ascending
  float fCell[TABLE_AXIS_MAX][TABLE_AXIS_MAX];   // [y][x]
};

class Table
{
public:
    Table() {
    };

    static const uint16_t nBaseIndex = 0x1A00;

    void SetConfig(Config_Table* config)
    {
        pConfig = config;
        pX = pVarMap[config->nXInput];
        pY = pVarMap[config->nYInput];
    }

    void Update();

    // Pure bilinear lookup with edge clamping; also exercised by tests/host_selftest.cpp.
    static float Interpolate(const Config_Table& cfg, float x, float y);

    float fVal;

private:
    Config_Table* pConfig;

    float *pX;
    float *pY;
};
