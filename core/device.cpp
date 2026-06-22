#include "device.h"
#include "ch.hpp"
#include "hal.h"
#include "port.h"
#include "mcu_utils.h"
#include "device_config.h"
#include "config.h"
#include "param_protocol.h"
#include "config_handler.h"
#include "hw_devices.h"
#include "can.h"
#include "can_input.h"
#include "can_outputs.h"
#include "virtual_input.h"
#include "flasher.h"
#include "counter.h"
#include "condition.h"
#include "mailbox.h"
#include "msg.h"
#include "error.h"
#include "request_msg.h"
#include "infomsg.h"
#include "status.h"
#include "overload_log.h"
#include <cstring>   // memset (Lua CAN bridge)
#if HAS_LUA
#include "lua_port.h"
#endif
#if HAS_WIPERS > 0
#include "wiper/wiper.h"
#endif
#if HAS_STARTER_DISABLE > 0
#include "starter.h"
#endif
#if HAS_USB
#include "usb.h"
#endif
#if NUM_KEYPADS > 0
#include "keypad/keypad.h"
#endif
#if CAN_SLEEP
#include "sleep.h"
#endif

CanInput canIn[NUM_CAN_INPUTS];
CanOutputs canOutputs;
VirtualInput virtIn[NUM_VIRT_INPUTS];
Flasher flasher[NUM_FLASHERS];
Counter counter[NUM_COUNTERS];
Condition condition[NUM_CONDITIONS];
#if HAS_WIPERS > 0
Wiper wiper;
#endif
#if HAS_STARTER_DISABLE > 0
Starter starter;
#endif
#if NUM_KEYPADS > 0
Keypad keypad[NUM_KEYPADS];
#endif

DeviceState eState = DeviceState::Run;
float fState; //For var map
FatalErrorType eError = FatalErrorType::NoError;
#ifndef CCM_BSS
#define CCM_BSS   // boards without CCM keep these buffers in normal RAM
#endif
DeviceConfig stConfig;
DeviceConfig stConfigTemp CCM_BSS; // staging buffer for new config before applying (CCM on CanBoard)
float *pVarMap[VAR_MAP_SIZE];
#if HAS_LUA
float fLuaOut[NUM_LUA_OUTPUTS];   // Lua output slots, written by setLuaOut(n,v)
volatile bool gLuaReload = false; // set by the upload handler; LuaThread reloads
#endif

float fBattVolt;
float fTempSensor;
bool bDeviceOverTemp;
bool bDeviceCriticalTemp;
bool bSleepRequest;
bool bBootloaderRequest;

void InitVarMap();
void CyclicUpdate();
void States();
void LuaBridgeCanRxFeed(const CANRxFrame *f);   // defined below; fed from the RX drain

struct DeviceThread : chibios_rt::BaseStaticThread<DEVICE_THREAD_STACK>
{
    void main()
    {
        setName("DeviceThread");

        while (true)
        {
            // Apply a committed CAN filter / bitrate change without a power cycle.
            if (gReinitCanRequested)
            {
                gReinitCanRequested = false;
                chThdSleepMilliseconds(50);          // let the Burn reply transmit first
                ApplyConfig(CanInput::nBaseIndex);   // rebuild the filter-ID table
                InitCan(&stConfig.stDevice);         // restart CAN: re-applies filters + bitrate
            }

            CyclicUpdate();
            States();
            chThdSleepMilliseconds(2);
        }
    }
};
static DeviceThread deviceThread;

#if (HAS_BATT_VOLT_SENSE || HAS_EXT_TEMP_SENSOR)
struct SlowThread : chibios_rt::BaseStaticThread<256>
{
    void main()
    {
        setName("SlowThread");

        while (true)
        {
            //=================================================================
            // Perform tasks that don't need to be done every cycle here
            //=================================================================
            fBattVolt = GetBattVolt();

            // Temp sensor is I2C, takes a while to read
            // Don't want to slow down main thread
            fTempSensor = tempSensor.GetTemp();
            bDeviceOverTemp = tempSensor.OverTempLimit();
            bDeviceCriticalTemp = tempSensor.CritTempLimit();

            // palToggleLine(LINE_E2);
            chThdSleepMilliseconds(250);
        }
    }
};
static SlowThread slowThread;
static chibios_rt::ThreadReference slowThreadRef;
#endif

