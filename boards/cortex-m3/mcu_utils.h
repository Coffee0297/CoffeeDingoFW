#pragma once

#define STM32_TEMP_3V3_30C  *((uint16_t*)0x1FFFF7B8)
#define STM32_TEMP_3V3_110C *((uint16_t*)0x1FFFF7C2)

// Request the OpenBLT CAN bootloader on the next reset (CANBoard). Sets a magic value in
// CCM SRAM that the bootloader reads at startup, then resets. See mcu_utils.cpp and the
// bootloader's CpuUserProgramStartHook() in bootloader/canboard/hooks.c.
void RequestBootloader();
