#include "can_msg.h"
#include "dbc.h"

bool CanMsg::Due(bool bMuted)
{
    // A standard id above 0x7FF would be cut to 11 bits and reach the wrong node, so it never sends
    bool bOn = pConfig->bEnabled && *pInput && (pConfig->nIDE || pConfig->nID <= 0x7FF);
    // The first pass after enable (or power-up) only samples: an input already on sends no Once frame by itself
    bool bRising = bOn && !bLast && bInit;
    bInit = pConfig->bEnabled;
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

void CanMsg::Fill(uint8_t *pData)
{
    for (uint8_t i = 0; i < 8; i++)
        pData[i] = pConfig->nData[i];

    // Bytes past the DLC aren't sent, so a counter or checksum there is off
    if (pConfig->nCounterByte < pConfig->nDLC)
        pData[pConfig->nCounterByte] = (pData[pConfig->nCounterByte] & 0xF0) | (nCount++ & 0x0F);

    if (pConfig->eChecksum >= CanOutputKind::ChecksumXor && pConfig->nChecksumByte < pConfig->nDLC)
        pData[pConfig->nChecksumByte] = Dbc::Checksum(pConfig->eChecksum, pData, pConfig->nDLC, pConfig->nChecksumByte);
}
