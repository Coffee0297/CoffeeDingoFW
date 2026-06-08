#pragma once

#if HAS_NEOPIXELS

#include "port.h"
#include "neopixel.h"

class NeoPixels{
public:
    NeoPixels(uint8_t numPixels, PWMDriver *pwmDriver, const PWMConfig *pwmCfg, PwmChannel pwmCh)
        : m_numPixels(numPixels)
    {};

    void Update();

private:
    const uint8_t m_numPixels;

    NeoPixel pixels[MAX_NEOPIXELS] = {};
};

#endif