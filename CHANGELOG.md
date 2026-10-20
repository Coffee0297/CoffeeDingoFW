# Changelog

Notable changes to this **dingoFW** fork (the dingoConfig feature set). Version is `MAJOR.MINOR.BUILD`
from `core/device_config.h`. 5.6.x and earlier are on the old fork line (`master` up to 5.6.6).

## [5.7.0] — unreleased

**New base: upstream dingoFW `development`.** The fork's features were re-built one by one on top of upstream's
current code (each one also offered upstream as its own PR), so this line follows upstream closely from now on.
**`CONFIG_VERSION` 0x0013 → 0x0009 (upstream numbering): saved configs reset to defaults. Write the project to the
module again after flashing.** Needs a dingoConfig that knows the 5.7 layout (it reads the version and picks it).

### Changed (compared with 5.6.6)
- Param indices, var-map order and status frames follow upstream: Timer `0x1A00` (upstream's, with reset input and
  one-shot / retriggerable pulse; "active while off" = a NOT virtual input in front of it), Table `0x1B00`; device
  subs 0–8 as upstream (`bDisableCanWake` replaces "wake on CAN"); new var-map entries and status frames sit after
  upstream's (tables base+33/34, PID base+35, ignition state base+36 on the PDMs; the ignition follower's main id
  must be set again).
- Currents in the status frames stay at 0.1 A/bit as in 5.6 (PDM Msg 0 total, Msg 1/2 outputs; upstream sends 1 A/bit);
  the DBC factor is 0.1.
- A CAN input that times out goes off as upstream does; "value on timeout" (sub 14) applies only with sub 15 on.
- Analog inputs keep upstream's uniform rotary mode; the calibrated positions are a mode next to it (sub 13).
- **CANBoard bootloader moved to 8 KB, app at 0x08002000** (was 16 KB / 0x08004000). Every CANBoard needs the new
  `bootloader/canboard` bootloader programmed once over SWD (e.g. `flash-dingo.ps1 -Hex canboard_blt.hex,canboard_v2.hex`);
  after that, updates over CAN as before. The old 16 KB bootloader does not start a 5.7 CANBoard app.
- CANBoard: ignition block (params 0x1E00, var map 88-94, state frame Msg 10 = base+12) now fits in the larger app region.
  No PWM input on its digital inputs (no RAM; the PDMs have it).
- PDM apps run behind the OpenBLT bootloader (0x08004000) as before; the bootloader is built from `bootloader/`.

### Added (new since 5.6.6)
- Condition hysteresis and on/off delays, output undercurrent trip, hard-short trip, soft-start current limit, soft
  stop, fault clear input, load shed, wire-load (thermal) model, output state signals, stay-alive outputs on sleep.
- Up to 4 lookup tables, curves, latch/gate logic, PID blocks, retained values, CAN output and CAN message
  counters/checksums and trigger inputs, config lock, wake on a chosen frame, reset cause, fault history (FRAM).
- Upstream's own work since the fork: pt-dpdm4 board, NeoPixels, param thread with Restart / WriteAllMissing,
  hardened USB parser, wrap-safe timing, packed CAN filters.

### Kept from 5.6.6
- Lua, overload trip log, bench test, identify, sleep info / wake reason / quiet wake, comfort blink, ignition (PDMs and CANBoard), PWM inputs (PDMs),
  math, CAN messages, CANBoard DO PWM / timers / sleep (M4F `-Os` build), USB SLCAN filter + identify, no-ACK
  back-off, board and version markers in the image, `.srec` output, little-endian CAN-input re-broadcast,
  CANBoard sleep countdown in Msg 1.
