#include "can.h"
#include "hal.h"
#include "port.h"
#include "device_config.h"
#include "mailbox.h"
#include "msg.h"
#include "param_protocol.h"

#include <iterator>

static volatile uint32_t nLastCanRxTime;
// While set only replies to dingoConfig are sent, see SetCanTxQuiet()
static volatile bool bTxQuiet;

// No peer ACKing (alone on the bus, or the bus is down): bxCAN retransmits each unacked frame at line rate.
// After a few TX timeouts in a row abort the mailboxes and send to USB only for a while, then re-probe.
#define CAN_NOACK_FAIL_STREAK 3
#define CAN_NOACK_BACKOFF_CYCLES 50 // cyclic rounds, 50 x CAN_TX_CYCLIC_MSG_DELAY
// Only with nothing heard this long: on a bus with error frames TX timeouts also come with peers there, and the
// back-off took a module off the bus for 5.1 s at a time
#define CAN_NOACK_QUIET_MS 500
static volatile uint16_t nNoAckBackoff;

extern DeviceConfig stConfig;

static THD_WORKING_AREA(waCanCyclicTxThread, CAN_THREAD_STACK);
void CanCyclicTxThread(void *)
{
    chRegSetThreadName("CAN Cyclic Tx");

    CANTxMsg msg;

    while (1)
    {
        // Counts down to the round that goes to CAN again and re-probes for a peer
        if (nNoAckBackoff > 0)
            nNoAckBackoff--;

        if (!IsParamOpInProgress() && !bTxQuiet && !CanTxMuted())
        {
            for (uint8_t i = 0; i < NUM_TX_MSGS; i++)
            {
                msg = TxMsgs[i]();
                if (!msg.bSend)
                    continue;
                msg.frame.IDE = CAN_IDE_STD;
                msg.frame.RTR = CAN_RTR_DATA;
                PostTxFrame(&msg.frame);
            }
        }

        if (chThdShouldTerminateX())
            chThdExit(MSG_OK);

        chThdSleepMilliseconds(CAN_TX_CYCLIC_MSG_DELAY);
    }
}

static THD_WORKING_AREA(waCanTxThread, CAN_THREAD_STACK);
void CanTxThread(void *)
{
    chRegSetThreadName("CAN Tx");

    CANTxFrame msg;
    uint8_t nTxFails = 0;

    while (1)
    {
        // Blocks until a frame is queued, no polling while idle
        // Timeout only so the thread can check for termination
        // Frames queued before a back-off already went to USB, don't feed them to a bus nobody ACKs
        if (FetchTxFrame(&msg, TIME_MS2I(100)) == MSG_OK && !CanNoPeer())
        {
            // IDE is set by each sender (CAN outputs can be extended), RTR isn't always initialized
            msg.RTR = CAN_RTR_DATA;

            const bool bConfigReply = (msg.IDE == CAN_IDE_STD) &&
                                      (msg.SID == stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET);

            if (!bTxQuiet || bConfigReply)
            {
                if (canTransmitTimeout(&CAND1, CAN_ANY_MAILBOX, &msg, TIME_MS2I(10)) == MSG_OK)
                    nTxFails = 0;
                else if (++nTxFails >= CAN_NOACK_FAIL_STREAK &&
                         static_cast<int32_t>(SYS_TIME - nLastCanRxTime) > CAN_NOACK_QUIET_MS)
                {
                    // Every TX mailbox stuck retransmitting: abort them (no-op in ChibiOS' FDCAN driver)
                    for (canmbx_t nMbx = 1; nMbx <= CAN_TX_MAILBOXES; nMbx++)
                        canTryAbortX(&CAND1, nMbx);
                    nNoAckBackoff = CAN_NOACK_BACKOFF_CYCLES;
                    nTxFails = 0;
                }
            }

            // Pacing between frames, set CAN_TX_MSG_SPLIT to 0 to send at bus rate
            #if CAN_TX_MSG_SPLIT > 0
            chThdSleepMicroseconds(CAN_TX_MSG_SPLIT);
            #endif
        }

        if (chThdShouldTerminateX())
            chThdExit(MSG_OK);
    }
}

