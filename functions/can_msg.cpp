#include "can_msg.h"

bool CanMsg::Due(uint32_t nTimeNow)
{
    const bool on = pConfig->bEnabled && pConfig->nInput != 0 && pInput && *pInput != 0.0f;
    const bool rising = on && !bLast;
    bLast = on;
    fVal = on ? 1.0f : 0.0f;
    if (!on) return false;

    if (pConfig->eMode == CanMsgMode::Once)
        return rising;

    // Repeat: at once when it turns true, then every interval
    const uint16_t every = pConfig->nIntervalMs < 10 ? 10 : pConfig->nIntervalMs;
    if (rising || (nTimeNow - nLastSent) >= every)
    {
        nLastSent = nTimeNow;
        return true;
    }
    return false;
}
