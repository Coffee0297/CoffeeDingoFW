// Host-side self-check for the pure function logic (no ChibiOS, no board). tests/host/port.h stands in for
// the board's port.h. From the repo root:
//   g++ -std=c++20 -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/timer.cpp -o build/host_selftest && build/host_selftest
//   g++ -std=c++20 -DMATH_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/math_block.cpp -o build/host_math && build/host_math
//   g++ -std=c++20 -DCAN_MSG_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/can_msg.cpp utils/dbc.cpp -o build/host_can_msg && build/host_can_msg
//   g++ -std=c++20 -DCAN_MSG_CHECKSUM_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/can_msg.cpp utils/dbc.cpp -o build/host_can_msg_cs && build/host_can_msg_cs
//   g++ -std=c++20 -Wall -DCHECKSUM_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp utils/dbc.cpp -o build/host_checksum && build/host_checksum
//   g++ -std=c++20 -DCAN_INPUT_TIMEOUT_ONLY -I tests/host -I functions -I core -I utils tests/host_selftest.cpp functions/can_input.cpp functions/input.cpp utils/dbc.cpp -o build/host_can_input_timeout && build/host_can_input_timeout
// Each feature adds its own section below, picked with -D<NAME>_ONLY, and its build line here.
#include <cassert>
#include <cstdio>
#include "port.h"

float *pVarMap[VAR_MAP_SIZE];
uint32_t nHostTimeMs = 0;

#if 0 // feature sections: #elif defined(<NAME>_ONLY)
#elif defined(MATH_ONLY)
#include "math_block.h"

int main()
{
    float zero = 0, a = 2, b = 3;
    pVarMap[0] = &zero; pVarMap[1] = &a; pVarMap[2] = &b;
    Config_Math c{};
    c.bEnabled = true; c.nInputA = 1; c.nInputB = 2; c.fFactor = 1;
    MathBlock m;
    auto eval = [&](MathOp op) { c.eOp = op; m.SetConfig(&c); m.Update(); return m.fVal; };

    assert(eval(MathOp::Add) == 5);
    assert(eval(MathOp::Sub) == -1);
    assert(eval(MathOp::Mul) == 6);
    assert(eval(MathOp::Min) == 2);
    assert(eval(MathOp::Max) == 3);
    assert(eval(MathOp::Average) == 2.5f);
    b = 0; assert(eval(MathOp::Div) == 0);          // no inf / NaN into the outputs
    a = 6; b = 3; assert(eval(MathOp::Div) == 2);

    // oil - coolant, then x0.5 + 10; B is the constant when its input is 0
    a = 110; b = 90; c.fFactor = 0.5f; c.fOffset = 10;
    assert(eval(MathOp::Sub) == 20);
    c.nInputB = 0; c.fConstB = 100; assert(eval(MathOp::Sub) == 15);
    b = 50; m.Update(); assert(m.fVal == 15);        // the constant, not the variable

    c.bEnabled = false; m.Update(); assert(m.fVal == 0);

    std::puts("host_selftest MATH OK");
    return 0;
}
#elif defined(CAN_MSG_ONLY)
#include "can_msg.h"

int main()
{
    float in = 0;
    pVarMap[3] = &in;
    Config_CanMsg c{};
    c.bEnabled = true; c.nInput = 3; c.nID = 0x123; c.nDLC = 8; c.eMode = CanMsgMode::Repeat; c.nInterval = 100;
    CanMsg k; k.SetConfig(&c);
    auto due = [&](uint32_t ms, bool bMuted = false) { nHostTimeMs = ms; return k.Due(bMuted); };

    assert(!due(0));                                    // input off: nothing
    in = 1;
    assert(due(10));                                    // turns on: at once
    assert(!due(50)); assert(!due(109));
    assert(due(110));                                   // then every interval
    assert(!due(210, true)); assert(!due(400, true));   // muted: held back
    assert(due(410));                                   // unmuted: at once, then the interval again
    assert(!due(500)); assert(due(510));
    in = 0; assert(!due(700));

    c.eMode = CanMsgMode::Once;
    in = 1; assert(due(800));                           // one frame each time it turns on
    assert(!due(1000)); assert(!due(5000));
    in = 0; due(5010); in = 1; assert(due(5020, true));  // Once is an event: not muted

    c.eMode = CanMsgMode::Repeat; in = 0; due(6000); in = 1;
    c.nID = 0x800; assert(!due(6010)); assert(!due(7000));   // standard id > 0x7FF: never
    c.nIDE = 1; assert(due(7010));                           // as an extended id it's fine
    c.bEnabled = false; assert(!due(8000));

    // the first pass only samples: an input already on at power-up sends no Once frame
    static CanMsg k2; c.bEnabled = true; c.eMode = CanMsgMode::Once; in = 1; k2.SetConfig(&c);
    nHostTimeMs = 9000; assert(!k2.Due(false));
    in = 0; nHostTimeMs = 9010; k2.Due(false); in = 1; nHostTimeMs = 9020; assert(k2.Due(false));

    std::puts("host_selftest CAN_MSG OK");
    return 0;
}
#elif defined(CAN_MSG_CHECKSUM_ONLY)
#include <cstring>
#include "can_msg.h"
#include "dbc.h"