static THD_WORKING_AREA(waCanRxThread, CAN_THREAD_STACK);
void CanRxThread(void *)
{
    CANRxFrame msg;

    CANTxFrame usbTx;

    chRegSetThreadName("CAN Rx");

    while (true)
    {

        msg_t res = canReceiveTimeout(&CAND1, CAN_ANY_MAILBOX, &msg, TIME_MS2I(100));
        if (res == MSG_OK)
        {
            // Bytes past the DLC are left over from an earlier frame in the mailbox: a CAN input reading them
            // (a short frame, a bit past its length) saw stale values and could act on them
            for (uint8_t i = (msg.DLC < 8) ? msg.DLC : 8; i < 8; i++)
                msg.data8[i] = 0;

            nLastCanRxTime = SYS_TIME;

            // Config frames go straight to the param thread
            if (!RouteParamFrame(&msg))
                res = PostRxFrame(&msg);

            if(stConfig.stDevice.bConnectUsbToCan)
            {
                //Copy data to USB for data pass through
                //Don't send if it's a settings msg for this device
                if((msg.SID != stConfig.stDevice.nBaseId + CONFIG_RX_OFFSET) && 
                   (msg.SID != stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET)) 
                {
                    //If USB not connected, mailbox will fill up and messages will be dropped
                    usbTx.SID = msg.SID;
                    usbTx.IDE = msg.IDE;
                    usbTx.DLC = msg.DLC;
                    for(size_t i = 0; i < msg.DLC; i++)
                        usbTx.data8[i] = msg.data8[i];
                    res = PostTxUsbFrame(&usbTx);
                }
            }
        }

        if (chThdShouldTerminateX())
            chThdExit(MSG_OK);
    }
}

static thread_t *canCyclicTxThreadRef;
static thread_t *canTxThreadRef;
static thread_t *canRxThreadRef;

msg_t InitCan(Config_Device *conf)
{
    if (canCyclicTxThreadRef || canTxThreadRef || canRxThreadRef)
    {
        StopCan();
    }

    msg_t ret = canStart(&CAND1, &GetCanConfig(conf->eCanSpeed));
    if (ret != HAL_RET_SUCCESS)
        return ret;

    UpdateCanFilters();

    canCyclicTxThreadRef = chThdCreateStatic(waCanCyclicTxThread, sizeof(waCanCyclicTxThread), NORMALPRIO + 1, CanCyclicTxThread, nullptr);
    canTxThreadRef = chThdCreateStatic(waCanTxThread, sizeof(waCanTxThread), NORMALPRIO + 1, CanTxThread, nullptr);
    // bxCAN keeps only 3 received frames: the RX thread runs above the TX and USB threads so a burst is not lost
    canRxThreadRef = chThdCreateStatic(waCanRxThread, sizeof(waCanRxThread), NORMALPRIO + 2, CanRxThread, nullptr);

    return HAL_RET_SUCCESS;
}

void StopCan()
{
    // Signal threads to terminate
    chThdTerminate(canCyclicTxThreadRef);
    chThdTerminate(canTxThreadRef);
    chThdTerminate(canRxThreadRef);

    // Wait for threads to exit
    chThdWait(canCyclicTxThreadRef);
    chThdWait(canTxThreadRef);
    chThdWait(canRxThreadRef);

    // Stop CAN driver
    canStop(&CAND1);

    // Reset thread references
    canCyclicTxThreadRef = NULL;
    canTxThreadRef = NULL;
    canRxThreadRef = NULL;
}

//=============================================================================
// Hardware filters
// Rebuilt from the live config and written straight to the bxCAN filter
// registers. The HAL's canSTM32SetFilters() needs the driver stopped (and turns
// the CAN clock off), this works while running: reception only pauses for the
// few us spent in filter init mode.
//=============================================================================

#define MAX_FILTER_IDS (STM32_CAN_MAX_FILTERS * 2) // 32-bit list mode, 2 IDs per bank

static uint32_t FilterReg(uint32_t nId, bool bExtended)
{
    // FR layout: STID[31:21] or EXID[31:3], IDE bit 2
    return bExtended ? ((nId << 3) | 0x04) : (nId << 21);
}

