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
