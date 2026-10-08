# Changelog

Notable changes to this **dingoFW** fork (the dingoConfig feature set). Version is `MAJOR.MINOR.BUILD`
from `core/device_config.h`; the `testing` CI build publishes it as a prerelease (`Testing v5.5.x`).

## [Unreleased]

### Fixed
- **CAN receive overruns on a busy bus** (`comms/can.cpp`). The RX thread read one frame per wake-up and
  the tickless sleep between wake-ups is at least `CH_CFG_ST_TIMEDELTA` ticks (200 µs), so back-to-back
  frames (~240 µs apart at 500 kbit/s) overran the 3-deep bxCAN FIFO. It now drains every waiting frame
  per wake-up and runs one priority above the CAN TX threads. Found in CoffeeDingoSim: a 604-frame
  WriteAll reached a CANBoard as 550 frames.
- **CANBoard RX mailbox 16 → 48 frames**, held in CCM (`RX_MAILBOX_SIZE`, `comms/mailbox.cpp`) so main
  SRAM is not touched (the heap grows from 680 to 1136 B). Other boards keep `MAILBOX_SIZE`.

### Added
- **PWM input mode on every digital input** (`functions/digital_input.cpp`, `functions/pwm_meter.h`).
  Param `0x1200+i` sub 5 `bPwm`, sub 6 `nPwmFreq` (Hz, 0 = auto-detect). Both edges are timestamped
  with the cycle counter in the pin's EXTI callback; each update averages the whole periods since the last
  one. New var-map entries per input, appended after the tables/timers so every existing index is
  unchanged: `PWM Duty` (0–100 %, Invert = time low) and `PWM Frequency` (Hz). No edge for 3 periods
  reads 0 % / 100 % from the pin level and drops the input's state. Uses: a Condition on PWM Duty
  ("on at x %"), or an output's variable duty following it. CANBoard: `PAL_USE_CALLBACKS` on.
  Sub 7 `nPwmMinPulseUs` is a glitch filter: two edges closer than it are a spike and both are dropped.
  The frequency setting is capped per board from the input circuit (`DI_PWM_MAX_FREQ`): dingoPDM 1 kHz
  (4.7k + 10 nF; open collector on the internal pull-up only ~100 Hz), CANBoard 5 kHz (10k, unfiltered).
  **`CONFIG_VERSION` 0x000F → 0x0011: saved configs reset to defaults; re-deploy from dingoConfig.**
- Diagnostic counters `gCanRxFrames` / `gCanRxMailboxDrops` (read with a debugger or the simulator).

## [5.5.107] — 2026-10-05

