#pragma once

#include "port.h"
#include "hal.h"
#include "input.h"
#include "enums.h"

struct Config_CanInput{
  bool bEnabled;
  bool bTimeoutEnabled;
  uint16_t nTimeout; //ms
  uint8_t nIDE; //0=STD, 1=EXT
  uint32_t nID;
  uint8_t nStartBit;   
  uint8_t nBitLength;  
  float fFactor;       
  float fOffset;       
  ByteOrder eByteOrder;
  bool bSigned;        
  Operator eOperator;
  float fOperand;       
  InputMode eMode;
  #if NUM_CAN_BUSES > 1
  uint8_t nBus;
  #endif
};

// Single bus boards don't store a bus, keeps their config layout unchanged
inline uint8_t CanInputBus(const Config_CanInput &conf)
{
  #if NUM_CAN_BUSES > 1
  return conf.nBus;
  #else
  (void)conf;
  return 0;
  #endif
}

class CanInput
{
public:
    CanInput() {
    };

    static const uint16_t nBaseIndex = 0x1300;

    bool CheckMsg(const CANRxFrame &frame, uint8_t nBus);

    void SetConfig(Config_CanInput* config) { pConfig = config; }
    void CheckTimeout();

    float fOutput;
    float fVal;

private:
    Config_CanInput* pConfig;

    Input input;

    uint32_t nLastRxTime;
};