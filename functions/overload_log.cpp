#include "overload_log.h"

#if NUM_OUTPUTS > 0

struct OvlEvent
{
    uint8_t nOut;
    uint8_t nState;
    uint8_t nPeak;  // highest sample byte
    float fLimitA;
    uint32_t nTime;
    uint8_t nSamples[OVL_TOTAL_SAMPLES];
};

static struct
{
    OvlEvent events[OVL_EVENTS];
    uint8_t nNext;                              // next slot to write
    uint8_t nUsed;
    uint8_t nPre[NUM_OUTPUTS][OVL_PRE_SAMPLES]; // ring, nPreHead = oldest
    uint16_t nPreHead[NUM_OUTPUTS];
    uint8_t nCapSlot[NUM_OUTPUTS];
    uint16_t nCapLeft[NUM_OUTPUTS];             // post-trip samples still to capture, 0 = none
    float fPeak[NUM_OUTPUTS];
} st;
static bool bWasTrip[NUM_OUTPUTS]; // outside st: a Clear doesn't re-log an output that is still tripped

static uint8_t AmpToByte(float fAmps)
{
    float f = fAmps / OVL_AMP_STEP + 0.5f;
    return f <= 0 ? 0 : (f >= 255 ? 255 : (uint8_t)f);
}

static void Push(OvlEvent &ev, uint16_t nIdx, uint8_t nVal)
{
    ev.nSamples[nIdx] = nVal;
    if (nVal > ev.nPeak)
        ev.nPeak = nVal;
}

void OvlLogUpdate(uint8_t nOut, float fAmps, ProfetState eState, float fLimitA, bool bSample)
{
    if (fAmps > st.fPeak[nOut])
        st.fPeak[nOut] = fAmps;

    if (bSample)
    {
        uint8_t nVal = AmpToByte(st.fPeak[nOut]);
        st.fPeak[nOut] = 0;

        st.nPre[nOut][st.nPreHead[nOut]] = nVal;
        st.nPreHead[nOut] = (st.nPreHead[nOut] + 1) % OVL_PRE_SAMPLES;

        if (st.nCapLeft[nOut] > 0)
        {
            Push(st.events[st.nCapSlot[nOut]], OVL_TOTAL_SAMPLES - st.nCapLeft[nOut], nVal);
            st.nCapLeft[nOut]--;
        }
    }

    // Log the edge into Overcurrent/Fault; Overcurrent -> Fault is the same trip
    bool bTrip = (eState == ProfetState::Overcurrent) || (eState == ProfetState::Fault);
    bool bNew = bTrip && !bWasTrip[nOut] && (st.nCapLeft[nOut] == 0);
    bWasTrip[nOut] = bTrip;
    if (!bNew)
        return;

    uint8_t nSlot = st.nNext;
    st.nNext = (st.nNext + 1) % OVL_EVENTS;
    if (st.nUsed < OVL_EVENTS)
        st.nUsed++;

    // A slot still capturing for another output is overwritten: stop that capture
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++)
        if (st.nCapLeft[i] > 0 && st.nCapSlot[i] == nSlot)
            st.nCapLeft[i] = 0;

    OvlEvent &ev = st.events[nSlot];
    ev = {};
    ev.nOut = nOut;
    ev.nState = (uint8_t)eState;
    ev.fLimitA = fLimitA;
    ev.nTime = SYS_TIME;
    for (uint16_t k = 0; k < OVL_PRE_SAMPLES; k++)
        Push(ev, k, st.nPre[nOut][(st.nPreHead[nOut] + k) % OVL_PRE_SAMPLES]);

    st.nCapSlot[nOut] = nSlot;
    st.nCapLeft[nOut] = OVL_POST_SAMPLES;
}

uint8_t OvlLogCount() { return st.nUsed; }

static OvlEvent *EventAt(uint8_t idx)
{
    if (idx >= st.nUsed)
        return nullptr;
    return &st.events[(st.nNext + OVL_EVENTS - 1 - idx) % OVL_EVENTS];
}

bool OvlLogGetHeader(uint8_t idx, uint8_t &nOut, uint8_t &nState, float &fPeakA, float &fLimitA)
{
    OvlEvent *ev = EventAt(idx);
    if (!ev)
        return false;
    nOut = ev->nOut;
    nState = ev->nState;
    fPeakA = ev->nPeak * OVL_AMP_STEP;
    fLimitA = ev->fLimitA;
    return true;
}

uint8_t OvlLogGetData(uint8_t idx, uint16_t nOff, uint8_t *pBuf, uint8_t nMax)
{
    OvlEvent *ev = EventAt(idx);
    uint8_t n = 0;
    while (ev && n < nMax && nOff + n < OVL_TOTAL_SAMPLES)
    {
        pBuf[n] = ev->nSamples[nOff + n];
        n++;
    }
    return n;
}

uint32_t OvlLogTime(uint8_t idx)
{
    OvlEvent *ev = EventAt(idx);
    return ev ? ev->nTime : 0;
}

void OvlLogClear()
{
    st = {};
}

#endif
