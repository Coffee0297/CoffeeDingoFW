#include "mailbox.h"
#include "can.h"
#include "ch.hpp"
#include "port.h"

// The RX side can be deeper than TX: a busy bus delivers frames in bursts faster than the device thread
// (2 ms loop) drains them. Boards short on SRAM put the RX buffers in CCM (CPU-only, which they are).
#ifndef RX_MAILBOX_SIZE
#define RX_MAILBOX_SIZE MAILBOX_SIZE
#endif
#ifndef CCM_BSS
#define CCM_BSS
#endif
static chibios_rt::Mailbox<CANRxFrame*, RX_MAILBOX_SIZE> rxMb CCM_BSS;   // the constructor initialises it
static chibios_rt::Mailbox<CANTxFrame*, MAILBOX_SIZE> txMb;
#if HAS_USB
static chibios_rt::Mailbox<CANTxFrame*, MAILBOX_SIZE> txUsbMb;
#endif

//Mailbox buffer of CAN frames
//Not managed by mailbox
CANRxFrame rxFrames[RX_MAILBOX_SIZE] CCM_BSS;   // contents only valid while rxMsgUsed[i]
CANTxFrame txFrames[MAILBOX_SIZE];
#if HAS_USB
CANTxFrame txUsbFrames[MAILBOX_SIZE];   // a board without USB has nobody to fetch them
#endif

//Used to manage the memory used by the mailbox
bool rxMsgUsed[RX_MAILBOX_SIZE];   // zeroed .bss: must not live in NOLOAD CCM
bool txMsgUsed[MAILBOX_SIZE];
#if HAS_USB
bool txUsbMsgUsed[MAILBOX_SIZE];
#endif

// Mutexes protecting each *MsgUsed array from concurrent access between
// Post* (CanRxThread/main thread) and Fetch* (CanTxThread/main thread).
// Priority inheritance ensures no priority inversion across thread priorities.
static chibios_rt::Mutex rxMutex;
static chibios_rt::Mutex txMutex;
#if HAS_USB
static chibios_rt::Mutex txUsbMutex;
#endif

msg_t PostTxFrame(CANTxFrame *frame)
{
    // No CAN peer is ACKing (can.cpp backs off): a CAN frame would only sit retransmitting in a full queue, and
    // senders that retry until it's taken (the ReadAll dump) would stall. The USB host still needs it —
    // dingoConfig over USB with the module alone on a bench — so hand it to USB once and report it sent.
    if (CanNoPeer())
        return PostTxUsbFrame(frame), MSG_OK;
    txMutex.lock();
    for (int i = 0; i < MAILBOX_SIZE; i++) {
        if (!txMsgUsed[i]) {
            txFrames[i] = *frame;
            txMsgUsed[i] = true;

            msg_t result = txMb.post(&txFrames[i], TIME_IMMEDIATE);
            if (result != MSG_OK) {
                txMsgUsed[i] = false;
                txMutex.unlock();
                return result;
            }
            txMutex.unlock();
            PostTxUsbFrame(frame);  // Only post to USB after successful CAN TX post
            return result;
        }
    }

    txMutex.unlock();
    return MSG_TIMEOUT;  // No free slots
}

msg_t FetchTxFrame(CANTxFrame *frame, sysinterval_t timeout)
{
    CANTxFrame *txFrame;
    msg_t result = txMb.fetch(&txFrame, timeout);
    if (result == MSG_OK) {
        txMutex.lock();
        for (int i = 0; i < MAILBOX_SIZE; i++) {
            if (txFrame == &txFrames[i]) {
                txMsgUsed[i] = false;
                break;
            }
        }
        txMutex.unlock();
        *frame = *txFrame;
    }
    return result;
}

msg_t PostTxUsbFrame(CANTxFrame *frame)
{
#if !HAS_USB
    (void)frame;
    return MSG_OK;   // no USB link: nothing to queue
#else
    txUsbMutex.lock();
    for (int i = 0; i < MAILBOX_SIZE; i++) {
        if (!txUsbMsgUsed[i]) {
            txUsbFrames[i] = *frame;
            txUsbMsgUsed[i] = true;

            msg_t result = txUsbMb.post(&txUsbFrames[i], TIME_IMMEDIATE);
            if (result != MSG_OK)
                txUsbMsgUsed[i] = false;
            txUsbMutex.unlock();
            return result;
        }
    }

    txUsbMutex.unlock();
    return MSG_TIMEOUT;  // No free slots
#endif
}

msg_t FetchTxUsbFrame(CANTxFrame *frame, sysinterval_t timeout)
{
#if !HAS_USB
    (void)frame; (void)timeout;
    return MSG_TIMEOUT;
#else
    CANTxFrame *txFrame;
    msg_t result = txUsbMb.fetch(&txFrame, timeout);
    if (result == MSG_OK) {
        txUsbMutex.lock();
        for (int i = 0; i < MAILBOX_SIZE; i++) {
            if (txFrame == &txUsbFrames[i]) {
                txUsbMsgUsed[i] = false;
                break;
            }
        }
        txUsbMutex.unlock();
        *frame = *txFrame;
    }
    return result;
#endif
}

msg_t PostRxFrame(CANRxFrame *frame)
{
    rxMutex.lock();
    for (int i = 0; i < RX_MAILBOX_SIZE; i++) {
        if (!rxMsgUsed[i]) {
            rxFrames[i] = *frame;
            rxMsgUsed[i] = true;

            msg_t result = rxMb.post(&rxFrames[i], TIME_IMMEDIATE);
            if (result != MSG_OK)
                rxMsgUsed[i] = false;
            rxMutex.unlock();
            return result;
        }
    }

    rxMutex.unlock();
    return MSG_TIMEOUT;  // No free slots
}   

msg_t FetchRxFrame(CANRxFrame *frame)
{
    CANRxFrame *rxFrame;
    msg_t result = rxMb.fetch(&rxFrame, TIME_IMMEDIATE);
    if (result == MSG_OK) {
        rxMutex.lock();
        for (int i = 0; i < RX_MAILBOX_SIZE; i++) {
            if (rxFrame == &rxFrames[i]) {
                rxMsgUsed[i] = false;
                break;
            }
        }
        rxMutex.unlock();
        *frame = *rxFrame;
    }
    return result;
}

bool RxFramesEmpty()
{
    return (rxMb.getUsedCountI() == 0);
}
