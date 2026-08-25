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
   The FPU isn't cosmetic: the analog scaling (`gain·mV+offset`), calibrated decode, and the PWM
   duty/soft-start-ramp math all run **every control loop**, and in soft-float those libgcc float
   helpers (~2.5 KB) are exactly what overflowed the 64 KB flash and would slow the 2 ms loop — the
   hardware FPU makes them single-cycle. Catch: FPU-on enlarges the exception stack frame (~104 B), which
   is why the CAN thread stacks had to grow in v5.5.103 (the bug that first silenced CAN on real hardware).
4. **CanBoard digital-output PWM** *(v5.5.103)* — the base PWM is tied to the PDM's Profet outputs
   (`NUM_OUTPUTS`), which the CanBoard doesn't have. This brings the same PWM model to the CanBoard's 4
   digital outputs (DO1–DO4): enable, fixed/variable duty, 0–400 Hz, soft-start, min duty. Each output
   runs on its own free timer (TIM3/15/16/17) as a software-toggled timebase, so frequencies are
   independent (flash 53.9 % → 59.0 %).
5. **OpenBLT CAN bootloader + firmware update over CAN** *(v5.5.104)* — a one-time SWD-flashed OpenBLT
   (Feaser) XCP-over-CAN bootloader in the first 16 KB of flash. The application is relocated above it
   and can then be reflashed **over CAN** from dingoConfig (via any SLCAN probe) — no SWD or USB after
   the first install. The bootloader reads the module's base ID + CAN speed from the config sector at
   runtime, so its XCP IDs (`base+12`/`base+13`) and bitrate follow the one firmware setting; the config
   sector is never erased, so settings survive a reflash. The vector block is written **last**, so an
   interrupted/brown-out update leaves the app invalid and the bootloader waiting (always re-flashable).
   Lives in [`bootloader/`](bootloader/) (vendored OpenBLT, trimmed to the core + the STM32F3 port).

### Firmware update over CAN (OpenBLT)

**Workflow: SWD once, then CAN forever.** Flash the bootloader to a blank board one time over SWD; every
application update after that goes over CAN from dingoConfig ("⬆ Over CAN" → pick the `.srec`).

CANBoard (STM32F303K8, 64 KB) flash map:

| Region | Sectors | Address | Size | Contents |
|---|---|---|---|---|
| Bootloader | 0–7 | `0x08000000` | 16 KB | OpenBLT (uses ~6.8 KB) — **SWD-flash once** |
| Application | 8–30 | `0x08004000` | 46 KB | relocated app — **reflashed over CAN** |
| Config | 31 | `0x0800F800` | 2 KB | persistent settings — never erased |

Build the bootloader: `cd bootloader/canboard && make` → `bin/canboard_blt.hex` (SWD program once). The
app build emits `build/<board>.srec` (the format the CAN flasher consumes). On the PDM/-Max the same
OpenBLT-CAN bootloader (`bootloader/dingopdm/`) keeps USB-DFU working **alongside** CAN update: OpenBLT
owns the reset vector and dispatches USB-DFU (`0xDEADBEEF`) to the STM32 ROM bootloader or stays for a
CAN session (`0xB00710AD`).

