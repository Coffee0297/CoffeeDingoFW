#pragma once

#include <cstdint>

#define STM32_TEMP_3V3_30C *((uint16_t *)0x1FFF7A2C)
#define STM32_TEMP_3V3_110C *((uint16_t *)0x1FFF7A2E)

#define STM32_VREF_INT_CAL *((uint16_t *)0x1FFF7A2A)

void EnterStopMode();
void RequestBootloader();
void RequestBootloaderCan(uint16_t baseId, uint8_t canSpeed);
void CheckBootloaderRequest(void);