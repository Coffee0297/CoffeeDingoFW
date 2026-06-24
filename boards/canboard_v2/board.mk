# STM32F303K8T6 is a Cortex-M4F. Run it as such: hardware FPU + size-optimised code.
#
# Why the FPU matters here (it is not cosmetic): the analog and PWM features do floating-point math
# every control-loop cycle —
#   * analog linear scaling   (scaled = gain*mV + offset, published to the var map each cycle),
#   * analog calibrated decode (per-position voltage compares),
#   * PWM                      (soft-start ramp step = targetDuty/rampTime, variable duty = input/denom,
#                               duty->pulse-width), recomputed on every PWM update.
# In soft-float the compiler turns each float op into a libgcc helper call (__aeabi_fmul/fadd/...):
# ~2.5 KB of extra code and tens of cycles per op. Adding the analog features that way overflowed the
# CanBoard's 64 KB flash (101.5 %). With the hardware FPU they become single-cycle VFP instructions —
# no soft-float lib (flash 101.5 % -> 53.9 %) and the per-cycle math stays cheap enough for the 2 ms
# loop. Trade-off: FPU-on makes exception entry push a larger stack frame (~104 B vs 32 B), so threads
# with tiny stacks had to grow — see waCanCyclicTxThread / waCanRxThread in comms/can.cpp (128 -> 256).
#
# MCUDIR stays cortex-m3 on purpose — the cortex-m4 MCU utils hardcode a bootloader magic address for
# the F4's 128 KB RAM, which is wrong for the F303.
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
				functions/digital_output.cpp \
				functions/pwm.cpp