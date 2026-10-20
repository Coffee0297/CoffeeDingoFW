// Host-test stand-in for a board port.h (no ChibiOS): just what the pure logic blocks use.
#pragma once
#include <cstdint>
#include "enums.h"
#define VAR_MAP_SIZE 8
#define NUM_TX_MSGS 31
extern uint32_t nHostTimeMs;
#define SYS_TIME nHostTimeMs
