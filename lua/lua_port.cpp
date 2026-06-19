//=============================================================================
// dingoPDM Lua port: static-arena allocator + interpreter bootstrap.
//
// Glue is C++ (so it can read board config from port.h); the Lua core stays C.
// The build is no-heap (no newlib malloc), so Lua gets its own fixed RAM arena
// and a custom lua_Alloc. The allocator is a first-fit free list with boundary
// coalescing — correct for Lua's many small alloc/realloc/free calls.
//
// ponytail: first-fit + coalesce is the simplest allocator that won't fragment
// itself to death under Lua's GC. If profiling later shows alloc latency in the
// LuaThread, swap in TLSF behind the same lua_Alloc signature — nothing else
// changes.
//=============================================================================
#include "lua_port.h"
#include "port.h"        // LUA_HEAP_SIZE
#include <cstring>
#include <cstdint>

#if HAS_LUA

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

//----------------------------------------------------------------------------
// Arena allocator
//----------------------------------------------------------------------------
#define LUA_ALIGN       8u
#define ALIGN_UP(n)     (((n) + (LUA_ALIGN - 1)) & ~((size_t)(LUA_ALIGN - 1)))

// Each block: [size_t size (incl. header, multiple of ALIGN)][payload...].
// Free blocks reuse the payload's first words as the free-list node.
struct FreeNode {
    size_t size;
    FreeNode *next;   // address-ordered free list
};

#define HDR_SIZE        ALIGN_UP(sizeof(size_t))
#define MIN_BLOCK       (HDR_SIZE + ALIGN_UP(sizeof(void *)))

static uint8_t luaHeap[LUA_HEAP_SIZE] __attribute__((aligned(LUA_ALIGN)));
static FreeNode *gFreeList;
static unsigned gHighWater;

static void ArenaInit()
{
    FreeNode *b = reinterpret_cast<FreeNode *>(luaHeap);
    b->size = sizeof(luaHeap);
    b->next = nullptr;
    gFreeList = b;
    gHighWater = 0;
}

static inline size_t *HdrOf(void *payload)
{
    return reinterpret_cast<size_t *>(static_cast<uint8_t *>(payload) - HDR_SIZE);
}

static void ArenaAccountUsed()
{
    size_t freeBytes = 0;
    for (FreeNode *n = gFreeList; n; n = n->next) freeBytes += n->size;
    unsigned used = static_cast<unsigned>(sizeof(luaHeap) - freeBytes);
    if (used > gHighWater) gHighWater = used;
}

static void *ArenaAlloc(size_t want)
{
    size_t need = ALIGN_UP(want) + HDR_SIZE;
    if (need < MIN_BLOCK) need = MIN_BLOCK;

    FreeNode *prev = nullptr, *cur = gFreeList;
    while (cur) {
        if (cur->size >= need) {
            if (cur->size - need >= MIN_BLOCK) {
                // Split: tail stays free.
                FreeNode *tail = reinterpret_cast<FreeNode *>(reinterpret_cast<uint8_t *>(cur) + need);
                tail->size = cur->size - need;
                tail->next = cur->next;
                cur->size = need;
                if (prev) prev->next = tail; else gFreeList = tail;
            } else {
                // Take whole block.
                if (prev) prev->next = cur->next; else gFreeList = cur->next;
            }
            *reinterpret_cast<size_t *>(cur) = cur->size;   // header
            ArenaAccountUsed();
            return reinterpret_cast<uint8_t *>(cur) + HDR_SIZE;
        }
        prev = cur;
        cur = cur->next;
    }
    return nullptr;   // out of arena
}

static void ArenaFree(void *payload)
{
    if (!payload) return;
    size_t *hdr = HdrOf(payload);
    FreeNode *blk = reinterpret_cast<FreeNode *>(hdr);
    blk->size = *hdr;

    // Insert address-ordered, coalescing with neighbours.
    FreeNode *prev = nullptr, *cur = gFreeList;
    while (cur && cur < blk) { prev = cur; cur = cur->next; }

    // Coalesce with next.
    if (cur && reinterpret_cast<uint8_t *>(blk) + blk->size == reinterpret_cast<uint8_t *>(cur)) {
        blk->size += cur->size;
        blk->next = cur->next;
    } else {
        blk->next = cur;
    }
    // Coalesce with prev.
    if (prev && reinterpret_cast<uint8_t *>(prev) + prev->size == reinterpret_cast<uint8_t *>(blk)) {
        prev->size += blk->size;
        prev->next = blk->next;
    } else if (prev) {
        prev->next = blk;
    } else {
        gFreeList = blk;
    }
}

// lua_Alloc: (ud, ptr, osize, nsize). nsize==0 => free; ptr==NULL => fresh alloc.
extern "C" void *LuaArenaAlloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
    (void)ud;
    if (nsize == 0) { ArenaFree(ptr); return nullptr; }
    if (ptr == nullptr) return ArenaAlloc(nsize);

    void *np = ArenaAlloc(nsize);
    if (!np) return nullptr;
    size_t copy = osize < nsize ? osize : nsize;
    memcpy(np, ptr, copy);
    ArenaFree(ptr);
    return np;
}

