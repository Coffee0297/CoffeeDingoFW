/************************************************************************************//**
* \file         flash_layout.c
* \brief        dingoFW custom flash layout for the CANBoard v2 (STM32F303K8, 64 KB).
* \details      Included by Source/ARMCM4_STM32F3/flash.c when BOOT_FLASH_CUSTOM_LAYOUT_ENABLE
*               is set to 1 in blt_conf.h. Defines which flash sectors the bootloader is
*               allowed to erase/program for the application.
*
*               Memory map (2 KB sectors, 32 total):
*                 0x08000000 - 0x08003FFF  sectors  0..7  (16 KB)  bootloader  -> RESERVED
*                 0x08004000 - 0x0800F7FF  sectors  8..30 (46 KB)  application -> programmable
*                 0x0800F800 - 0x0800FFFF  sector  31     ( 2 KB)  config      -> EXCLUDED
*
*               The config sector is deliberately NOT listed here, so a CAN firmware update
*               never erases the device's persisted settings. The bootloader's own sectors
*               (0..7) are also not listed, so it cannot overwrite itself.
*
*               flashLayout[0].sector_start (0x08004000) is the application base address that
*               the bootloader jumps to, and the lowest address it will program.
****************************************************************************************/
static const tFlashSector flashLayout[] =
{
  { 0x08004000, 0x00800 },              /* flash sector  8 - 2kb  (application base)    */
  { 0x08004800, 0x00800 },              /* flash sector  9 - 2kb                        */
  { 0x08005000, 0x00800 },              /* flash sector 10 - 2kb                        */
  { 0x08005800, 0x00800 },              /* flash sector 11 - 2kb                        */
  { 0x08006000, 0x00800 },              /* flash sector 12 - 2kb                        */
  { 0x08006800, 0x00800 },              /* flash sector 13 - 2kb                        */
  { 0x08007000, 0x00800 },              /* flash sector 14 - 2kb                        */
  { 0x08007800, 0x00800 },              /* flash sector 15 - 2kb                        */
  { 0x08008000, 0x00800 },              /* flash sector 16 - 2kb                        */
  { 0x08008800, 0x00800 },              /* flash sector 17 - 2kb                        */
  { 0x08009000, 0x00800 },              /* flash sector 18 - 2kb                        */
  { 0x08009800, 0x00800 },              /* flash sector 19 - 2kb                        */
  { 0x0800A000, 0x00800 },              /* flash sector 20 - 2kb                        */
  { 0x0800A800, 0x00800 },              /* flash sector 21 - 2kb                        */
  { 0x0800B000, 0x00800 },              /* flash sector 22 - 2kb                        */
  { 0x0800B800, 0x00800 },              /* flash sector 23 - 2kb                        */
  { 0x0800C000, 0x00800 },              /* flash sector 24 - 2kb                        */
  { 0x0800C800, 0x00800 },              /* flash sector 25 - 2kb                        */
  { 0x0800D000, 0x00800 },              /* flash sector 26 - 2kb                        */
  { 0x0800D800, 0x00800 },              /* flash sector 27 - 2kb                        */
  { 0x0800E000, 0x00800 },              /* flash sector 28 - 2kb                        */
  { 0x0800E800, 0x00800 },              /* flash sector 29 - 2kb                        */
  { 0x0800F000, 0x00800 },              /* flash sector 30 - 2kb  (last app sector)     */
  /* sector 31 @ 0x0800F800 (config) intentionally omitted - never erased on update     */
};

/*********************************** end of flash_layout.c ****************************/
