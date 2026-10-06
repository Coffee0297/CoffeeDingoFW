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

    // Bench test override (MsgCmd::OutputTest): force the output on (mode 1) or, with HAS_DIG_PWM, PWM at
    // a duty + frequency (mode 2) for nHoldSec, ignoring its input. Expires on its own; mode 0 releases early.
    void SetTest(uint8_t nMode, uint8_t nDuty, uint16_t nFreq, uint8_t nHoldSec);
    bool TestActive();

#if HAS_DIG_PWM
    // Duty reported to CAN: the live PWM duty when the output is on, else 0
    // (mirrors Profet::GetDutyCycle). Returns 0 for a non-PWM output.
    uint8_t GetDutyCycle() { if (!(bool)fVal) return 0; return bTestPwm ? nTestDuty : pwm.GetDutyCycle(); }
    bool IsPwmEnabled() { return pConfig->stPwm.bEnabled; }
#endif

    float fVal;

private:
    const ioline_t m_line;

    Config_DigOutput *pConfig;

    float *pInput;

    // Bench test state (see SetTest). nTestMode is written last from the CAN RX thread and read by the
    // control loop; a single torn cycle is harmless for a test.
    uint8_t nTestMode = 0;     // 0 none, 1 on, 2 pwm
    uint8_t nTestDuty = 0;
    uint16_t nTestFreq = 100;
    uint32_t nTestEnd = 0;     // ms (SYS_TIME) at which the override expires
    bool bTest = false;        // override active this cycle
    bool bTestPwm = false;     // override is PWM

#if HAS_DIG_PWM
    Pwm pwm;
#endif
};
