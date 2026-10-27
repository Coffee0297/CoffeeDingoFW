#pragma once

#include <cstdint>
#include "hal.h"

msg_t PostTxFrame(CANTxFrame *frame, uint8_t nBus = 0);
msg_t PostTxUsbFrame(CANTxFrame *frame);
msg_t FetchTxFrame(CANTxFrame *frame, uint8_t nBus, sysinterval_t timeout = TIME_IMMEDIATE);
msg_t FetchTxUsbFrame(CANTxFrame *frame, sysinterval_t timeout = TIME_IMMEDIATE);
msg_t PostRxFrame(CANRxFrame *frame, uint8_t nBus = 0);
msg_t FetchRxFrame(CANRxFrame *frame, uint8_t *pBus);
