#include "port.h"
#include "mcu_utils.h"

/*
 FDCAN kernel clock = PCLK1 = 170MHz, 17 tq per bit (Seg1 = 13, Seg2 = 3), 82.4% sample point
 Prescaler = 170MHz / (17 * bitrate). Subtract '1' for register values
 Classic CAN only, non-matching frames rejected so the filters in can.cpp decide what's received
*/
#define FDCAN_CLASSIC_CONFIG(nPrescaler)                                                       \
{                                                                                              \
    OPMODE_CAN,                                                                                \
    FDCAN_CONFIG_NBTP_NSJW(2) | FDCAN_CONFIG_NBTP_NBRP((nPrescaler) - 1) |                     \
    FDCAN_CONFIG_NBTP_NTSEG1(12) | FDCAN_CONFIG_NBTP_NTSEG2(2),                                \
    0,                                                                                         \
    0,                                                                                         \
    0,                                                                                         \
    FDCAN_CONFIG_RXGFC_ANFS_REJECT | FDCAN_CONFIG_RXGFC_ANFE_REJECT                           \
}

static const CANConfig canConfig1000 = FDCAN_CLASSIC_CONFIG(10);
static const CANConfig canConfig500 = FDCAN_CLASSIC_CONFIG(20);
static const CANConfig canConfig250 = FDCAN_CLASSIC_CONFIG(40);
static const CANConfig canConfig125 = FDCAN_CLASSIC_CONFIG(80);
static const CANConfig canConfig100 = FDCAN_CLASSIC_CONFIG(100);

const CANConfig& GetCanConfig(CanBitrate bitrate) {
    switch(bitrate) {
        case CanBitrate::Bitrate_1000K:
            return canConfig1000;
        case CanBitrate::Bitrate_500K:
            return canConfig500;
        case CanBitrate::Bitrate_250K:
            return canConfig250;
        case CanBitrate::Bitrate_125K:
            return canConfig125;
        case CanBitrate::Bitrate_100K:
            return canConfig100;
        default:
            return canConfig500;
    }
    return canConfig500;
}

volatile adcsample_t adc1_samples[ADC1_NUM_CHANNELS] = {0};
//0 = OutIS1 - ADC1_IN0
//1 = OutIS2 - ADC1_IN12
//2 = OutIS3_4 - ADC1_IN13
//3 = OutIS5_6 - ADC1_IN1
//4 = OutIS7_8 - ADC1_IN2
//5 = BattVolt - ADC1_IN3
//6 = TempSensor
//7 = VRefInt

static const ADCConversionGroup adc1_cfg = {
    .circular = true,
    .num_channels = ADC1_NUM_CHANNELS,
    .end_cb = NULL,
    .error_cb = NULL,
    .cr1 = 0,
    .cr2 = ADC_CR2_SWSTART | ADC_CR2_CONT,
    .smpr1 = ADC_SMPR1_SMP_AN12(ADC_SAMPLE_15)   |
             ADC_SMPR1_SMP_AN13(ADC_SAMPLE_15)   |
             ADC_SMPR1_SMP_SENSOR(ADC_SAMPLE_15) |
             ADC_SMPR1_SMP_VREF(ADC_SAMPLE_15),
    .smpr2 = ADC_SMPR2_SMP_AN0(ADC_SAMPLE_15)    |
             ADC_SMPR2_SMP_AN1(ADC_SAMPLE_15)    |
             ADC_SMPR2_SMP_AN2(ADC_SAMPLE_15)    |
             ADC_SMPR2_SMP_AN3(ADC_SAMPLE_15),
    .htr = 0,                              
    .ltr = 0, 
    .sqr1 = 0,
    .sqr2 = ADC_SQR2_SQ7_N(ADC_CHANNEL_SENSOR) |
            ADC_SQR2_SQ8_N(ADC_CHANNEL_VREFINT),
    .sqr3 = ADC_SQR3_SQ1_N(ADC_CHANNEL_IN0)  |
            ADC_SQR3_SQ2_N(ADC_CHANNEL_IN12) |
            ADC_SQR3_SQ3_N(ADC_CHANNEL_IN13) |
            ADC_SQR3_SQ4_N(ADC_CHANNEL_IN1)  |
            ADC_SQR3_SQ5_N(ADC_CHANNEL_IN2)  |
            ADC_SQR3_SQ6_N(ADC_CHANNEL_IN3)                   
    };

msg_t InitAdc()
{
    msg_t ret;
    ret = adcStart(&ADCD1, NULL);
    if(ret != HAL_RET_SUCCESS)
        return ret;

    adcSTM32EnableTSVREFE(); //Enable temp sensor and vref

    //Need to continuous conversion to read both channels of the BTS7008-2EPA
    //Requires 2 channels to be read with a 100us delay between them
    //Profet DSEL pin toggled in profet.cpp
    adcStartConversion(&ADCD1, &adc1_cfg, const_cast<adcsample_t*>(adc1_samples), ADC1_BUF_DEPTH);

    return HAL_RET_SUCCESS;
}

void DeInitAdc()
{
    adcStopConversion(&ADCD1);
    adcStop(&ADCD1);
}

uint16_t GetAdcRaw(AnalogChannel channel)
{
    return adc1_samples[static_cast<uint8_t>(channel)];
}

float GetBattVolt()
{
    // MCU vRef = 3.3v
    // 4095 counts full scale
    float mcuVolts = (GetVDDA() / 4095) * GetAdcRaw(AnalogChannel::BattVolt);

    const float rUpper = 47000;
    const float rLower = 4700;

    return mcuVolts * ((rUpper + rLower) / rLower);
}

float GetTemperature()
{
    return (STM32_TS_CAL1_TEMP + ((float)(GetAdcRaw(AnalogChannel::TempSensor) - STM32_TS_CAL1) / (STM32_TS_CAL2 - STM32_TS_CAL1)) * (STM32_TS_CAL2_TEMP - STM32_TS_CAL1_TEMP));
}

float GetVDDA()
{
    uint16_t raw = GetAdcRaw(AnalogChannel::VRefInt);
    if (raw == 0)
        return 3.3; // No conversion yet, assume nominal

    return ((float)STM32_VREF_INT_CAL / (float)raw) * STM32_CAL_VDDA;
}