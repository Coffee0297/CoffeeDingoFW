#pragma once

#include "port.h"
#include "input.h"

// Calibrated multi-position switch: each position has its own measured centre voltage, so the
// detents can be UNEVENLY spaced. The transmitted position is a 4-bit nibble, so 0..14 are real
// positions and 15 is reserved as "no position" (reading is in a dead zone / fault).
#define MAX_SWITCH_POS 12
#define ROTARY_NO_POS  15

struct Config_AnalogSwitch{
  bool bEnabled;
  InputMode eMode;
  bool bInvert;
  uint16_t nThreshold;
};

struct Config_RotarySwitch{
  bool bEnabled;
  bool bInvert;
  float fOffset;                    // legacy uniform decode (used when bUsePoints == false)
  float fStep;
  float fMaxPos;
  bool bUsePoints;                  // decode by calibrated per-position points instead of offset/step
  uint8_t nNumPos;                  // number of calibrated points (2..MAX_SWITCH_POS)
  uint16_t nTolerance;              // max half-window (mV) accepted around each point
  uint16_t nPoint[MAX_SWITCH_POS];  // calibrated centre voltage (mV) per position, ascending
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