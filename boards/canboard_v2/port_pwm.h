#pragma once

#include "hal.h"

#if HAS_DIG_PWM

//===============================================
// Digital-output PWM (CanBoard)
//
// The timer is used only as a timebase: the period (update) ISR sets the DO line
// high at the start of each cycle, the channel-compare ISR clears it at the duty
// point. The DO pins stay plain GPIO (no timer AF needed), exactly like the PDM's
// Profet PWM. One timer per output for independent frequencies:
//   DO1 -> TIM3 (PWMD3)    DO2 -> TIM15 (PWMD15)
//   DO3 -> TIM16 (PWMD16)  DO4 -> TIM17 (PWMD17)
// TIM2 is the ChibiOS system tick; TIM1/TIM4 are deliberately left alone.
//===============================================

// Period (update) callbacks — set the line high if the channel is enabled and duty > 0
static void pwmDo1pcb(PWMDriver *pwmp)
{
    if ((pwmp->enabled & (1 << 0)) && (pwmp->tim->CCR[0] > 0))
        palSetLine(LINE_DO1);
}
static void pwmDo2pcb(PWMDriver *pwmp)
{
    if ((pwmp->enabled & (1 << 0)) && (pwmp->tim->CCR[0] > 0))
        palSetLine(LINE_DO2);
}
static void pwmDo3pcb(PWMDriver *pwmp)
{
    if ((pwmp->enabled & (1 << 0)) && (pwmp->tim->CCR[0] > 0))
        palSetLine(LINE_DO3);
}
static void pwmDo4pcb(PWMDriver *pwmp)
{
    if ((pwmp->enabled & (1 << 0)) && (pwmp->tim->CCR[0] > 0))
        palSetLine(LINE_DO4);
}

// Channel (compare) callbacks — clear the line at the duty-cycle point
static void pwmDo1cb(PWMDriver *pwmp)
{
    if (pwmp->enabled & (1 << 0))
        palClearLine(LINE_DO1);
}
static void pwmDo2cb(PWMDriver *pwmp)
{
    if (pwmp->enabled & (1 << 0))
        palClearLine(LINE_DO2);
}
static void pwmDo3cb(PWMDriver *pwmp)
{
    if (pwmp->enabled & (1 << 0))
        palClearLine(LINE_DO3);
}
static void pwmDo4cb(PWMDriver *pwmp)
{
    if (pwmp->enabled & (1 << 0))
        palClearLine(LINE_DO4);
}

// 1 MHz timebase / period 10000 => 100 Hz default; Pwm::UpdateFrequency() retunes
// the period to nFreq at runtime (same as the PDM PWM configs).
static const PWMConfig pwm3Cfg = {
    .frequency = 1000000,
    .period = 10000,
    .callback = pwmDo1pcb,
    .channels = {
        {PWM_OUTPUT_ACTIVE_HIGH, pwmDo1cb},
        {PWM_OUTPUT_DISABLED, NULL},
        {PWM_OUTPUT_DISABLED, NULL},
        {PWM_OUTPUT_DISABLED, NULL}
    },
    .cr2 = 0,
    .bdtr = 0,
    .dier = 0
};

static const PWMConfig pwm15Cfg = {
    .frequency = 1000000,
    .period = 10000,
    .callback = pwmDo2pcb,
    .channels = {
        {PWM_OUTPUT_ACTIVE_HIGH, pwmDo2cb},
        {PWM_OUTPUT_DISABLED, NULL},
        {PWM_OUTPUT_DISABLED, NULL},
        {PWM_OUTPUT_DISABLED, NULL}
    },
    .cr2 = 0,
    .bdtr = 0,
    .dier = 0
};

static const PWMConfig pwm16Cfg = {
    .frequency = 1000000,
    .period = 10000,
    .callback = pwmDo3pcb,
    .channels = {
        {PWM_OUTPUT_ACTIVE_HIGH, pwmDo3cb},
        {PWM_OUTPUT_DISABLED, NULL},
        {PWM_OUTPUT_DISABLED, NULL},
        {PWM_OUTPUT_DISABLED, NULL}
    },
    .cr2 = 0,
    .bdtr = 0,
    .dier = 0
};

static const PWMConfig pwm17Cfg = {
    .frequency = 1000000,
    .period = 10000,
    .callback = pwmDo4pcb,
    .channels = {
        {PWM_OUTPUT_ACTIVE_HIGH, pwmDo4cb},
        {PWM_OUTPUT_DISABLED, NULL},
        {PWM_OUTPUT_DISABLED, NULL},
        {PWM_OUTPUT_DISABLED, NULL}
    },
    .cr2 = 0,
    .bdtr = 0,
    .dier = 0
};

#endif
