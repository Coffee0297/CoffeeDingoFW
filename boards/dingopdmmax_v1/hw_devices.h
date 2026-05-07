#pragma once

#include "port.h"
#include "profet.h"
#include "digital.h"
#include "led.h"
#include "hardware/mcp9808.h"

extern Profet pf[NUM_OUTPUTS];
extern Digital in[NUM_INPUTS];
extern Led statusLed;
extern Led errorLed;
extern MCP9808 tempSensor;