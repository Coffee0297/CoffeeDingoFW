// Host-side self-check for the pure function logic (no ChibiOS, no board). tests/host/port.h stands in for
// the board's port.h. From the repo root:
//   g++ -std=c++20 -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/timer.cpp -o build/host_selftest && build/host_selftest
// Each feature adds its own section below, picked with -D<NAME>_ONLY, and its build line here.
//   g++ -std=c++20 -Wall -DCHECKSUM_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp utils/dbc.cpp -o build/host_checksum && build/host_checksum
#include <cassert>
#include <cstdio>
#include "port.h"

float *pVarMap[VAR_MAP_SIZE];
uint32_t nHostTimeMs = 0;

#if 0 // feature sections: #elif defined(<NAME>_ONLY)
#elif defined(CHECKSUM_ONLY)
#include "dbc.h"

int main()
{
    const uint8_t check[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    assert(Dbc::Checksum(CanOutputKind::Crc8SaeJ1850, check, 9, 0xFF) == 0x4B);   // published check values
    assert(Dbc::Checksum(CanOutputKind::Crc8Autosar, check, 9, 0xFF) == 0xDF);

    uint8_t f[8] = {0x01, 0x02, 0xAA, 0x04, 0x80, 0x80, 0x00, 0x00};
    assert(Dbc::Checksum(CanOutputKind::ChecksumXor, f, 8, 2) == (0x01 ^ 0x02 ^ 0x04));       // skips byte 2
    assert(Dbc::Checksum(CanOutputKind::ChecksumSum, f, 8, 2) == ((0x01 + 0x02 + 0x04 + 0x100) & 0xFF)); // mod 256
    assert(Dbc::Checksum(CanOutputKind::Crc8SaeJ1850, f, 6, 2) != Dbc::Checksum(CanOutputKind::Crc8SaeJ1850, f, 6, 0xFF));

    // rolling counter: the output encodes nCount++ with EncodeInt, which keeps the low bits = wrap at the length
    uint8_t d[8] = {0xFF, 0xFF, 0, 0, 0, 0, 0, 0};
    Dbc::EncodeInt(d, 17, 4, 4);
    assert(d[0] == 0x1F && d[1] == 0xFF);           // 17 & 0xF = 1 in bits 4-7, neighbours untouched

    std::puts("host_selftest CHECKSUM OK");
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
