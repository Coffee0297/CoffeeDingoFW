#include "mcu_utils.h"
#include "hal.h"

void EnterStopMode()
{
    PWR->CR &= ~PWR_CR_PDDS;	            // cleared PDDS means stop mode (not standby) 
	PWR->CR |= PWR_CR_CWUF | PWR_CR_CSBF;	// clear wakeup flag, clear standby flag
    PWR->CR |= PWR_CR_FPDS | PWR_CR_LPDS;	// turn off flash, regulator in low power mode
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;      // enable deep sleep mode

    __disable_irq();
    
    __WFI();

    // Resume here after wakeup
    NVIC_SystemReset();
}

 void RequestBootloader()
{
    // Set the magic code
    *((volatile unsigned long *)0x2001FFF0) = 0xDEADBEEF; // End of RAM

    // Reset the microcontroller to start the bootloader on next boot
    // The OpenBLT reset handler (bootloader/dingopdm) jumps to the
    // ROM USB-DFU bootloader if the magic code is set
    NVIC_SystemReset();
    
    // No further code will execute after this point
}

void RequestBootloaderCan(uint16_t nBaseId, uint8_t nCanSpeed)
{
    // OpenBLT can't read the FRAM config: hand it the CAN settings at the top of RAM,
    // magic last (bootloader/Source/ARMCM4_STM32F4/can.c, dingopdm/hooks.c)
    volatile uint32_t *pHandoff = (volatile uint32_t *)0x2001FFF0;
    pHandoff[1] = nBaseId;
    pHandoff[2] = nCanSpeed;
    pHandoff[0] = 0xB00710AD;
    __DSB();

    NVIC_SystemReset();
}