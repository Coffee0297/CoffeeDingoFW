[![Donate](https://img.shields.io/badge/Donate-PayPal-blue.svg)](https://www.paypal.com/donate/?hosted_button_id=HDE8YCVY9NR2L) 
[![GitHub Release](https://img.shields.io/github/v/release/corygrant/dingoPDM_FW?include_prereleases&display_name=tag)](https://github.com/corygrant/dingoPDM_FW/releases)
![GitHub Downloads (all assets, all releases)](https://img.shields.io/github/downloads/corygrant/dingoPDM_FW/total)
[![GitHub last commit](https://img.shields.io/github/last-commit/corygrant/dingoPDM_FW)](https://github.com/corygrant/DingoPDM_FW/commits/master/)
[![GitHub Issues or Pull Requests](https://img.shields.io/github/issues/corygrant/dingoPDM_FW)](https://github.com/corygrant/DingoPDM_FW/issues)
[![Website](https://img.shields.io/website?url=https%3A%2F%2Fcorygrant.github.io%2FdingoPDM%2F&label=docs)](https://corygrant.github.io/dingoPDM/)
![Discord](https://img.shields.io/discord/1243358184266010667?label=discord)

# dingoFW

Firmware repo for dingoPDM, dingoPDM-Max, CANBoard and other dingoFW based devices. 

dingoPDM is an Infineon Profet based Power Distribution Module. 

## This fork — dingoConfig feature set

This fork adds the firmware features driven by the **dingoConfig** configurator
([CoffeeDingoConfig](https://github.com/Coffee0297/CoffeeDingoConfig)). Those features need **this
firmware build** (**v5.5.101**, the `testing` prerelease) to work — they're new CAN commands and config
params, so an older/stock build won't expose them. The tool expects firmware **≥ 5.5.100** and shows a
"firmware needs updating" notice below that.

### Features added since the fork (vs upstream dingoFW)

1. **Analog input → multi-position / rotary switch** *(v5.5.101)* — decode up to **10 positions** on one
   analog input from per-position **calibrated voltages**, so *uneven* steps (wiper/blinker stalks) work.
   Each position has a tolerance window; a reading outside every window reports **"no position"**.
2. **Analog input → linear sensor scaling** *(v5.5.101)* — map the input millivolts to engineering units
   (`scaled = gain·mV + offset`) for a pressure/temperature/etc. sensor. The scaled value is published in
   the variable map, so **outputs and conditions can use it** (e.g. a fan driven by a temperature sensor).
3. **Analog input → on/off switch** — use an analog input as a simple threshold switch (momentary/latched,
   invertible). An input is on/off **or** multi-position **or** linear-scaled — mutually exclusive.
4. **Lua scripting** — drive outputs, virtual inputs and CAN outputs from a Lua program.
5. **On-device overload (trip) log** — each output trip recorded with a current waveform (≈ −10 s … +3 s).
6. **Warning & open-load detection** — per-output warn limit and open-load / broken-bulb floor (reported).
7. **Expanded sleep** — configurable auto-sleep timeout plus an input-driven sleep source.
8. **CanBoard built for the STM32F303K8T6 (Cortex-M4F)** *(v5.5.101)* — hardware FPU enabled and the
   CanBoard image size-optimised (`-Os`), and the config staging buffer moved into the 4 KB CCM. This is
   what lets the calibrated-switch + scaling features fit the CanBoard's 62 KB flash / 12 KB SRAM.

> ⚠️ **v5.5.101 has not been flashed/tested on a CanBoard yet** — it compiles for all three boards and
> the sizes fit, but the FPU/`-Os` switch and the new decode are behaviour changes. Flash and verify on
> hardware before relying on it.

The detailed bullets:

- **Calibrated multi-position analog input** *(new in v5.5.101)* — decode a rotary/selector
  switch on one analog input from per-position **calibrated voltages**, so *uneven* steps
  (wiper/blinker stalks) work. Each position has a tolerance window; a reading outside every window
  reports "no position". Configured/calibrated from dingoConfig. See [CHANGELOG](CHANGELOG.md).
- **Linear sensor scaling** *(new in v5.5.101)* — `scaled = gain·mV + offset` from two calibration
  points; exposed in the variable map for use in outputs/conditions/CAN.
- **Lua scripting** — drive outputs, virtual inputs and CAN outputs from a Lua program
  (`setLuaOut`, `readVar`, `txCan`, `canRxAdd`, `onCanRx`, `onTick`, `setTickRate`, `Timer`, …). One
  assembled program is stored in config; it's uploaded and its runtime errors are read back over CAN.
- **On-device overload (trip) log** — each output trip is recorded with a current waveform around it
  (≈ −10 s … +3 s), so a trip that happened while the tool was disconnected is still recoverable.
- **Warning & open-load detection** — per-output warn limit (soft over-current) and open-load /
  broken-bulb floor, *reported only* (the output keeps running). Peak-current capture catches
  sub-frame spikes.
- **Expanded sleep** — configurable auto-sleep timeout plus an **input-driven sleep** source (a
  digital input drives sleep directly, with configurable active level and "ignore always-on outputs").

See [PR #1](https://github.com/Coffee0297/CoffeeDingoFW/pull/1) for the full changelog and the new
CAN command set.

# [**Documentation**](https://corygrant.github.io/dingoPDM/)

# [**Store**](https://dingo-electronics.square.site/product/dingopdm/1)

## Disclaimer
Please note that this product has been designed by a hobbyist, not a professional. It is intended for off-road and testing use only. Users should operate the product at their own discretion and risk. The designer explicitly disclaims any responsibility for damage or injury that may result from the use of this product.
