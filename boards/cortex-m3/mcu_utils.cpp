#include "mcu_utils.h"
#include "hal.h"
#include <cstdint>

// OpenBLT CAN bootloader activation (CANBoard, STM32F303K8 / cortex-m3 MCU utils).
//
// The OpenBLT bootloader occupies flash sectors 0..7 and owns the reset vector. To enter it
// on demand (e.g. dingoConfig sends MsgCmd::Bootloader over CAN) we leave a magic value in
// CCM SRAM and reset. On the next boot the bootloader's CpuUserProgramStartHook() reads this
// magic and stays in CAN-update mode instead of starting the application.
//
// Why CCM at 0x10000FFC: it is the last word of the 4 KB CCM RAM, well clear of the config
// staging buffer (stConfigTemp, ~1.2 KB at the start of CCM), survives a warm reset, and is
// not cleared by either the application's or the bootloader's startup code. Crucially it is a
// VALID address for the F303's memory map. The cortex-m4 path uses a RAM magic at 0x2001FFF0
// ("end of RAM" for the F4's 128 KB) which is OUTSIDE the F303's 12 KB SRAM - that mismatch is
// exactly why the CANBoard was kept off the shared bootloader path until now.
#define BOOTLOADER_MAGIC_ADDR   (0x10000FFCUL)
#define BOOTLOADER_MAGIC_VALUE  (0xB00710ADUL)

void RequestBootloader()
{
    *((volatile uint32_t *)BOOTLOADER_MAGIC_ADDR) = BOOTLOADER_MAGIC_VALUE;
    __DSB();                 // ensure the write reaches CCM before the reset
    NVIC_SystemReset();
    // Does not return; the bootloader runs after the reset.
}

// STOP mode (CAN_SLEEP): any armed EXTI edge (DI / CAN RX) wakes the core, then a full reset —
// same contract as the cortex-m4 version. The F303 has no FPDS bit; LPDS = regulator in low power.
// With PRIMASK set, WFI still returns at once on ANY enabled pending interrupt (a CAN RX or timer IRQ that
// fired just before the mask) and the module resets instead of sleeping, then wakes the fleet. Leave only the
// EXTI wake lines enabled; an edge already latched in EXTI->PR re-pends its IRQ and wakes at once, as it should.
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
    PWR->CR &= ~PWR_CR_PDDS;                // stop, not standby
    PWR->CR |= PWR_CR_CWUF | PWR_CR_CSBF;   // clear wakeup / standby flags
    PWR->CR |= PWR_CR_LPDS;
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

    __disable_irq();
    QuietNvic();
    __WFI();

    NVIC_SystemReset();
}
