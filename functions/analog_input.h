#pragma once

#include "port.h"
#include "input.h"

#ifndef ROTARY_MAX_POS
#define ROTARY_MAX_POS 10 // calibrated positions, even
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
  uint16_t nTolerance; // mV each side of a centre, at most half the gap to the next one
  uint32_t nPointPair[ROTARY_MAX_POS / 2]; // centres in mV, two per word: low half = even position
};

struct Config_AnalogInput{
  bool bEnabled;
  Config_AnalogSwitch stSwitch;
  Config_RotarySwitch stRotary;
  bool bScale;
  float fScaleGain; // units per mV
  float fScaleOffset; // units at 0 mV
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
    float fScaledVal;

private:
    const AnalogChannel m_channel;

    Config_AnalogInput* pConfig;    

    Input input;

    void RotaryUpdate();
    void SwitchUpdate();
};