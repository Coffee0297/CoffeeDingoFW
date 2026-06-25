#include "request_msg.h"
#include "device.h"
#include "can.h"
#include "config.h"
#include "config_handler.h"
#include "mcu_utils.h"
#include "device_config.h"
#include "mailbox.h"
#include "enums.h"

// External variables from pdm.cpp that we need access to
extern DeviceConfig stConfig;
extern bool bSleepRequest;
#if HAS_LUA
#include "lua_port.h"
extern volatile bool gLuaReload;   // set here on upload-complete; LuaThread reloads
#endif
#if NUM_OUTPUTS > 0
#include "overload_log.h"
#endif

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
        txMsg.DLC = 2;
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

        // Apply CAN filter / bitrate changes at runtime: DeviceThread re-runs InitCan()
        // shortly after this reply is sent, so no power cycle is needed.
        gReinitCanRequested = true;
    }

    // Check for bootloader request (all boards). The "BOOTL" signature guards against
    // accidental entry. The handler is board-specific: on the CANBoard, RequestBootloader()
    // enters the OpenBLT CAN bootloader (boards/cortex-m3/mcu_utils.cpp). On the PDMs (HAS_USB)
    // byte 6 selects the path: 0 (default) = USB-DFU system bootloader, 1 = OpenBLT CAN update
    // of the relocated app (boards/cortex-m4/mcu_utils.cpp).
    if ((frame->DLC == 8) &&
        (frame->data8[0] == static_cast<uint8_t>(MsgCmd::Bootloader)) &&
        (frame->data8[1] == 'B') && (frame->data8[2] == 'O') &&
        (frame->data8[3] == 'O') && (frame->data8[4] == 'T') && (frame->data8[5] == 'L'))
    {
        #if HAS_USB
        if (frame->data8[6] == 1)
            RequestBootloaderCan(stConfig.stDevice.nBaseId,
                                 static_cast<uint8_t>(stConfig.stDevice.eCanSpeed));
        else
            RequestBootloader();
        #else
        RequestBootloader();
        #endif
    }

    #if HAS_LUA
    // ---- Lua program upload (chunked) -------------------------------------
    // LuaWrite:        [cmd, offHi, offLo, b0, b1, b2, b3, b4]  (5 source bytes)
    // LuaWriteComplete:[cmd, lenHi, lenLo]  -> set length, persist, reload
    // LuaRead:         [cmd, offHi, offLo]  -> reply 5 bytes at offset
    if (frame->DLC == 8 && frame->data8[0] == static_cast<uint8_t>(MsgCmd::LuaWrite))
    {
        uint16_t off = (uint16_t)((frame->data8[1] << 8) | frame->data8[2]);
        for (uint8_t i = 0; i < 5; i++) {
            uint16_t o = (uint16_t)(off + i);
            if (o < LUA_SCRIPT_MAX) stConfig.stLua.acScript[o] = (char)frame->data8[3 + i];
        }
        CANTxFrame tx; tx.SID = stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET;
        tx.IDE = CAN_IDE_STD; tx.DLC = 3;
        tx.data8[0] = static_cast<uint8_t>(MsgCmd::LuaWrite);
        tx.data8[1] = frame->data8[1]; tx.data8[2] = frame->data8[2];
        PostTxFrame(&tx);
    }
    else if (frame->DLC == 8 && frame->data8[0] == static_cast<uint8_t>(MsgCmd::LuaWriteComplete))
    {
        uint16_t len = (uint16_t)((frame->data8[1] << 8) | frame->data8[2]);
        if (len >= LUA_SCRIPT_MAX) len = LUA_SCRIPT_MAX - 1;
        stConfig.stLua.nLength = len;
        stConfig.stLua.acScript[len] = '\0';
        stConfig.stLua.bEnabled = (len > 0);
        bool ok = WriteConfig();          // persist to FRAM/flash
        gLuaReload = true;                // LuaThread recompiles next cycle
        CANTxFrame tx; tx.SID = stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET;
        tx.IDE = CAN_IDE_STD; tx.DLC = 4;
        tx.data8[0] = static_cast<uint8_t>(MsgCmd::LuaWriteComplete);
        tx.data8[1] = frame->data8[1]; tx.data8[2] = frame->data8[2];
        tx.data8[3] = ok ? 1 : 0;
        PostTxFrame(&tx);
    }
    else if (frame->DLC == 8 && frame->data8[0] == static_cast<uint8_t>(MsgCmd::LuaRead))
    {
        uint16_t off = (uint16_t)((frame->data8[1] << 8) | frame->data8[2]);
        CANTxFrame tx; tx.SID = stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET;
        tx.IDE = CAN_IDE_STD; tx.DLC = 8;
        tx.data8[0] = static_cast<uint8_t>(MsgCmd::LuaRead);
        tx.data8[1] = frame->data8[1]; tx.data8[2] = frame->data8[2];
        for (uint8_t i = 0; i < 5; i++) {
            uint16_t o = (uint16_t)(off + i);
            tx.data8[3 + i] = (o < LUA_SCRIPT_MAX) ? (uint8_t)stConfig.stLua.acScript[o] : 0;
        }
        PostTxFrame(&tx);
    }
    else if (frame->DLC == 8 && frame->data8[0] == static_cast<uint8_t>(MsgCmd::LuaErr))
    {
        const char *err = LuaLastError();
        uint16_t len = 0; while (err[len]) len++;
        uint16_t off = (uint16_t)((frame->data8[1] << 8) | frame->data8[2]);
        CANTxFrame tx; tx.SID = stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET;
        tx.IDE = CAN_IDE_STD; tx.DLC = 8;
        tx.data8[0] = static_cast<uint8_t>(MsgCmd::LuaErr);
        tx.data8[1] = frame->data8[1]; tx.data8[2] = frame->data8[2];
        for (uint8_t i = 0; i < 5; i++) {
            uint16_t o = (uint16_t)(off + i);
            tx.data8[3 + i] = (o < len) ? (uint8_t)err[o] : 0;
        }
        PostTxFrame(&tx);
    }
    #endif // HAS_LUA

    #if NUM_OUTPUTS > 0
    // Overload-log read-back (see MsgCmd OvlCount/OvlHeader/OvlData/OvlClear)
    if (frame->DLC == 8 && frame->data8[0] == static_cast<uint8_t>(MsgCmd::OvlCount))
    {
        CANTxFrame tx; tx.SID = stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET;
        tx.IDE = CAN_IDE_STD; tx.DLC = 2;
        tx.data8[0] = static_cast<uint8_t>(MsgCmd::OvlCount);
        tx.data8[1] = OvlLogCount();
        PostTxFrame(&tx);
    }
    else if (frame->DLC == 8 && frame->data8[0] == static_cast<uint8_t>(MsgCmd::OvlHeader))
    {
        uint8_t idx = frame->data8[1];
        uint8_t nOut = 0, nState = 0; float fPeak = 0, fLimit = 0;
        bool ok = OvlLogGetHeader(idx, nOut, nState, fPeak, fLimit);
        uint16_t nPeak = ok ? (uint16_t)(fPeak * 10.0f) : 0;     // 0.1A
        uint16_t nLimit = ok ? (uint16_t)(fLimit * 10.0f) : 0;
        CANTxFrame tx; tx.SID = stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET;
        tx.IDE = CAN_IDE_STD; tx.DLC = 8;
        tx.data8[0] = static_cast<uint8_t>(MsgCmd::OvlHeader);
        tx.data8[1] = idx;
        tx.data8[2] = ok ? nOut : 0xFF;     // 0xFF = invalid index
        tx.data8[3] = nState;
        tx.data8[4] = nPeak & 0xFF; tx.data8[5] = nPeak >> 8;
        tx.data8[6] = nLimit & 0xFF; tx.data8[7] = nLimit >> 8;
        PostTxFrame(&tx);
    }
    else if (frame->DLC == 8 && frame->data8[0] == static_cast<uint8_t>(MsgCmd::OvlData))
    {
        uint8_t idx = frame->data8[1];
        uint16_t off = (uint16_t)((frame->data8[2] << 8) | frame->data8[3]);
        uint8_t buf[4] = {0, 0, 0, 0};
        OvlLogGetData(idx, off, buf, 4);
        CANTxFrame tx; tx.SID = stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET;
        tx.IDE = CAN_IDE_STD; tx.DLC = 8;
        tx.data8[0] = static_cast<uint8_t>(MsgCmd::OvlData);
        tx.data8[1] = idx; tx.data8[2] = frame->data8[2]; tx.data8[3] = frame->data8[3];
        tx.data8[4] = buf[0]; tx.data8[5] = buf[1]; tx.data8[6] = buf[2]; tx.data8[7] = buf[3];
        PostTxFrame(&tx);
    }
    else if (frame->DLC == 8 && frame->data8[0] == static_cast<uint8_t>(MsgCmd::OvlClear))
    {
        OvlLogClear();
        CANTxFrame tx; tx.SID = stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET;
        tx.IDE = CAN_IDE_STD; tx.DLC = 1;
        tx.data8[0] = static_cast<uint8_t>(MsgCmd::OvlClear);
        PostTxFrame(&tx);
    }
    #endif // NUM_OUTPUTS > 0

    // Check for version request
    if ((frame->DLC == 8) &&
        (frame->data8[0] == static_cast<uint8_t>(MsgCmd::Version)))
    {
        CANTxFrame txMsg;
        txMsg.SID = stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET;
        txMsg.IDE = CAN_IDE_STD;
        txMsg.DLC = 8;
        txMsg.data8[0] = static_cast<uint8_t>(MsgCmd::Version);
        txMsg.data8[1] = BOARD_ID; //device-type id (0=PDM, 1=PDM-MAX, 2=CANBoard) — lets the tool identify the board
        txMsg.data8[2] = 0;
        txMsg.data8[3] = 0;
        txMsg.data8[4] = MAJOR_VERSION;
        txMsg.data8[5] = MINOR_VERSION;
        txMsg.data8[6] = BUILD >> 8;
        txMsg.data8[7] = BUILD & 0xFF;

        PostTxFrame(&txMsg);
    }
}