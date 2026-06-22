#pragma once

#include "port.h"
#include "input.h"

// Calibrated multi-position switch: each position has its own measured centre voltage, so the
// detents can be UNEVENLY spaced. The transmitted position is a 4-bit nibble, so 0..14 are real
// positions and 15 is reserved as "no position" (reading is in a dead zone / fault).
// CanBoard flash is tight, so the per-position voltages are stored PACKED two-per-uint32
// (nPointPair[]) and the SDO param table exposes them as uint32 words — see core/param_defs.h.
#define MAX_SWITCH_POS  10
#define NUM_POINT_PAIRS ((MAX_SWITCH_POS + 1) / 2)   // 2 points packed per uint32 word
#define ROTARY_NO_POS   15

struct Config_AnalogSwitch{
  bool bEnabled;
  InputMode eMode;
  bool bInvert;
  uint16_t nThreshold;
};

struct Config_RotarySwitch{
  bool bEnabled;
  bool bInvert;
  uint8_t nNumPos;                       // number of calibrated points (2..MAX_SWITCH_POS)
  uint16_t nTolerance;                   // max half-window (mV) accepted around each point
  uint32_t nPointPair[NUM_POINT_PAIRS];  // calibrated centre voltages (mV), 2 per word: lo=even pos, hi=odd pos
};

// Linear scaling for a sensor (pressure, temperature, ...): fScaled = fGain * mV + fOffset.
// The tool computes gain/offset from two calibration points. The scaled value is published in the
// variable map so outputs/conditions can use it (e.g. a fan driven by a temperature sensor).
struct Config_AnalogScale{
  bool bEnabled;
  float fGain;                           // engineering units per millivolt
  float fOffset;                         // engineering units at 0 mV
};

struct Config_AnalogInput{
  bool bEnabled;
  Config_AnalogSwitch stSwitch;
  Config_RotarySwitch stRotary;
  Config_AnalogScale  stScale;
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
    void ScaleUpdate();

    // Unpack calibrated point k (mV) from the packed pairs (lo half = even index, hi half = odd).
    uint16_t Point(int k) const
    {
        const uint32_t w = pConfig->stRotary.nPointPair[k >> 1];
        return (k & 1) ? static_cast<uint16_t>(w >> 16) : static_cast<uint16_t>(w & 0xFFFF);
    }
};
