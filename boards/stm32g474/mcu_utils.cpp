#include "mcu_utils.h"
#include "hal.h"

void EnterStopMode()
{
    PWR->CR1 = (PWR->CR1 & ~PWR_CR1_LPMS) | PWR_CR1_LPMS_STOP1; // Stop 1 (regulator in low power, flash off automatically)
    PWR->SCR = PWR_SCR_CWUF | PWR_SCR_CSBF;                 // clear wakeup flags, clear standby flag
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;                      // enable deep sleep mode

    __disable_irq();
    
    __WFI();

    // Resume here after wakeup, interrupts still disabled; the caller resets (after CaptureWake: a reset here
    // lost the wake reason, so every CAN wake looked like a power-on and skipped the quiet wake)
}

 void RequestBootloader()
{
    // Set the magic code
    *((volatile unsigned long *)0x2001FFF0) = 0xDEADBEEF; // End of RAM

    // Reset the microcontroller to start the bootloader on next boot
    // See enter_bootloader.S, which overrides the reset handler
    // to jump to the bootloader if the magic code is set
    NVIC_SystemReset();
    
    // No further code will execute after this point
}