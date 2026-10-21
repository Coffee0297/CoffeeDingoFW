#pragma once

#include <cstdint>
#include "port.h"

extern float *pVarMap[VAR_MAP_SIZE];

// Comfort (lane change) blink. A tap of the turn lever shorter than nTapMs blinks nBlinks times, held longer it
// blinks until released. A tap the other way during a comfort blink cancels it. Blinks are counted on the
// indicator clock (nClockInput, e.g. the flasher), so every module running a copy stops on the same blink.
// fLeft / fRight replace the lever: "indicator on" for that side.
struct Config_ComfortBlink{
  bool bEnabled;
  uint16_t nLeftInput;
  uint16_t nRightInput;
  uint16_t nClockInput;
  uint16_t nTapMs;
  uint8_t nBlinks;
};

class ComfortBlink
{
public:
    ComfortBlink() {
    };

    static const uint16_t nBaseIndex = 0x1F00;

    void SetConfig(Config_ComfortBlink* config)
    {
        pConfig = config;
        pLeft = pVarMap[config->nLeftInput];
        pRight = pVarMap[config->nRightInput];
        pClock = pVarMap[config->nClockInput];
    }

    void Update(uint32_t nTimeNow);

    float fLeft, fRight;

private:
    struct Side {
        bool bLast = false;    // Lever was on last update
        bool bHeld = false;    // Held past nTapMs: a normal blink
        bool bComfort = false; // Released early: blinking out the comfort blinks
        bool bCancel = false;  // This press cancelled the other side's comfort blink
        uint32_t nSince = 0;   // When the lever went on
        uint8_t nBlinks = 0;   // Clock on-phases shown since the lever went on
        bool On() const { return (bLast && !bCancel) || bComfort; }
    };
    void Step(Side &s, Side &other, bool bIn, bool bClock, uint32_t nTimeNow);

    Config_ComfortBlink* pConfig = nullptr;
    float *pLeft, *pRight, *pClock;
    Side stLeft, stRight;
    bool bLastClock;
};