//=============================================================================
// Lua thread: runs the single embedded program's tick() at a fixed rate, well
// below the 2 ms control loop. A runaway script is bounded by the instruction
// hook inside LuaRunTick(), so it can never stall DeviceThread or the CAN stack.
//=============================================================================
#if HAS_LUA
extern volatile bool gLuaReload;

// Fallback demo program (no stored script yet): a ~1 Hz square wave on Lua output
// slot 0 via a Timer, proving setLuaOut + Timer + onTick end-to-end.
static const char LUA_DEFAULT_SCRIPT[] =
    "local t = Timer.new()\n"
    "function onTick()\n"
    "  if t:getElapsedSeconds() >= 1.0 then t:reset() end\n"
    "  setLuaOut(0, (t:getElapsedSeconds() < 0.5) and 1 or 0)\n"
    "end\n"
    "setTickRate(50)\n";

// Load the stored program if present, else the demo. Runs on the LuaThread so
// the (stack-hungry) parser uses the LuaThread's stack.
static void LuaLoadFromConfig()
{
    if (stConfig.stLua.bEnabled && stConfig.stLua.nLength > 0 &&
        stConfig.stLua.nLength < LUA_SCRIPT_MAX) {
        stConfig.stLua.acScript[stConfig.stLua.nLength] = '\0';
        LuaLoadString(stConfig.stLua.acScript);
    } else {
        LuaLoadString(LUA_DEFAULT_SCRIPT);
    }
}

struct LuaThread : chibios_rt::BaseStaticThread<12288>   // Lua compiler/VM is stack-hungry; 4K overflowed on larger scripts
{
    void main()
    {
        setName("LuaThread");
        // Let USB/CAN fully come up before touching Lua, so even if a stored script
        // misbehaves the device has already enumerated and stays re-flashable.
        chThdSleepMilliseconds(2000);
        LuaPortInit();
        LuaLoadFromConfig();
        while (true)
        {
            if (gLuaReload) { gLuaReload = false; LuaLoadFromConfig(); }
            LuaService();
            chThdSleepMilliseconds(2);   // service base rate; onTick gated by setTickRate
        }
    }
};
static LuaThread luaThread;
#endif // HAS_LUA

void InitDevice()
{
    #if HAS_SE_LEDS
    Error::Initialize(&statusLed, &errorLed);
    #endif

    InitVarMap(); // Set val pointers

    #if HAS_I2C
    if (!i2cStart(&I2CD1, &i2cConfig) == HAL_RET_SUCCESS)
        Error::SetFatalError(FatalErrorType::ErrI2C, MsgSrc::Init);
    #endif

    InitConfig();

    ApplyAllConfig();

    if(!InitAdc() == HAL_RET_SUCCESS)
        Error::SetFatalError(FatalErrorType::ErrADC, MsgSrc::Init);
        
    if(!InitCan(&stConfig.stDevice) == HAL_RET_SUCCESS) // Starts CAN threads
        Error::SetFatalError(FatalErrorType::ErrCAN, MsgSrc::Init);

    #if HAS_USB
    if (!InitUsb() == HAL_RET_SUCCESS) // Starts USB threads
        Error::SetFatalError(FatalErrorType::ErrUSB, MsgSrc::Init);
    #endif

    #if HAS_EXT_TEMP_SENSOR
    if (!tempSensor.Init(BOARD_TEMP_WARN, BOARD_TEMP_CRIT))
        Error::SetFatalError(FatalErrorType::ErrTempSensor, MsgSrc::Init);
    #endif

    #if NUM_KEYPADS > 0
    Keypad::InitThread(keypad);
    #endif

    InitInfoMsgs();

    #if NUM_OUTPUTS > 0
    OvlLogInit();
    #endif

    #if CAN_SLEEP
    palClearLine(LINE_CAN_STANDBY);
    #endif

    #if (HAS_BATT_VOLT_SENSE || HAS_EXT_TEMP_SENSOR)
    slowThreadRef = slowThread.start(NORMALPRIO);
    #endif

    #if HAS_LUA
    // Below the control loop so a Lua tick can never preempt safety-critical work.
    luaThread.start(NORMALPRIO - 1);
    #endif

    deviceThread.start(NORMALPRIO);
}

