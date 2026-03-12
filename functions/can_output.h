#pragma once

#include "port.h"
#include "config.h"
#include "hal.h"

extern float *pVarMap[PDM_VAR_MAP_SIZE];

class CanOutput
{
public:
    CanOutput() {
    };

    static const uint16_t nBaseIndex = 0x2000;

    void SetConfig(Config_CanOutput* config, uint8_t index) {
        pConfig = config;
        nIndex = index;
        pInput = pVarMap[config->nInput];
    }

    static void InitAllFrames();
    void Update();

private:
    Config_CanOutput* pConfig;

    float *pInput;

    uint8_t nIndex;
    uint32_t nLastTxTime;

    // Frame indexes are independent of CAN output indexes, multiple outputs may be packed into the same frame based on ID
    static CANTxFrame frames[PDM_NUM_CAN_OUTPUTS];
    static int8_t assignedFrame[PDM_NUM_CAN_OUTPUTS]; // Maps CAN output index to frame index, -1 if not assigned

    void CheckTime();
    static void ClearFrames();
    static uint8_t CalcDLC(uint8_t nStartBit, uint8_t nBitLength);
};


// CAN Outputs are packed into frames based on ID, multiple outputs may share a frame if they have the same ID and IDE. 
// Each output configures which bits of the frame it uses for its value. 
// When an output is updated, it updates the relevant bits in its assigned frame, and the frame is transmitted at the configured interval.