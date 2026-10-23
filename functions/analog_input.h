#pragma once

#include "port.h"
#include "input.h"

#ifndef ROTARY_MAX_POS
#define ROTARY_MAX_POS 10 // calibrated positions
#endif
#define ROTARY_NO_POS 15 // calibrated mode: between the windows (the CAN nibble's last value)

struct Config_AnalogSwitch{
  bool bEnabled;
  InputMode eMode;
  bool bInvert;
  uint16_t nThreshold;
};

struct Config_RotarySwitch{
  bool bEnabled;
  bool bInvert;
  float fOffset;
  float fStep;
  float fMaxPos;
  RotaryMode eMode;
  uint8_t nNumPos;
  uint16_t nTolerance; // mV each side of a centre
  uint16_t nCenter[ROTARY_MAX_POS]; // measured mV of each position
};

struct Config_AnalogInput{
  bool bEnabled;
  Config_AnalogSwitch stSwitch;
  Config_RotarySwitch stRotary;
};

class Analog_Input
{
public:
    Analog_Input(const AnalogChannel channel) 
                : m_channel(channel) 
    {};

    static const uint16_t nBaseIndex = 0x2200;

    void SetConfig(Config_AnalogInput *config)
    {
        pConfig = config;
    }

    void Update();

    float fVal;
    float fValMillivolts;
    float fRotaryPos;
    float fSwitchVal;

private:
    const AnalogChannel m_channel;

    Config_AnalogInput* pConfig;    

    Input input;

    void RotaryUpdate();
    void SwitchUpdate();
};