void States()
{
    #if HAS_EXT_TEMP_SENSOR
    if (bDeviceCriticalTemp)
    {
        #if NUM_OUTPUTS > 0
        //Turn off all outputs
        for (uint8_t i = 0; i < NUM_OUTPUTS; i++)
            pf[i].Update(false);
        #endif

        Error::SetFatalError(FatalErrorType::ErrTemp, MsgSrc::State_Overtemp);
    }
    #endif

    if (eState == DeviceState::Run)
    {
        #if HAS_EXT_TEMP_SENSOR
        if (bDeviceOverTemp)
            eState = DeviceState::OverTemp;
        #endif

        #if NUM_OUTPUTS > 0
        if (GetAnyOvercurrent() && !GetAnyFault())
        {
            statusLed.Blink();
            errorLed.Solid(false);
        }

        if (GetAnyFault())
        {
            statusLed.Blink();
            errorLed.Solid(true);
        }

        if (!GetAnyOvercurrent() && !GetAnyFault())
        {
            statusLed.Solid(true);
            errorLed.Solid(false);
        }
        #endif
    }

    #if HAS_EXT_TEMP_SENSOR
    if (eState == DeviceState::OverTemp)
    {
        statusLed.Blink();
        errorLed.Blink();

        if (!bDeviceOverTemp)
            eState = DeviceState::Run;
    }
    #endif

    #if CAN_SLEEP
    if (CheckEnterSleep())
    {
        statusLed.Solid(false);
        errorLed.Solid(false);
        eState = DeviceState::Sleep;
    }

    if (eState == DeviceState::Sleep)
    {
        bSleepRequest = false;
        palSetLine(LINE_CAN_STANDBY); // CAN disabled
        EnterSleep();
    }
    #endif

    if (eState == DeviceState::Error)
    {
        // Not required?

        #if NUM_OUTPUTS > 0
        //Turn off all outputs
        for (uint8_t i = 0; i < NUM_OUTPUTS; i++)
            pf[i].Update(false);
        #endif

        Error::SetFatalError(eError, MsgSrc::State_Error);
    }

    CheckInfoMsgs();

    fState = static_cast<float>(eState);
}

