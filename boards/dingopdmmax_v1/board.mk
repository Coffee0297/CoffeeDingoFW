MCU = cortex-m4
MCUDIR = boards/stm32f446

# List of all the board related files.
BOARDSRC = ./boards/dingopdmmax_v1/board.c

# Required include directories
BOARDINC = ./boards/dingopdmmax_v1

# Shared variables
ALLCSRC += $(BOARDSRC)
ALLINC  += $(BOARDINC)

include $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/mk/startup_stm32f4xx.mk
include $(CHIBIOS)/os/hal/ports/STM32/STM32F4xx/platform.mk
include $(CHIBIOS)/os/common/ports/ARMv7-M/compilers/GCC/mk/port.mk

CPPSRC_BOARD += comms/usb.cpp \
				core/sleep.cpp \
				core/lua_script.cpp \
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

# Embedded Lua 5.5, vendored in lua/
ALLCSRC += $(wildcard ./lua/*.c)
USE_COPT += -D'luai_makeseed()=0u' # no clock for time() on the module
USE_COPT += -DLUAI_MAXCCALLS=32 # C stack depth (parser nesting, nested pcall/metamethods): ~200 B a level at -O0 in the 12 KB Lua stack
UINC_BOARD += ./lua
