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
#include "timer.h"
#include "math_block.h"
#if HAS_COMFORT_BLINK
#include "comfort_blink.h"
#endif
#include "curve.h"
#include "latch_gate.h"
#include "pid.h"
#include "table.h"
#include "can_msg.h"
#if HAS_IGNITION
#include "ignition.h"
#endif
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

#define CONFIG_VERSION 0x000B //Increment when config structure changes

struct Config_Device{
  uint16_t nConfigVersion;
  uint16_t nBaseId;
  CanBitrate eCanSpeed;
  bool bCanFilterEnabled;

  //Not used by all devices
  bool bSleepEnabled;
  bool bDisableDigInWake;
  bool bDisableCanWake;
  bool bConnectUsbToCan;
  uint16_t nMuteCanTxInput;
  uint16_t nForceSleepInput;
  uint16_t nRetryAllInput;
  uint16_t nShedInput;
  uint16_t nSleepTimeoutMs;
  bool bSleepIgnoreAlwaysOn;  // outputs on ALWAYS_TRUE (var 1) don't keep the module awake
  uint8_t nWakeDigInputMask;  // bit i = digital input i+1 wakes (unless bDisableDigInWake)
};

// While enabled, only this frame (and dingoConfig's) keeps the device awake
struct Config_WakeFrame{
  bool bEnabled;
  uint32_t nId;
  uint8_t nIde; //0=STD, 1=EXT
  uint8_t nMinLen;
  uint32_t nMaskLo; //data bytes 0-3, byte 0 in the low byte
  uint32_t nMaskHi; //data bytes 4-7
  uint32_t nValueLo;
  uint32_t nValueHi;
};

struct DeviceConfig{
  Config_Device stDevice;
  #if CAN_SLEEP
  Config_WakeFrame stWakeFrame;
  #endif
  Config_VirtualInput stVirtualInput[NUM_VIRT_INPUTS];
  Config_Flasher stFlasher[NUM_FLASHERS];
  Config_CanInput stCanInput[NUM_CAN_INPUTS];
  Config_CanOutput stCanOutput[NUM_CAN_OUTPUTS];
  Config_Counter stCounter[NUM_COUNTERS];
  Config_Condition stCondition[NUM_CONDITIONS];
  #if NUM_TIMERS > 0
  Config_Timer stTimer[NUM_TIMERS];
  #endif
  #if NUM_MATH > 0
  Config_Math stMath[NUM_MATH];
  #endif
  #if NUM_CURVES > 0
  Config_Curve stCurve[NUM_CURVES];
  #endif
  #if NUM_LATCH_GATES > 0
  Config_LatchGate stLatchGate[NUM_LATCH_GATES];
  #endif
  #if NUM_PIDS > 0
  Config_Pid stPid[NUM_PIDS];
  #endif
  #if NUM_TABLES > 0
  Config_Table stTable[NUM_TABLES];
  #endif
  #if NUM_CAN_MSGS > 0
  Config_CanMsg stCanMsg[NUM_CAN_MSGS];
  #endif

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
  #if HAS_COMFORT_BLINK
  Config_ComfortBlink stComfortBlink;
  #endif
  #if HAS_IGNITION
  Config_Ignition stIgnition;
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
};

// Without external memory the config and its CRC32 live in one flash sector: fail the build, not the flash write
#if !HAS_EXT_MEMORY
static_assert(sizeof(DeviceConfig) + sizeof(uint32_t) <= CONFIG_SECTOR_SIZE, "DeviceConfig no longer fits its flash sector");
#endif

extern DeviceConfig stConfig;
extern DeviceConfig stConfigTemp; // Used for staging new config before applying

void InitConfig();
bool WriteConfig();