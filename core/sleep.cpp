#include "sleep.h"
#include "device.h"
#include "port.h"
#include "can.h"
#if HAS_USB
#include "usb.h"
#else
static bool GetUsbConnected() { return false; }   // no USB port (CANBoard)
#endif
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

// 5.6 sleep status for the tool
static uint8_t nCountdown = 0xFF, nBlockers = 0, nWhy = 0;
static WakeReason eWake = WakeReason::PowerOn;
static uint8_t nWakeSource = 0;
static uint16_t nWakes = 0;

// Kept across the wake reset: RAM the startup code does not clear (top of SRAM next to the bootloader words on
// the PDMs, CCM next to the bootloader magic on the CANBoard). ponytail: a magic word marks it valid; on a board
// or emulator that clears RAM on reset the reason reads PowerOn.
#if defined(STM32F4XX)
static volatile uint32_t *const kWakeKeep = (volatile uint32_t *)0x2001FFE0;
#else
static volatile uint32_t *const kWakeKeep = (volatile uint32_t *)0x10000FF0;
#endif
#define WAKE_MAGIC 0x57414B45u   // "WAKE"
#define WAKE_NEW   0x80000000u   // kWakeKeep[1]: the reason is of a wake not yet read (a plain reset keeps only the count)

// External variables from pdm.cpp that we need access to
extern DeviceConfig stConfig;
extern bool bSleepRequest;
#if NUM_OUTPUTS > 0
extern Profet pf[NUM_OUTPUTS];
#endif
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

// CheckEnterSleep — called every control-loop tick; true = go to sleep now (Sleep state ->
// outputs forced off, CAN transceiver to standby, EnterSleep()). Three ways in:
//
//   1. Force sleep   nForceSleepInput != 0 and that var-map signal is true (ignition DI, CAN
//                    input, timer, virtual input, Lua var). Immediate: ignores outputs-on,
//                    CAN traffic and bSleepEnabled. Blocked only by USB connected and the
//                    first SLEEP_BOOT_GRACE_MS after boot.
//   2. Auto sleep    bSleepEnabled and, for nSleepTimeoutMs (0 = SLEEP_TIMEOUT): no outputs on
//                    (bSleepIgnoreAlwaysOn skips ALWAYS_TRUE outputs), USB unplugged, and no
//                    CAN RX that passes this module's filter. One chatty sender keeps it awake.
//   3. Sleep request CAN "Sleep QUIT" to BaseId-1 (dingoConfig's Sleep button) sets bSleepRequest.
//                    Needs bSleepEnabled, no outputs on, no USB (CAN idle not needed), then waits
//                    SLEEP_REQUEST_DELAY; any condition breaking in that window cancels it.
//
// Mute (nMuteTxInput, comms/can.cpp) is not a sleep mode: it only withholds the cyclic telemetry
// so the fleet can go quiet and every module's auto-sleep timer can run out. CAN outputs, Lua
// txCan and config replies still transmit and still count as bus traffic.
//
// Wake (EnterSleep): DIs in nWakeDigInputMask, CAN RX edge if bWakeOnCan, USB always. Any wake is a
// full MCU reset — vars, timers and Lua state start from zero; analog inputs cannot wake.
// Boards with CAN_SLEEP FALSE never sleep; for them only mute applies.
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
    {
        nWhy = 2;
        return true;
    }

    // ---- 5.6 status: what blocks auto-sleep, and how long until it if nothing does ----------
    uint32_t elapsed = SYS_TIME - nAllOutputsOffTime;
    if ((SYS_TIME - nUsbDisconnectedTime) < elapsed) elapsed = SYS_TIME - nUsbDisconnectedTime;
    if (nCanRxIdleTime < elapsed) elapsed = nCanRxIdleTime;
    nBlockers = (stConfig.stDevice.bSleepEnabled ? 0 : 1) | (nNumOutputsOn > 0 ? 2 : 0) | (GetUsbConnected() ? 4 : 0) |
                (nCanRxIdleTime < nTimeout && nCanRxIdleTime == elapsed ? 8 : 0);
    nCountdown = (nBlockers & 7) ? 0xFF : (elapsed >= nTimeout ? 0 : (uint8_t)((nTimeout - elapsed + 999) / 1000));

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

    if (bEnterSleep) nWhy = 1;
    else if (bRequestSleep) nWhy = 3;
    return bEnterSleep || bRequestSleep;
}