Timer functions (upstream dingoFW #61), 2-axis lookup tables (dingoConfig #58) and the expanded sleep
model agreed in dingoFW #52 (force-sleep / mute-TX signals + configurable wake sources). **Breaking** —
config struct changed (`CONFIG_VERSION` 0x000E → 0x000F), so devices load defaults on first boot after
flashing. Needs **dingoConfig ≥ 0.7.0** (new params, var-map entries and frames). Build-verified on all
three boards; not yet flashed to hardware.

### Added
- **Output bench test** (`MsgCmd::OutputTest = 48`, PDM/-Max Profet outputs and CANBoard digital outputs) —
  the tool can force an output **on**, or
  **PWM at a duty + frequency**, for a bounded hold (1–30 s; the tool re-sends while the test runs, so a
  dropped link releases it by itself). The output ignores its input but keeps its current limits and
  fault handling (PDM), and only an enabled output is accepted. Frame `[48, out, mode (0 off / 1 on / 2 pwm),
  duty %, freqLo, freqHi, holdSec, 0]`; the reply echoes bytes 0–6 with byte 7 = accepted.
- **Timer function** (`functions/timer.*`, params `0x1B00+`, 8 per PDM / 4 per CANBoard). One var-map
  input, a preset (ms, up to 1 h) and a mode: **on-delay** (TON — output on once the input has been
  active for the preset, off with the input), **off-delay** (TOF — follows the input on, holds for the
  preset after it drops) or **pulse** (TP — one preset-long pulse per activation, retriggerable). `eEdge`
  picks the active level (Rising = input true, Falling = input false) so a timer can run while something
  is *off* — "ignition off for 30 s" is a falling-edge on-delay. Output is in the var map, so it drives
  outputs, conditions, CAN outputs or the new force-sleep input.
- **Lookup table function** (`functions/table.*`, params `0x1A00+`, 2 per PDM/-Max; none on the
  CANBoard — a 328-byte table doesn't fit its 2 KB config sector). Up to **8×8** cells, X/Y from the var
  map, **bilinear interpolation**, edge values held outside the axis range; `nYSize = 1` makes a 1-D
  curve. Param layout: sub 0–4 header, 5–12 X axis, 13–20 Y axis, 21–84 cells (row-major). Outputs are
  broadcast in a new PDM **Msg 27 (`base+29`)** as two float32 LE — the PDM's CAN footprint is now
  `base .. base+29` (`NUM_TX_MSGS` 28).
- **Timer outputs on CAN**: PDM Msg 3 byte 7 (bits 56–63, was 0) and CANBoard Msg 2 upper nibble of
  byte 3 (bits 28–31). DBC builders + `dbc/*_0.5.1.dbc` regenerated.
- **Expanded sleep (FW #52)** — the decision stays local to each module, no inter-module handshake:
  - `nForceSleepInput` (0x0000:10) — a var-map signal that forces sleep *now*, ignoring the idle rules and
    the auto-sleep enable (a digital ignition input, a CAN "sleep now" input, a Timer…). USB connected
    still blocks it (the #36 soft-lock) and a 1 s boot grace lets inputs settle after a wake.
  - `nMuteTxInput` (0x0000:11) — a var-map signal that withholds the cyclic telemetry so a fleet can go
    quiet on cue and every module's CAN-idle timer can expire. Config replies, bridge frames and user CAN
    outputs are not muted.
  - `nWakeDigInputMask` (0x0000:12) + `bWakeOnCan` (0x0000:13) — which digital inputs (per-pin bits) and
    whether CAN traffic re-arm as wake sources. USB always wakes.
  - Sleep entry now switches **every output off** and gives the CAN TX thread 100 ms to flush before the
    transceiver goes to standby: a forced sleep can arrive with outputs on, and in stop mode nothing
    would protect the load (same step upstream `development` takes).
  - Replaces the 5.5.106 digital-input-only sleep trigger (`bSleepInputEnabled` / `nSleepInput` /
    `bSleepInputActiveHigh`, subs 6–8 — retired, not reused, so an old tool writing them gets "param not
    found" instead of a silently re-purposed value).
- `static_assert` that `DeviceConfig` (+CRC) still fits the 2 KB config flash sector on boards without
  FRAM — a struct that outgrew it would silently program past the sector.
- `tests/host_selftest.cpp` — a host-side (plain `g++`) check of the pure Timer/Table logic; same cases as
  dingoConfig's `LookupTableTests` / `table.test.js` so the three implementations can't drift.

### Fixed
- **`primaryOutput` param range** was `-1..VAR_MAP_SIZE-1` on an `int8_t`: a value of 128..258 wrapped negative and
  `stOutput[]` was indexed out of bounds. Now `-1..NUM_OUTPUTS-1` (`core/param_defs.h`).
- **Keypad model param range** was `0..13`, so the Grayhill models (`20..24` in `KeypadModel`) could never be written;
  now `0..24`.
- **DBC drift**: `OutputState` now lists `Warning` (4) and `OpenLoad` (5); the CANBoard DBC advertised 32 CAN inputs,
  16 virtual inputs and 32 conditions for a board that has 8 of each (`dbc/dbc_builder/canboard`), regenerated.

### Changed
- **CAN-input value re-broadcast is always little-endian** (PDM Msg 7–22, CANBoard Msg 5–8). It used
  the input's own byte order, so a Motorola (big-endian) CAN input was re-encoded as a sawtooth from bit 0
  that spilled into the neighbouring value's bytes — undecodable. The input's byte order only describes
  the frame it *listens* to; the telemetry container is a fixed LE int32 (dingoConfig #59 follow-up).
- Var-map order: timers and tables are appended **after** the Lua output slots (not before), so every
  pre-existing index — including saved "Lua Out N" output bindings — is unchanged. New blocks go after.
- `VAR_MAP_SIZE`: PDM/-Max +10 (8 timers + 2 tables); CANBoard +4. The PDM analog term is now `×5` like
  the CANBoard's (it was `×4`, harmless with 0 analog inputs, but one less trap).

### Notes
- `build/` is shared between boards: run `rm -rf build/obj build/lst` (or `make clean`) when switching
  `BOARD=`, or stale objects from the previous board get linked (the CANBoard then fails with "cannot move
  location counter backwards" from the PDM's 128 KB of RAM structures).

## [5.5.104] — 2026-06-25

OpenBLT CAN bootloader + application relocation, so the app can be reflashed over CAN from dingoConfig
("Update firmware over CAN") after a one-time SWD flash of the bootloader. No config-struct change.
Verified end-to-end on a CANBoard (SWD-once → CAN-after, settings preserved, interrupted-flash recovery).
Also: a USB `I` identify command and always-live analog inputs.

### Added
- **USB `I` identify command** (`comms/usb.cpp`) — a non-standard SLCAN extension (mirrors the `X`
  accept-filter): when this board is the USB↔CAN bridge, a host sends `I` and the board replies
  `I<baseId>` over USB only (never onto CAN). Lets dingoConfig learn which board is the bridge so it
  flashes that board over USB and every other module over CAN. Standalone SLCAN adapters ignore it.
- **Analog inputs always read live** (`functions/analog_input.cpp`) — the raw ADC voltage is now
  sampled and broadcast on CAN whether or not the input is `bEnabled`; only the derived decoders
  (rotary / switch / scale) stay config-gated. Previously a disabled input forced 0 mV, so a probe on
  an unconfigured pin read 0 in dingoConfig.
- **OpenBLT (Feaser) XCP-over-CAN bootloader** for the CANBoard, vendored under `bootloader/` (trimmed to
  the core + the `ARMCM4_STM32F3` port; CAN-only). One-time SWD install (`bootloader/canboard/`,
  `make` → `bin/canboard_blt.hex`); thereafter the app is flashed over CAN.
  - **Runtime CAN config from the config sector**: the bootloader reads `nBaseId` (offset 2) and
    `eCanSpeed` (offset 4) at startup, so its XCP command/response IDs (`base+12`/`base+13`) and bitrate
    track the one firmware setting. Falls back to `0x640`/500 kbit if the sector is blank.
  - **HSE-clocked** (crystal accuracy required for reliable CAN — the demo's HSI is not).
  - **Vector block written last** (`FlashDone` reorder): an interrupted/brown-out CAN update leaves the
    app invalid and the bootloader waiting — always re-flashable. App validity = vector-table presence.
  - Application is **relocated** above the 16 KB bootloader: `flash0` `0x08000000` → `0x08004000`
    (46 KB), config sector at `0x0800F800` untouched. `core/config.h` pins the config offsets the
    bootloader reads (`static_assert`).
- `RequestBootloader()` for the CANBoard (`boards/cortex-m3/mcu_utils.cpp`): sets a magic in CCM at
  `0x10000FFC` and resets; the bootloader consumes it and stays in CAN-update mode. `MsgCmd::Bootloader`
  (33) is now handled on all boards (was USB-only).
- The application build emits `build/<board>.srec` (`SREC = $(CP) -O srec`) — the S-record the CAN
  flasher consumes.
- **OpenBLT CAN bootloader ported to the F446 PDMs** (`dingopdm_v7`, `dingopdmmax_v1`), vendored
  under `bootloader/dingopdm/` (core + the `ARMCM4_STM32F4` port; CAN-only). One bootloader serves
  both: 16 KB in flash sector 0, app relocated to sector 1 (`0x08004000`, sectors 1–6 = 368 KB),
  sector 7 left for config. CAN on PB8/PB9 AF9, 144 MHz HSE clock (APB1 = 36 MHz). **USB-DFU is
  kept** (decision #6): the app still enters it with the `0xDEADBEEF` reset-magic, but since OpenBLT
  now owns the reset vector the dispatch moved into it — the bootloader's **reset handler** (before
  crt0, from a pristine state) sees `0xDEADBEEF` → jumps to the STM32 ROM bootloader (USB-DFU), and a
  non-matching value is left in place so `0xB00710AD` survives to `CpuUserProgramStartHook`, which
  keeps the bootloader up for an XCP-over-CAN session. (Doing the ROM jump later, in `main()` after
  crt0 had moved VTOR / enabled the FPU, faulted — that was found and fixed on hardware.) Because the PDMs
  keep config in external FRAM (unreadable by the minimal bootloader), the app hands the bootloader
  its base id + CAN speed in a reserved word at the top of SRAM at entry; cold-boot falls back to
  the PDM default base `0x0DE`. New `RequestBootloaderCan()` + a cmd-33 sub-action (byte 6 = 1) on
  the PDMs select CAN update vs USB-DFU; the CANBoard is unchanged. The PDM apps are now built `-Os`
  (the `-O0` image does not fit the 368 KB app region) and the relocated app uses ChibiOS's default
  reset handler (the `enter_bootloader.S` trampoline is dropped — OpenBLT owns reset).
- **No-ACK CAN flood back-off** (`comms/can.cpp`) — when transmits stop being ACKed (this is the only
  live node, or the bus is down), bxCAN would otherwise retransmit each frame at line rate (a bus
  flood). `CAN_MCR_NART` (one-shot TX) stops it but **breaks bridged XCP flashing** — a forwarded
  frame is dropped the instant the flash target is mid-reset and misses one ACK. So auto-retransmit
  is kept and the TX thread instead aborts the stuck mailboxes and pauses *cyclic telemetry* after 3
  consecutive no-ACK timeouts, resuming the moment a peer ACKs. Config replies and forwarded (bridge)
  frames are never paused. Validated by flashing a CANBoard over CAN through the PDM bridge on a
  Kvaser-saturated **3000 msg/s** bus.
- **SLCAN `X` acceptance-filter command** (`comms/usb.cpp`) — lets the host tell a USB↔CAN bridge
  which ID(s) to forward, so the bridge can drop a bus flood instead of relaying every frame to USB.

### Fixed
- **Keypad timeout reset wrote out of bounds** (`functions/keypad/keypad.cpp`): the reset loop ran to
  `KEYPAD_MAX_BUTTONS` (20) while indexing `fDialVal` (2) and `fAnalogVal` (4), overrunning both —
  harmless at `-O0` but real UB that the new PDM `-Os` build both warns on and may miscompile. Now
  each array is reset to its own bound.

### Notes
- ✅ **PDM bootloader port validated on a dingoPDM-v7** (2026-06-24): bootloader installed over
  USB-DFU; the relocated app boots and runs (telemetry clean, **0 bus errors**); an **OpenBLT
  CAN-update session connected over XCP** on the runtime-derived IDs `0x0EA/0x0EB`, confirming the
  FRAM→RAM base-id handoff, and `PROGRAM_RESET` returned to the app; and **both USB-DFU paths reach
  the ROM bootloader cleanly** — the BOOT0 switch and the software trigger (`0xDEADBEEF`, dispatched
  in the bootloader's reset handler). dingoPDM-Max is a mirror (same bootloader + app base; only
  config storage differs) — build-verified, not yet hardware-tested.
- The **bootloader itself can only be updated over USB-DFU** — it can't erase/rewrite its own
  sector 0 while executing from it, so its flash map excludes that sector. Application updates go
  over CAN; a bootloader change (rare) needs USB-DFU (BOOT0 or the software trigger).
- ⚠️ Installing OpenBLT relocates the app, so the device runs this fork firmware afterwards; its
  stored config resets to fork defaults on first boot (base `0x0DE` / 500 kbit unchanged). Keep a
  read-out backup of the original flash and a BOOT0 path before installing on any board.

## [5.5.103] — 2026-06-24 (testing prerelease)

CANBoard digital-output PWM. **Breaking** — config struct changed (`CONFIG_VERSION` 0x000B → 0x000C),
so devices load defaults on first boot after flashing. Needs the matching dingoConfig update to expose
the new params (0x2100 sub 2–10) and decode the new duty frame (0x64B). The CANBoard DBC was
regenerated (`dbc/CANBoard_0.5.1.dbc`); downstream consumers (dingoConfig, dingoPDM MCP) pick up the
new signals from it.

### Added
- **PWM on the CANBoard's 4 digital outputs (DO1–DO4)** — mirrors the dingoPDM Profet PWM: enable,
  fixed or variable duty (from a var-map input ÷ denominator), 0–400 Hz, soft-start ramp, min duty.
  Reuses the existing `Pwm` class / `Config_PwmOutput`. Each output gets its own timebase
  (DO1→TIM3, DO2→TIM15, DO3→TIM16, DO4→TIM17) for independent frequencies; the timer just drives the
  period/compare ISRs that toggle the plain-GPIO DO line (no timer-AF pin needed). TIM2 (system tick)
  and TIM1/TIM4 are left alone. New params at base 0x2100 sub 2–10. Flash 53.9 % → 59.2 %.
  ✅ **Flashed and verified on a CanBoard** (SWD via Raspberry Pi Pico / CMSIS-DAP) — boots clean and
  CAN broadcasts confirmed, after the FPU stack fix below.
- **CANBoard duty-cycle CAN broadcast** — new cyclic frame **Msg 9 at `base+0x0B` (0x64B default)**,
  mirroring the PDM's Msg 23: one byte per output, 0–100 % (`DigitalOutputDC_1..4`), bytes 0–3.
  Only sent when at least one DO has PWM enabled. On/off state stays in Msg 2 byte 6. Slots into the
  free `0x64B–0x64F` range (cyclic frames previously ended at 0x64A).

### Fixed
- **CAN went silent on the CanBoard** under the M4F / hardware-FPU build (the FPU switch from v5.5.101):
  with the FPU enabled, exception entry pushes an extended (~104 B) stack frame onto the active thread's
  stack, overflowing the **128 B** `waCanCyclicTxThread` / `waCanRxThread` stacks (`comms/can.cpp`) and
  corrupting RAM, so broadcasts stopped. Bumped both to **256 B**. The v5.5.101 FPU change shipped
  without resizing these; the first on-hardware test surfaced it. Upstream (soft-float) was unaffected.
- DBC builder (`dbc/dbc_builder/main.py`) now writes **LF** line endings, so regenerating on Windows
  no longer rewrites every line of all three DBCs (cantools emits CRLF; text-mode write doubled it).

## [5.5.102] — 2026-06-23 (testing prerelease)

CAN broadcast wire-format fixes. **Breaking** — reflash to keep telemetry correct. Pairs with the
matching dingoConfig decode update and the new `docs/can-frame-map.md` frame reference.

### Fixed
- **2nd CAN-input value in each value-pair frame** was encoded at start bit 33 instead of 32
  (`boards/*/msg.cpp`), so on the wire it sat at bits 33–63 with its MSB truncated and did not match
  the DBC. Now byte-aligned at bytes 4–7 (bit 32). Affects dingoPDM, dingoPDM-Max and CANBoard.
- **Total/Output current** now transmitted at **0.1 A/bit** (×10), matching the DBCs, the overload
  log and the battery/temperature fields, instead of 1 A/bit. Removed the incorrect "Already scaled
  by 10" note on dingoPDM-Max.
- **Keypad-2 dials DBC** (`dingoPdm*_0.5.1.dbc` Msg 26) corrected to bytes 0–7 (was start bit 24,
  with one signal past the frame); generator `dbc_builder/*/build_msg_26.py` fixed to match.

## [5.5.101] — 2026-06-22 (testing prerelease)

Gives the analog input three mutually-exclusive modes — on/off switch, calibrated multi-position
switch, and **linear sensor scaling** — and makes them fit the CanBoard by building it as the
Cortex-M4F it is. Pairs with **dingoConfig v0.6.0-rc.1**.

### Added
- **Analog input — calibrated multi-position decode** (`functions/analog_input.*`). Each position has
  its own measured centre voltage (up to `MAX_SWITCH_POS = 10`). A reading registers for a position
  only inside its window — half-width on each side = `min(tolerance, gap-to-neighbour/2)` — so windows
  never overlap and a far-apart switch gets a narrow band, not half the rail. Outside every window
  reports **`ROTARY_NO_POS` (15)**. Point voltages are stored **packed two per 32-bit word**
  (`nPointPair[5]`) and exposed as 5 SDO words, to save flash.
- **Analog input — linear sensor scaling** (`Config_AnalogScale`): `fScaled = fGain·mV + fOffset`,
  published per input in the variable map (`fScaledVal`) so outputs/conditions can use the scaled
  value (e.g. a fan driven by a temperature sensor).
- **New SDO map** at `0x2200` (sub **0–16**) per analog input: input enable; switch enable/mode/
  invert/threshold; rotary enable/invert/numPos/tolerance + 5 packed point-words; scale enable/gain/
  offset (`core/param_defs.h`).

### Changed
- **CanBoard now builds for the STM32F303K8T6 as a Cortex-M4F** (`boards/canboard_v2/board.mk`):
  hardware FPU enabled (`-mfloat-abi=hard -mfpu=fpv4-sp-d16`, removes ~2.5 KB of soft-float) and the
  image is size-optimised (`-Os` instead of the inherited `-O0`). Flash use drops from **101.5 %
  (overflow)** to **53.9 %**. The `cortex-m3` MCU-utils dir is kept on purpose (the `cortex-m4` utils
  hardcode an F4-only bootloader RAM address). PDM boards are unchanged.
- **Config staging buffer (`stConfigTemp`) moved into the 4 KB CCM** (`ram4`), growing the heap from
  448 B to ~1600 B.
- **Dropped the legacy uniform `offset/step` rotary decode** — calibrated points are the only mode.
- **`CONFIG_VERSION` 0x0A → 0x0B** — the analog config struct changed, so stored config resets to
  defaults on first boot of this build (re-send config from the tool).

### Notes
- ⚠️ **Prerelease — NOT yet flashed/tested on a CanBoard.** It compiles for all three boards and the
  sizes fit, but the FPU + `-Os` switch and the reworked decode are behaviour changes. Flash and verify
  on hardware before relying on it.
- Decoded position is still transmitted as the existing **4-bit** nibble (0–14 = position, 15 = none).