//----------------------------------------------------------------------------
// dingoPDM API bindings
//----------------------------------------------------------------------------
static char gErr[128];   // last Lua error (top-level pcall or luaLog); read-back via LuaErr

// luaLog(msg) : record a runtime error string (the assembled program calls this from
// each snippet's pcall guard). Overwrites the last error; read back over CAN.
static int l_luaLog(lua_State *L)
{
    const char *m = luaL_optstring(L, 1, "");
    size_t n = 0;
    while (m[n] && n < sizeof(gErr) - 1) { gErr[n] = m[n]; n++; }
    gErr[n] = '\0';
    return 0;
}

// readVar(index) -> number : read any var-map signal.
static int l_readVar(lua_State *L)
{
    int idx = (int)luaL_checkinteger(L, 1);
    lua_pushnumber(L, (lua_Number)LuaBridgeReadVar(idx));
    return 1;
}

// setLuaOut(slot, value) : drive a Lua output slot (0..NUM_LUA_OUTPUTS-1).
static int l_setLuaOut(lua_State *L)
{
    int slot = (int)luaL_checkinteger(L, 1);
    lua_Number v = luaL_checknumber(L, 2);
    LuaBridgeSetOut(slot, (float)v);
    return 0;
}

// canRxAdd(id) : ask for frames with this id to be delivered to onCanRx().
static int l_canRxAdd(lua_State *L)
{
    LuaBridgeCanRxRegister((uint32_t)luaL_checkinteger(L, 1));
    return 0;
}

// txCan(bus, id, ext, data) : transmit. data = 1-indexed table of bytes (<=8).
static int l_txCan(lua_State *L)
{
    (void)luaL_optinteger(L, 1, 1);                 // bus — one CAN bus, ignored
    lua_Integer id = luaL_checkinteger(L, 2);
    int ext = lua_toboolean(L, 3);                  // accepts 1/true for extended
    luaL_checktype(L, 4, LUA_TTABLE);
    uint8_t data[8] = {0};
    lua_Integer n = luaL_len(L, 4);
    if (n > 8) n = 8;
    for (lua_Integer i = 1; i <= n; i++) {
        lua_geti(L, 4, i);
        data[i - 1] = (uint8_t)(lua_tointeger(L, -1) & 0xFF);
        lua_pop(L, 1);
    }
    LuaBridgeTxCan((uint32_t)id, ext, data, (int)n);
    return 0;
}

// setTickRate(hz) : how often onTick() is called (1..1000 Hz).
static uint32_t gTickRateHz = 100;
static uint32_t gLastTickMs = 0;
static int l_setTickRate(lua_State *L)
{
    lua_Integer hz = luaL_checkinteger(L, 1);
    if (hz < 1) hz = 1;
    if (hz > 1000) hz = 1000;
    gTickRateHz = (uint32_t)hz;
    return 0;
}

//---- Timer userdata: Timer.new() / t:reset() / t:getElapsedSeconds() --------
#define TIMER_MT "dingoTimer"
static int l_timer_new(lua_State *L)
{
    uint32_t *t = (uint32_t *)lua_newuserdatauv(L, sizeof(uint32_t), 0);
    *t = LuaBridgeMillis();
    luaL_setmetatable(L, TIMER_MT);
    return 1;
}
static int l_timer_reset(lua_State *L)
{
    uint32_t *t = (uint32_t *)luaL_checkudata(L, 1, TIMER_MT);
    *t = LuaBridgeMillis();
    return 0;
}
static int l_timer_elapsed(lua_State *L)
{
    uint32_t *t = (uint32_t *)luaL_checkudata(L, 1, TIMER_MT);
    uint32_t e = LuaBridgeMillis() - *t;
    lua_pushnumber(L, (lua_Number)e / (lua_Number)1000.0);
    return 1;
}

static void RegisterApi(lua_State *L)
{
    lua_register(L, "readVar", l_readVar);
    lua_register(L, "setLuaOut", l_setLuaOut);
    lua_register(L, "canRxAdd", l_canRxAdd);
    lua_register(L, "txCan", l_txCan);
    lua_register(L, "setTickRate", l_setTickRate);
    lua_register(L, "luaLog", l_luaLog);

    // Timer metatable (methods via __index) + global Timer table with .new
    luaL_newmetatable(L, TIMER_MT);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, l_timer_reset);
    lua_setfield(L, -2, "reset");
    lua_pushcfunction(L, l_timer_elapsed);
    lua_setfield(L, -2, "getElapsedSeconds");
    lua_pop(L, 1);

    lua_newtable(L);
    lua_pushcfunction(L, l_timer_new);
    lua_setfield(L, -2, "new");
    lua_setglobal(L, "Timer");
}

//----------------------------------------------------------------------------
// Interpreter bootstrap + execution
//----------------------------------------------------------------------------
static lua_State *gL;
static bool       gScriptOk;          // a tick()-capable script is loaded

