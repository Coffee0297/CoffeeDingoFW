#pragma once

#include <cstdint>
#include "port.h"

extern float *pVarMap[VAR_MAP_SIZE];

struct Config_Pid{
  bool bEnabled;
  uint16_t nSetpointInput;
  uint16_t nProcessInput;
  uint16_t nActivateInput;
  float fFeedForward;
  uint16_t nFeedFwdInput;
  float fKp;
  float fKi;
  float fKd;
  float fIMin;
  float fIMax;
  float fOutMin;
  float fOutMax;
  bool bInvert;
  float fSpRate; //units/s, 0 = off
  float fDeadband;
  float fPvMin; //pv fault window, both 0 = off
  float fPvMax;
  float fFallback;
  uint16_t nPeriodMs;
  float fSetpoint; //used when nSetpointInput is 0
};

class Pid
{
public:
    Pid() {
    };

    static const uint16_t nBaseIndex = 0x2300;

    void SetConfig(Config_Pid* config)
    {
        pConfig = config;
        pSetpoint = config->nSetpointInput ? pVarMap[config->nSetpointInput] : &config->fSetpoint;
        pProcess = pVarMap[config->nProcessInput];
        pActivate = pVarMap[config->nActivateInput];
        pFeedFwd = pVarMap[config->nFeedFwdInput];
    }

    void Update();

    float fVal;
    float fP;
    float fI;
    float fD;

private:
    Config_Pid* pConfig;

    float *pSetpoint;
    float *pProcess;
    float *pActivate;
    float *pFeedFwd;

    bool bInit;
    bool bPrevValid;
    float fPrevPv;
    float fSpRamp;
    uint32_t nLastTime;
};
