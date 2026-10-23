#pragma once

#include <cstdint>
#include "port.h"
#include "enums.h"

// Trip log: the last OVL_EVENTS output trips (Overcurrent/Fault) with the current waveform around them,
// read back over CAN (MsgCmd OvlCount..OvlClear, OvlTime). Each sample is the peak over its OVL_SAMPLE_MS
// bucket, so a ms-scale spike between samples still shows.
// ponytail: RAM only, a power cycle (e.g. to clear a latched Fault) wipes it; persist to FRAM if needed.

#define OVL_SAMPLE_MS 40                                        // 25 Hz
#define OVL_PRE_SAMPLES ((10 * 1000) / OVL_SAMPLE_MS)           // 10 s before the trip
#define OVL_POST_SAMPLES ((3 * 1000) / OVL_SAMPLE_MS)           // 3 s after
#define OVL_TOTAL_SAMPLES (OVL_PRE_SAMPLES + OVL_POST_SAMPLES)
#define OVL_EVENTS 3
#define OVL_AMP_STEP 0.5f                                       // A per sample byte

// Call every device cycle for each output; bSample on the cycle that closes an OVL_SAMPLE_MS bucket.
void OvlLogUpdate(uint8_t nOut, float fAmps, ProfetState eState, float fLimitA, bool bSample);

uint8_t OvlLogCount();
// idx 0 = newest. Peak and limit in A. False if idx is not stored.
bool OvlLogGetHeader(uint8_t idx, uint8_t &nOut, uint8_t &nState, float &fPeakA, float &fLimitA);
// Copies up to nMax sample bytes from sample offset nOff, returns the count (0 past the end).
uint8_t OvlLogGetData(uint8_t idx, uint16_t nOff, uint8_t *pBuf, uint8_t nMax);
uint32_t OvlLogTime(uint8_t idx); // SYS_TIME at the trip, 0 = not stored
void OvlLogClear();
