#pragma once

#include "port.h"
#include "pwm.h"

extern float *pVarMap[VAR_MAP_SIZE];

struct Config_DigOutput{
  bool bEnabled;
  uint16_t nInput;
  Config_PwmOutput stPwm;
};

class Digital_Output
{
public:
    Digital_Output(ioline_t line, PWMDriver *pwmDrv, const PWMConfig *pwmCfg, PwmChannel pwmCh)
        : m_line(line), pwm(pwmDrv, pwmCfg, pwmCh)
    {};

    static const uint16_t nBaseIndex = 0x2100;

    void SetConfig(Config_DigOutput *config)
    {
        pConfig = config;
        pInput = pVarMap[config->nInput];
        pwm.SetConfig(&config->stPwm);
    }

    void Update();

    uint8_t GetDutyCycle() { return fVal ? pwm.GetDutyCycle() : 0; }
    bool IsPwmEnabled() { return pwm.IsEnabled(); }

    float fVal;

private:
    const ioline_t m_line; 

    Config_DigOutput *pConfig;

    float *pInput;

    Pwm pwm;
};