int main()
{
    const uint8_t s[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', 0};    // published check values
    assert(Dbc::Checksum(CanOutputKind::Crc8SaeJ1850, s, 10, 9) == 0x4B);
    assert(Dbc::Checksum(CanOutputKind::Crc8Autosar, s, 10, 9) == 0xDF);

    float in = 1;
    pVarMap[3] = &in;
    Config_CanMsg c{};
    c.bEnabled = true; c.nInput = 3; c.nID = 0x123; c.nDLC = 8; c.nInterval = 100;
    for (uint8_t i = 0; i < 8; i++) c.nData[i] = 0x10 * i + 1;
    c.nCounterByte = 0xFF;
    static CanMsg k; k.SetConfig(&c);   // static: zeroed like the firmware's globals
    uint8_t d[8];

    k.Fill(d); assert(std::memcmp(d, c.nData, 8) == 0);            // no counter, no checksum: the data as set

    c.nCounterByte = 1; c.nData[1] = 0xA7;                          // counter in the low nibble, high nibble kept
    for (int n = 0; n < 17; n++) { k.Fill(d); assert(d[1] == (0xA0 | (n & 0x0F))); }

    c.eChecksum = CanOutputKind::ChecksumXor; c.nChecksumByte = 7;
    k.Fill(d);                                                      // counter first, checksum over it
    uint8_t x = 0; for (int i = 0; i < 7; i++) x ^= d[i];
    assert(d[1] == 0xA1 && d[7] == x);
    c.eChecksum = CanOutputKind::ChecksumSum; k.Fill(d);
    x = 0; for (int i = 0; i < 7; i++) x += d[i];
    assert(d[7] == x);
    c.eChecksum = CanOutputKind::Crc8SaeJ1850; c.nChecksumByte = 0; k.Fill(d);
    assert(d[0] == Dbc::Checksum(CanOutputKind::Crc8SaeJ1850, d, 8, 0));

    c.nDLC = 4; c.nChecksumByte = 5; c.nCounterByte = 6; k.Fill(d);   // past the DLC: off
    assert(d[5] == c.nData[5] && d[6] == c.nData[6]);
    c.eChecksum = CanOutputKind::RollingCounter; c.nChecksumByte = 3; k.Fill(d);   // 1 is no checksum kind
    assert(d[3] == c.nData[3]);

    std::puts("host_selftest CAN_MSG_CHECKSUM OK");
    return 0;
}
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
#elif defined(CAN_INPUT_TIMEOUT_ONLY)
#include "can_input.h"

int main()
{
    // coolant temp in byte 0; output on above 100 (a fan rule); timeout 500 ms
    Config_CanInput c{};
    c.bEnabled = true; c.bTimeoutEnabled = true; c.nTimeout = 500; c.nID = 0x100;
    c.nBitLength = 8; c.fFactor = 1; c.eOperator = Operator::GreaterThan; c.fOperand = 100;
    c.fTimeoutValue = 120;
    CanInput k; k.SetConfig(&c);
    CANRxFrame rx{}; rx.SID = 0x100;
    auto frame = [&](uint32_t ms, uint8_t v) { nHostTimeMs = ms; rx.data8[0] = v; k.CheckMsg(rx); };
    auto idle = [&](uint32_t ms) { nHostTimeMs = ms; k.CheckTimeout(); };

    frame(0, 90); idle(400); assert(k.fVal == 90 && k.fOutput == 0);
    idle(501); assert(k.fVal == 0 && k.fOutput == 0);          // flag off: as before, 0 and off

    c.bTimeoutUseValue = true;
    frame(1000, 90); idle(1501); assert(k.fVal == 120 && k.fOutput == 1);   // sender lost: reads 120, fan on
    frame(1600, 90); assert(k.fVal == 90 && k.fOutput == 0);                // sender back: live value again
    c.fTimeoutValue = 50; idle(2101); assert(k.fVal == 50 && k.fOutput == 0);

    c.eMode = InputMode::Latching; c.fTimeoutValue = 120;     // not through the latch: no toggle on timeout
    frame(3000, 110); assert(k.fOutput == 1); frame(3010, 90); assert(k.fOutput == 1);
    idle(3511); assert(k.fOutput == 1); idle(3600); assert(k.fOutput == 1);
    c.fTimeoutValue = 0; idle(3700); assert(k.fOutput == 0); idle(3800); assert(k.fOutput == 0);

    c.bTimeoutEnabled = false; c.fTimeoutValue = 120; idle(9000); assert(k.fVal == 0);   // no timeout: holds
    c.bTimeoutEnabled = true; c.bEnabled = false; idle(9000); assert(k.fVal == 0 && k.fOutput == 0);   // disabled input never uses the value

    std::puts("host_selftest CAN_INPUT_TIMEOUT OK");
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
