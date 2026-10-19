#pragma once

#include <cstdint>
#include "port.h"

extern float *pVarMap[VAR_MAP_SIZE];

#define TABLE_AXIS_MAX 8

// X and Y from the var map, bilinear between the four surrounding cells. nYSize = 1 is a 1-D curve
// along X. Outside an axis the edge value holds. Axes must be ascending.
struct Config_Table{
  bool bEnabled;
  uint16_t nXInput;
  uint16_t nYInput;
  uint8_t nXSize;
  uint8_t nYSize;
  float fXAxis[TABLE_AXIS_MAX];
  float fYAxis[TABLE_AXIS_MAX];
  float fCell[TABLE_AXIS_MAX][TABLE_AXIS_MAX]; // [y][x]
};

class Table
{
public:
    Table() {
    };

    static const uint16_t nBaseIndex = 0x1B00;

    void SetConfig(Config_Table* config)
    {
        pConfig = config;
        pX = pVarMap[config->nXInput];
        pY = pVarMap[config->nYInput];
    }

    void Update();

    static float Interpolate(const Config_Table& c, float x, float y);

    float fVal;

private:
    Config_Table* pConfig;

    float *pX;
    float *pY;
};