static void WriteFilterRegs(const uint32_t *pIds, uint8_t nNumIds)
{
    CAN_TypeDef *can = CAND1.can;
    uint8_t nBanks = (nNumIds + 1) / 2;

    can->FMR |= CAN_FMR_FINIT;
    #if STM32_HAS_CAN2
    // HAL splits the banks 50/50 with CAN2, CAN2 is unused so give CAN1 all of them
    can->FMR = (can->FMR & ~CAN_FMR_CAN2SB) | (STM32_CAN_MAX_FILTERS << CAN_FMR_CAN2SB_Pos);
    #endif
    can->FA1R = 0;
    can->FFA1R = 0; // All banks to FIFO 0

    if (nNumIds == 0)
    {
        // Filtering disabled - bank 0, 32-bit mask mode, mask 0 = accept everything
        can->FM1R = 0;
        can->FS1R = 1;
        can->sFilterRegister[0].FR1 = 0;
        can->sFilterRegister[0].FR2 = 0;
        nBanks = 1;
    }
    else
    {
        // 32-bit list mode, odd count repeats the last ID in the spare slot
        for (uint8_t b = 0; b < nBanks; b++)
        {
            can->sFilterRegister[b].FR1 = pIds[b * 2];
            can->sFilterRegister[b].FR2 = (b * 2 + 1 < nNumIds) ? pIds[b * 2 + 1] : pIds[b * 2];
        }
        can->FM1R = (1UL << nBanks) - 1;
        can->FS1R = (1UL << nBanks) - 1;
    }

    can->FA1R = (1UL << nBanks) - 1;
    can->FMR &= ~CAN_FMR_FINIT;
}

void UpdateCanFilters()
{
    // Registers are only clocked while the driver is running, InitCan() calls this after canStart()
    if (CAND1.state != CAN_READY)
        return;

    uint32_t nIds[MAX_FILTER_IDS];
    uint8_t nNumIds = 0;
    bool bOverflow = false;

    auto Add = [&](uint32_t nId, bool bExtended) {
        if (nNumIds < MAX_FILTER_IDS)
            nIds[nNumIds++] = FilterReg(nId, bExtended);
        else
            bOverflow = true;
    };

    if (stConfig.stDevice.bCanFilterEnabled)
    {
        // Config/request frames from dingoConfig
        Add(stConfig.stDevice.nBaseId + CONFIG_RX_OFFSET, false);

        for (uint8_t i = 0; i < NUM_CAN_INPUTS; i++)
        {
            if (stConfig.stCanInput[i].bEnabled)
                Add(stConfig.stCanInput[i].nID, stConfig.stCanInput[i].nIDE == 1);
        }

        #if NUM_KEYPADS > 0
        for (uint8_t i = 0; i < NUM_KEYPADS; i++)
        {
            const Config_Keypad &kp = stConfig.stKeypad[i];
            if (!kp.bEnabled)
                continue;

            // Both brands send button state on the CANopen TPDO1 ID
            Add(kp.nNodeId + static_cast<uint16_t>(BlinkMarineMessageId::ButtonState), false);

            if (kp.eModel < KeypadModel::Grayhill6Key)
            {
                Add(kp.nNodeId + static_cast<uint16_t>(BlinkMarineMessageId::DialState1), false);
                Add(kp.nNodeId + static_cast<uint16_t>(BlinkMarineMessageId::DialState2), false);
                Add(kp.nNodeId + static_cast<uint16_t>(BlinkMarineMessageId::AnalogInput), false);
            }
        }
        #endif
    }

    // Filtering disabled, or more IDs than filter slots - accept everything rather than drop frames
    if (bOverflow)
        nNumIds = 0;

    WriteFilterRegs(nIds, nNumIds);
}

// Before sleep every device on the bus has to stop transmitting, a frame from
// one that is still awake wakes the others straight back up. Covers the cyclic
// status, CAN outputs, CAN messages and keypad frames alike, as they all go
// through CanTxThread. Frames queued while quiet are dropped, not delayed.
void SetCanTxQuiet(bool bQuiet)
{
    bTxQuiet = bQuiet;
}

uint32_t GetLastCanRxTime()
{
    return nLastCanRxTime;
}

bool CanTxMuted()
{
    return *pVarMap[stConfig.stDevice.nMuteCanTxInput];
}

bool CanNoPeer()
{
    return nNoAckBackoff > 0;
}