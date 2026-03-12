#include "can_output.h"
#include "mailbox.h"
#include "dbc.h"

extern CanOutput canOut[PDM_NUM_CAN_OUTPUTS];

void CanOutput::ClearFrames()
{
    for (int i = 0; i < PDM_NUM_CAN_OUTPUTS; ++i)
    {
        frames[i].data64[0] = 0;
        frames[i].DLC = 0;
        frames[i].IDE = 0;
        frames[i].EID = 0; //Clears SID as well

        assignedFrame[i] = -1;
    }
}

void CanOutput::InitAllFrames()
{
    ClearFrames();

    for (int i = 0; i < PDM_NUM_CAN_OUTPUTS; ++i)
    {
        if (!canOut[i].pConfig->bEnabled) continue;

        uint8_t  nIDE       = canOut[i].pConfig->nIDE;
        uint16_t nID       = canOut[i].pConfig->nID;
        uint8_t  nStartBit  = canOut[i].pConfig->nStartBit;
        uint8_t  nBitLength = canOut[i].pConfig->nBitLength;

        // Look for an existing frame with a matching ID
        for (int j = 0; j < PDM_NUM_CAN_OUTPUTS; ++j)
        {
            if (frames[j].DLC == 0) continue; // Unused frame slot

            bool bMatch = (nIDE == 0) ? (frames[j].IDE == 0 && frames[j].SID == nID)
                                      : (frames[j].IDE == 1 && frames[j].EID == nID);
            if (bMatch)
            {
                assignedFrame[i] = j;

                uint8_t newDlc = CalcDLC(nStartBit, nBitLength);
                if (frames[j].DLC < newDlc)
                    frames[j].DLC = newDlc;

                break;
            }
        }

        if (assignedFrame[i] != -1) continue;

        // No existing frame found, assign to first available slot
        for (int j = 0; j < PDM_NUM_CAN_OUTPUTS; ++j)
        {
            if (frames[j].DLC == 0)
            {
                assignedFrame[i] = j;
                frames[j].IDE = nIDE;

                //Only set one
                //SID and EID are a union
                if(nIDE == 0)
                    frames[j].SID = nID;
                else
                    frames[j].EID = nID;

                frames[j].DLC = CalcDLC(nStartBit, nBitLength);
                break;
            }
        }
    }
}

void CanOutput::Update()
{
    if (!pConfig->bEnabled) return;
    if (assignedFrame[nIndex] == -1) return;

    Dbc::EncodeFloat( frames[assignedFrame[nIndex]].data8, static_cast<float>(*pInput), pConfig->nStartBit, pConfig->nBitLength,
                    pConfig->fFactor, pConfig->fOffset, pConfig->eByteOrder);

    CheckTime();
}

void CanOutput::CheckTime()
{
    if (!pConfig->bEnabled) return;
    if (assignedFrame[nIndex] == -1) return;

    uint32_t nCurrentTime = SYS_TIME;

    if (nCurrentTime - nLastTxTime >= pConfig->nInterval)
    {
        PostTxFrame(&frames[assignedFrame[nIndex]]);

        nLastTxTime = nCurrentTime;
    }
}

uint8_t CanOutput::CalcDLC(uint8_t nStartBit, uint8_t nBitLength)
{
    uint8_t nLastBit  = nStartBit + nBitLength - 1;
    uint8_t nEndByte  = nLastBit / 8;

    return (nEndByte + 1);
}
