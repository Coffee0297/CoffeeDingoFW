#pragma once

#include "port.h"
#include "profet.h"
#include "digital.h"
#include "digital_input.h"
#include "digital_output.h"
#include "analog_input.h"
#include "led.h"
#include "hardware/mcp9808.h"

extern Profet pf[NUM_OUTPUTS];
extern Digital in[NUM_INPUTS];
extern Digital_Input digIn[NUM_DIG_INPUTS];
extern Digital_Output digOut[NUM_DIG_OUTPUTS];
extern Analog_Input analogIn[NUM_ANALOG_INPUTS];
extern Led statusLed;
extern Led errorLed;
extern MCP9808 tempSensor;