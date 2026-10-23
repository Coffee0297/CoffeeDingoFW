// Host-test stand-in for ChibiOS hal.h: only the CAN frame the function blocks read.
#pragma once
#include <cstdint>
struct CANRxFrame { uint8_t DLC; uint8_t RTR; uint8_t IDE; uint32_t SID; uint32_t EID; uint8_t data8[8]; };
// Host-test stand-in for ChibiOS hal.h: only the CAN frame the function blocks read.
#pragma once
#include <cstdint>
struct CANRxFrame { uint8_t DLC; uint8_t RTR; uint8_t IDE; uint32_t SID; uint32_t EID; uint8_t data8[8]; };
#define CAN_IDE_STD 0
#define CAN_IDE_EXT 1
