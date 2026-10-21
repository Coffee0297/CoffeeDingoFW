#pragma once

#include "port.h"
#include "enums.h"
#include "input.h"
#if DI_PWM_MAX_FREQ > 0
#include "pwm_meter.h"
#endif

struct Config_DigInput{
  bool bEnabled;
  InputMode eMode;
  bool bInvert;
  uint16_t nDebounceTime; //ms
  InputPull ePull;
  #if DI_PWM_MAX_FREQ > 0
  bool bPwm; // measure duty and frequency instead of on/off
  uint16_t nPwmFreq; // Hz, 0 = measure it
  uint16_t nPwmMinPulseUs; // glitch filter, 0 = off
  #endif
};

class Digital_Input
{
public:
    Digital_Input(ioline_t line)
        : m_line(line)
    {};

    static const uint16_t nBaseIndex = 0x1200;

    void SetConfig(Config_DigInput *config);

    ioline_t GetLine() { return m_line; }

    void Update();

    float fVal;

    #if DI_PWM_MAX_FREQ > 0
    void OnEdge();
    void StopPwm();

    // PWM mode: fVal is 1 while a signal is present
    float fDuty; // % at the active level
    float fFreq; // Hz
    #endif

private:
    const ioline_t m_line;

    void SetPull(InputPull pull);

    Config_DigInput *pConfig;

    Input input;

    bool bInit;
    bool bLast;
    bool bCheck;
    uint32_t nLastTrigTime;

    #if DI_PWM_MAX_FREQ > 0
    void UpdatePwm();

    // written by OnEdge (ISR), read by UpdatePwm under the system lock
    bool bPwmActive;
    PwmMeter meter;
    uint32_t nLastEdgeMs;
    #endif
};