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

// CAN Message block (5.6): sends a whole user-defined frame (11/29-bit ID, length, 8 fixed bytes) while a
// var-map signal is true: Repeat = every nIntervalMs while true; Once = one frame each time it turns true.
// E.g. "tell the dash to shut down". fVal mirrors the trigger (true while it sends) for other blocks.
struct Config_CanMsg{
  bool bEnabled;
  uint16_t nInput;
  uint8_t nIDE;          // 0 = 11-bit, 1 = 29-bit
  uint32_t nID;
  uint8_t nDLC;          // 0..8
  uint8_t nData[8];
  CanMsgMode eMode;
  uint16_t nIntervalMs;  // Repeat: 10..60000
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

    // True when a frame is due now; the caller sends the configured frame (Config()).
    bool Due(uint32_t nTimeNow);
    const Config_CanMsg* Config() const { return pConfig; }

    float fVal = 0;

private:
    Config_CanMsg* pConfig;
    float *pInput = nullptr;
    bool bLast = false;
    uint32_t nLastSent = 0;
};
