# Changelog

Notable changes to this **dingoFW** fork (the dingoConfig feature set). Version is `MAJOR.MINOR.BUILD`
from `core/device_config.h`; the `testing` CI build publishes it as a prerelease (`Testing v5.5.x`).

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
