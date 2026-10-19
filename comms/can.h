#pragma once

#include <cstdint>
#include "enums.h"
#include "config.h"

msg_t InitCan(Config_Device *conf);
void StopCan();
// Rebuild hardware filters from live config (config ID, CAN inputs, keypads), safe while running
void UpdateCanFilters();
uint32_t GetLastCanRxTime();
// Stop all transmit except replies to dingoConfig, used before sleep
void SetCanTxQuiet(bool bQuiet);
// Mute input set: no periodic frames (telemetry, CAN outputs), so the bus can go idle and the fleet sleep
bool CanTxMuted();
// True while no CAN peer ACKs our frames (TX backed off), frames then go to USB only
bool CanNoPeer();
