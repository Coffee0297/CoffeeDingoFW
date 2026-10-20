# Changelog

Notable changes to this **dingoFW** fork (the dingoConfig feature set). Version is `MAJOR.MINOR.BUILD`
from `core/device_config.h`. 5.6.x and earlier are on the old fork line (`master` up to 5.6.6).

## [5.7.0] — unreleased

**New base: upstream dingoFW `development`.** The fork's features were re-built one by one on top of upstream's
current code (each one also offered upstream as its own PR), so this line follows upstream closely from now on.
**`CONFIG_VERSION` 0x0013 → 0x000A (upstream numbering + the ignition parity round): saved configs reset to defaults. Write the project to the
module again after flashing.** Needs a dingoConfig that knows the 5.7 layout (it reads the version and picks it).

### Changed (compared with 5.6.6)
- Param indices, var-map order and status frames follow upstream: Timer `0x1A00` (upstream's, with reset input and
  one-shot / retriggerable pulse; "active while off" = a NOT virtual input in front of it), Table `0x1B00`; device
  subs 0–8 as upstream (`bDisableCanWake` replaces "wake on CAN"); new var-map entries and status frames sit after
  upstream's (tables base+33/34, PID base+35, ignition status base+36 on the PDMs).
- **Ignition sync on its own id**: the main module broadcasts `[state, flags]` (dingoFW #72's frame) on the sync id
  (0x1E00 sub 10, default 0x6F1, or an offset from the main module's base with sub 11 = 1 and, on followers, the main
  module's base in sub 12) every 100 ms and at once on a change, instead of the 'I' state frame on base+36 / base+12.
  That slot now carries the ignition status (state/role, dash state, sleep status, link, sleep countdown, output
  flags, wake source). Every module of a car needs the same sync id.
- Currents in the status frames stay at 0.1 A/bit as in 5.6 (PDM Msg 0 total, Msg 1/2 outputs; upstream sends 1 A/bit);
  the DBC factor is 0.1.
- A CAN input that times out goes off as upstream does; "value on timeout" (sub 14) applies only with sub 15 on.
- Analog inputs keep upstream's uniform rotary mode; the calibrated positions are a mode next to it (sub 13).
- **CANBoard bootloader moved to 8 KB, app at 0x08002000** (was 16 KB / 0x08004000). Every CANBoard needs the new
  `bootloader/canboard` bootloader programmed once over SWD (e.g. `flash-dingo.ps1 -Hex canboard_blt.hex,canboard_v2.hex`);
  after that, updates over CAN as before. The old 16 KB bootloader does not start a 5.7 CANBoard app.
- CANBoard: ignition block (params 0x1E00, status frame Msg 10 = base+12), 2 PID blocks (0x2300-0x2301, output frame
  Msg 11 = base+13) and PWM measuring on all 8 digital inputs (0x1200 subs 5-7, up to 5 kHz) now fit. CANBoard var map
  after the timers: DI PWM duty/freq 79-94, analog scaled 95-99, math 100-103, ignition 104-110, comfort blink 111-112,
  PID 113-120.
- Lua heap 40 KB on the PDMs (CAN RX queue 64 and TX queue 128 frames, device/param thread stacks 1.5 KB, all measured in
  the Sim with room to spare).
- PDM apps run behind the OpenBLT bootloader (0x08004000) as before; the bootloader is built from `bootloader/`.

### Added (new since 5.6.6)
- Condition hysteresis and on/off delays, output undercurrent trip, hard-short trip, soft-start current limit, soft
  stop, fault clear input, load shed, wire-load (thermal) model, output state signals, stay-alive outputs on sleep.
- Up to 4 lookup tables, curves, latch/gate logic, PID blocks, retained values, CAN output and CAN message
  counters/checksums and trigger inputs, config lock, wake on a chosen frame, reset cause, fault history (FRAM).
- Ignition, everything of dingoFW #65 / #67-#74 the 5.6 block lacked: role Standalone (no broadcast), output roles
  (Ignition/Accessory/Dash/Starter per output, PDMs), start button read straight from a CAN frame, stop input,
  power-up crank interlock, dash power down (grace, halting with a DashShutdown var for a CAN Message to send the
  dash's shutdown frame, 2 s unpowered after) and a timed door input, ignition sleep (sleep delay; the main module
  announces it in the sync frame, every module goes quiet for 2 s, then sleeps; goodbye why 4 at the start of the
  quiet). DashShutdown is the last var-map entry (v7 342, Max 310, pt-dpdm4 320, CANBoard 121).
- Sleep: before the transceiver sleeps, periodic TX stops, the CAN controller is stopped and TX parked recessive, and
  edges latched while arming are cleared (dingoFW #65); a wake on an unknown line reads WakeReason Other (4).
- CAN Message: an input already on at power-up sends no Once frame (the first pass only samples, dingoFW #68).
- Upstream's own work since the fork: pt-dpdm4 board, NeoPixels, param thread with Restart / WriteAllMissing,
  hardened USB parser, wrap-safe timing, packed CAN filters.

### Kept from 5.6.6
- Lua, overload trip log, bench test, identify, sleep info / wake reason / quiet wake, comfort blink, ignition (PDMs and CANBoard), PWM inputs (PDMs and CANBoard),
  math, CAN messages, CANBoard DO PWM / timers / sleep (M4F `-Os` build), USB SLCAN filter + identify, no-ACK
  back-off, board and version markers in the image, `.srec` output, little-endian CAN-input re-broadcast,
  CANBoard sleep countdown in Msg 1.

### Fixed
- CANBoard: reading the config at boot no longer overflows the main-thread stack into the interrupt stack (a 1.7 KB
  copy on a 1 KB stack); an interrupt during the read could reset the config to defaults.
