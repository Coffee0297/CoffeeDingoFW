#pragma once

#include <cstdint>
#include "port.h"
#include "enums.h"

extern float *pVarMap[VAR_MAP_SIZE];

struct Config_LatchGate{
  bool bEnabled;
  LatchGateType eType;
  uint16_t nInput;        //Latch: set, Gate: data
  uint16_t nControlInput; //Latch: reset, Gate: enable
  uint32_t nTime;         //Latch auto-cancel ms, 0 = none
};

class LatchGate
{
public:
    LatchGate() {
    };

    static const uint16_t nBaseIndex = 0x2600;

    void SetConfig(Config_LatchGate* config)
    {
        pConfig = config;
        pInput = pVarMap[config->nInput];
        pControlInput = pVarMap[config->nControlInput];
    }

    void Update();

    float fVal;

private:
    Config_LatchGate* pConfig;

    float *pInput;
    float *pControlInput;

    bool bInit;
    bool bLastIn;
    bool bLastControl;
    uint32_t nSetTime;
};
