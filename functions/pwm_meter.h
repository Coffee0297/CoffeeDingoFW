#pragma once

#include <cstdint>

// Duty/period accumulator for a PWM input, fed one edge at a time with free-running cycle-counter
// timestamps (wrap-safe unsigned differences). A period closes at each rising edge; its high time is the
// fall in between. Take() hands over everything since the last call. Pure, so tests/host_selftest.cpp
// drives it with synthetic edges.
//
// Glitch filter (nMinCycles > 0): each edge is held until the next one; if that comes sooner than
// nMinCycles, the two form a spike and both are dropped, whichever phase the spike lands in. A real pulse
// narrower than the filter is dropped the same way (the signal reads as its steady level).
struct PwmMeter
{
    uint32_t nRiseAt, nFallAt;
    uint32_t nSumHigh, nSumPeriod, nPeriods;
    uint32_t nLastPeriod;
    bool bSeenRise, bSeenFall;
    uint32_t nMinCycles;        // glitch filter, 0 = off (kept by Reset)
    bool bHeld, bHeldHigh;
    uint32_t nHeldAt;

    void Reset() { uint32_t f = nMinCycles; *this = PwmMeter{}; nMinCycles = f; }

    // true when an edge got through the filter: only those count as signal activity (timeout)
    bool Edge(bool high, uint32_t now)
    {
        if (nMinCycles == 0) { Commit(high, now); return true; }
        bool passed = false;
        if (bHeld)
        {
            bHeld = false;
            if (now - nHeldAt < nMinCycles) return false;   // spike: drop the held edge and this one
            Commit(bHeldHigh, nHeldAt);
            passed = true;
        }
        bHeld = true; bHeldHigh = high; nHeldAt = now;
        return passed;
    }

    void Commit(bool high, uint32_t now)
    {
        if (high)
        {
            if (bSeenRise && bSeenFall)
            {
                uint32_t period = now - nRiseAt;
                uint32_t hi = nFallAt - nRiseAt;
                if (hi <= period && nSumPeriod < 0x80000000U)
                {
                    nSumHigh += hi;
                    nSumPeriod += period;
                    nPeriods++;
                }
                nLastPeriod = period;
            }
            nRiseAt = now;
            bSeenRise = true;
            bSeenFall = false;
        }
        else if (bSeenRise)
        {
            nFallAt = now;
            bSeenFall = true;
        }
    }

    struct Sample { uint32_t sumHigh, sumPeriod, periods, lastPeriod; };
    Sample Take()
    {
        Sample s{nSumHigh, nSumPeriod, nPeriods, nLastPeriod};
        nSumHigh = nSumPeriod = nPeriods = 0;
        return s;
    }

    // duty % of the high level; Hz from `clk` cycles per second. Both 0 when no whole period was seen.
    static float Duty(const Sample &s) { return s.sumPeriod ? 100.0f * (float)s.sumHigh / (float)s.sumPeriod : 0.0f; }
    static float Freq(const Sample &s, uint32_t clk) { return s.sumPeriod ? (float)clk * (float)s.periods / (float)s.sumPeriod : 0.0f; }
};
