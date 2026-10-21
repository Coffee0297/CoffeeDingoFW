#pragma once

#include <cstdint>
#include "hal.h"
#include "port.h"

#if HAS_LUA
struct Config_Lua{
  uint16_t nLength; // 0 = no program
  char acScript[LUA_SCRIPT_MAX];
};

extern float fLuaOut[NUM_LUA_OUTPUTS]; // var map, written by setLuaOut(n, v)

void InitLua();                       // starts the Lua thread, which loads the stored program
void ReloadLua();                     // after an upload: load the stored program again
void LuaCanRx(const CANRxFrame &frame); // from the device thread, every received frame
const char *GetLuaError();            // last load or run error, "" if none
#endif
