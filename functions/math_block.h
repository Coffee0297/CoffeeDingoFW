#pragma once

#include <cstdint>
#include "enums.h"
#ifdef DINGO_HOST_TEST
#ifndef VAR_MAP_SIZE
#define VAR_MAP_SIZE 1
#endif
#else
#include "port.h"
#endif

extern float *pVarMap[VAR_MAP_SIZE];

// Math block (5.6): fVal = (A op B) * fFactor + fOffset. A is a var-map signal; B is a signal, or the constant
// fConstB when nInputB is 0. Division by zero gives 0. E.g. oil temp - coolant temp, the total current of two
// outputs, or scaling a CAN value.
struct Config_Math{
  bool bEnabled;
  MathOp eOp;
  uint16_t nInputA;
  uint16_t nInputB;     // 0 = use fConstB
  float fConstB;
  float fFactor;
  float fOffset;
};

float MathEval(MathOp op, float a, float b);   // A op B (pure, for tests)

class MathBlock
{
public:
    MathBlock() {
    };

    static const uint16_t nBaseIndex = 0x1C00;

    void SetConfig(Config_Math* config)
    {
        pConfig = config;
        pA = pVarMap[config->nInputA];
        pB = config->nInputB ? pVarMap[config->nInputB] : nullptr;
    }

    void Update();

    float fVal = 0;

private:
    Config_Math* pConfig;
    float *pA = nullptr;
    float *pB = nullptr;
};
