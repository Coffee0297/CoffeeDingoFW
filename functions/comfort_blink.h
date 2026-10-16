#pragma once

#include <cstdint>
#ifdef DINGO_HOST_TEST
#ifndef VAR_MAP_SIZE
#define VAR_MAP_SIZE 1
#endif
#else
#include "port.h"
#endif

extern float *pVarMap[VAR_MAP_SIZE];

// Comfort blink (lane change) (5.6.1). A short tap of the turn lever blinks nBlinks times, then stops; held
// longer than nTapMs it is a normal blink that stops when the lever is released. A tap the other way during a
// comfort blink cancels it (held past nTapMs, that side blinks normally). Blinks are counted on the shared
// indicator clock (nClockInput), so every module running its own copy stops on the same blink.
// Left / Right are var-map signals: "indicator on" for that side, in place of the lever.
struct Config_ComfortBlink{
  bool bEnabled;
  uint16_t nLeftInput;    // lever left
  uint16_t nRightInput;   // lever right
  uint16_t nClockInput;   // the indicator clock (a flasher, or the clock frame from the master module)
  uint16_t nTapMs;        // shorter than this is a tap: a comfort blink (100..5000)
  uint8_t nBlinks;        // blinks of a comfort blink (1..10)
};

class ComfortBlink
{
public:
    static const uint16_t nBaseIndex = 0x1F00;

    void SetConfig(Config_ComfortBlink* config)
    {
        pConfig = config;
        pLeft = pVarMap[config->nLeftInput];
        pRight = pVarMap[config->nRightInput];
        pClock = pVarMap[config->nClockInput];
    }

    void Update(uint32_t nTimeNow);

    float fLeft = 0, fRight = 0;   // var-map signals (1/0)

private:
    struct Side {
        bool bLast = false;      // the lever was on last update
        bool bHeld = false;      // held past nTapMs: a normal blink
        bool bComfort = false;   // released early: blinking out the comfort blinks
        bool bCancel = false;    // this press cancelled the other side's comfort blink
        uint32_t nSince = 0;     // when the lever went on
        uint8_t nBlinks = 0;     // clock on-phases shown since the lever went on
        bool On() const { return (bLast && !bCancel) || bComfort; }
    };
    void Step(Side &s, Side &other, bool bIn, bool bClock, uint32_t nTimeNow);

    Config_ComfortBlink* pConfig = nullptr;
    float *pLeft = nullptr, *pRight = nullptr, *pClock = nullptr;
    Side stLeft, stRight;
    bool bLastClock = false;
};
