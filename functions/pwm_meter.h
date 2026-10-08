#pragma once

#include <cstdint>

// Duty/period accumulator for a PWM input, fed one edge at a time with free-running cycle-counter
// timestamps (wrap-safe unsigned differences). A period closes at each rising edge; its high time is the
// fall in between. Take() hands over everything since the last call. Pure, so tests/host_selftest.cpp
// drives it with synthetic edges.
struct PwmMeter
{
    uint32_t nRiseAt, nFallAt;
    uint32_t nSumHigh, nSumPeriod, nPeriods;
    uint32_t nLastPeriod;
    bool bSeenRise, bSeenFall;

    void Reset() { *this = PwmMeter{}; }

    void Edge(bool high, uint32_t now)
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
