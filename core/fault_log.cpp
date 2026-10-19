#include "fault_log.h"

#if HAS_EXT_MEMORY && (NUM_OUTPUTS > 0)
#include <cstring>
#include "config.h"
#include "device.h"
#include "hardware/mb85rc.h"

static_assert(sizeof(DeviceConfig) + 4 <= FAULT_LOG_ADDR, "config + CRC overlaps the fault log");

extern MB85RC fram;

#define FAULT_LOG_MAGIC 0xFA17

struct FaultLog
{
    uint16_t nMagic;
    uint16_t nBootCount;
    uint8_t nHead; // next slot to write
    uint8_t nCount;
    uint16_t nReserved;
    FaultLogEntry stEntry[FAULT_LOG_SIZE];
};
static_assert(sizeof(FaultLog) <= 0x800, "fault log region is 2 KB");

static FaultLog stLog;
static bool bReady;          // FRAM read OK, so writing back can't wipe a log we failed to read
static volatile bool bDirty; // RAM copy changed, SlowThread writes it
static ProfetState eLastState[NUM_OUTPUTS];

void InitFaultLog()
{
    bReady = fram.Read(FAULT_LOG_ADDR, reinterpret_cast<uint8_t*>(&stLog), sizeof(stLog));

    if (!bReady || (stLog.nMagic != FAULT_LOG_MAGIC) ||
        (stLog.nHead >= FAULT_LOG_SIZE) || (stLog.nCount > FAULT_LOG_SIZE))
    {
        uint16_t nBootCount = (stLog.nMagic == FAULT_LOG_MAGIC) ? stLog.nBootCount : 0;
        memset(&stLog, 0, sizeof(stLog));
        stLog.nMagic = FAULT_LOG_MAGIC;
        stLog.nBootCount = nBootCount;
    }

    stLog.nBootCount++;
    bDirty = true;
}

// Device thread, after the outputs: log each output's transition into Overcurrent or Fault
void UpdateFaultLog()
{
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++)
    {
        ProfetState eNow = pf[i].GetState();
        FaultLogKind eKind = FaultLogKind::None;

        // Followers mirror their primary, which logs the trip
        // Only the first overcurrent since the output turned on: retries would flush the ring
        if (pf[i].pPrimary == nullptr && eNow != eLastState[i])
        {
            if (eNow == ProfetState::Overcurrent && pf[i].GetOcCount() == 1)
                eKind = FaultLogKind::Overcurrent;
            else if (eNow == ProfetState::Fault)
                eKind = (eLastState[i] == ProfetState::Overcurrent) ? FaultLogKind::OvercurrentLatch : FaultLogKind::Fault;
        }
        eLastState[i] = eNow;

        if (eKind == FaultLogKind::None)
            continue;

        FaultLogEntry stEntry = {SYS_TIME, 0, i, eKind,
                                 static_cast<uint16_t>(pf[i].GetCurrent() * 10),
                                 static_cast<uint16_t>(fBattVolt * 100)};
        chSysLock();
        stEntry.nBootCount = stLog.nBootCount;
        stLog.stEntry[stLog.nHead] = stEntry;
        stLog.nHead = (stLog.nHead + 1) % FAULT_LOG_SIZE;
        if (stLog.nCount < FAULT_LOG_SIZE)
            stLog.nCount++;
        chSysUnlock();
        bDirty = true;
    }
}

// SlowThread: FRAM writes stay out of the 2 ms device loop
void WriteFaultLog()
{
    if (!bReady || !bDirty)
        return;

    // Cleared first: a change during the write sets it again and is written on the next pass
    bDirty = false;
    if (!fram.Write(FAULT_LOG_ADDR, reinterpret_cast<uint8_t*>(&stLog), sizeof(stLog)))
        bDirty = true;
}

// Entry n, 0 = newest. Returns the entry count; an n past it gives an empty entry
// (kind None) carrying the current boot count
uint8_t ReadFaultLog(uint8_t n, FaultLogEntry *pEntry)
{
    chSysLock();
    uint8_t nCount = stLog.nCount;
    if (n < nCount)
        *pEntry = stLog.stEntry[(stLog.nHead + FAULT_LOG_SIZE - 1 - n) % FAULT_LOG_SIZE];
    else
        *pEntry = {0, stLog.nBootCount, 0, FaultLogKind::None, 0, 0};
    chSysUnlock();
    return nCount;
}

void ClearFaultLog()
{
    chSysLock();
    stLog.nHead = 0;
    stLog.nCount = 0;
    chSysUnlock();
    bDirty = true;
}

#endif
