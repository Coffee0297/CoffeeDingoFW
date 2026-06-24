#pragma once

#include "port.h"
#if HAS_DIG_PWM
#include "pwm.h"
#endif

extern float *pVarMap[VAR_MAP_SIZE];

struct Config_DigOutput{
  bool bEnabled;
  uint16_t nInput;
#if HAS_DIG_PWM
  Config_PwmOutput stPwm;
#endif
};

class Digital_Output
{
public:
#if HAS_DIG_PWM
    Digital_Output(ioline_t line, PWMDriver *pwmDrv, const PWMConfig *pwmCfg, PwmChannel pwmCh)
        : m_line(line), pwm(pwmDrv, pwmCfg, pwmCh)
    {};
#else
    Digital_Output(ioline_t line)
        : m_line(line)
    {};
#endif

    static const uint16_t nBaseIndex = 0x2100;

    void SetConfig(Config_DigOutput *config)
    {
        pConfig = config;
        pInput = pVarMap[config->nInput];
#if HAS_DIG_PWM
        pwm.SetConfig(&config->stPwm);
#endif
    }

    void Update();

#if HAS_DIG_PWM
    // Duty reported to CAN: the live PWM duty when the output is on, else 0
    // (mirrors Profet::GetDutyCycle). Returns 0 for a non-PWM output.
    uint8_t GetDutyCycle() { return (bool)fVal ? pwm.GetDutyCycle() : 0; }
    bool IsPwmEnabled() { return pConfig->stPwm.bEnabled; }
#endif

    float fVal;

private:
    const ioline_t m_line;

    Config_DigOutput *pConfig;

    float *pInput;

#if HAS_DIG_PWM
    Pwm pwm;
#endif
};