extern "C" int LuaPanic(lua_State *L) { (void)L; return 0; }   // never longjmp out

// Abort a runaway script: fired by the count hook after N VM instructions.
static void CountHook(lua_State *L, lua_Debug *ar)
{
    (void)ar;
    luaL_error(L, "script exceeded instruction budget");
}

#define LUA_TICK_INSTR_BUDGET 200000   // per tick() call; tune on-target

static void OpenSafeLibs(lua_State *L)
{
    // Only libs that make sense on a bare-metal controller — no io/os/package/debug.
    static const luaL_Reg libs[] = {
        { LUA_GNAME,       luaopen_base },
        { LUA_TABLIBNAME,  luaopen_table },
        { LUA_STRLIBNAME,  luaopen_string },
        { LUA_MATHLIBNAME, luaopen_math },
        { LUA_COLIBNAME,   luaopen_coroutine },
        { LUA_UTF8LIBNAME, luaopen_utf8 },
        { nullptr, nullptr }
    };
    for (const luaL_Reg *l = libs; l->func; l++) {
        luaL_requiref(L, l->name, l->func, 1);
        lua_pop(L, 1);
    }
}

int LuaPortInit()
{
    if (gL) return 0;
    ArenaInit();
    gErr[0] = '\0';
    gScriptOk = false;
    gL = lua_newstate(LuaArenaAlloc, nullptr, 0);
    if (!gL) return -1;
    lua_atpanic(gL, LuaPanic);
    OpenSafeLibs(gL);
    RegisterApi(gL);
    return 0;
}

int LuaLoadString(const char *src)
{
    if (!gL && LuaPortInit() != 0) return -1;
    gScriptOk = false;
    // Compile + run top-level once to define globals (tick/onTick/onCanRx/...).
    if (luaL_loadstring(gL, src) != LUA_OK || lua_pcall(gL, 0, 0, 0) != LUA_OK) {
        const char *m = lua_tostring(gL, -1);
        if (m) { size_t n = 0; while (m[n] && n < sizeof(gErr) - 1) { gErr[n] = m[n]; n++; } gErr[n] = '\0'; }
        lua_pop(gL, 1);
        return -2;
    }
    gScriptOk = true;
    return 0;
}

// pcall the function already on the stack top, with nargs below it, bounded by
// the instruction hook. On error: save the message and disable the script
// (don't retry a faulting script in a tight loop).
static void BoundedPcall(lua_State *L, int nargs)
{
    lua_sethook(L, CountHook, LUA_MASKCOUNT, LUA_TICK_INSTR_BUDGET);
    int rc = lua_pcall(L, nargs, 0, 0);
    lua_sethook(L, nullptr, 0, 0);
    if (rc != LUA_OK) {
        const char *m = lua_tostring(L, -1);
        if (m) { size_t n = 0; while (m[n] && n < sizeof(gErr) - 1) { gErr[n] = m[n]; n++; } gErr[n] = '\0'; }
        lua_pop(L, 1);
        gScriptOk = false;
    }
}

void LuaService()
{
    if (!gL || !gScriptOk) return;

    // 1) Deliver queued CAN frames -> onCanRx(bus, id, dlc, data).
    uint32_t id; int ext; uint8_t data[8]; int dlc;
    while (LuaBridgeCanRxPop(&id, &ext, data, &dlc)) {
        lua_getglobal(gL, "onCanRx");
        if (lua_isfunction(gL, -1)) {
            lua_pushinteger(gL, 1);                       // bus
            lua_pushinteger(gL, (lua_Integer)id);
            lua_pushinteger(gL, dlc);
            lua_createtable(gL, 8, 0);                    // data table, 1-indexed
            for (int i = 0; i < 8; i++) { lua_pushinteger(gL, data[i]); lua_seti(gL, -2, i + 1); }
            BoundedPcall(gL, 4);
        } else {
            lua_pop(gL, 1);
        }
        if (!gScriptOk) return;
    }

    // 2) Call onTick() at the configured rate.
    uint32_t now = LuaBridgeMillis();
    uint32_t interval = (gTickRateHz > 0) ? (1000u / gTickRateHz) : 10u;
    if (interval == 0) interval = 1;
    if ((uint32_t)(now - gLastTickMs) >= interval) {
        gLastTickMs = now;
        lua_getglobal(gL, "onTick");
        if (lua_isfunction(gL, -1)) BoundedPcall(gL, 0);
        else lua_pop(gL, 1);
    }
}

const char *LuaLastError() { return gErr; }

int LuaSelfTest()
{
    if (!gL && LuaPortInit() != 0) return -1;
    if (luaL_dostring(gL, "return 1+2") != LUA_OK) { lua_pop(gL, 1); return -2; }
    int r = static_cast<int>(lua_tointeger(gL, -1));
    lua_pop(gL, 1);
    return r;   // expect 3
}

unsigned LuaArenaUsed() { return gHighWater; }

#endif // HAS_LUA