void CyclicUpdate()
{
    CANRxFrame rxMsg;

    while (!RxFramesEmpty())
    {
        msg_t res = FetchRxFrame(&rxMsg);
        if (res == MSG_OK)
        {
            for (uint8_t i = 0; i < NUM_CAN_INPUTS; i++)
                canIn[i].CheckMsg(rxMsg);

            #if HAS_LUA
            LuaBridgeCanRxFeed(&rxMsg);   // queue for onCanRx() if id was registered
            #endif

            #if NUM_KEYPADS > 0
            for (uint8_t i = 0; i < NUM_KEYPADS; i++)
                keypad[i].CheckMsg(rxMsg);
            #endif

            CheckRequestMsgs(&rxMsg);
            
            uint16_t nIndex = 0;
            MsgCmd cmd = ProcessParamMsg(&rxMsg, &nIndex);
            if (cmd == MsgCmd::WriteAllComplete)
            {
                ApplyAllConfig();
            }
            if (cmd == MsgCmd::Write)
            {
                ApplyConfig(nIndex & 0xFF00); // Mask instance, only base index is needed
            }
        }
    }

    #if NUM_OUTPUTS > 0
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++)
        pf[i].Update(starter.fVal[i]);

    // Overload log: decimate the 500Hz loop to one OVL_SAMPLE_MS waveform point (the
    // peak over the bucket, via GetPeakReset), and log a trip on the rising edge into
    // Overcurrent/Fault. Raw GetState() is used so Warning/OpenLoad don't count as trips.
    {
        static uint8_t nOvlDiv = 0;
        static ProfetState ePrevRaw[NUM_OUTPUTS];
        bool bSample = (++nOvlDiv >= (OVL_SAMPLE_MS / 2));
        if (bSample) nOvlDiv = 0;
        for (uint8_t i = 0; i < NUM_OUTPUTS; i++)
        {
            if (bSample)
                OvlLogSample(i, pf[i].GetPeakReset());

            ProfetState raw = pf[i].GetState();
            bool bTrip = (raw == ProfetState::Overcurrent || raw == ProfetState::Fault);
            bool bWasTrip = (ePrevRaw[i] == ProfetState::Overcurrent || ePrevRaw[i] == ProfetState::Fault);
            if (bTrip && !bWasTrip)
                OvlLogTrigger(i, raw, stConfig.stOutput[i].fCurrentLimit);
            ePrevRaw[i] = raw;
        }
    }
    #endif

    #if NUM_DIG_INPUTS > 0
    for (uint8_t i = 0; i < NUM_DIG_INPUTS; i++)
        digIn[i].Update();
    #endif

    #if NUM_DIG_OUTPUTS > 0
    for (uint8_t i = 0; i < NUM_DIG_OUTPUTS; i++)
        digOut[i].Update();
    #endif

    #if NUM_ANALOG_INPUTS > 0
    for (uint8_t i = 0; i < NUM_ANALOG_INPUTS; i++)
        analogIn[i].Update();
    #endif

    #if NUM_CAN_INPUTS > 0
    for (uint8_t i = 0; i < NUM_CAN_INPUTS; i++)
        canIn[i].CheckTimeout();
    #endif

    #if NUM_CAN_OUTPUTS > 0
    canOutputs.Update();
    #endif

    #if NUM_VIRT_INPUTS > 0
    for (uint8_t i = 0; i < NUM_VIRT_INPUTS; i++)
        virtIn[i].Update();
    #endif

    #if HAS_WIPERS
    wiper.Update();
    #endif

    #if HAS_STARTER_DISABLE
    starter.Update();
    #endif

    #if NUM_FLASHERS > 0
    for (uint8_t i = 0; i < NUM_FLASHERS; i++)
        flasher[i].Update(SYS_TIME);
    #endif

    #if NUM_COUNTERS > 0
    for (uint8_t i = 0; i < NUM_COUNTERS; i++)
        counter[i].Update();
    #endif

    #if NUM_CONDITIONS > 0    
    for (uint8_t i = 0; i < NUM_CONDITIONS; i++)
        condition[i].Update();
    #endif

    #if NUM_KEYPADS > 0
    for (uint8_t i = 0; i < NUM_KEYPADS; i++)
        keypad[i].Update();
    #endif
}

