// Host-side self-check for the pure function logic (no ChibiOS, no board). tests/host/port.h stands in for
// the board's port.h. From the repo root:
//   g++ -std=c++20 -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/timer.cpp -o build/host_selftest && build/host_selftest
//   g++ -std=c++20 -DANALOG_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/input.cpp -o build/host_analog && build/host_analog
// Each feature adds its own section below, picked with -D<NAME>_ONLY, and its build line here.
#include <cassert>
#include <cstdio>
#include "port.h"

float *pVarMap[VAR_MAP_SIZE];
uint32_t nHostTimeMs = 0;

#if 0 // feature sections: #elif defined(<NAME>_ONLY)
#elif defined(ANALOG_ONLY)
enum class AnalogChannel { Ai1 };     // board port.h stand-ins, ahead of analog_input.h
float fHostMv = 0;
uint16_t GetAdcRaw(AnalogChannel) { return static_cast<uint16_t>(fHostMv); }
float GetAdcVolts(AnalogChannel) { return fHostMv / 1000.0f; }
#include "analog_input.cpp"         // same unit: it needs the stand-ins above

int main()
{
    Config_AnalogInput k{};
    k.stSwitch.bEnabled = true; k.stSwitch.nThreshold = 1000;
    Analog_Input a(AnalogChannel::Ai1); a.SetConfig(&k);
    fHostMv = 2500; a.Update();
    assert(a.fVal == 2500 && a.fValMillivolts > 2499 && a.fValMillivolts < 2501);  // disabled: raw still live (was 0)
    assert(a.fSwitchVal == 0);                                                     // decoders stay off
    k.bEnabled = true; a.Update(); assert(a.fSwitchVal == 1);
    k.bEnabled = false; fHostMv = 500; a.Update();
    assert(a.fValMillivolts < 501 && a.fSwitchVal == 0);                            // was frozen at 2500 / 1
    std::puts("host_analog OK");
    return 0;
}
#else
#include "timer.h"

int main()
{
    float in = 0, rst = 0;
    pVarMap[0] = &in; pVarMap[1] = &rst;
    Config_Timer c{};
    c.bEnabled = true; c.nInput = 0; c.nResetInput = 1; c.eMode = TimerMode::OnDelay; c.nTime = 100;
    Timer t; t.SetConfig(&c);
    auto at = [&](uint32_t ms) { nHostTimeMs = ms; t.Update(); };

    at(0);                                          // first pass only samples the input
    in = 1; at(10); assert(t.fVal == 0);
    at(109); assert(t.fVal == 0);
    at(110); assert(t.fVal == 1);                   // on after 100 ms
    in = 0; at(120); assert(t.fVal == 0);           // off at once

    rst = 1; in = 1; at(130); at(500); assert(t.fVal == 0);   // reset held: stays cleared
    rst = 0; at(510); at(610); assert(t.fVal == 1);          // released with the input on: delay starts again

    std::puts("host_selftest OK");
    return 0;
}
#endif
