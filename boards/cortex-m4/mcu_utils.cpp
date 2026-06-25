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
    // USB-DFU entry. Set the magic code at the top of SRAM and reset.
    // The OpenBLT bootloader (which owns the reset vector once installed) sees this magic
    // and jumps to the STM32 system (USB-DFU) bootloader; on a board still running the old
    // enter_bootloader.S reset handler it does the same. Either way -> USB-DFU.
    *((volatile unsigned long *)0x2001FFF0) = 0xDEADBEEF; // End of RAM

    NVIC_SystemReset();

    // No further code will execute after this point
}

void RequestBootloaderCan(uint16_t baseId, uint8_t canSpeed)
{
    // OpenBLT CAN-update entry. The dingoPDM keeps its config in external FRAM, which the
    // minimal bootloader cannot read, so hand it our base id + CAN speed in the reserved
    // block at the top of SRAM, then reset. The bootloader reads them (DingoLoadCanConfig)
    // and stays active for an XCP-over-CAN session (CpuUserProgramStartHook). Distinct magic
    // from the USB-DFU 0xDEADBEEF so the bootloader can tell the two requests apart.
    volatile unsigned long *p = (volatile unsigned long *)0x2001FFF0;
    p[1] = baseId;          // 0x2001FFF4
    p[2] = canSpeed;        // 0x2001FFF8
    p[0] = 0xB00710AD;      // 0x2001FFF0 - magic written last
    __DSB();

    NVIC_SystemReset();

    // No further code will execute after this point
}