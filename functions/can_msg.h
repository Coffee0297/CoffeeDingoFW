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
  CanOutputKind eChecksum; //Signal = none, ChecksumXor..Crc8Autosar
  uint8_t nChecksumByte;
  uint8_t nCounterByte; //low nibble counts 0-15 per frame, 0xFF = none
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

    // The frame's data: the configured bytes, then the counter, then the checksum
    void Fill(uint8_t *pData);

private:
    Config_CanMsg* pConfig;

    float *pInput;

    bool bInit;
    bool bLast;
    uint32_t nLastSent;
    uint8_t nCount;
};
