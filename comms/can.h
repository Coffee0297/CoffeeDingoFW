#pragma once

#include <cstdint>
#include "enums.h"
#include "config.h"

msg_t InitCan(Config_Device *conf);
void StopCan();
void ClearCanFilters();
void SetCanFilterId(uint8_t nFilterNum, uint32_t nId, bool bExtended);
void SetCanFilterEnabled(bool bEnabled);
uint32_t GetLastCanRxTime();
// True while no CAN peer is ACKing our frames (TX backed off); frames then go to USB only.
bool CanNoPeer();
// True while the module's mute-transmit input is on: periodic frames (telemetry, CAN outputs) are withheld.
bool CanTxMuted();

// Set by the config-commit (Burn) handler; DeviceThread re-runs InitCan() so a changed
// CAN filter enable / filter IDs / bitrate take effect without a power cycle.
extern volatile bool gReinitCanRequested;