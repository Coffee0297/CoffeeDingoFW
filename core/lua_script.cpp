// One stored Lua program on its own thread, below every other thread. Each call into the script is
// bounded by an instruction-count hook; a script that errors (or hits the bound) stops until the next upload.
//
// Script API: readVar(i), setLuaOut(slot, v), canRxAdd(id), txCan(bus, id, ext, {bytes}), setTickRate(hz),
// luaLog(msg), Timer.new() / t:reset() / t:getElapsedSeconds(); callbacks onTick() and onCanRx(bus, id, dlc, {bytes}).
#include "lua_script.h"

#if HAS_LUA
#include <cstring>
#include "ch.hpp"
#include "config.h"
#include "mailbox.h"
#include "can.h"
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

extern float *pVarMap[VAR_MAP_SIZE];

#define LUA_THREAD_STACK (12 * 1024) // the parser recurses; 4 KB overflowed on larger scripts
#define LUA_INSTR_LIMIT 200000      // per call into the script
#define LUA_RX_IDS 16
#define LUA_RX_QUEUE 16

float fLuaOut[NUM_LUA_OUTPUTS];

static CH_HEAP_AREA(luaHeapArea, LUA_HEAP_SIZE);
static memory_heap_t luaHeap;
static lua_State *pLua;
static volatile bool bReload = true;
static bool bRunning;
static char acError[128];
static uint32_t nTickMs, nLastTick;

static volatile uint32_t nRxIds[LUA_RX_IDS];
static volatile uint8_t nNumRxIds;
static objects_fifo_t rxFifo;
static CANRxFrame rxFrames[LUA_RX_QUEUE];
static msg_t rxMsgs[LUA_RX_QUEUE];

static void SetError(const char *pMsg)
{
    strncpy(acError, pMsg ? pMsg : "error", sizeof(acError) - 1);
}

static void *Alloc(void *, void *ptr, size_t nOld, size_t nNew)
{
    if (nNew == 0)
    {
        if (ptr)
            chHeapFree(ptr);
        return nullptr;
    }
    void *p = chHeapAlloc(&luaHeap, nNew);
    if (!p)
        return (ptr && nNew <= nOld) ? ptr : nullptr; // Lua expects a shrink never to fail
    if (ptr)
    {
        memcpy(p, ptr, nOld < nNew ? nOld : nNew);
        chHeapFree(ptr);
    }
    return p;
}

static void Hook(lua_State *L, lua_Debug *)
{
    luaL_error(L, "script exceeded instruction budget");
}

//=============================================================================
// Script API
//=============================================================================
static int ReadVar(lua_State *L)
{
    lua_Integer i = luaL_checkinteger(L, 1);
    lua_pushnumber(L, (i >= 0 && i < VAR_MAP_SIZE) ? *pVarMap[i] : 0);
    return 1;
}

static int SetLuaOut(lua_State *L)
{
    lua_Integer i = luaL_checkinteger(L, 1);
    float f = (float)luaL_checknumber(L, 2);
    if (i >= 0 && i < NUM_LUA_OUTPUTS)
        fLuaOut[i] = f;
    return 0;
}

static int CanRxAdd(lua_State *L)
{
    uint32_t nId = (uint32_t)luaL_checkinteger(L, 1);
    for (uint8_t i = 0; i < nNumRxIds; i++)
        if (nRxIds[i] == nId)
            return 0;
    if (nNumRxIds < LUA_RX_IDS)
    {
        nRxIds[nNumRxIds] = nId;
        nNumRxIds = nNumRxIds + 1; // after the id: the device thread may be reading
    }
    return 0;
}

