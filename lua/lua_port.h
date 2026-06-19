#pragma once
//=============================================================================
// dingoPDM <-> Lua 5.5 embedding port.
// One shared lua_State, allocated from a fixed static RAM arena (no newlib
// malloc). C linkage so the C++ firmware (device.cpp) can drive it.
//=============================================================================
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---- engine lifecycle (called from device.cpp / LuaThread) ----------------

// Create the shared lua_State over the static arena and open the safe stdlibs
// (base, table, string, math, coroutine, utf8) + the dingoPDM API. Idempotent.
// Returns 0 on success.
int LuaPortInit(void);

// Compile + run a source chunk (the one assembled program). Top-level code runs
// once (defining globals like tick()/onTick()/onCanRx()). Returns 0 on success;
// on error the message is left in errBuf (see LuaLastError).
int LuaLoadString(const char *src);

// Service the script once per LuaThread cycle: drains queued CAN frames into
// onCanRx(...) and calls onTick() at the configured rate. Each Lua call is
// bounded by the instruction-count hook. No-op if no script is loaded.
void LuaService(void);

// Last compile/runtime error string (empty if none).
const char *LuaLastError(void);

// Self-test chunk ("return 1+2") -> 3. Keeps the interpreter referenced.
int LuaSelfTest(void);

// Free arena high-water mark (bytes) for headroom reporting.
unsigned LuaArenaUsed(void);

// ---- bridge implemented in device.cpp (var-map <-> Lua) --------------------

// Read any var-map index (bounds-checked); 0 if out of range / null.
float LuaBridgeReadVar(int idx);

// Write a Lua output slot 0..NUM_LUA_OUTPUTS-1 (bounds-checked).
void LuaBridgeSetOut(int slot, float val);

// Milliseconds since boot (for Timer userdata).
uint32_t LuaBridgeMillis(void);

// Register a CAN id the script wants delivered to onCanRx().
void LuaBridgeCanRxRegister(uint32_t id);

// Pop one queued (registered-id) RX frame for delivery to onCanRx; returns 1
// if a frame was returned, 0 if the queue is empty. Drained on the LuaThread.
int LuaBridgeCanRxPop(uint32_t *id, int *ext, uint8_t *data, int *dlc);

// Transmit a CAN frame (txCan). dlc clamped to 8.
void LuaBridgeTxCan(uint32_t id, int ext, const uint8_t *data, int dlc);

#ifdef __cplusplus
}
#endif
