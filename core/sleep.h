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
// Sends the SleepInfo frame (why it woke, wakes, countdown, blockers); done unasked once after boot.
void SendSleepInfo();
// Woken by CAN and not yet heard a real frame: everything periodic stays quiet (see core/sleep.cpp).
bool QuietWake();
void EndQuietWake();
uint16_t GetWakeCount();
void CaptureWakeAfterStop();   // EnterStopMode: right after WFI, before the reset
void ReadWakeAtBoot();         // InitDevice
// Why it is going to sleep now (1 auto, 2 forced, 3 request), for the "going to sleep" frame
uint8_t SleepWhy();
void EnableLineEventWithPull(ioline_t line, InputPull pull);