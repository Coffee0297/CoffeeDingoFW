#include "sleep.h"
#include "device.h"
#include "port.h"
#include "can.h"
#include "usb.h"
#include "status.h"
#include "mcu_utils.h"
#include "device_config.h"

#if CAN_SLEEP
#define SLEEP_BOOT_GRACE_MS 1000 // inputs and CAN inputs settle before force sleep or a request can act
#define SLEEP_REQUEST_DELAY 2000 // a sleep request waits so the other modules settle too

// Static variables that were in pdm.cpp
static uint8_t nNumOutputsOn;
static uint8_t nLastNumOutputsOn;
static uint32_t nAllOutputsOffTime;
static bool bLastUsbConnected;
static uint32_t nUsbDisconnectedTime;
static uint32_t nNoSleepRequestTime;

// External variables from pdm.cpp that we need access to
extern DeviceConfig stConfig;
bool bSleepRequest;
extern Profet pf[NUM_OUTPUTS];

bool CheckEnterSleep()
{
    bool bEnterSleep = false;

    // Count number of outputs on
    nNumOutputsOn = 0;
    for (int i = 0; i < NUM_OUTPUTS; i++)
    {
        if ((GetOutputState(i) != ProfetState::Off) &&
            !(stConfig.stDevice.bSleepIgnoreAlwaysOn && (stConfig.stOutput[i].nInput == 1)))
            nNumOutputsOn++;
    }

    // All outputs just turned off, save time - wait the sleep timeout before sleep
    if ((nNumOutputsOn == 0) && (nLastNumOutputsOn > 0))
    {
        nAllOutputsOffTime = SYS_TIME;
    }
    nLastNumOutputsOn = nNumOutputsOn;

    //USB disconnected, save time - wait the sleep timeout before sleep
    if (!GetUsbConnected() && bLastUsbConnected)
        nUsbDisconnectedTime = SYS_TIME;

    bLastUsbConnected = GetUsbConnected();

    // CAN Rx thread can stamp a time slightly after SYS_TIME was read here,
    // so treat a negative (signed) difference as 0. Wrap-safe.
    int32_t nCanRxDiff = static_cast<int32_t>(SYS_TIME - GetLastCanRxTime());
    uint32_t nCanRxIdleTime = (nCanRxDiff > 0) ? nCanRxDiff : 0;
    uint32_t nTimeout = stConfig.stDevice.nSleepTimeoutMs;

    // No outputs on, no CAN msgs received and no USB connected
    // Go to sleep after timeout
    bEnterSleep = stConfig.stDevice.bSleepEnabled &&
                  (nNumOutputsOn == 0) &&
                  (nLastNumOutputsOn == 0) &&
                  !GetUsbConnected() &&
                  ((SYS_TIME - nUsbDisconnectedTime) > nTimeout) &&
                  ((SYS_TIME - nAllOutputsOffTime) > nTimeout) &&
                  (nCanRxIdleTime > nTimeout);

    // Never with USB connected: the USB wake resets the MCU under the host and the COM port goes stale
    bool bSleepAllowed = !GetUsbConnected() && (SYS_TIME > SLEEP_BOOT_GRACE_MS);

    // Sleep request: only with sleep enabled and no outputs on, and only if that still holds after the delay
    if (!(bSleepAllowed && stConfig.stDevice.bSleepEnabled && (nNumOutputsOn == 0)))
        bSleepRequest = false;
    if (!bSleepRequest)
        nNoSleepRequestTime = SYS_TIME;
    bool bRequestSleep = (SYS_TIME - nNoSleepRequestTime) > SLEEP_REQUEST_DELAY;

    bool bForceSleep = bSleepAllowed && *pVarMap[stConfig.stDevice.nForceSleepInput];

    return bEnterSleep || bRequestSleep || bForceSleep;
}

void EnableLineEventWithPull(ioline_t line, InputPull pull) 
{
    uint32_t eventMode = PAL_EVENT_MODE_BOTH_EDGES;
    
    switch(pull) {
        case InputPull::Up:
            eventMode |= PAL_STM32_PUPDR_PULLUP;
            break;
        case InputPull::Down:
            eventMode |= PAL_STM32_PUPDR_PULLDOWN;
            break;
        default:
            eventMode |= PAL_STM32_PUPDR_FLOATING;
            break;
    }
    
    palEnableLineEvent(line, eventMode);
}

void EnterSleep()
{
    // Clear CAN sleep request so we don't immediately re-enter sleep after waking up
    bSleepRequest = false;

    // Forced sleep can arrive with outputs on - shut them off, no current protection while stopped
    #if NUM_OUTPUTS > 0
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++)
        pf[i].ForceOff();
    #endif

    // Let CanTxThread flush queued frames before the transceiver goes to standby
    chThdSleepMilliseconds(100);

    palSetLine(LINE_CAN_STANDBY); // CAN disabled

    // Set wakeup sources

    // Digital inputs change detection, with configured pullup or pulldown
    // Can be disabled in config
    if(!stConfig.stDevice.bDisableDigInWake)
    {
        for(uint8_t i = 0; i < NUM_DIG_INPUTS; i++)
        {
            if (stConfig.stDevice.nWakeDigInputMask & (1u << i))
                EnableLineEventWithPull(digIn[i].GetLine(), stConfig.stDigInput[i].ePull);
        }
    }

    // CAN receive detection - can be disabled in config
    if(!stConfig.stDevice.bDisableCanWake)
    {
        palSetLineMode(LINE_CAN_RX, PAL_MODE_INPUT);
        palEnableLineEvent(LINE_CAN_RX, PAL_EVENT_MODE_BOTH_EDGES | PAL_STM32_PUPDR_FLOATING);
    }

    // USB detection - always active
    palSetLineMode(LINE_USB_DP, PAL_MODE_INPUT);
    palEnableLineEvent(LINE_USB_DP, PAL_EVENT_MODE_BOTH_EDGES | PAL_STM32_PUPDR_FLOATING);
    palSetLineMode(LINE_USB_DM, PAL_MODE_INPUT);
    palEnableLineEvent(LINE_USB_DM, PAL_EVENT_MODE_BOTH_EDGES | PAL_STM32_PUPDR_FLOATING);

    EnterStopMode();
}

#endif