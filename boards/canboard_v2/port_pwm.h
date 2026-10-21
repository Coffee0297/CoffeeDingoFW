#pragma once

#include "hal.h"

//===============================================
// Digital output PWM
// The timers are only a timebase: the period callback sets the DO line,
// the channel 1 compare callback clears it (the DO pins stay GPIO).
// One timer per output so each can have its own frequency.
//===============================================
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
