#pragma once

#include "hal.h"

#if HAS_USB
msg_t InitUsb();
bool GetUsbConnected();
#endif