#pragma once

#include "ch.h"
#include "hal.h"
#include "config.h"
#include "enums.h"

// Sleep management functions extracted from pdm.cpp

bool CheckEnterSleep();
void EnterSleep();

// 5.6 sleep status: seconds until auto-sleep (0xFF = not counting) and what blocks it (MsgCmd::SleepInfo bits).
uint8_t SleepCountdownSec();
uint8_t SleepBlockers();
// Why the module last woke (kept across the wake reset), and how many times it woke since power-on.
WakeReason GetWakeReason();
uint8_t GetWakeSource();
uint16_t GetWakeCount();
void CaptureWakeAfterStop();   // EnterStopMode: right after WFI, before the reset
void ReadWakeAtBoot();         // InitDevice
// Why it is going to sleep now (1 auto, 2 forced, 3 request), for the "going to sleep" frame
uint8_t SleepWhy();
void EnableLineEventWithPull(ioline_t line, InputPull pull);