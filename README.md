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

This `testing` line adds the firmware features driven by the **dingoConfig** configurator
([CoffeeDingoConfig](https://github.com/Coffee0297/CoffeeDingoConfig)). That UI **requires this
firmware** (≥ **v0.5.1000**, config `0x000A`) to use any of them; basic read/write/configure still
works on stock firmware.

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
