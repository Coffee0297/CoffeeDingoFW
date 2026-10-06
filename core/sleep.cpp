#include "sleep.h"
#include "device.h"
#include "port.h"
#include "can.h"
#include "usb.h"
#include "status.h"
#include "mcu_utils.h"
#include "device_config.h"

#if CAN_SLEEP
#define SLEEP_REQUEST_DELAY 2000   // FW #52: wait after a sleep request so other modules settle
#define SLEEP_BOOT_GRACE_MS 1000   // let inputs debounce / CAN inputs arrive before honouring force-sleep

// Static variables that were in pdm.cpp
static uint8_t nNumOutputsOn;
static uint8_t nLastNumOutputsOn;
static uint32_t nAllOutputsOffTime;
static bool bLastUsbConnected;
static uint32_t nUsbDisconnectedTime;

// Sleep-request coordination (FW #52)
static bool bSleepPending;          // a request is in its settle window
static uint32_t nSleepPendingTime;  // when the request started

// External variables from pdm.cpp that we need access to
extern DeviceConfig stConfig;
extern bool bSleepRequest;
extern Profet pf[NUM_OUTPUTS];
#if NUM_DIG_INPUTS > 0
extern Digital_Input digIn[NUM_DIG_INPUTS];
#endif

// Outputs currently on, optionally ignoring always-on outputs (varmap index 1 = ALWAYS_TRUE).
// Without ignoring them an always-on output would block sleep forever (FW #52).
static uint8_t CountOutputsOn()
{
    uint8_t n = 0;
#if NUM_OUTPUTS > 0
    for (int i = 0; i < NUM_OUTPUTS; i++)
    {
        if (GetOutputState(i) == ProfetState::Off) continue;
        if (stConfig.stDevice.bSleepIgnoreAlwaysOn && stConfig.stOutput[i].nInput == 1) continue;
        n++;
    }
#endif
    return n;
}

// True while the var-map signal picked as the force-sleep input is asserted (0 = unused).
static bool ForceSleepAsserted()
{
    const uint16_t n = stConfig.stDevice.nForceSleepInput;
    return n != 0 && n < VAR_MAP_SIZE && pVarMap[n] != nullptr && *pVarMap[n] != 0.0f;
}

bool CheckEnterSleep()
{
    bool bEnterSleep = false;

    // Effective outputs-on count (honours "ignore always-on").
    nNumOutputsOn = CountOutputsOn();

    // All outputs just turned off, save time - wait the timeout before sleep
    if ((nNumOutputsOn == 0) && (nLastNumOutputsOn > 0))
        nAllOutputsOffTime = SYS_TIME;
    nLastNumOutputsOn = nNumOutputsOn;

    //USB disconnected, save time - wait the timeout before sleep
    if (!GetUsbConnected() && bLastUsbConnected)
        nUsbDisconnectedTime = SYS_TIME;
    bLastUsbConnected = GetUsbConnected();

    // Had issue with SYS_TIME being < GetLastCanRxTime when msgs come in quickly
    uint32_t sys = SYS_TIME;
    uint32_t canLast = GetLastCanRxTime();
    uint32_t nCanRxIdleTime = (sys >= canLast) ? (sys - canLast) : 0;

    uint32_t nTimeout = stConfig.stDevice.nSleepTimeoutMs ? stConfig.stDevice.nSleepTimeoutMs : SLEEP_TIMEOUT;

    // ---- 1. Force sleep (FW #52) ----------------------------------------------------
    // Any var-map signal — a digital input (ignition), a CAN input (a fleet "sleep now"
    // message), a Timer output, a virtual input — forces sleep immediately, ignoring the
    // CAN-idle and outputs-on restrictions and the auto-sleep enable. The decision stays
    // local to this module: no handshake with anyone. USB must be unplugged (USB activity
    // instantly wakes the MCU -> reset -> stale COM port, the #36 soft-lock). A short boot
    // grace lets debounce / CAN inputs settle so a half-read pin can't put a freshly woken
    // module straight back to sleep.
    if (ForceSleepAsserted() && !GetUsbConnected() && sys > SLEEP_BOOT_GRACE_MS)
        return true;

    // ---- 2. Automatic sleep --------------------------------------------------------
    // No outputs on, no CAN traffic, no USB, for the configured timeout.
    bEnterSleep = stConfig.stDevice.bSleepEnabled &&
                  (nNumOutputsOn == 0) &&
                  (nLastNumOutputsOn == 0) &&
                  !GetUsbConnected() &&
                  ((SYS_TIME - nUsbDisconnectedTime) > nTimeout) &&
                  ((SYS_TIME - nAllOutputsOffTime) > nTimeout) &&
                  (nCanRxIdleTime > nTimeout);

    // ---- 3. Sleep request (tool / CAN sleep msg) -----------------------------------
    // Honour only when safe: sleep enabled, no USB, no outputs on (CAN idle NOT required).
    // Then wait SLEEP_REQUEST_DELAY so other modules settle before sleeping. A module that
    // can't sleep keeps broadcasting, which wakes any module that already slept (FW #52
    // "if any device failed to sleep, all are woken").
    bool bRequestConditions = stConfig.stDevice.bSleepEnabled &&
                              (nNumOutputsOn == 0) &&
                              !GetUsbConnected();
    if (bSleepRequest)
    {
        if (!bRequestConditions) { bSleepRequest = false; bSleepPending = false; }   // reject unsafe request
        else if (!bSleepPending) { bSleepPending = true; nSleepPendingTime = SYS_TIME; }
    }

    bool bRequestSleep = false;
    if (bSleepPending)
    {
        if (!bRequestConditions) { bSleepPending = false; bSleepRequest = false; }    // conditions broke -> abort
        else if ((SYS_TIME - nSleepPendingTime) > SLEEP_REQUEST_DELAY) bRequestSleep = true;
    }

    return bEnterSleep || bRequestSleep;
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

static void ArmUsbWake()
{
    palSetLineMode(LINE_USB_DP, PAL_MODE_INPUT);
    palEnableLineEvent(LINE_USB_DP, PAL_EVENT_MODE_BOTH_EDGES | PAL_STM32_PUPDR_FLOATING);
    palSetLineMode(LINE_USB_DM, PAL_MODE_INPUT);
    palEnableLineEvent(LINE_USB_DM, PAL_EVENT_MODE_BOTH_EDGES | PAL_STM32_PUPDR_FLOATING);
}

void EnterSleep()
{
    // Wake sources are configurable (FW #52): each digital input has its own checkbox
    // (nWakeDigInputMask) and CAN traffic can be excluded (bWakeOnCan) so a module parked on
    // a chattering bus — or one meant to wake only from its ignition pin — stays asleep.
    // USB is always armed so plugging in for config always recovers the module.

#if NUM_DIG_INPUTS > 0
    for (uint8_t i = 0; i < NUM_DIG_INPUTS; i++)
    {
        if (stConfig.stDevice.nWakeDigInputMask & (1u << i))
            EnableLineEventWithPull(digIn[i].GetLine(), stConfig.stDigInput[i].ePull);
    }
#endif

    if (stConfig.stDevice.bWakeOnCan)
    {
        palSetLineMode(LINE_CAN_RX, PAL_MODE_INPUT);
        palEnableLineEvent(LINE_CAN_RX, PAL_EVENT_MODE_BOTH_EDGES | PAL_STM32_PUPDR_FLOATING);
    }

    ArmUsbWake();

    EnterStopMode();
}

#endif
