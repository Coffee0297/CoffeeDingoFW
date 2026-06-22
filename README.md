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

### What this fork actually adds to the firmware

Diffed against the dingoFW `testing` base this forked from. **Only these three are new** — the on/off
analog switch, the basic analog rotary, Lua 5.5, the CanBoard itself, the overload/trip log, PWM,
expanded sleep, etc. are all **already in the base dingoFW**, not added here.

1. **Analog input — per-position *calibrated* multi-position decode** *(v5.5.101)* — the base had a
   uniform offset/step rotary; this adds decoding from per-position **calibrated voltages** (up to
   **10 positions**), so *uneven* steps (wiper/blinker stalks) work. Each position has a tolerance
   window; a reading outside every window reports **"no position"**.
2. **Analog input — linear sensor scaling** *(v5.5.101)* — map the input millivolts to engineering units
   (`scaled = gain·mV + offset`) for a pressure/temperature/etc. sensor. The scaled value is published in
   the variable map, so **outputs and conditions can use it** (e.g. a fan driven by a temperature sensor).
3. **CanBoard built for the STM32F303K8T6 as the Cortex-M4F it is** *(v5.5.101)* — the base built the
   CanBoard as `cortex-m3` / soft-float / `-O0`; this enables the **hardware FPU**, size-optimises it
   (`-Os`), and moves the config staging buffer into the 4 KB **CCM**. That's what frees the room for the
   two analog features (flash 101.5 % → 53.9 %; heap 448 B → 1600 B).

> ⚠️ **v5.5.101 has not been flashed/tested on a CanBoard yet** — it compiles for all three boards and
> the sizes fit, but the FPU/`-Os` switch and the new decode are behaviour changes. Flash and verify on
> hardware before relying on it. See [CHANGELOG](CHANGELOG.md).

# [**Documentation**](https://corygrant.github.io/dingoPDM/)

# [**Store**](https://dingo-electronics.square.site/product/dingopdm/1)

## Disclaimer
Please note that this product has been designed by a hobbyist, not a professional. It is intended for off-road and testing use only. Users should operate the product at their own discretion and risk. The designer explicitly disclaims any responsibility for damage or injury that may result from the use of this product.
