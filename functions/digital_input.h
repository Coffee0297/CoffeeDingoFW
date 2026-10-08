#pragma once

#include "port.h"
#include "enums.h"
#include "input.h"
#include "pwm_meter.h"

struct Config_DigInput{
  bool bEnabled;
  InputMode eMode;
  bool bInvert;
  uint16_t nDebounceTime; //ms
  InputPull ePull;
  bool bPwm;            // measure duty/frequency instead of an on/off state
  uint16_t nPwmFreq;    // Hz the signal runs at; 0 = auto-detect (measured period)
};

class Digital_Input
{
public:
    Digital_Input(ioline_t line)
        : m_line(line)
    {};

    static const uint16_t nBaseIndex = 0x1200;

    void SetConfig(Config_DigInput *config);

    void Update();

    ioline_t GetLine() const { return m_line; }

    // PWM mode: the pin's edge interrupt (both edges, cycle-counter timestamps)
    void OnEdge();
    void StopPwm();

    float fVal;       // on/off; in PWM mode 1 while a signal is present
    float fDuty;      // PWM mode: % of each period at the active level (Invert = low is active)
    float fFreq;      // PWM mode: Hz (measured, or nPwmFreq when fixed)

private:
    const ioline_t m_line;

    void SetPull(InputPull pull);
    void UpdatePwm();

    Config_DigInput *pConfig;

    Input input;

    bool bInit;
    bool bLast;
    bool bCheck;
    uint32_t nLastTrigTime;

    // written by OnEdge (ISR), taken by UpdatePwm under the system lock
    bool bPwmActive;
    PwmMeter meter;
    uint32_t nLastEdgeMs;
};