static int TxCan(lua_State *L)
{
    // Arg 1 = bus, one bus only
    uint32_t nId = (uint32_t)luaL_checkinteger(L, 2);
    bool bExt = lua_toboolean(L, 3);
    luaL_checktype(L, 4, LUA_TTABLE);

    CANTxFrame tx = {};
    tx.IDE = bExt ? CAN_IDE_EXT : CAN_IDE_STD;
    if (bExt)
        tx.EID = nId;
    else
        tx.SID = nId;
    tx.RTR = CAN_RTR_DATA;
    lua_Integer n = luaL_len(L, 4);
    tx.DLC = n > 8 ? 8 : (n < 0 ? 0 : n);
    for (uint8_t i = 0; i < tx.DLC; i++)
    {
        lua_geti(L, 4, i + 1);
        tx.data8[i] = (uint8_t)lua_tointeger(L, -1);
        lua_pop(L, 1);
    }
    if (!CanTxMuted())
        PostTxFrame(&tx);
    return 0;
}

static int SetTickRate(lua_State *L)
{
    lua_Integer nHz = luaL_checkinteger(L, 1);
    nTickMs = 1000 / (nHz < 1 ? 1 : (nHz > 1000 ? 1000 : nHz));
    return 0;
}

static int LuaLog(lua_State *L)
{
    SetError(luaL_optstring(L, 1, ""));
    return 0;
}

#define TIMER_MT "dingoTimer"
static int TimerNew(lua_State *L)
{
    *(uint32_t *)lua_newuserdatauv(L, sizeof(uint32_t), 0) = SYS_TIME;
    luaL_setmetatable(L, TIMER_MT);
    return 1;
}

static int TimerReset(lua_State *L)
{
    *(uint32_t *)luaL_checkudata(L, 1, TIMER_MT) = SYS_TIME;
    return 0;
}

static int TimerElapsed(lua_State *L)
{
    lua_pushnumber(L, (SYS_TIME - *(uint32_t *)luaL_checkudata(L, 1, TIMER_MT)) / (lua_Number)1000);
    return 1;
}

//=============================================================================
// Protected calls: everything that can allocate runs inside lua_pcall
//=============================================================================
static int Load(lua_State *L)
{
    // No io/os/package/debug on the module
    static const luaL_Reg libs[] = {
        {LUA_GNAME, luaopen_base}, {LUA_TABLIBNAME, luaopen_table}, {LUA_STRLIBNAME, luaopen_string},
        {LUA_MATHLIBNAME, luaopen_math}, {LUA_COLIBNAME, luaopen_coroutine}, {LUA_UTF8LIBNAME, luaopen_utf8}};
    for (const luaL_Reg &lib : libs)
    {
        luaL_requiref(L, lib.name, lib.func, 1);
        lua_pop(L, 1);
    }
    static const luaL_Reg api[] = {
        {"readVar", ReadVar}, {"setLuaOut", SetLuaOut}, {"canRxAdd", CanRxAdd}, {"txCan", TxCan},
        {"setTickRate", SetTickRate}, {"luaLog", LuaLog}};
    for (const luaL_Reg &fn : api)
        lua_register(L, fn.name, fn.func);

    luaL_newmetatable(L, TIMER_MT);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, TimerReset);
    lua_setfield(L, -2, "reset");
    lua_pushcfunction(L, TimerElapsed);
    lua_setfield(L, -2, "getElapsedSeconds");
    lua_pop(L, 1);
    lua_newtable(L);
    lua_pushcfunction(L, TimerNew);
    lua_setfield(L, -2, "new");
    lua_setglobal(L, "Timer");

    // Source text only: a binary chunk could crash the VM
    if (luaL_loadbufferx(L, stConfig.stLua.acScript, stConfig.stLua.nLength, "script", "t") != LUA_OK)
        lua_error(L);
    lua_call(L, 0, 0);
    return 0;
}

static int CallTick(lua_State *L)
{
    if (lua_getglobal(L, "onTick") == LUA_TFUNCTION)
        lua_call(L, 0, 0);
    return 0;
}

static int CallCanRx(lua_State *L)
{
    const CANRxFrame *f = (const CANRxFrame *)lua_touserdata(L, 1);
    if (lua_getglobal(L, "onCanRx") != LUA_TFUNCTION)
        return 0;
    lua_pushinteger(L, 1);
    lua_pushinteger(L, f->IDE == CAN_IDE_EXT ? f->EID : f->SID);
    lua_pushinteger(L, f->DLC);
    lua_createtable(L, 8, 0);
    for (uint8_t i = 0; i < 8; i++)
    {
        lua_pushinteger(L, f->data8[i]);
        lua_seti(L, -2, i + 1);
    }
    lua_call(L, 4, 0);
    return 0;
}

