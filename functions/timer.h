#pragma once

#include <cstdint>
#include "enums.h"
#ifdef DINGO_HOST_TEST
#define VAR_MAP_SIZE 1
#else
#include "port.h"
#endif

extern float *pVarMap[VAR_MAP_SIZE];

// Timer function (FW #61): a single var-map input starts a timer; the output follows one of
// three classic PLC timer shapes. `eEdge` picks the input level that counts as "active":
// Rising = input true, Falling = input false (so a timer can run while something is OFF).
struct Config_Timer{
  bool bEnabled;
  uint16_t nInput;
  InputEdge eEdge;      // Rising / Falling: which input level is the active (trigger) state
  TimerMode eMode;      // OnDelay (TON) / OffDelay (TOF) / Pulse (TP)
  uint32_t nPreset;     // ms
};

class Timer
{
public:
    Timer() {
    };

    static const uint16_t nBaseIndex = 0x1B00;

    void SetConfig(Config_Timer* config)
    {
        pConfig = config;
        pInput = pVarMap[config->nInput];
    }

    void Update(uint32_t nTimeNow);

    float fVal;

private:
    Config_Timer* pConfig;

    float *pInput;

    bool bLastActive;
    bool bTiming;
    uint32_t nStartTime;
};