void InitVarMap()
{
    uint16_t index = 0;
    
    //System vars
    pVarMap[index++] = const_cast<float*>(&ALWAYS_FALSE);
    pVarMap[index++] = const_cast<float*>(&ALWAYS_TRUE);
    pVarMap[index++] = &fState; 
    #if HAS_EXT_TEMP_SENSOR
    pVarMap[index++] = &fTempSensor;
    #endif
    #if HAS_BATT_VOLT_SENSE
    pVarMap[index++] = &fBattVolt;
    #endif

    #if NUM_DIG_INPUTS > 0
    for (uint8_t i = 0; i < NUM_DIG_INPUTS; i++)
        pVarMap[index++] = &digIn[i].fVal;
    #endif

    #if NUM_DIG_OUTPUTS > 0
    for (uint8_t i = 0; i < NUM_DIG_OUTPUTS; i++)
        pVarMap[index++] = &digOut[i].fVal;
    #endif

    #if NUM_ANALOG_INPUTS > 0
    for (uint8_t i = 0; i < NUM_ANALOG_INPUTS; i++)
    {
        pVarMap[index++] = &analogIn[i].fVal;
        pVarMap[index++] = &analogIn[i].fValMillivolts;
        pVarMap[index++] = &analogIn[i].fRotaryPos;
        pVarMap[index++] = &analogIn[i].fSwitchVal;
        pVarMap[index++] = &analogIn[i].fScaledVal;
    }
    #endif

    #if NUM_CAN_INPUTS > 0
    for (uint8_t i = 0; i < NUM_CAN_INPUTS; i++)
    {
        pVarMap[index++] = &canIn[i].fOutput;
        pVarMap[index++] = &canIn[i].fVal;
    }
    #endif

    #if NUM_VIRT_INPUTS > 0
    for (uint8_t i = 0; i < NUM_VIRT_INPUTS; i++)
    {
        pVarMap[index++] = &virtIn[i].fVal;
    }
    #endif

    #if NUM_OUTPUTS > 0
    // Outputs
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++)
    {
        pVarMap[index++] = &pf[i].fOutput;
        pVarMap[index++] = &pf[i].fCurrent;
        pVarMap[index++] = &pf[i].fOvercurrent;
        pVarMap[index++] = &pf[i].fFault;
    }
    #endif

    #if NUM_FLASHERS > 0
    // Flashers
    for (uint8_t i = 0; i < NUM_FLASHERS; i++)
    {
        pVarMap[index++] = &flasher[i].fVal;
    }
    #endif

    #if NUM_CONDITIONS > 0
    // Conditions
    for (uint8_t i = 0; i < NUM_CONDITIONS; i++)
    {
        pVarMap[index++] = &condition[i].fVal;
    }
    #endif

    #if NUM_COUNTERS > 0
    // Counters
    for (uint8_t i = 0; i < NUM_COUNTERS; i++)
    {
        pVarMap[index++] = &counter[i].fVal;
    }
    #endif

    #if HAS_WIPERS > 0
    // Wiper
    pVarMap[index++] = &wiper.fSlowOut;
    pVarMap[index++] = &wiper.fFastOut;
    pVarMap[index++] = &wiper.fParkOut;
    pVarMap[index++] = &wiper.fInterOut;
    pVarMap[index++] = &wiper.fWashOut;
    pVarMap[index++] = &wiper.fSwipeOut;
    #endif

    #if NUM_KEYPADS > 0
    // Keypads
    for (uint8_t i = 0; i < NUM_KEYPADS; i++)
    {
        for (uint8_t j = 0; j < KEYPAD_MAX_BUTTONS; j++)
        {
            pVarMap[index++] = &keypad[i].fButtonVal[j];
        }

        for (uint8_t j = 0; j < KEYPAD_MAX_DIALS; j++)
        {
            pVarMap[index++] = &keypad[i].fDialVal[j];
        }

        for (uint8_t j = 0; j < KEYPAD_MAX_ANALOG_INPUTS; j++)
        {
            pVarMap[index++] = &keypad[i].fAnalogVal[j];
        }
    }
    #endif

    #if HAS_LUA
    // Lua output slots (written by setLuaOut(n,v) from the Lua program). An output,
    // virtual input, CAN output, etc. is driven by Lua by setting its nInput to one
    // of these indices. This block is intentionally last so existing indices are stable.
    for (uint16_t i = 0; i < NUM_LUA_OUTPUTS; i++)
        pVarMap[index++] = &fLuaOut[i];
    #endif

    //VarMap size must match the expected size
    if (index != VAR_MAP_SIZE)
        Error::SetFatalError(FatalErrorType::ErrVarMap, MsgSrc::Init);

}

