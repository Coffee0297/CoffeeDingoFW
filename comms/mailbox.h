#pragma once

#include <cstdint>
#include "hal.h"

msg_t PostTxFrame(CANTxFrame *frame);
msg_t PostTxUsbFrame(CANTxFrame *frame);
// timeout: how long to wait for a frame (the TX threads block here instead of polling)
msg_t FetchTxFrame(CANTxFrame *frame, sysinterval_t timeout = TIME_IMMEDIATE);
msg_t FetchTxUsbFrame(CANTxFrame *frame, sysinterval_t timeout = TIME_IMMEDIATE);
msg_t PostRxFrame(CANRxFrame *frame);
msg_t FetchRxFrame(CANRxFrame *frame);
bool RxFramesEmpty();