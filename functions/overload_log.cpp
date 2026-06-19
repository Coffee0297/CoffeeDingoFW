#include "overload_log.h"

#if NUM_OUTPUTS > 0

struct OvlEvent
{
    bool     bUsed;
    uint8_t  nOut;
    uint8_t  nState;
    uint8_t  nPeakByte;                  // max waveform byte in this event
    float    fLimitA;
    uint8_t  acSamples[OVL_TOTAL_SAMPLES];
};

static OvlEvent     events[OVL_EVENTS];
static uint8_t      preRing[NUM_OUTPUTS][OVL_PRE_SAMPLES];
static uint16_t     preHead[NUM_OUTPUTS]; // next write index (oldest sample)
static int8_t       capSlot[NUM_OUTPUTS]; // event slot currently capturing post-window, -1 = none
static uint16_t     capRemain[NUM_OUTPUTS];
static uint8_t      nWriteSlot;           // next event slot to allocate
static uint8_t      nUsed;                // valid events, capped at OVL_EVENTS

static inline uint8_t AmpToByte(float a)
{
    int v = (int)(a / OVL_AMP_STEP + 0.5f);
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    return (uint8_t)v;
}

void OvlLogInit()
{
    nWriteSlot = 0;
    nUsed = 0;
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++)
    {
        preHead[i] = 0;
        capSlot[i] = -1;
        capRemain[i] = 0;
    }
    for (uint8_t e = 0; e < OVL_EVENTS; e++)
        events[e].bUsed = false;
}

void OvlLogSample(uint8_t nOut, float fAmps)
{
    if (nOut >= NUM_OUTPUTS) return;
    uint8_t b = AmpToByte(fAmps);

    // Roll into the pre-trip ring.
    preRing[nOut][preHead[nOut]] = b;
    preHead[nOut] = (preHead[nOut] + 1) % OVL_PRE_SAMPLES;

    // Feed an in-progress post-trip capture for this output.
    if (capSlot[nOut] >= 0)
    {
        OvlEvent *ev = &events[capSlot[nOut]];
        uint16_t idx = OVL_PRE_SAMPLES + (OVL_POST_SAMPLES - capRemain[nOut]);
        if (idx < OVL_TOTAL_SAMPLES)
        {
            ev->acSamples[idx] = b;
            if (b > ev->nPeakByte) ev->nPeakByte = b;
        }
        if (--capRemain[nOut] == 0)
            capSlot[nOut] = -1;          // event committed
    }
}

void OvlLogTrigger(uint8_t nOut, ProfetState eState, float fLimitA)
{
    if (nOut >= NUM_OUTPUTS) return;
    if (capSlot[nOut] >= 0) return;      // already logging a trip for this output

    uint8_t slot = nWriteSlot;
    nWriteSlot = (nWriteSlot + 1) % OVL_EVENTS;
    if (nUsed < OVL_EVENTS) nUsed++;

    OvlEvent *ev = &events[slot];
    ev->bUsed = true;
    ev->nOut = nOut;
    ev->nState = (uint8_t)eState;
    ev->fLimitA = fLimitA;
    ev->nPeakByte = 0;

    // Copy the pre-trip ring in chronological order (oldest first) into samples[0..PRE-1].
    uint16_t h = preHead[nOut];
    for (uint16_t k = 0; k < OVL_PRE_SAMPLES; k++)
    {
        uint8_t b = preRing[nOut][(h + k) % OVL_PRE_SAMPLES];
        ev->acSamples[k] = b;
        if (b > ev->nPeakByte) ev->nPeakByte = b;
    }
    // Post-window fills in as samples arrive; zero it meanwhile.
    for (uint16_t k = OVL_PRE_SAMPLES; k < OVL_TOTAL_SAMPLES; k++)
        ev->acSamples[k] = 0;

    capSlot[nOut] = slot;
    capRemain[nOut] = OVL_POST_SAMPLES;
}

uint8_t OvlLogCount() { return nUsed; }

// Map a newest-first index to a physical slot.
static OvlEvent *EventAt(uint8_t idx)
{
    if (idx >= nUsed) return nullptr;
    uint8_t slot = (nWriteSlot + OVL_EVENTS - 1 - idx) % OVL_EVENTS;
    OvlEvent *ev = &events[slot];
    return ev->bUsed ? ev : nullptr;
}

bool OvlLogGetHeader(uint8_t idx, uint8_t &nOut, uint8_t &nState, float &fPeakA, float &fLimitA)
{
    OvlEvent *ev = EventAt(idx);
    if (!ev) return false;
    nOut = ev->nOut;
    nState = ev->nState;
    fPeakA = ev->nPeakByte * OVL_AMP_STEP;
    fLimitA = ev->fLimitA;
    return true;
}

uint8_t OvlLogGetData(uint8_t idx, uint16_t off, uint8_t *pBuf, uint8_t nMax)
{
    OvlEvent *ev = EventAt(idx);
    if (!ev || off >= OVL_TOTAL_SAMPLES) return 0;
    uint8_t n = 0;
    while (n < nMax && (off + n) < OVL_TOTAL_SAMPLES)
    {
        pBuf[n] = ev->acSamples[off + n];
        n++;
    }
    return n;
}

void OvlLogClear() { OvlLogInit(); }

#endif
