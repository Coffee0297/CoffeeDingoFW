#pragma once

#include "hal.h"
#include "enums.h"
#include "port.h"

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
  uint8_t nBus; //Only needed with >1 bus
};

// Single bus boards ignore the stored bus
inline uint8_t CanOutputBus(const Config_CanOutput &conf)
{
  #if NUM_CAN_BUSES > 1
  return conf.nBus;
  #else
  (void)conf;
  return 0;
  #endif
}

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
    uint32_t nLastTxTime;
    uint8_t nBus;
};