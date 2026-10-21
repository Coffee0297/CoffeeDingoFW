#pragma once

#include <cstdint>
#include "port.h"
#include "enums.h"

extern float *pVarMap[VAR_MAP_SIZE];

struct Config_Math{
  bool bEnabled;
  MathOp eOp;
  uint16_t nInputA;
  uint16_t nInputB; //0 = use fConstB
  float fConstB;
  float fFactor;
  float fOffset;
};

class MathBlock
{
public:
    MathBlock() {
    };

    static const uint16_t nBaseIndex = 0x1C00;

    void SetConfig(Config_Math* config)
    {
        pConfig = config;
        pInputA = pVarMap[config->nInputA];
        pInputB = config->nInputB ? pVarMap[config->nInputB] : &config->fConstB;
    }

    void Update();

    float fVal;

private:
    Config_Math* pConfig;

    float *pInputA;
    float *pInputB;
};
