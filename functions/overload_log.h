#pragma once

#include <cstdint>
#include "port.h"
#include "enums.h"

#if NUM_OUTPUTS > 0

// On-device overload (trip) log. The device records the last OVL_EVENTS output trips
// (Overcurrent / Fault) with a current waveform around the trip, so the tool can read
// them back over CAN for troubleshooting even if it wasn't connected when it happened.
//
// Sampling: the control loop (500Hz) tracks per-output peak; every OVL_SAMPLE_MS the
// peak-since-last-sample is pushed here as one waveform point — so a sub-millisecond
// inrush/short spike inside a sample bucket is preserved as that bucket's value.
//
// ponytail: log lives in RAM only — a power cycle (e.g. to clear a latched Fault) wipes
// it. Persist event headers to FRAM if trips must survive a reboot.

// ponytail: rate/events kept small — this firmware's RAM is tight (Lua 48K heap + 12K
// stack). Each sample is the peak over its bucket, so 25Hz still catches a ms-scale
// inrush/short. Bump these if RAM frees up.
#define OVL_SAMPLE_MS    40                                   // 25 Hz waveform
#define OVL_PRE_S        10                                   // seconds captured before the trip
#define OVL_POST_S       3                                    // seconds captured after the trip
#define OVL_PRE_SAMPLES  ((OVL_PRE_S  * 1000) / OVL_SAMPLE_MS)// 250
#define OVL_POST_SAMPLES ((OVL_POST_S * 1000) / OVL_SAMPLE_MS)// 75
#define OVL_TOTAL_SAMPLES (OVL_PRE_SAMPLES + OVL_POST_SAMPLES)// 325
#define OVL_EVENTS       3                                    // most-recent trips kept
#define OVL_AMP_STEP     0.5f                                 // amps per waveform byte (0..127.5A)

void OvlLogInit();
// Push one waveform sample (the peak amps over the last OVL_SAMPLE_MS) for an output.
void OvlLogSample(uint8_t nOut, float fAmps);
// Record a trip: snapshots the pre-window and begins capturing the post-window.
void OvlLogTrigger(uint8_t nOut, ProfetState eState, float fLimitA);

uint8_t OvlLogCount();                                        // number of stored events
// Header for event idx (0 = newest). peakA/limitA in amps. Returns false if idx invalid.
bool OvlLogGetHeader(uint8_t idx, uint8_t &nOut, uint8_t &nState, float &fPeakA, float &fLimitA);
// Copy up to nMax waveform bytes from sample offset off. Returns bytes copied (0 past end).
uint8_t OvlLogGetData(uint8_t idx, uint16_t off, uint8_t *pBuf, uint8_t nMax);
void OvlLogClear();

#endif
