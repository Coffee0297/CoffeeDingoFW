#pragma once

#include <cstdint>
#include "port.h"

#define CURVE_MAX_POINTS 12

extern float *pVarMap[VAR_MAP_SIZE];

struct Config_Curve{
  bool bEnabled;
  uint16_t nInput;
  uint8_t nPoints;
  float fX[CURVE_MAX_POINTS]; //Ascending
  float fY[CURVE_MAX_POINTS];
};

class Curve
{
public:
    Curve() {
    };

    static const uint16_t nBaseIndex = 0x2400;

    void SetConfig(Config_Curve* config)
    {
        pConfig = config;
        pInput = pVarMap[config->nInput];
    }

    void Update();

    float fVal;

private:
    Config_Curve* pConfig;

    float *pInput;
};
