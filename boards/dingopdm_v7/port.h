#pragma once

#include "hal.h"
#include "enums.h"

#define PDM_TYPE 0 //0 = PDM

#define NUM_OUTPUTS 8
#define NUM_INPUTS 2
#define NUM_VIRT_INPUTS 16
#define NUM_CAN_INPUTS 32
#define NUM_CAN_OUTPUTS 32
#define NUM_FLASHERS 4
#define NUM_WIPER_INTER_DELAYS 6
#define NUM_WIPER_SPEED_MAP 8
#define NUM_COUNTERS 4
#define NUM_CONDITIONS 32
#define NUM_KEYPADS 2

#define KEYPAD_MAX_BUTTONS 20
#define KEYPAD_MAX_ANALOG_INPUTS 4
#define KEYPAD_MAX_DIALS 2

#define VAR_MAP_SYS_VARS 5
#define VAR_MAP_WIPER_VARS 6

#define VAR_MAP_SIZE ( \
    VAR_MAP_SYS_VARS + \
    (NUM_INPUTS * 1) + \
    (NUM_CAN_INPUTS * 2) + \
    (NUM_VIRT_INPUTS * 1) + \
    (NUM_OUTPUTS * 4) + \
    (NUM_FLASHERS * 1) + \
    (NUM_CONDITIONS * 1) + \
    (NUM_COUNTERS * 1) + \
    VAR_MAP_WIPER_VARS + \
    (NUM_KEYPADS * (KEYPAD_MAX_BUTTONS + KEYPAD_MAX_DIALS + KEYPAD_MAX_ANALOG_INPUTS)) \
)

#define PDM_NUM_TX_MSGS 27

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