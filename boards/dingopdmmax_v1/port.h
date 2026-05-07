#pragma once

#include "hal.h"
#include "enums.h"

#define PDM_TYPE 1 //1 = PDM-MAX 

#define PDM_NUM_OUTPUTS 4
#define PDM_NUM_INPUTS 2
#define PDM_NUM_VIRT_INPUTS 16
#define PDM_NUM_CAN_INPUTS 32
#define PDM_NUM_CAN_OUTPUTS 32
#define PDM_NUM_FLASHERS 4
#define PDM_NUM_WIPER_INTER_DELAYS 6
#define PDM_NUM_WIPER_SPEED_MAP 8
#define PDM_NUM_COUNTERS 4
#define PDM_NUM_CONDITIONS 32
#define PDM_NUM_KEYPADS 2

#define KEYPAD_MAX_BUTTONS 20
#define KEYPAD_MAX_ANALOG_INPUTS 4
#define KEYPAD_MAX_DIALS 2

#define PDM_VAR_MAP_SYS_VARS 5
#define PDM_VAR_MAP_WIPER_VARS 6

#define PDM_VAR_MAP_SIZE ( \
    PDM_VAR_MAP_SYS_VARS + \
    (PDM_NUM_INPUTS * 1) + \
    (PDM_NUM_CAN_INPUTS * 2) + \
    (PDM_NUM_VIRT_INPUTS * 1) + \
    (PDM_NUM_OUTPUTS * 4) + \
    (PDM_NUM_FLASHERS * 1) + \
    (PDM_NUM_CONDITIONS * 1) + \
    (PDM_NUM_COUNTERS * 1) + \
    PDM_VAR_MAP_WIPER_VARS + \
    (PDM_NUM_KEYPADS * (KEYPAD_MAX_BUTTONS + KEYPAD_MAX_DIALS + KEYPAD_MAX_ANALOG_INPUTS)) \
)   

#define PDM_NUM_TX_MSGS 27

#define ADC1_NUM_CHANNELS 7
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
    IS3,
    IS4,
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