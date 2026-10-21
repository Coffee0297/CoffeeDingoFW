#pragma once

#include "ch.h"
#include "hal.h"
#include "config.h"
#include "enums.h"

// Sleep management functions extracted from pdm.cpp

bool CheckEnterSleep();
void EnterSleep();
void InitSleep();
void SendSleepInfo();
uint8_t SleepCountdownSec();
bool QuietWake();
void CheckQuietWake(const CANRxFrame *frame);
void CheckQuietWakeInputs();
void EnableLineEventWithPull(ioline_t line, InputPull pull);