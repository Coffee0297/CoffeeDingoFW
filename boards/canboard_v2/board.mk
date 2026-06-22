# STM32F303K8T6 is a Cortex-M4F. Run it as such: hardware FPU (frees ~2.5 KB of soft-float libgcc
# and speeds float math) and size-optimised code. MCUDIR stays cortex-m3 on purpose — the cortex-m4
# MCU utils hardcode a bootloader magic address for the F4's 128 KB RAM, which is wrong for the F303.
MCU = cortex-m4
MCUDIR = boards/cortex-m3
USE_FPU = hard
# CanBoard has only 62 KB flash; -Os is essential (the PDM boards have 384 KB and stay at the -O0 default).
USE_OPT = -Os -ggdb -fomit-frame-pointer -falign-functions=16 -fsingle-precision-constant

# List of all the board related files.
BOARDSRC = ./boards/canboard_v2/board.c

# Required include directories
BOARDINC = ./boards/canboard_v2

# Shared variables
ALLCSRC += $(BOARDSRC)
ALLINC  += $(BOARDINC)

include $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/mk/startup_stm32f3xx.mk
include $(CHIBIOS)/os/hal/ports/STM32/STM32F3xx/platform.mk
include $(CHIBIOS)/os/common/ports/ARMv7-M/compilers/GCC/mk/port.mk

CPPSRC_BOARD += functions/analog_input.cpp \
				functions/digital_input.cpp \
				functions/digital_output.cpp