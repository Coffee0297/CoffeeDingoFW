#pragma once

#include "enums.h"
#include "config.h"
#include "status.h"

class Digital;
class CanInput;
class CanOutputs;
class VirtualInput;
class Profet;
class Wiper;
class Starter;
class Flasher;
class Counter;
class Condition;
class Keypad;

extern Digital in[NUM_INPUTS];
extern CanInput canIn[NUM_CAN_INPUTS];
extern CanOutputs canOutputs;
extern VirtualInput virtIn[NUM_VIRT_INPUTS];
extern Profet pf[NUM_OUTPUTS];
extern Wiper wiper;
extern Starter starter;
extern Flasher flasher[NUM_FLASHERS];
extern Counter counter[NUM_COUNTERS];
extern Condition condition[NUM_CONDITIONS];
extern Keypad keypad[NUM_KEYPADS];

extern PdmConfig stConfig;
extern PdmConfig stConfigTemp; // Used for staging new config before applying
extern float *pVarMap[VAR_MAP_SIZE];
extern DeviceState eState;
extern float fTempSensor;
extern float fBattVolt;
extern bool bSleepRequest;

void CheckBootloaderRequest();
void InitDevice();