static void Run(lua_CFunction fn, const void *pArg)
{
    lua_pushcfunction(pLua, fn);
    lua_pushlightuserdata(pLua, const_cast<void *>(pArg));
    lua_sethook(pLua, Hook, LUA_MASKCOUNT, LUA_INSTR_LIMIT);
    int nRes = lua_pcall(pLua, 1, 0, 0);
    lua_sethook(pLua, nullptr, 0, 0);
    if (nRes != LUA_OK)
    {
        SetError(lua_tostring(pLua, -1));
        lua_pop(pLua, 1);
        bRunning = false; // don't retry a failing script every tick
    }
}

static void Reload()
{
    // A fresh heap and state: nothing of the previous program (globals, callbacks, CAN ids) survives
    bRunning = false;
    nNumRxIds = 0;
    CANRxFrame *f;
    while (chFifoReceiveObjectTimeout(&rxFifo, (void **)&f, TIME_IMMEDIATE) == MSG_OK)
        chFifoReturnObject(&rxFifo, f);
    for (float &fOut : fLuaOut)
        fOut = 0;
    memset(acError, 0, sizeof(acError));
    nTickMs = 10;

    chHeapObjectInit(&luaHeap, luaHeapArea, sizeof(luaHeapArea));
    pLua = lua_newstate(Alloc, nullptr, 0);
    if (!pLua)
    {
        SetError("not enough memory");
        return;
    }

    // No stored program: nothing runs and the slots stay 0
    if ((stConfig.stLua.nLength == 0) || (stConfig.stLua.nLength >= LUA_SCRIPT_MAX))
        return;

    bRunning = true;
    Run(Load, nullptr);
    nLastTick = SYS_TIME;
}

struct LuaThread : chibios_rt::BaseStaticThread<LUA_THREAD_STACK>
{
    void main()
    {
        setName("LuaThread");

        while (true)
        {
            if (bReload)
            {
                bReload = false;
                Reload();
            }

            CANRxFrame *f;
            while (bRunning && chFifoReceiveObjectTimeout(&rxFifo, (void **)&f, TIME_IMMEDIATE) == MSG_OK)
            {
                CANRxFrame frame = *f;
                chFifoReturnObject(&rxFifo, f);
                Run(CallCanRx, &frame);
            }

            if (bRunning && (SYS_TIME - nLastTick) >= nTickMs)
            {
                nLastTick = SYS_TIME;
                Run(CallTick, nullptr);
            }

            chThdSleepMilliseconds(2);
        }
    }
};
static LuaThread luaThread;

void InitLua()
{
    chFifoObjectInit(&rxFifo, sizeof(CANRxFrame), LUA_RX_QUEUE, rxFrames, rxMsgs);
    // Lowest priority: a script can never hold off the control loop, CAN or the param thread
    luaThread.start(NORMALPRIO - 2);
}

void ReloadLua()
{
    bReload = true;
}

// ponytail: with the CAN filter enabled, only ids that pass it reach onCanRx; add the canRxAdd ids to
// UpdateCanFilters() if scripts need foreign ids on a filtered bus.
void LuaCanRx(const CANRxFrame &frame)
{
    uint32_t nId = frame.IDE == CAN_IDE_EXT ? frame.EID : frame.SID;
    for (uint8_t i = 0; i < nNumRxIds; i++)
    {
        if (nRxIds[i] != nId)
            continue;
        CANRxFrame *f = (CANRxFrame *)chFifoTakeObjectTimeout(&rxFifo, TIME_IMMEDIATE);
        if (f == nullptr)
            return; // queue full, drop
        *f = frame;
        chFifoSendObject(&rxFifo, f);
        return;
    }
}

const char *GetLuaError()
{
    return acError;
}
#endif
