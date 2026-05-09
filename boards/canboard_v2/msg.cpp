#include "msg.h"
#include "config.h"
#include "status.h"
#include "device.h"

uint8_t nHeartbeat = 0;

CANTxMsg TxMsg0()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 0 (Analog inputs 1-4 millivolts)
    //=======================================================
    stMsg.frame.IDE = CAN_IDE_STD;
    stMsg.frame.SID = stConfig.stDevConfig.nBaseId + CYCLIC_TX_OFFSET + 0;
    stMsg.frame.DLC = 8;
    stMsg.frame.data16[0] = (uint16_t)(GetAnalogInputMv(0));
    stMsg.frame.data16[1] = (uint16_t)(GetAnalogInputMv(1));
    stMsg.frame.data16[2] = (uint16_t)(GetAnalogInputMv(2));
    stMsg.frame.data16[3] = (uint16_t)(GetAnalogInputMv(3));

    stMsg.bSend = true; // Always send

    return stMsg;
}

CANTxMsg TxMsg1()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 1 (Analog input 5 millivolts and temperature)
    //=======================================================
    stMsg.frame.IDE = CAN_IDE_STD;
    stMsg.frame.SID = stConfig.stDevConfig.nBaseId + CYCLIC_TX_OFFSET + 1;
    stMsg.frame.DLC = 8;
    stMsg.frame.data16[0] = (uint16_t)(GetAnalogInputMv(4));
    stMsg.frame.data16[1] = 0;
    stMsg.frame.data16[2] = 0;
    stMsg.frame.data16[3] = GetTemperature();

    stMsg.bSend = true; // Always send

    return stMsg;
}

CANTxMsg TxMsg2()
{
    CANTxMsg stMsg;
    //=======================================================
    // Build Msg 2 (Rotary switches, dig inputs, analog input switches, low side output status, heartbeat)
    //=======================================================
    stMsg.frame.IDE = CAN_IDE_STD;
    stMsg.frame.SID = stConfig.stDevConfig.nBaseId + CYCLIC_TX_OFFSET + 2;
    stMsg.frame.DLC = 8;
    stMsg.frame.data8[0] = (GetRotarySwitchPos(1) << 4) + GetRotarySwitchPos(0);
    stMsg.frame.data8[1] = (GetRotarySwitchPos(3) << 4) + GetRotarySwitchPos(2);
    stMsg.frame.data8[2] = GetRotarySwitchPos(4);
    stMsg.frame.data8[3] = 0; //Empty
    stMsg.frame.data8[4] = (GetDigInputVal(7) << 7) + (GetDigInputVal(6) << 6) + (GetDigInputVal(5) << 5) + (GetDigInputVal(4) << 4) + 
                           (GetDigInputVal(3) << 3) + (GetDigInputVal(2) << 2) + (GetDigInputVal(1) << 1) + GetDigInputVal(0);
    stMsg.frame.data8[5] = (GetAnalogSwitchVal(4) << 4) + (GetAnalogSwitchVal(3) << 3) + (GetAnalogSwitchVal(2) << 2) + 
                           (GetAnalogSwitchVal(1) << 1) + GetAnalogSwitchVal(0);
    stMsg.frame.data8[6] = (GetDigOutputState(3) << 3) + (GetDigOutputState(2) << 2) + (GetDigOutputState(1) << 1) + GetDigOutputState(0);
    stMsg.frame.data8[7] = nHeartbeat;

    nHeartbeat++; // Increment heartbeat for each msg sent

    stMsg.bSend = true; // Always send

    return stMsg;
}