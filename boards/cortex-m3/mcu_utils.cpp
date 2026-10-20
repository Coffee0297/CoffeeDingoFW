#include "mcu_utils.h"
#include "hal.h"

void RequestBootloader()
{
    // The OpenBLT bootloader (bootloader/canboard/hooks.c) stays in CAN update mode when it finds
    // this in the last CCM word (kept out of the app's ram4); a plain reset doesn't clear CCM
    *((volatile uint32_t *)0x10000FFC) = 0xB00710AD;
    __DSB();

    NVIC_SystemReset();
}
