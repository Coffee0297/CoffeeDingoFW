#include "can_outputs.h"
#include "mailbox.h"
#include "dbc.h"

Config_CanOutput* CanOutputs::pConfigs[NUM_CAN_OUTPUTS];
float* CanOutputs::pInput[NUM_CAN_OUTPUTS];
uint32_t CanOutputs::nCount[NUM_CAN_OUTPUTS];
float* CanOutputs::pTrigger[NUM_CAN_OUTPUTS];
bool CanOutputs::bLastTrigger[NUM_CAN_OUTPUTS];
CanOutput CanOutputs::canOut[CAN_OUT_FRAMES];
int8_t CanOutputs::nAssignedOut[NUM_CAN_OUTPUTS];

void CanOutputs::ClearFrames()
{
    for (int i = 0; i < CAN_OUT_FRAMES; ++i)
    {
        canOut[i].stFrame.data64[0] = 0;
        canOut[i].stFrame.DLC = 0;
        canOut[i].stFrame.IDE = 0;
        canOut[i].stFrame.EID = 0; //Clears SID as well, union
        canOut[i].bTriggered = false;
        canOut[i].bSendNow = false;
    }

    for (int i = 0; i < NUM_CAN_OUTPUTS; ++i)
    {
        nAssignedOut[i] = -1;
    }
}

void CanOutputs::InitAllFrames()
{
    ClearFrames();

    for (int i = 0; i < NUM_CAN_OUTPUTS; ++i)
    {
        if (!pConfigs[i]->bEnabled) continue;

        uint8_t  nNewIDE      = pConfigs[i]->nIDE;
        uint32_t nNewID       = pConfigs[i]->nID; // 29-bit when extended
        uint8_t  nNewStartBit = pConfigs[i]->nStartBit;
        uint8_t  nNewBitLen   = pConfigs[i]->nBitLength;
        uint16_t nNewInterval = pConfigs[i]->nInterval;

        if(nNewBitLen == 0 || nNewBitLen > 64) continue; // Invalid config, skip
        // Checksums fill one whole byte: bits 0-7 (Intel) or 7-0 (Motorola) of it
        if(pConfigs[i]->eKind >= CanOutputKind::ChecksumXor &&
           (nNewBitLen != 8 || nNewStartBit % 8 != (pConfigs[i]->eByteOrder == ByteOrder::BigEndian ? 7 : 0))) continue;
        if(nNewInterval == 0) nNewInterval = 100; // Default to 100ms

        // Look for an existing frame with a matching ID
        for (int j = 0; j < CAN_OUT_FRAMES; ++j)
        {
            if (canOut[j].stFrame.DLC == 0) continue; // Unused frame slot

            bool bMatch = (nNewIDE == 0) ? (canOut[j].stFrame.IDE == 0 && canOut[j].stFrame.SID == nNewID)
                                      : (canOut[j].stFrame.IDE == 1 && canOut[j].stFrame.EID == nNewID);
            if (bMatch)
            {
                nAssignedOut[i] = j;

                //Expand DLC to fit longest output assigned to the frame
                uint8_t newDlc = CalcDlc(nNewStartBit, nNewBitLen);
                if (canOut[j].stFrame.DLC < newDlc)
                    canOut[j].stFrame.DLC = newDlc;

                //Use shortest interval of any output assigned to the frame
                if (canOut[j].nInterval > nNewInterval)
                    canOut[j].nInterval = nNewInterval;

                break;
            }
        }

        if (nAssignedOut[i] != -1) continue;

        // No existing frame found, assign to first available slot
        for (int j = 0; j < CAN_OUT_FRAMES; ++j)
        {
            if (canOut[j].stFrame.DLC == 0)
            {
                nAssignedOut[i] = j;
                canOut[j].stFrame.IDE = nNewIDE;

                //Only set one
                //SID and EID are a union
                if(nNewIDE == 0)
                    canOut[j].stFrame.SID = nNewID;
                else
                    canOut[j].stFrame.EID = nNewID;

                canOut[j].stFrame.DLC = CalcDlc(nNewStartBit, nNewBitLen);
                canOut[j].nInterval = nNewInterval;
                break;
            }
        }
    }

    // A frame with any triggered output is sent only on trigger edges, not on the interval
    for (int i = 0; i < NUM_CAN_OUTPUTS; ++i)
    {
        if (nAssignedOut[i] != -1 && pConfigs[i]->nTriggerInput)
            canOut[nAssignedOut[i]].bTriggered = true;
    }
}

void CanOutputs::Update()
{
    for (int j = 0; j < NUM_CAN_OUTPUTS; ++j)
    {
        if (nAssignedOut[j] == -1 || !pConfigs[j]->nTriggerInput) continue;

        bool bTrig = *pTrigger[j] > 0.5f;
        if (bTrig && !bLastTrigger[j])
            canOut[nAssignedOut[j]].bSendNow = true;
        bLastTrigger[j] = bTrig;
    }

    for (int i = 0; i < CAN_OUT_FRAMES; ++i)
    {
        if (canOut[i].stFrame.DLC == 0) continue; // Skip unused frames

        bool bSend = canOut[i].bTriggered ? canOut[i].bSendNow : canOut[i].CheckTxTime();
        canOut[i].bSendNow = false;

        if (bSend)
        {
            // Update data from all assigned outputs before sending
            for (int j = 0; j < NUM_CAN_OUTPUTS; ++j)
            {
                if (nAssignedOut[j] != i) continue;

                if (pConfigs[j]->eKind == CanOutputKind::Signal)
                    Dbc::EncodeFloat( canOut[i].stFrame.data8, static_cast<float>(*pInput[j]), pConfigs[j]->nStartBit, pConfigs[j]->nBitLength,
                                    pConfigs[j]->fFactor, pConfigs[j]->fOffset, pConfigs[j]->eByteOrder);
                else if (pConfigs[j]->eKind == CanOutputKind::RollingCounter)
                    Dbc::EncodeInt(canOut[i].stFrame.data8, nCount[j]++, pConfigs[j]->nStartBit, pConfigs[j]->nBitLength, pConfigs[j]->eByteOrder);
            }

            // Checksums last, over the assembled frame
            for (int j = 0; j < NUM_CAN_OUTPUTS; ++j)
            {
                if (nAssignedOut[j] != i || pConfigs[j]->eKind < CanOutputKind::ChecksumXor) continue;

                uint8_t nByte = pConfigs[j]->nStartBit / 8;
                canOut[i].stFrame.data8[nByte] = Dbc::Checksum(pConfigs[j]->eKind, canOut[i].stFrame.data8, canOut[i].stFrame.DLC, nByte);
            }

            PostTxFrame(&canOut[i].stFrame);
        }
    }
}

uint8_t CanOutputs::CalcDlc(uint8_t nStartBit, uint8_t nBitLength)
{
    uint8_t nLastBit  = nStartBit + nBitLength - 1;
    uint8_t nEndByte  = nLastBit / 8;
    uint8_t nDlc      = nEndByte + 1;

    return (nDlc > 8) ? 8 : nDlc;
}