uint8_t SleepCountdownSec() { return nCountdown; }
uint8_t SleepBlockers() { return nBlockers; }
uint8_t SleepWhy() { return nWhy; }
WakeReason GetWakeReason() { return eWake; }

// Quiet wake (5.6.3): woken by CAN traffic, the module stays silent until it hears a real frame (anything but
// another module's goodbye). A goodbye wakes every sleeping module; were they to restart talking (they forget
// "sleep now" across the STOP reset), they would wake each other for ever. With nothing real on the bus it
// goes back to sleep without a goodbye of its own.
static bool bQuietWake = false;
bool QuietWake() { return bQuietWake; }
void EndQuietWake() { if (bQuietWake) { bQuietWake = false; SendSleepInfo(); } }
uint8_t GetWakeSource() { return nWakeSource; }
uint16_t GetWakeCount() { return nWakes; }

void CaptureWakeAfterStop()
{
    // EXTI pending bits = the lines that ended STOP: CAN RX, USB D+/D-, or a digital input
    const uint32_t pr = EXTI->PR;
    // Only an armed source ends STOP: with no digital input or USB line pending it was CAN (a short frame can
    // leave no pending bit by the time this runs).
    uint32_t reason = static_cast<uint32_t>(stConfig.stDevice.bWakeOnCan ? WakeReason::Can : WakeReason::PowerOn), source = 0;
    if (pr & (1u << PAL_PAD(LINE_CAN_RX))) reason = static_cast<uint32_t>(WakeReason::Can);
#if HAS_USB
    if (pr & ((1u << PAL_PAD(LINE_USB_DP)) | (1u << PAL_PAD(LINE_USB_DM)))) reason = static_cast<uint32_t>(WakeReason::Usb);
#endif
#if NUM_DIG_INPUTS > 0
    for (uint8_t i = 0; i < NUM_DIG_INPUTS; i++)
        if ((stConfig.stDevice.nWakeDigInputMask & (1u << i)) && (pr & (1u << PAL_PAD(digIn[i].GetLine()))))
        { reason = static_cast<uint32_t>(WakeReason::DigitalInput); source = i + 1; break; }
#endif
    const uint32_t wakes = ((kWakeKeep[0] == WAKE_MAGIC ? (kWakeKeep[1] >> 16) & 0x7FFF : 0) + 1) & 0x7FFF;
    kWakeKeep[1] = WAKE_NEW | (wakes << 16) | (source << 8) | reason;
    kWakeKeep[0] = WAKE_MAGIC;
}

void ReadWakeAtBoot()
{
    // The count runs from power-on: it was cleared at every boot, so it read 1 after any number of wakes. Power-on
    // leaves no magic (count 0); a reset that is not a wake keeps the count but reports power-on.
    if (kWakeKeep[0] != WAKE_MAGIC)
        return;
    const uint32_t v = kWakeKeep[1];
    nWakes = (uint16_t)((v >> 16) & 0x7FFF);
    if (v & WAKE_NEW)
    {
        eWake = static_cast<WakeReason>(v & 0xFF);
        nWakeSource = (v >> 8) & 0xFF;
        bQuietWake = eWake == WakeReason::Can;
        kWakeKeep[1] = v & ~WAKE_NEW;
    }
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
#if HAS_USB
    palSetLineMode(LINE_USB_DP, PAL_MODE_INPUT);
    palEnableLineEvent(LINE_USB_DP, PAL_EVENT_MODE_BOTH_EDGES | PAL_STM32_PUPDR_FLOATING);
    palSetLineMode(LINE_USB_DM, PAL_MODE_INPUT);
    palEnableLineEvent(LINE_USB_DM, PAL_EVENT_MODE_BOTH_EDGES | PAL_STM32_PUPDR_FLOATING);
#endif
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
        digIn[i].StopPwm();   // its edge event is re-armed (or not) as a wake source below
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
