MCU = cortex-m4
MCUDIR = boards/cortex-m4
# dingoFW: size-optimise. Relocating the app above the 16 KB OpenBLT bootloader leaves sectors
# 1..6 (368 KB) for the application (sector 7 is reserved for config / left for FRAM). The -O0
# default image (~400 KB) does not fit; -Os brings it well under 368 KB. No FPU/float-ABI change.
USE_OPT = -Os -ggdb -fomit-frame-pointer -falign-functions=16 -fsingle-precision-constant

# List of all the board related files.
BOARDSRC = ./boards/dingopdm_v7/board.c

# Required include directories
BOARDINC = ./boards/dingopdm_v7

# Shared variables
ALLCSRC += $(BOARDSRC)
ALLINC  += $(BOARDINC)

include $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/mk/startup_stm32f4xx.mk
include $(CHIBIOS)/os/hal/ports/STM32/STM32F4xx/platform.mk
include $(CHIBIOS)/os/common/ports/ARMv7-M/compilers/GCC/mk/port.mk

# dingoFW: enter_bootloader.S (the reset-handler USB-DFU trampoline) is no longer linked - the
# OpenBLT bootloader owns the reset vector and dispatches both USB-DFU (magic 0xDEADBEEF) and CAN
# updates from its own clean state. The app uses ChibiOS's default Reset_Handler (crt0 sets VTOR
# from the relocated _vectors). See bootloader/dingopdm/main.c + boards/cortex-m4/mcu_utils.cpp.

CPPSRC_BOARD += comms/usb.cpp \
				core/sleep.cpp \
				functions/profet.cpp \
				functions/overload_log.cpp \
				functions/pwm.cpp \
				functions/digital_input.cpp \
				functions/starter.cpp \
				functions/wiper/wiper_digin.cpp \
				functions/wiper/wiper_intin.cpp \
				functions/wiper/wiper_mixin.cpp \
				functions/wiper/wiper.cpp \
				functions/keypad/blink/blink_button.cpp \
				functions/keypad/blink/blink_dial.cpp \
				functions/keypad/blink/blink_analog_input.cpp \
				functions/keypad/blink/blink_keypad.cpp \
				functions/keypad/grayhill/grayhill_button.cpp \
				functions/keypad/grayhill/grayhill_keypad.cpp \
				functions/keypad/keypad_button.cpp \
				functions/keypad/keypad.cpp \
				hardware/mcp9808.cpp \
				hardware/mb85rc.cpp

UINC_BOARD += 	./functions/wiper \
				./functions/keypad/blink \
				./functions/keypad/grayhill \
				./functions/keypad