> ✅ **Validated on a dingoPDM-v7** (bootloader installed over USB-DFU; relocated app boots/runs
> clean; app reflashed/connected **over CAN** via XCP; both USB-DFU paths — BOOT0 switch and the
> software trigger — reach the ROM bootloader). dingoPDM-Max mirrors it (same bootloader + app base;
> only config storage differs) and is build-verified, not yet hardware-tested.
>
> ⚠️ **Installing the bootloader overwrites flash sector 0** and relocates the app, so the board
> runs this fork firmware afterwards (config resets to fork defaults). **The bootloader itself can
> only be updated over USB-DFU** (it can't rewrite its own sector while running) — install it, and
> later update *it*, via USB-DFU; only the *application* goes over CAN. Always keep a **read-out
> backup of the original flash** and a **BOOT0 recovery path** (BOOT0 high → permanent STM32 ROM
> USB-DFU, reflashable independent of flash contents) before installing on a board without SWD.

> ✅ **Verified end-to-end on a CanBoard** (SWD via Raspberry Pi Pico / CMSIS-DAP + a Kvaser/SLCAN bus):
> bootloader installed once over SWD, then the application reflashed **over CAN** from dingoConfig
> (program → read-back verify → reboot into the new app), settings preserved, interrupted-flash
> recovery confirmed. The v5.5.103 PWM outputs still want a bench check (scope DO1–DO4, `0x64B` duty
> frame). See [CHANGELOG](CHANGELOG.md).

### Batch SWD flashing (`flash-dingo.ps1`)

[`flash-dingo.ps1`](flash-dingo.ps1) drives the **SWD-once** step above in a loop — for the
one-time bootloader install, or for programming a batch of blank boards off the reel. Handles both
**CANBoard v2** (STM32F303K8) and **dingoPDM v7 / -Max v1** (STM32F446).

```powershell
.\flash-dingo.ps1 -Hex C:/path/to/canboard_v2_FW_v0-5-8.hex            # batch loop
.\flash-dingo.ps1 -Hex C:/path/to/dingopdm_v7_FW_v0-5-8.hex            # PDM, same script
.\flash-dingo.ps1 -Hex bootloader/canboard/bin/canboard_blt.hex -Once  # one board
.\flash-dingo.ps1 -SelfTest                                            # no hardware needed
```

Per board: wait for SWD → **identify the MCU** → **check the image belongs on it** → read all of
flash back → require every byte `0xFF` → `pyocd load -e sector` (so the persistent config sector
survives) → beep → wait for unplug → repeat. A board that **isn't blank is skipped, not
overwritten**, unless `-Force`. High beep = pass, low = fail/skip.

**The board is identified, never assumed.** Its `DBGMCU_IDCODE` (`0xE0042000`) gives the `DEV_ID`,
which selects the pyocd target, and the part's own flash-size register gives its real capacity:

| `DEV_ID` | Part | Board | pyocd target | pack |
|---|---|---|---|---|
| `0x438` | STM32F303x6/x8 | CANBoard v2 | `stm32f303k8` | `Keil.STM32F3xx_DFP` |
| `0x421` | STM32F446 | dingoPDM v7 / -Max v1 | `stm32f446re` | `Keil.STM32F4xx_DFP` |

Before writing anything it refuses the flash if the image doesn't belong on the detected part:

- **filename names a different board** — `dingopdm_*.hex` on a CANBoard, or `canboard_*.hex` on a PDM
- **image doesn't fit** the part's real flash size — the 162 KB PDM firmware cannot land on a 64 KB F303
- **two images overlap** — e.g. a standalone firmware at `0x08000000` passed alongside
  `canboard_blt.hex`, which claims the same address

`-IgnoreMismatch` overrides, `-Target` forces a pyocd target. It also prompts once at startup to
confirm the image and board before a batch (`-Yes` skips).

**Contact time matters if you hand-hold the probe.** Measured on a dingoPDM (162 KB image): **23 s →
13 s**, by blank-checking only the span the image occupies (159 KB, not the whole 512 KB chip) and
defaulting to a 4 MHz SWD clock. The remaining ~4.4 s is F446 erase time — flash-controller bound,
and pyocd offers no way to skip erasing already-blank sectors.

> ⚠️ Never pipe this script through `Select-Object -First N` or `head`. That closes the upstream
> pipeline early and **kills it mid-write**, leaving a half-programmed board in CPU lockup.
> Recovery is `-Force` with the full image.

| Flag | Default | |
|---|---|---|
| `-Hex` | *(required)* | one or more images; `.hex`/`.elf` carry their load addresses, a raw `.bin` needs `@0x08000000` appended |
| `-Target` | *auto from IDCODE* | override the detected pyocd target |
| `-Frequency` | `4M` | SWD clock; falls back `2M`→`1M`→`500k` automatically if the target won't answer |
| `-Fast` | off | verify by CRC32 instead of a full readback — quicker, weaker guarantee |
| `-Retries` | `2` | flash attempts before calling the board bad |
| `-Force` | off | reflash a board that isn't blank |
| `-IgnoreMismatch` | off | flash even if the image looks wrong for the board — last resort |
| `-Yes` | off | skip the one-time confirmation prompt |
| `-Once` | off | do one board and exit, instead of looping |
| `-SelfTest` | — | assert the script's parsing and mismatch logic, then exit |

Requires `pyocd` on `PATH` and a CMSIS-DAP probe (a Raspberry Pi Debugprobe/Pico works). Install
the device packs once: `pyocd pack install stm32f303k8 stm32f446re`. Only `.hex` extents can be
parsed — pass a `.hex` if you want the fit and overlap checks. Confirm where any image lands with
`arm-none-eabi-objdump -h <file.elf>`.

Two pyocd traps the script works around — worth knowing before scripting this yourself:

- **`pyocd cmd` exits `0` even when SWD gets no ACK.** Board presence must be judged on its *output*
  (the `Core <n> (...)` line), never on `$LASTEXITCODE`, or you get phantom "connected" boards.
  `pyocd load` does exit non-zero correctly.
- **The commander strips `\` as an escape**, so `savemem 0x08000000 65536 C:\Users\...\dump.bin`
  prints `Saved 65536 bytes` while writing to a mangled drive-relative path. Pass forward slashes.

`No ACK` across every connect mode *and* every clock from 2 MHz down to 100 kHz almost always means
the **target is unpowered** — the Debugprobe supplies no target power. Check that, and the shared
ground, before chasing `-Frequency`.

> ✅ **Verified on a dingoPDM v7** (STM32F446, Raspberry Pi Debugprobe): board identified from
> `DEV_ID 0x421` with 512 KB read from the part's own flash-size register; a CANBoard image was
> refused on it by name; the correct image flashed and verified, checked independently by reading
> the SP, reset vector and image tail back off the chip; a half-programmed board in lockup was
> recovered with `-Force`. Also verified off-hardware: Intel-HEX extent parsing (against
> `canboard_blt.hex`, `build/canboard_v2.hex` and both v0.5.8 release images) and the
> mismatch/fit/overlap guards.
>
> ⚠️ **Not yet exercised: the CANBoard/F303 write path** — detection and blank-check refusal are
> proven there, writing is not. Run `-Once` on one known-blank CANBoard before trusting a batch.

# [**Documentation**](https://corygrant.github.io/dingoPDM/)

# [**Store**](https://dingo-electronics.square.site/product/dingopdm/1)

## Disclaimer
Please note that this product has been designed by a hobbyist, not a professional. It is intended for off-road and testing use only. Users should operate the product at their own discretion and risk. The designer explicitly disclaims any responsibility for damage or injury that may result from the use of this product.
