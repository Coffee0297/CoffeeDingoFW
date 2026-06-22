# Changelog

Notable changes to this **dingoFW** fork (the dingoConfig feature set). Version is `MAJOR.MINOR.BUILD`
from `core/device_config.h`; the `testing` CI build publishes it as a prerelease (`Testing v5.5.x`).

## [5.5.101] — 2026-06-22 (testing prerelease)

Adds **calibrated, per-position decoding** for an analog input used as a multi-position switch, so
switches with **uneven** voltage steps (wiper/blinker stalks, odd resistor ladders) decode correctly.
Pairs with **dingoConfig v0.5.0-rc.1**, which configures and calibrates it.

### Added
- **Analog input — calibrated multi-position decode** (`functions/analog_input.*`). Each position
  has its own measured centre voltage (`nPoint[]`, up to `MAX_SWITCH_POS = 12`). A reading registers
  for a position only inside its window — half-width on each side = `min(tolerance, gap-to-neighbour/2)`
  — so windows never overlap and a far-apart switch gets a narrow band, not half the rail. A reading
  outside every window reports **`ROTARY_NO_POS` (15)**. The legacy uniform `offset/step` decode is
  kept for `bUsePoints == false`.
- **New SDO params** at `0x2200` sub **10–24** per analog input: `bUsePoints`, `nNumPos`,
  `nTolerance`, and `nPoint[0..11]` (`core/param_defs.h`).

### Changed
- **`CONFIG_VERSION` 0x0A → 0x0B** — the analog config struct grew, so stored config resets to
  defaults on first boot of this build (re-send config from the tool).

### Notes
- **Prerelease / untested on hardware.** The decode and params compile against the existing tree but
  have not been flashed/validated on a CANBoard yet — flash and verify before relying on it.
- Decoded position is still transmitted as the existing **4-bit** nibble (0–14 = position, 15 = none).
