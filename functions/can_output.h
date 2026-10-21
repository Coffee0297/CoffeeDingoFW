#pragma once

#include "hal.h"
#include "enums.h"

struct Config_CanOutput{
  bool bEnabled;
  uint16_t nInput;
  uint8_t nIDE; //0=STD, 1=EXT
  uint32_t nID;
  uint8_t nStartBit;   
  uint8_t nBitLength;  
  float fFactor;       
  float fOffset;       
  ByteOrder eByteOrder;
  bool bSigned;
  uint16_t nInterval; //ms
  CanOutputKind eKind;
  uint16_t nTriggerInput; //0 = send on the interval, else once per rising edge
};

class CanOutput
{
public:
    bool CheckTxTime()
    {
        if (SYS_TIME - nLastTxTime >= nInterval)
        {
            nLastTxTime = SYS_TIME;
            return true;
        }

        return false;
    }

    CANTxFrame stFrame;
    uint16_t nInterval; //ms
    bool bTriggered; //Sent on trigger edges instead of the interval
    bool bSendNow;
    uint32_t nLastTxTime;
};