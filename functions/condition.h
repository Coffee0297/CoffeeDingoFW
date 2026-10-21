#pragma once

#include <cstdint>
#include "port.h"

extern float *pVarMap[VAR_MAP_SIZE];

struct Config_Condition{
  bool bEnabled;
  uint16_t nInput;
  Operator eOperator;
  float fArg;
  float fHyst;
  uint16_t nDelayOn;  //ms the compare must be true before the result turns on
  uint16_t nDelayOff; //ms the compare must be false before the result turns off
};

class Condition
{
public:
    Condition() {
    };

    static const uint16_t nBaseIndex = 0x1500;

    void SetConfig(Config_Condition* config)
    {
        pConfig = config;
        pInput = pVarMap[config->nInput];
        nLastSame = SYS_TIME;
    }

    void Update();

    float fVal;

private:
    Config_Condition* pConfig;
    
    float *pInput;

    bool bOut;
    uint32_t nLastSame; //Last time the compare matched bOut
};