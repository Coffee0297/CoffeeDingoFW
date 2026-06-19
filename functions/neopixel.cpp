#include "neopixel.h"

void NeoPixel::Update() {
    
    stScaledColor.r = (uint8_t)(((uint16_t)stActiveColor.r * nBrightness) / 255);
    stScaledColor.g = (uint8_t)(((uint16_t)stActiveColor.g * nBrightness) / 255);
    stScaledColor.b = (uint8_t)(((uint16_t)stActiveColor.b * nBrightness) / 255);
}