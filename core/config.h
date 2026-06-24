#pragma once

#include <cstdint>
#include "port.h"
#include "enums.h"
#include "dbc.h"
#include "virtual_input.h"
#include "flasher.h"
#include "can_input.h"
#include "can_output.h"
#include "counter.h"
#include "condition.h"
#if NUM_OUTPUTS > 0
#include "profet.h"
#endif
#if HAS_WIPERS > 0
#include "wiper.h"
#endif
#if HAS_STARTER_DISABLE > 0
#include "starter.h"
#endif
#if NUM_KEYPADS > 0
#include "keypad.h"
#endif
#if NUM_DIG_INPUTS > 0
#include "digital_input.h"
#endif
#if NUM_DIG_OUTPUTS > 0
#include "digital_output.h"
#endif
#if NUM_ANALOG_INPUTS > 0
#include "analog_input.h"
#endif  

#define CONFIG_VERSION 0x000C //Increment when config structure changes

struct Config_Device{
  uint16_t nConfigVersion;
  uint16_t nBaseId;
  CanBitrate eCanSpeed;
  bool bCanFilterEnabled;

  //Not used by all devices
  bool bSleepEnabled;
  bool bConnectUsbToCan;
  uint16_t nSleepTimeoutMs;     // auto-sleep delay after outputs off / USB out / CAN idle (was hardcoded SLEEP_TIMEOUT)

  // Expanded sleep (FW #52)
  bool bSleepInputEnabled;      // a digital input directly controls sleep (ignores CAN/outputs restrictions)
  uint16_t nSleepInput;         // 1-based digital input that drives sleep (0 = none)
  bool bSleepInputActiveHigh;   // input level that means "go to sleep" (true=high, false=low)
  bool bSleepIgnoreAlwaysOn;    // ignore always-on outputs when checking "no outputs on" (else sleep never happens)
};

#if HAS_LUA
// One assembled Lua program (the tool concatenates all per-function snippets).
struct Config_Lua{
  bool     bEnabled;
  uint16_t nLength;                 // bytes of source in acScript
  char     acScript[LUA_SCRIPT_MAX];
};
#endif

struct DeviceConfig{
  Config_Device stDevice;
  Config_VirtualInput stVirtualInput[NUM_VIRT_INPUTS];
  Config_Flasher stFlasher[NUM_FLASHERS];
  Config_CanInput stCanInput[NUM_CAN_INPUTS];
  Config_CanOutput stCanOutput[NUM_CAN_OUTPUTS];
  Config_Counter stCounter[NUM_COUNTERS];
  Config_Condition stCondition[NUM_CONDITIONS];

  #if NUM_DIG_INPUTS > 0
  Config_DigInput stDigInput[NUM_DIG_INPUTS];
  #endif
  #if NUM_OUTPUTS > 0
  Config_Output stOutput[NUM_OUTPUTS];
  #endif
  #if HAS_WIPERS 
  Config_Wiper stWiper;
  #endif
  #if HAS_STARTER_DISABLE
  Config_Starter stStarter;
  #endif
  #if NUM_KEYPADS > 0
  Config_Keypad stKeypad[NUM_KEYPADS];
  #endif
  #if NUM_DIG_OUTPUTS > 0
  Config_DigOutput stDigOutput[NUM_DIG_OUTPUTS];
  #endif
  #if NUM_ANALOG_INPUTS > 0
  Config_AnalogInput stAnalogInput[NUM_ANALOG_INPUTS];
  #endif
  #if HAS_LUA
  Config_Lua stLua;
  #endif
};

extern DeviceConfig stConfig;
extern DeviceConfig stConfigTemp; // Used for staging new config before applying

void InitConfig();
bool WriteConfig();