#include "neopixel.h"

void NeoPixel::Update() {
    if (!pConfig->bEnabled) {
        stActiveColor = Color_Off;
        return;
    }

    for (uint8_t i = 0; i < 4; i++)
    {
        // Check if the value is equal to the index of the color
        // If so, set the color to the corresponding value color
        // Boolean values only check values 0 and 1
        // Integer values check values 0 to nNumOfValColors - 1
        if (static_cast<uint8_t>(*pVars[i]) == 1)
        {
            stActiveColor = static_cast<NeoPixelColor>(pConfig->stColors[i]);

            if (pConfig->bBlink[i])
            {
                //TODO: Handle blinking logic
            }
        }
    }

    // Fault color takes precedence over value colors
    if (*pFaultVar == 1)
    {
        stActiveColor = (NeoPixelColor)pConfig->stFaultColor;

        if(pConfig->bFaultBlink)
        {
            //TODO: Handle fault blinking logic
        }
    }
}