# Changelog

Notable changes to this **dingoFW** fork (the dingoConfig feature set). Version is `MAJOR.MINOR.BUILD`
from `core/device_config.h`; the `testing` CI build publishes it as a prerelease (`Testing v5.5.x`).

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
