# The STM32F303 is a Cortex-M4F: hardware FPU (Makefile default) instead of soft-float.
MCU = cortex-m4
MCUDIR = boards/stm32f303

# -Os: the 62KB flash can't hold -O0 code plus any new feature; no function alignment padding either
USE_OPT := $(subst -O0,-Os,$(filter-out -falign-functions=16,$(USE_OPT)))

# Stacks measured in the Sim (CH_DBG_FILL_THREADS): main thread 212 B, interrupts 96 B of 1 KB each
PROCESS_STACK = 0x200
USE_EXCEPTIONS_STACKSIZE = 0x200

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
				functions/digital_output.cpp \
				functions/pwm.cpp \
				core/sleep.cpp
