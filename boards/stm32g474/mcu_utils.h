#pragma once

// G4 factory calibration is taken at VDDA = 3.0V, at 30C and 130C (RM0440)
#define STM32_TS_CAL1 *((uint16_t *)0x1FFF75A8)
#define STM32_TS_CAL2 *((uint16_t *)0x1FFF75CA)
#define STM32_TS_CAL1_TEMP 30.0
#define STM32_TS_CAL2_TEMP 130.0
#define STM32_CAL_VDDA 3.0

#define STM32_VREF_INT_CAL *((uint16_t *)0x1FFF75AA)

void EnterStopMode();
void RequestBootloader();
// Before the reset that ends a sleep: tells the OpenBLT bootloader to start the app at once (no update window)
void MarkWakeReset();
void CheckBootloaderRequest(void);
