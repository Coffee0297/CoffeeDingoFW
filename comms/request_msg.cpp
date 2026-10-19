#include "request_msg.h"
#include "device.h"
#include "can.h"
#include "config.h"
#include "config_handler.h"
#include "mcu_utils.h"
#include "device_config.h"
#include "mailbox.h"
#include "enums.h"
#include "fault_log.h"

// External variables from pdm.cpp that we need access to
extern DeviceConfig stConfig;
extern bool bSleepRequest;
extern volatile uint32_t nIdentifyStart;
extern volatile uint32_t nIdentifyMs;

void CheckRequestMsgs(CANRxFrame *frame)
{
    //Check for settings request message, (Base ID - 1)
    if(frame->SID != stConfig.stDevice.nBaseId + CONFIG_RX_OFFSET)
        return;

    #if CAN_SLEEP
    // Check for sleep request
    if ((frame->DLC == 8) && 
        (frame->data8[0] == static_cast<uint8_t>(MsgCmd::Sleep)) &&
        (frame->data8[1] == 'Q') && (frame->data8[2] == 'U') && 
        (frame->data8[3] == 'I') && (frame->data8[4] == 'T'))
    {
        CANTxFrame txMsg;
        txMsg.SID = stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET;
        txMsg.IDE = CAN_IDE_STD;
        txMsg.DLC = 8;
        txMsg.data8[0] = static_cast<uint8_t>(MsgCmd::Sleep);
        txMsg.data8[1] = 'Q';
        txMsg.data8[2] = 'U';
        txMsg.data8[3] = 'I';
        txMsg.data8[4] = 'T';
        txMsg.data8[5] = 1; // Acknowledge sleep request
        txMsg.data8[6] = 0;
        txMsg.data8[7] = 0;

        PostTxFrame(&txMsg);

        bSleepRequest = true;
    }
    #endif

    // Identify: the LEDs take turns for 1-60 s so the module can be found
    if ((frame->DLC == 8) &&
        (frame->data8[0] == static_cast<uint8_t>(MsgCmd::Identify)))
    {
        uint8_t nSecs = frame->data8[1] < 1 ? 1 : (frame->data8[1] > 60 ? 60 : frame->data8[1]);
        nIdentifyStart = SYS_TIME;
        nIdentifyMs = nSecs * 1000;

        CANTxFrame txMsg;
        txMsg.SID = stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET;
        txMsg.IDE = CAN_IDE_STD;
        txMsg.DLC = 8;
        txMsg.data8[0] = static_cast<uint8_t>(MsgCmd::Identify);
        txMsg.data8[1] = nSecs;
        txMsg.data8[2] = HAS_SE_LEDS;
        txMsg.data8[3] = 0;
        txMsg.data8[4] = 0;
        txMsg.data8[5] = 0;
        txMsg.data8[6] = 0;
        txMsg.data8[7] = 0;
        PostTxFrame(&txMsg);
    }

    // Check for burn request
    if ((frame->DLC == 8) && 
        (frame->data8[0] == static_cast<uint8_t>(MsgCmd::BurnSettings)) &&
        (frame->data8[1] == 1) &&
        (frame->data8[2] == 3) && 
        (frame->data8[3] == 8))
    {
        CANTxFrame txMsg;
        txMsg.SID = stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET;
        txMsg.IDE = CAN_IDE_STD;
        txMsg.DLC = 8;
        txMsg.data8[0] = static_cast<uint8_t>(MsgCmd::BurnSettings);
        txMsg.data8[1] = 1;
        txMsg.data8[2] = 3;
        txMsg.data8[3] = 8;
        txMsg.data8[4] = WriteConfig();
        txMsg.data8[5] = 0;
        txMsg.data8[6] = 0;
        txMsg.data8[7] = 0;
        PostTxFrame(&txMsg);
    }

    #if HAS_USB
    // Check for bootloader request
    if ((frame->DLC == 8) &&
        (frame->data8[0] == static_cast<uint8_t>(MsgCmd::Bootloader)) && 
        (frame->data8[1] == 'B') && (frame->data8[2] == 'O') && 
        (frame->data8[3] == 'O') && (frame->data8[4] == 'T') && (frame->data8[5] == 'L'))
    {
        RequestBootloader();
    }
    #endif

    // Check for restart request - applies settings that need a reset (e.g. CAN bitrate)
    // Unburned settings are lost, host should burn first
    if ((frame->DLC == 8) &&
        (frame->data8[0] == static_cast<uint8_t>(MsgCmd::Restart)) &&
        (frame->data8[1] == 'R') && (frame->data8[2] == 'E') &&
        (frame->data8[3] == 'S') && (frame->data8[4] == 'E') && (frame->data8[5] == 'T'))
    {
        CANTxFrame txMsg;
        txMsg.SID = stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET;
        txMsg.IDE = CAN_IDE_STD;
        txMsg.DLC = 8;
        txMsg.data8[0] = static_cast<uint8_t>(MsgCmd::Restart);
        txMsg.data8[1] = 'R';
        txMsg.data8[2] = 'E';
        txMsg.data8[3] = 'S';
        txMsg.data8[4] = 'E';
        txMsg.data8[5] = 'T';
        txMsg.data8[6] = 1; // Acknowledge restart request
        txMsg.data8[7] = 0;

        PostTxFrame(&txMsg);

        // Let the ack go out before resetting, same margin as sleep
        chThdSleepMilliseconds(100);

        NVIC_SystemReset();
    }

    // Output bench test, reply echoes bytes 0-6 with byte 7 = accepted
    if ((frame->DLC == 8) &&
        (frame->data8[0] == static_cast<uint8_t>(MsgCmd::OutputTest)))
    {
        CANTxFrame txMsg;
        txMsg.SID = stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET;
        txMsg.IDE = CAN_IDE_STD;
        txMsg.DLC = 8;
        for (uint8_t i = 0; i < 7; i++)
            txMsg.data8[i] = frame->data8[i];
        txMsg.data8[7] = SetOutputTest(frame->data8[1], frame->data8[2], frame->data8[3],
                                       frame->data8[4] | (frame->data8[5] << 8), frame->data8[6]);

        PostTxFrame(&txMsg);
    }

    // Check for version request
    if ((frame->DLC == 8) &&
        (frame->data8[0] == static_cast<uint8_t>(MsgCmd::Version)))
    {
        CANTxFrame txMsg;
        txMsg.SID = stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET;
        txMsg.IDE = CAN_IDE_STD;
        txMsg.DLC = 8;
        txMsg.data8[0] = static_cast<uint8_t>(MsgCmd::Version);
        txMsg.data8[1] = BOARD_ID;
        txMsg.data8[2] = 0;
        txMsg.data8[3] = 0;
        txMsg.data8[4] = MAJOR_VERSION;
        txMsg.data8[5] = MINOR_VERSION;
        txMsg.data8[6] = BUILD >> 8;
        txMsg.data8[7] = BUILD & 0xFF;

        PostTxFrame(&txMsg);
    }

    // Check for reset cause request
    if ((frame->DLC == 8) &&
        (frame->data8[0] == static_cast<uint8_t>(MsgCmd::ResetCause)))
    {
        CANTxFrame txMsg = {};
        txMsg.SID = stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET;
        txMsg.IDE = CAN_IDE_STD;
        txMsg.DLC = 8;
        txMsg.data8[0] = static_cast<uint8_t>(MsgCmd::ResetCause);
        txMsg.data8[1] = nResetCause;

        PostTxFrame(&txMsg);
    }

    #if HAS_EXT_MEMORY && (NUM_OUTPUTS > 0)
    // Fault log entry n (0 = newest), two frames:
    // [53, n, count, output | kind << 4, boot count (u16), current x10 (u16)]
    // [53, n | 0x80, uptime ms (u32), battery V x100 (u16)]
    if ((frame->DLC == 8) &&
        (frame->data8[0] == static_cast<uint8_t>(MsgCmd::FaultLogRead)))
    {
        FaultLogEntry stEntry;
        uint8_t n = frame->data8[1] & 0x7F;
        uint8_t nCount = ReadFaultLog(n, &stEntry);

        CANTxFrame txMsg = {};
        txMsg.SID = stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET;
        txMsg.IDE = CAN_IDE_STD;
        txMsg.DLC = 8;
        txMsg.data8[0] = static_cast<uint8_t>(MsgCmd::FaultLogRead);
        txMsg.data8[1] = n;
        txMsg.data8[2] = nCount;
        txMsg.data8[3] = stEntry.nOutput | (static_cast<uint8_t>(stEntry.eKind) << 4);
        txMsg.data8[4] = stEntry.nBootCount & 0xFF;
        txMsg.data8[5] = stEntry.nBootCount >> 8;
        txMsg.data8[6] = stEntry.nCurrent & 0xFF;
        txMsg.data8[7] = stEntry.nCurrent >> 8;
        PostTxFrame(&txMsg);

        txMsg.data8[1] = n | 0x80;
        txMsg.data8[2] = stEntry.nUptimeMs & 0xFF;
        txMsg.data8[3] = (stEntry.nUptimeMs >> 8) & 0xFF;
        txMsg.data8[4] = (stEntry.nUptimeMs >> 16) & 0xFF;
        txMsg.data8[5] = stEntry.nUptimeMs >> 24;
        txMsg.data8[6] = stEntry.nBattVolt & 0xFF;
        txMsg.data8[7] = stEntry.nBattVolt >> 8;
        PostTxFrame(&txMsg);
    }

    // Clear the fault log, reply [54, 1]
    if ((frame->DLC == 8) &&
        (frame->data8[0] == static_cast<uint8_t>(MsgCmd::FaultLogClear)))
    {
        ClearFaultLog();

        CANTxFrame txMsg = {};
        txMsg.SID = stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET;
        txMsg.IDE = CAN_IDE_STD;
        txMsg.DLC = 8;
        txMsg.data8[0] = static_cast<uint8_t>(MsgCmd::FaultLogClear);
        txMsg.data8[1] = 1;
        PostTxFrame(&txMsg);
    }
    #endif
}