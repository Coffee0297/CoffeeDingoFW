#include "sleep.h"
#include "device.h"
#include "port.h"
#include "can.h"
#include "usb.h"
#include "status.h"
#include "mcu_utils.h"
#include "device_config.h"
#include "mailbox.h"
#include <algorithm>

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

// For SleepInfo: seconds until auto sleep (0xFF = not counting), what blocks it, why it is going to sleep
static uint8_t nCountdown = 0xFF;
static uint8_t nBlockers;
static uint8_t nWhy;

// Why it woke and how often since power-on, kept across the wake reset in ChibiOS's no-init RAM (.ram0 is
// reserved by the linker and not cleared at startup). Valid only with the magic.
// nWakeKeep[1] = WAKE_NEW | wakes << 16 | source << 8 | reason
// nWakeKeep[2] = the digital inputs when it went to sleep (bit i = DI i+1 on), what a quiet wake compares against
static volatile uint32_t nWakeKeep[3] __attribute__((section(".ram0")));
static uint32_t nDiAtSleep;
#define WAKE_MAGIC 0x57414B45 // "WAKE"
#define WAKE_NEW 0x80000000   // a wake not yet reported; a reset that isn't a wake keeps only the count
static WakeReason eWakeReason = WakeReason::PowerOn;
static uint8_t nWakeSource;
static uint16_t nWakes;

// Woken by CAN and not yet heard a real frame: nothing periodic goes out (see CheckQuietWake)
static volatile bool bQuietWake;

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
    // A quiet wake was asleep a moment ago and heard nothing real since: it goes back to sleep
    bEnterSleep = (stConfig.stDevice.bSleepEnabled || bQuietWake) &&
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

    // Blockers: 1 sleep disabled, 2 outputs on, 4 USB, 8 CAN traffic within the timeout
    nBlockers = (stConfig.stDevice.bSleepEnabled ? 0 : 1) | ((nNumOutputsOn > 0) ? 2 : 0) |
                (GetUsbConnected() ? 4 : 0) | ((nCanRxIdleTime < nTimeout) ? 8 : 0);
    uint32_t nIdle = std::min({SYS_TIME - nAllOutputsOffTime, SYS_TIME - nUsbDisconnectedTime, nCanRxIdleTime});
    if (nBlockers & 7)
        nCountdown = 0xFF;
    else
        nCountdown = (nIdle >= nTimeout) ? 0 : (nTimeout - nIdle + 999) / 1000;
    nWhy = bForceSleep ? 2 : (bRequestSleep ? 3 : 1);

    return bEnterSleep || bRequestSleep || bForceSleep;
}

uint8_t SleepCountdownSec()
{
    return nCountdown;
}

void SendSleepInfo()
{
    CANTxFrame txMsg;
    txMsg.SID = stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET;
    txMsg.IDE = CAN_IDE_STD;
    txMsg.DLC = 8;
    txMsg.data8[0] = static_cast<uint8_t>(MsgCmd::SleepInfo);
    txMsg.data8[1] = static_cast<uint8_t>(eWakeReason);
    txMsg.data8[2] = nWakeSource;
    txMsg.data8[3] = nWakes & 0xFF;
    txMsg.data8[4] = nWakes >> 8;
    txMsg.data8[5] = nCountdown;
    txMsg.data8[6] = nBlockers;
    txMsg.data8[7] = 0;
    PostTxFrame(&txMsg);
}

// At boot, before the CAN threads start: pick up the wake left by CaptureWake
void InitSleep()
{
    // No magic = power-on (count 0); a reset that isn't a wake keeps the count and reports PowerOn
    if (nWakeKeep[0] == WAKE_MAGIC)
    {
        uint32_t nKeep = nWakeKeep[1];
        nWakes = (nKeep >> 16) & 0x7FFF;
        if (nKeep & WAKE_NEW)
        {
            eWakeReason = static_cast<WakeReason>(nKeep & 0xFF);
            nWakeSource = (nKeep >> 8) & 0xFF;
            nWakeKeep[1] = nKeep & ~WAKE_NEW;
            bQuietWake = (eWakeReason == WakeReason::Can);
            nDiAtSleep = nWakeKeep[2];
        }
    }
}

bool QuietWake()
{
    return bQuietWake;
}

