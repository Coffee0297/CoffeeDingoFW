#pragma once

#include "hal.h"
#include "enums.h"

#define PDM_TYPE 0 //0 = PDM
#define BOARD_ID 0 //device-type id reported in the Version reply (0=dingoPDM, 1=PDM-MAX, 2=CANBoard)

#define PROCESS_STACK 0x1000

#define HAS_SE_LEDS TRUE
#define HAS_USB TRUE
#define HAS_I2C TRUE
#define HAS_EXT_TEMP_SENSOR TRUE
#define HAS_EXT_MEMORY TRUE
#define HAS_BATT_VOLT_SENSE TRUE
#define CAN_SLEEP TRUE

#define NUM_OUTPUTS 8
#define NUM_DIG_OUTPUTS 0
#define NUM_DIG_INPUTS 2
#define NUM_ANALOG_INPUTS 0
#define NUM_VIRT_INPUTS 16
#define NUM_CAN_INPUTS 32
#define NUM_CAN_OUTPUTS 32
#define NUM_FLASHERS 4
#define NUM_WIPER_INTER_DELAYS 6
#define NUM_WIPER_SPEED_MAP 8
#define NUM_COUNTERS 4
#define NUM_CONDITIONS 32
#define NUM_KEYPADS 2
#define HAS_WIPERS TRUE
#define HAS_STARTER_DISABLE TRUE

#define KEYPAD_MAX_BUTTONS 20
#define KEYPAD_MAX_ANALOG_INPUTS 4
#define KEYPAD_MAX_DIALS 2

// Lua output slots: a pool the Lua program writes via setLuaOut(n,v). Any output/
// virtual-input/CAN-output is "Lua-driven" by pointing its nInput at one of these.
#define NUM_LUA_OUTPUTS 32

#define VAR_MAP_SYS_VARS 5
#define VAR_MAP_WIPER_VARS 6

#define VAR_MAP_SIZE ( \
    VAR_MAP_SYS_VARS + \
    (NUM_DIG_INPUTS * 1) + \
    (NUM_ANALOG_INPUTS * 4) + \
    (NUM_DIG_OUTPUTS * 1) + \
    (NUM_CAN_INPUTS * 2) + \
    (NUM_VIRT_INPUTS * 1) + \
    (NUM_OUTPUTS * 4) + \
    (NUM_FLASHERS * 1) + \
    (NUM_CONDITIONS * 1) + \
    (NUM_COUNTERS * 1) + \
    VAR_MAP_WIPER_VARS + \
    (NUM_KEYPADS * (KEYPAD_MAX_BUTTONS + KEYPAD_MAX_DIALS + KEYPAD_MAX_ANALOG_INPUTS)) + \
    (NUM_LUA_OUTPUTS * 1) \
)

#define MAILBOX_SIZE 128
#define DEVICE_THREAD_STACK 2048

// Last 2KB sector of flash (sector 31, 0x0800F800)
// Max program flash size goes to the end of sector 30 (0x0800F000) to leave config space at the end of flash
// !!! Flash must be smaller thank 63488 bytes !!!
#define CONFIG_SECTOR       31U
#define CONFIG_FLASH_OFFSET (CONFIG_SECTOR * 2048U)
#define CONFIG_FLASH        getBaseFlash(&EFLD1)

#define NUM_TX_MSGS 27
#define DEFAULT_BASE_ID 0x0DE

// Lua: embedded interpreter enabled on this board.
#define HAS_LUA TRUE
#define LUA_HEAP_SIZE (48 * 1024)   // static RAM arena (no newlib malloc)
#define LUA_SCRIPT_MAX 4096         // max assembled program size stored in config

#define ADC1_NUM_CHANNELS 8
#define ADC1_BUF_DEPTH 1

#define BTS7002_1EPP_KILIS 22950
#define BTS7008_2EPA_KILIS 5950
#define BTS70012_1ESP_KILIS 35000

#define SLEEP_TIMEOUT 30000

#define SYS_TIME TIME_I2MS(chVTGetSystemTimeX())

static const float ALWAYS_FALSE = 0.0f;
static const float ALWAYS_TRUE = 1.0f;
 
enum class AnalogChannel
{
    IS1 = 0,
    IS2,
    IS3_4,
    IS5_6,
    IS7_8,
    BattVolt,
    TempSensor,
    VRefInt
};

enum class LedType
{
    Status,
    Error
};

const CANConfig &GetCanConfig(CanBitrate bitrate);

const I2CConfig i2cConfig = {
    OPMODE_I2C,
    400000,
    FAST_DUTY_CYCLE_2,
};

msg_t InitAdc();
void DeInitAdc();
uint16_t GetAdcRaw(AnalogChannel channel);
float GetBattVolt();
float GetTemperature();
float GetVDDA();