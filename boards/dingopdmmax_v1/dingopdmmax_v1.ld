/*
 * dingoPDM-Max-v1 (STM32F446RE) memory setup, relocated above the OpenBLT bootloader.
 *
 * The 16 KB OpenBLT CAN bootloader occupies flash sector 0 (0x08000000). The application is
 * relocated to sector 1 (0x08004000) and spans sectors 1..6 = 368 KB, ending at 0x08060000.
 * Sector 7 (0x08060000, 128 KB) is left out of the application image - it holds the persistent
 * config on dingoPDM-Max and must never be erased by a firmware update. ChibiOS crt0 sets VTOR
 * from _vectors, so the relocated vector table is picked up automatically; the bootloader hands
 * over by jumping to this app's reset vector.
 *
 * Self-contained (does NOT include STM32F446xE.ld) so flash0's origin can be overridden - the
 * shared startup linker fixes it at 0x08000000. Note: ram1/ram2 are mutually exclusive with ram0.
 */
MEMORY
{
    flash0 (rx) : org = 0x08004000, len = 368k    /* sectors 1..6 - relocated application      */
    flash1 (rx) : org = 0x00000000, len = 0
    flash2 (rx) : org = 0x00000000, len = 0
    flash3 (rx) : org = 0x00000000, len = 0
    flash4 (rx) : org = 0x00000000, len = 0
    flash5 (rx) : org = 0x00000000, len = 0
    flash6 (rx) : org = 0x00000000, len = 0
    flash7 (rx) : org = 0x00000000, len = 0
    ram0   (wx) : org = 0x20000000, len = 128k    /* SRAM1 + SRAM2 */
    ram1   (wx) : org = 0x20000000, len = 112k    /* SRAM1 */
    ram2   (wx) : org = 0x00000000, len = 16k     /* SRAM2 */
    ram3   (wx) : org = 0x00000000, len = 0
    ram4   (wx) : org = 0x00000000, len = 0
    ram5   (wx) : org = 0x00000000, len = 0
    ram6   (wx) : org = 0x00000000, len = 0
    ram7   (wx) : org = 0x00000000, len = 0
}

/* For each data/text section two region are defined, a virtual region
   and a load region (_LMA suffix).*/

/* Flash region to be used for exception vectors.*/
REGION_ALIAS("VECTORS_FLASH", flash0);
REGION_ALIAS("VECTORS_FLASH_LMA", flash0);

/* Flash region to be used for constructors and destructors.*/
REGION_ALIAS("XTORS_FLASH", flash0);
REGION_ALIAS("XTORS_FLASH_LMA", flash0);

/* Flash region to be used for code text.*/
REGION_ALIAS("TEXT_FLASH", flash0);
REGION_ALIAS("TEXT_FLASH_LMA", flash0);

/* Flash region to be used for read only data.*/
REGION_ALIAS("RODATA_FLASH", flash0);
REGION_ALIAS("RODATA_FLASH_LMA", flash0);

/* Flash region to be used for various.*/
REGION_ALIAS("VARIOUS_FLASH", flash0);
REGION_ALIAS("VARIOUS_FLASH_LMA", flash0);

/* Flash region to be used for RAM(n) initialization data.*/
REGION_ALIAS("RAM_INIT_FLASH_LMA", flash0);

/* RAM region to be used for Main stack. This stack accommodates the processing
   of all exceptions and interrupts.*/
REGION_ALIAS("MAIN_STACK_RAM", ram0);

/* RAM region to be used for the process stack. This is the stack used by
   the main() function.*/
REGION_ALIAS("PROCESS_STACK_RAM", ram0);

/* RAM region to be used for data segment.*/
REGION_ALIAS("DATA_RAM", ram0);
REGION_ALIAS("DATA_RAM_LMA", flash0);

/* RAM region to be used for BSS segment.*/
REGION_ALIAS("BSS_RAM", ram0);

/* RAM region to be used for the default heap.*/
REGION_ALIAS("HEAP_RAM", ram0);

/* Generic rules inclusion.*/
INCLUDE rules.ld
