#include "can_msg.h"

bool CanMsg::Due(bool bMuted)
{
    // A standard id above 0x7FF would be cut to 11 bits and reach the wrong node, so it never sends
    bool bOn = pConfig->bEnabled && *pInput && (pConfig->nIDE || pConfig->nID <= 0x7FF);
    bool bRising = bOn && !bLast;
    bLast = bOn;

    if (!bOn)
        return false;

    if (pConfig->eMode == CanMsgMode::Once)
        return bRising;

    if (bMuted)
        return false;

    // At once when the input turns on, then every interval
    if (bRising || (SYS_TIME - nLastSent >= pConfig->nInterval))
    {
        nLastSent = SYS_TIME;
        return true;
    }

    return false;
}
