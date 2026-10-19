#pragma once

#include "hal.h"
#include "enums.h"

#define PROCESS_STACK 0x0400

#define HAS_SE_LEDS FALSE
#define HAS_USB FALSE
#define HAS_I2C FALSE
#define HAS_EXT_TEMP_SENSOR FALSE
#define HAS_EXT_MEMORY FALSE
#define HAS_BATT_VOLT_SENSE FALSE
#define CAN_SLEEP FALSE

#define NUM_OUTPUTS 0
#define NUM_DIG_OUTPUTS 4
#define DI_PWM_MAX_FREQ 0 // no PWM measuring on the inputs: no RAM left for it
#define NUM_DIG_INPUTS 8
#define NUM_ANALOG_INPUTS 5
#define ROTARY_MAX_POS 8 // ponytail: RAM only holds 8 calibrated positions, 10 once there's room
#define NUM_VIRT_INPUTS 8
#define NUM_CAN_INPUTS 8
#define NUM_CAN_OUTPUTS 8
#define NUM_FLASHERS 4
#define NUM_WIPER_INTER_DELAYS 0
#define NUM_WIPER_SPEED_MAP 0
#define NUM_COUNTERS 4
#define NUM_CONDITIONS 8
#define NUM_MATH 0
#define NUM_CAN_MSGS 0
#define NUM_CURVES 0
#define NUM_TIMERS 0
#define NUM_TABLES 0
#define NUM_KEYPADS 0
#define HAS_WIPERS FALSE
#define HAS_STARTER_DISABLE FALSE

#define KEYPAD_MAX_BUTTONS 0
#define KEYPAD_MAX_ANALOG_INPUTS 0
#define KEYPAD_MAX_DIALS 0

#define HAS_NEOPIXELS FALSE

//Always false
//Always true
//State
//Temperature sensor
//Battery voltage
#define VAR_MAP_SYS_VARS 3
#define VAR_MAP_WIPER_VARS 0

#define VAR_MAP_SIZE ( \
    VAR_MAP_SYS_VARS + \
    (NUM_DIG_INPUTS * 1) + \
    (NUM_ANALOG_INPUTS * 5) + \
    (NUM_DIG_OUTPUTS * 1) + \
    (NUM_CAN_INPUTS * 2) + \
    (NUM_VIRT_INPUTS * 1) + \
    (NUM_OUTPUTS * 7) + \
    (NUM_FLASHERS * 1) + \
    (NUM_CONDITIONS * 1) + \
    (NUM_COUNTERS * 1) + \
    (NUM_TIMERS * 1) + \
    (NUM_OUTPUTS * 1) + \
    (NUM_MATH * 1) + \
    (NUM_TABLES * 1) + \
    (NUM_CURVES * 1) + \
    VAR_MAP_WIPER_VARS + \
    (NUM_KEYPADS * (KEYPAD_MAX_BUTTONS + KEYPAD_MAX_DIALS + KEYPAD_MAX_ANALOG_INPUTS)) \
)

// Last 2KB sector of flash (sector 31, 0x0800F800)
// Max program flash size goes to the end of sector 30 (0x0800F000) to leave config space at the end of flash
// !!! Flash must be smaller thank 63488 bytes !!!
#define CONFIG_SECTOR       31U
#define CONFIG_SECTOR_SIZE  2048U // STM32F3 flash page, = STM32_FLASH_SECTOR_SIZE in ChibiOS hal_efl_lld.c
#define CONFIG_FLASH_OFFSET (CONFIG_SECTOR * CONFIG_SECTOR_SIZE)
#define CONFIG_FLASH        getBaseFlash(&EFLD1)

#define MAILBOX_SIZE 16
#define DEVICE_THREAD_STACK 512
#define CAN_THREAD_STACK 256

#define NUM_TX_MSGS 9
#define DEFAULT_BASE_ID 0x640

#define ADC1_NUM_CHANNELS 5
#define ADC1_BUF_DEPTH 1

#define ADC2_NUM_CHANNELS   1
#define ADC2_BUF_DEPTH      1

// Milliseconds from the 64-bit timestamp, truncated to 32 bits so it wraps cleanly
// every ~49.7 days. Always compare as (SYS_TIME - start), never as absolute times.
#define SYS_TIME ((uint32_t)(chVTGetTimeStamp() / (CH_CFG_ST_FREQUENCY / 1000)))

static const float ALWAYS_FALSE = 0.0f;
static const float ALWAYS_TRUE = 1.0f;

enum class AnalogChannel
{
    AnIn1 = 0,
    AnIn2,
    AnIn3,
    AnIn4,
    AnIn5,
    TempSensor
};

const CANConfig &GetCanConfig(CanBitrate bitrate);

msg_t InitAdc();
uint16_t GetAdcRaw(AnalogChannel channel);
float GetAdcVolts(AnalogChannel channel);
uint16_t GetTemperature();