// CAN RX thread. A goodbye wakes every sleeping module; if each then started talking (they forget "sleep now"
// across the wake reset) they would wake each other for ever. So only a frame that isn't another module's
// goodbye [Sleep, 'Z', 'Z', 'Z', ...] ends a quiet wake; then the module announces why it is awake.
void CheckQuietWake(const CANRxFrame *frame)
{
    if (bQuietWake &&
        !((frame->DLC == 8) && (frame->data8[0] == static_cast<uint8_t>(MsgCmd::Sleep)) &&
          (frame->data8[1] == 'Z') && (frame->data8[2] == 'Z') && (frame->data8[3] == 'Z')))
    {
        bQuietWake = false;
        SendSleepInfo();
    }
}

// Device loop, after the inputs update. A wake input of its own that is not as it was when the module went to
// sleep also ends a quiet wake: a switch turned on while it slept or listens quietly (the hazards with the key
// off) is a reason to be awake. EXTI->PR can miss that edge (CaptureWake then says CAN), and once it went by
// no later wake from STOP would bring it.
void CheckQuietWakeInputs()
{
    #if NUM_DIG_INPUTS > 0
    if (!bQuietWake || stConfig.stDevice.bDisableDigInWake)
        return;
    for (uint8_t i = 0; i < NUM_DIG_INPUTS; i++)
    {
        // a latched input's state is lost in the reset, only a momentary one reads back as it was
        if (!(stConfig.stDevice.nWakeDigInputMask & (1U << i)) || stConfig.stDigInput[i].eMode != InputMode::Momentary)
            continue;
        if ((digIn[i].fVal != 0) != (((nDiAtSleep >> i) & 1U) != 0))
        {
            eWakeReason = WakeReason::DigitalInput;
            nWakeSource = i + 1;
            bQuietWake = false;
            SendSleepInfo();
            return;
        }
    }
    #endif
}

// Right after STOP ends, before the reset clears the EXTI pending bits: which armed line woke it
static void CaptureWake()
{
    uint32_t nPr = EXTI->PR1; // ChibiOS aliases PR1 to PR on the F3/F4
    // A short CAN frame can be gone from PR by now; with CAN armed and nothing else pending, it was CAN
    WakeReason eReason = stConfig.stDevice.bDisableCanWake ? WakeReason::PowerOn : WakeReason::Can;
    uint8_t nSource = 0;
    if (nPr & ((1U << PAL_PAD(LINE_USB_DP)) | (1U << PAL_PAD(LINE_USB_DM))))
        eReason = WakeReason::Usb;
    if (!stConfig.stDevice.bDisableDigInWake)
    {
        for (uint8_t i = 0; i < NUM_DIG_INPUTS; i++)
        {
            if ((stConfig.stDevice.nWakeDigInputMask & (1U << i)) && (nPr & (1U << PAL_PAD(digIn[i].GetLine()))))
            {
                eReason = WakeReason::DigitalInput;
                nSource = i + 1;
                break;
            }
        }
    }
    uint32_t nCount = (nWakeKeep[0] == WAKE_MAGIC) ? ((nWakeKeep[1] >> 16) & 0x7FFF) : 0;
    nWakeKeep[1] = WAKE_NEW | (((nCount + 1) & 0x7FFF) << 16) | (nSource << 8) | static_cast<uint32_t>(eReason);
    nWakeKeep[0] = WAKE_MAGIC;
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

    // Goodbye, so the tool can tell asleep from unplugged: [Sleep, 'Z', 'Z', 'Z', why (1 auto, 2 forced, 3 request)].
    // Not from a quiet wake: it would wake the others again.
    if (!bQuietWake)
    {
        CANTxFrame txMsg;
        txMsg.SID = stConfig.stDevice.nBaseId + CONFIG_TX_OFFSET;
        txMsg.IDE = CAN_IDE_STD;
        txMsg.DLC = 8;
        txMsg.data8[0] = static_cast<uint8_t>(MsgCmd::Sleep);
        txMsg.data8[1] = 'Z';
        txMsg.data8[2] = 'Z';
        txMsg.data8[3] = 'Z';
        txMsg.data8[4] = nWhy;
        txMsg.data8[5] = 0;
        txMsg.data8[6] = 0;
        txMsg.data8[7] = 0;
        PostTxFrame(&txMsg);
    }

    // Let CanTxThread flush queued frames before the transceiver goes to standby
    chThdSleepMilliseconds(100);

    palSetLine(LINE_CAN_STANDBY); // CAN disabled

    // What the inputs were at sleep, for the quiet wake after (CheckQuietWakeInputs)
    #if NUM_DIG_INPUTS > 0
    uint32_t nDi = 0;
    for (uint8_t i = 0; i < NUM_DIG_INPUTS; i++)
        if (digIn[i].fVal != 0)
            nDi |= 1U << i;
    nWakeKeep[2] = nDi;
    #endif

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
    CaptureWake();
    NVIC_SystemReset();
}

#endif