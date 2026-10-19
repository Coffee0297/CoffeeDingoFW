#pragma once

#include <cstdint>
#include "port.h"

#if HAS_EXT_MEMORY && (NUM_OUTPUTS > 0)

// Output trips kept in FRAM across power cycles: a ring of the last FAULT_LOG_SIZE entries
#define FAULT_LOG_ADDR 0x7000
#define FAULT_LOG_SIZE 32

enum class FaultLogKind : uint8_t
{
    None = 0,
    Overcurrent = 1,
    Fault = 2,
    OvercurrentLatch = 3 // Overcurrent latched to Fault (reset mode None, or the reset count ran out)
};

struct FaultLogEntry
{
    uint32_t nUptimeMs;
    uint16_t nBootCount;
    uint8_t nOutput; // 0 = output 1
    FaultLogKind eKind;
    uint16_t nCurrent;  // A x10
    uint16_t nBattVolt; // V x100
};

void InitFaultLog();
void UpdateFaultLog();
void WriteFaultLog();
uint8_t ReadFaultLog(uint8_t n, FaultLogEntry *pEntry);
void ClearFaultLog();

#endif
