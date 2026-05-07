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
#include "wiper/wiper.h"
#include "starter.h"
#include "flasher.h"
#include "counter.h"
#include "condition.h"
#include "mailbox.h"
#include "msg.h"
#include "error.h"
#include "usb.h"
#include "keypad/keypad.h"
#include "sleep.h"
#include "request_msg.h"
#include "infomsg.h"
#include "status.h"

CanInput canIn[NUM_CAN_INPUTS];
CanOutputs canOutputs;
VirtualInput virtIn[NUM_VIRT_INPUTS];
Wiper wiper;
Starter starter;
Flasher flasher[NUM_FLASHERS];
Counter counter[NUM_COUNTERS];
Condition condition[NUM_CONDITIONS];
Keypad keypad[NUM_KEYPADS];

DeviceState eState = DeviceState::Run;
float fState; //For var map
FatalErrorType eError = FatalErrorType::NoError;
PdmConfig stConfig;
PdmConfig stConfigTemp; // Used for staging new config before applying
float *pVarMap[VAR_MAP_SIZE];

float fBattVolt;
float fTempSensor;
bool bDeviceOverTemp;
bool bDeviceCriticalTemp;
bool bSleepRequest;
bool bBootloaderRequest;

void InitVarMap();
void CyclicUpdate();
void States();

struct DeviceThread : chibios_rt::BaseStaticThread<2048>
{
    void main()
    {
        setName("DeviceThread");

        while (true)
        {
            CyclicUpdate();
            States();
            palToggleLine(LINE_E1);
            chThdSleepMilliseconds(2);
        }
    }
};
static DeviceThread deviceThread;

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

void InitDevice()
{
    Error::Initialize(&statusLed, &errorLed);

    InitVarMap(); // Set val pointers

    if (!i2cStart(&I2CD1, &i2cConfig) == HAL_RET_SUCCESS)
        Error::SetFatalError(FatalErrorType::ErrI2C, MsgSrc::Init);

    InitConfig(); // Read config from FRAM

    ApplyAllConfig();

    if(!InitAdc() == HAL_RET_SUCCESS)
        Error::SetFatalError(FatalErrorType::ErrADC, MsgSrc::Init);
        
    if(!InitCan(&stConfig.stDevConfig) == HAL_RET_SUCCESS) // Starts CAN threads
        Error::SetFatalError(FatalErrorType::ErrCAN, MsgSrc::Init);

    if (!InitUsb() == HAL_RET_SUCCESS) // Starts USB threads
        Error::SetFatalError(FatalErrorType::ErrUSB, MsgSrc::Init);

    if (!tempSensor.Init(BOARD_TEMP_WARN, BOARD_TEMP_CRIT))
        Error::SetFatalError(FatalErrorType::ErrTempSensor, MsgSrc::Init);

    Keypad::InitThread(keypad);

    InitInfoMsgs();

    palClearLine(LINE_CAN_STANDBY); // CAN enabled

    slowThreadRef = slowThread.start(NORMALPRIO);
    deviceThread.start(NORMALPRIO);
}

void States()
{
    if (bDeviceCriticalTemp)
    {
        //Turn off all outputs
        for (uint8_t i = 0; i < NUM_OUTPUTS; i++)
            pf[i].Update(false);
            
        Error::SetFatalError(FatalErrorType::ErrTemp, MsgSrc::State_Overtemp);
    }

    if (eState == DeviceState::Run)
    {
        if (bDeviceOverTemp)
            eState = DeviceState::OverTemp;

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
    }

    if (eState == DeviceState::OverTemp)
    {
        statusLed.Blink();
        errorLed.Blink();

        if (!bDeviceOverTemp)
            eState = DeviceState::Run;
    }

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

    if (eState == DeviceState::Error)
    {
        // Not required?

        //Turn off all outputs
        for (uint8_t i = 0; i < NUM_OUTPUTS; i++)
            pf[i].Update(false);

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

            for (uint8_t i = 0; i < NUM_KEYPADS; i++)
                keypad[i].CheckMsg(rxMsg);

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

    for (uint8_t i = 0; i < NUM_OUTPUTS; i++)
        pf[i].Update(starter.fVal[i]);

    for (uint8_t i = 0; i < NUM_INPUTS; i++)
        in[i].Update();

    for (uint8_t i = 0; i < NUM_CAN_INPUTS; i++)
        canIn[i].CheckTimeout();

    canOutputs.Update();

    for (uint8_t i = 0; i < NUM_VIRT_INPUTS; i++)
        virtIn[i].Update();

    wiper.Update();

    starter.Update();

    for (uint8_t i = 0; i < NUM_FLASHERS; i++)
        flasher[i].Update(SYS_TIME);

    for (uint8_t i = 0; i < NUM_COUNTERS; i++)
        counter[i].Update();

    for (uint8_t i = 0; i < NUM_CONDITIONS; i++)
        condition[i].Update();

    for (uint8_t i = 0; i < NUM_KEYPADS; i++)
        keypad[i].Update();
}

void InitVarMap()
{
    uint16_t index = 0;
    
    //System vars
    pVarMap[index++] = const_cast<float*>(&ALWAYS_FALSE);
    pVarMap[index++] = const_cast<float*>(&ALWAYS_TRUE);
    pVarMap[index++] = &fState; 
    pVarMap[index++] = &fTempSensor;
    pVarMap[index++] = &fBattVolt;

    // Digital inputs
    for (uint8_t i = 0; i < NUM_INPUTS; i++)
        pVarMap[index++] = &in[i].fVal;

    // CAN Inputs
    for (uint8_t i = 0; i < NUM_CAN_INPUTS; i++)
    {
        pVarMap[index++] = &canIn[i].fOutput;
        pVarMap[index++] = &canIn[i].fVal;
    }

    // Virtual Inputs
    for (uint8_t i = 0; i < NUM_VIRT_INPUTS; i++)
    {
        pVarMap[index++] = &virtIn[i].fVal;
    }

    // Outputs
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++)
    {
        pVarMap[index++] = &pf[i].fOutput;
        pVarMap[index++] = &pf[i].fCurrent;
        pVarMap[index++] = &pf[i].fOvercurrent;
        pVarMap[index++] = &pf[i].fFault;
    }

    // Flashers
    for (uint8_t i = 0; i < NUM_FLASHERS; i++)
    {
        pVarMap[index++] = &flasher[i].fVal;
    }

    // Conditions
    for (uint8_t i = 0; i < NUM_CONDITIONS; i++)
    {
        pVarMap[index++] = &condition[i].fVal;
    }

    // Counters
    for (uint8_t i = 0; i < NUM_COUNTERS; i++)
    {
        pVarMap[index++] = &counter[i].fVal;
    }

    // Wiper
    pVarMap[index++] = &wiper.fSlowOut;
    pVarMap[index++] = &wiper.fFastOut;
    pVarMap[index++] = &wiper.fParkOut;
    pVarMap[index++] = &wiper.fInterOut;
    pVarMap[index++] = &wiper.fWashOut;
    pVarMap[index++] = &wiper.fSwipeOut;

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

    //VarMap size must match the expected size
    if (index != VAR_MAP_SIZE)
        Error::SetFatalError(FatalErrorType::ErrVarMap, MsgSrc::Init);

}