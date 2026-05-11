#pragma once

#include "port.h"
#include "digital_input.h"
#include "digital_output.h"
#include "analog_input.h"

extern Digital_Input in[NUM_DIG_INPUTS];
extern Digital_Output digOut[NUM_DIG_OUTPUTS];
extern Analog_Input analogIn[NUM_ANALOG_INPUTS];