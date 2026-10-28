#include "mcu_utils.h"
#include "hal.h"

// With PRIMASK set, WFI still returns at once on any enabled pending IRQ (a CAN RX or timer IRQ that fired just
// before the mask), so the module reset instead of sleeping. Leave only the EXTI wake lines enabled; an edge
// already latched in EXTI->PR re-pends its IRQ and wakes at once, as it should.
static void QuietNvic()
{
    SysTick->CTRL = 0;
    for (uint32_t i = 0; i < 8; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFu;
        NVIC->ICPR[i] = 0xFFFFFFFFu;
    }
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
    static const IRQn_Type wake[] = {EXTI0_IRQn, EXTI1_IRQn, EXTI2_TSC_IRQn, EXTI3_IRQn, EXTI4_IRQn, EXTI9_5_IRQn, EXTI15_10_IRQn};
    for (IRQn_Type n : wake)
        NVIC_EnableIRQ(n);
}

void EnterStopMode()
{
    PWR->CR &= ~PWR_CR_PDDS;                // cleared PDDS means stop mode (not standby)
    PWR->CR |= PWR_CR_CWUF | PWR_CR_CSBF;   // clear wakeup flag, clear standby flag
    PWR->CR |= PWR_CR_LPDS;                 // regulator in low power mode (the F303 has no FPDS)
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;      // enable deep sleep mode

    __disable_irq();
    QuietNvic();

    __WFI();

    // Resume here after wakeup, interrupts still disabled; the caller resets (after CaptureWake: a reset here
    // lost the wake reason, so every CAN wake looked like a power-on and skipped the quiet wake)
}

void RequestBootloader()
{
    // The OpenBLT bootloader (bootloader/canboard/hooks.c) stays in CAN update mode when it finds
    // this in the last CCM word (kept out of the app's ram4); a plain reset doesn't clear CCM
    *((volatile uint32_t *)0x10000FFC) = 0xB00710AD;
    __DSB();

    NVIC_SystemReset();
}

// See boards/stm32f446/mcu_utils.cpp: the same 'WAKE' word, in the last CCM word the bootloader already reads
void MarkWakeReset()
{
    *((volatile uint32_t *)0x10000FFC) = 0x57414B45;
    __DSB();
}
