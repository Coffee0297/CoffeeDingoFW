#pragma once

#include <cstdint>

#define FORCE_QUIET_MS 1000      // forced sleep waits for this much bus quiet: the others' goodbyes are past
#define FORCE_QUIET_MAX_MS 10000 // ...or this long after it was forced, whatever is still talking

// Forced sleep: the goodbye goes out when it is forced, while every module is still awake, then nothing is sent
// until the bus has been quiet FORCE_QUIET_MS. Modules forced together all sleep after the same quiet, so no
// goodbye reaches one that is already asleep and wakes it.
inline bool ForceSleepDue(uint32_t nCanRxIdleMs, uint32_t nSinceForcedMs)
{
    return (nCanRxIdleMs >= FORCE_QUIET_MS) || (nSinceForcedMs >= FORCE_QUIET_MAX_MS);
}
