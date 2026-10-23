#pragma once

#include "port.h"

extern float *pVarMap[VAR_MAP_SIZE];

struct Config_DigOutput{
  bool bEnabled;
  uint16_t nInput;
};

class Digital_Output
{
public:
    Digital_Output(ioline_t line)
        : m_line(line)
    {};

    static const uint16_t nBaseIndex = 0x2100;

    void SetConfig(Config_DigOutput *config)
    {
        pConfig = config;
        pInput = pVarMap[config->nInput];
    }

    void Update();

    // Bench test: solid on for nHoldSec (no PWM on these outputs)
    bool SetTest(uint8_t nMode, uint8_t nHoldSec);

    ioline_t GetLine() { return m_line; }

    float fVal;

private:
    const ioline_t m_line; 

    Config_DigOutput *pConfig;

    float *pInput;

    bool bTest;
    uint32_t nTestEnd;
};