//=============================================================================
// Lua bridge: lets the Lua port (lua_port.cpp) read any signal and drive the
// Lua output slots, without exposing the var-map internals to C.
//=============================================================================
#if HAS_LUA
extern "C" float LuaBridgeReadVar(int idx)
{
    if (idx < 0 || idx >= VAR_MAP_SIZE || pVarMap[idx] == nullptr) return 0.0f;
    return *pVarMap[idx];
}

extern "C" void LuaBridgeSetOut(int slot, float val)
{
    if (slot >= 0 && slot < NUM_LUA_OUTPUTS) fLuaOut[slot] = val;
}

extern "C" uint32_t LuaBridgeMillis() { return (uint32_t)SYS_TIME; }

//-----------------------------------------------------------------------------
// Lua CAN bridge. Producer = DeviceThread (CyclicUpdate RX drain); consumer =
// LuaThread. Single-producer/single-consumer ring, so volatile head/tail indices
// are sufficient without locks.
//-----------------------------------------------------------------------------
#define LUA_CANRX_REG_MAX 16
#define LUA_CANRX_QUEUE   16
static volatile uint32_t luaRxIds[LUA_CANRX_REG_MAX];
static volatile uint8_t  luaRxIdCount = 0;
struct LuaRxFrame { uint32_t id; uint8_t ext; uint8_t dlc; uint8_t data[8]; };
static LuaRxFrame        luaRxQ[LUA_CANRX_QUEUE];
static volatile uint8_t  luaRxHead = 0;   // producer
static volatile uint8_t  luaRxTail = 0;   // consumer

extern "C" void LuaBridgeCanRxRegister(uint32_t id)
{
    for (uint8_t i = 0; i < luaRxIdCount; i++)
        if (luaRxIds[i] == id) return;
    if (luaRxIdCount < LUA_CANRX_REG_MAX) luaRxIds[luaRxIdCount++] = id;
}

// Called from CyclicUpdate for every RX frame; queues it only if its id was
// registered via canRxAdd().
void LuaBridgeCanRxFeed(const CANRxFrame *f)
{
    uint32_t id = (f->IDE == CAN_IDE_EXT) ? f->EID : f->SID;
    bool match = false;
    for (uint8_t i = 0; i < luaRxIdCount; i++)
        if (luaRxIds[i] == id) { match = true; break; }
    if (!match) return;

    uint8_t next = (uint8_t)((luaRxHead + 1) % LUA_CANRX_QUEUE);
    if (next == luaRxTail) return;   // full — drop oldest-policy: just drop new
    LuaRxFrame *d = &luaRxQ[luaRxHead];
    d->id = id;
    d->ext = (f->IDE == CAN_IDE_EXT);
    d->dlc = f->DLC;
    for (int i = 0; i < 8; i++) d->data[i] = f->data8[i];
    luaRxHead = next;
}

extern "C" int LuaBridgeCanRxPop(uint32_t *id, int *ext, uint8_t *data, int *dlc)
{
    if (luaRxTail == luaRxHead) return 0;
    LuaRxFrame *s = &luaRxQ[luaRxTail];
    *id = s->id;
    *ext = s->ext;
    *dlc = s->dlc;
    for (int i = 0; i < 8; i++) data[i] = s->data[i];
    luaRxTail = (uint8_t)((luaRxTail + 1) % LUA_CANRX_QUEUE);
    return 1;
}

extern "C" void LuaBridgeTxCan(uint32_t id, int ext, const uint8_t *data, int dlc)
{
    CANTxFrame f;
    memset(&f, 0, sizeof(f));
    f.IDE = ext ? CAN_IDE_EXT : CAN_IDE_STD;
    if (ext) f.EID = id; else f.SID = id;
    f.RTR = CAN_RTR_DATA;
    if (dlc > 8) dlc = 8;
    f.DLC = (uint8_t)dlc;
    for (int i = 0; i < dlc; i++) f.data8[i] = data[i];
    PostTxFrame(&f);
}
#endif // HAS_LUA