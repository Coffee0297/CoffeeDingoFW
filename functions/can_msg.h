#pragma once

#include <cstdint>
#include "port.h"
#include "enums.h"

extern float *pVarMap[VAR_MAP_SIZE];

struct Config_CanMsg{
  bool bEnabled;
  uint16_t nInput;
  uint8_t nIDE; //0=STD, 1=EXT
  uint32_t nID;
  uint8_t nDLC;
  CanMsgMode eMode;
  uint8_t nData[8];
  uint16_t nInterval; //ms, Repeat mode
};

class CanMsg
{
public:
    CanMsg() {
    };

    static const uint16_t nBaseIndex = 0x1D00;

    void SetConfig(Config_CanMsg* config)
    {
        pConfig = config;
        pInput = pVarMap[config->nInput];
    }

    // True when the configured frame is due now. Repeat frames are held back while bMuted.
    bool Due(bool bMuted);

private:
    Config_CanMsg* pConfig;

    float *pInput;

    bool bLast;
    uint32_t nLastSent;
};
