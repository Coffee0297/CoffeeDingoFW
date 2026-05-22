#pragma once

#include "port.h"

extern float *pVarMap[VAR_MAP_SIZE];

struct NeoPixelColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t w;
};

struct Config_NeoPixel {
    bool bEnabled;
    NeoPixelColor stColors[4];
    NeoPixelColor stFaultColor;
    uint16_t nVars[4];
    uint16_t nFaultVar;
    bool bBlink[4];
    bool bFaultBlink;
    uint8_t nBlinkColors[4];
    uint8_t nFaultBlinkColor;
};

/*
* Individual NeoPixel
*/
class NeoPixel {
public:
    NeoPixel() = default;

    static const uint16_t nBaseIndex = 0x2100;

    void SetConfig(Config_NeoPixel *config)
    {
        pConfig = config;
        pVars[0] = pVarMap[config->nVars[0]];
        pVars[1] = pVarMap[config->nVars[1]];
        pVars[2] = pVarMap[config->nVars[2]];
        pVars[3] = pVarMap[config->nVars[3]];
        pFaultVar = pVarMap[config->nFaultVar];
    }

    void Update();

    NeoPixelColor GetColor() { return stActiveColor; };
    uint8_t GetRed() { return stActiveColor.r; };
    uint8_t GetGreen() { return stActiveColor.g; };
    uint8_t GetBlue() { return stActiveColor.b; };
    uint8_t GetWhite() { return stActiveColor.w; };
    
private:
    Config_NeoPixel *pConfig;

    float *pVars[4] = {};
    float *pFaultVar = nullptr;

    NeoPixelColor stActiveColor;
};
