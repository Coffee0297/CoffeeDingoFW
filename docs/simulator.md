# Simulating the real firmware off the car

How to run the **actual `dingopdm_v7.elf`, `dingopdmmax_v1.elf` and `canboard_v2.elf`** that `make BOARD=…`
produces — the same bytes that get flashed — on a PC, wire a whole vehicle's worth of modules to one virtual
CAN bus, drive their inputs from a script, and talk to them from dingoConfig exactly as you would in the car.
The point is to catch firmware and configuration mistakes (a sleep sequence that never wakes, an output rule
one Lua slot off, a CAN frame nobody decodes) before anything is loaded into the vehicle.

This is a build guide, not a finished tool: it says what to build, in what order, and where the sharp edges are.
Nothing here exists in the repo yet except the host self-test (`tests/host_selftest.cpp`).

---

## 1. Pick the right tool

| Approach | Runs the real binary? | CAN between modules | Effort | Verdict |
|---|---|---|---|---|
| **Renode** (Antmicro, open source) — instruction-level Cortex-M emulation with peripheral models, many machines per emulation, a CAN hub, scriptable I/O, Robot Framework tests, CI-friendly | **Yes** (the `.elf`, unmodified) | Yes (`CANHub`) | medium | **Build this** |
| QEMU | Yes in principle | No usable STM32 bxCAN/ADC models for these parts | high | no |
| Host build of the logic (`-DDINGO_HOST_TEST`, already in the tree for Timer/Table) | No — recompiled for the PC | n/a | low | keep for unit logic, not for validation |
| Bench HIL: real modules on a desk + a USB-CAN adapter, dingoConfig's **Sim** adapter replaying a CAN log | Yes (real hardware) | real bus | low per run, needs the hardware | the complement: do this before the first drive anyway |

Renode is the only option that executes the shipped image with CAN, ADC, timers and stop mode in the loop
and lets several modules share a bus. Everything below is about Renode.

---

## 2. What the firmware needs from the machine

Taken from `boards/*/port.h`, `*.ld` and `cfg/halconf.h` in this tree.

| | dingoPDM (`dingopdm_v7`) / PDM-Max (`dingopdmmax_v1`) | CANBoard (`canboard_v2`) |
|---|---|---|
| MCU | STM32F446RE, Cortex-M4F @ 180 MHz | STM32F303K8, Cortex-M4F @ 72 MHz |
| Flash | 512 KB at `0x08000000`; **application linked at `0x08004000`** (sector 0 = OpenBLT bootloader, 16 KB) | 64 KB at `0x08000000`; application at `0x08004000`; **config sector 31 (`0x0800F800`, 2 KB) written through the EFL driver** |
| RAM | 128 KB at `0x20000000` | 12 KB at `0x20000000` (+4 KB CCM) |
| Vector table | the startup code sets `VTOR` from `_vectors` — the app runs with or without the bootloader if the PC/SP are taken from the table at `0x08004000` | same |
| ChibiOS HAL drivers | PAL, ADC (ADCD1, 8 channels: 5 Profet current-sense lines IS1, IS2, IS3/4, IS5/6, IS7/8 — the dual-channel BTS7008s share one IS pin via DSEL — plus battery voltage, MCU temperature and VREFINT), CAN (CAND1 bxCAN), I2C (I2CD1: **MB85RC FRAM** at 0x50 for the config, **MCP9808** temperature sensor), PWM (one timer channel per output), USB OTG FS + SERIAL_USB (SLCAN over USB) | PAL, ADC (ADCD1 + ADCD2: 5 analog inputs, MCU temp), CAN, EFL (internal flash as config store), PWM (TIM3/15/16/17, one per digital output — the ISRs toggle plain GPIO) |
| Digital I/O | 2 digital inputs (`LINE_DI1` PA10, `LINE_DI2` PC9), 8 Profet IN/DEN/DSEL lines, `LINE_CAN_STANDBY` PC12, status/error LEDs | 8 digital inputs (pull-up/down selectable), 4 low-side outputs |
| Low power | `EnterStopMode()` → `WFI` + `NVIC_SystemReset()` on wake; wake sources are EXTI lines (configured digital inputs, `LINE_CAN_RX` PB8, USB D+/D−) | never sleeps (`CAN_SLEEP` off) |
| System tick | TIM2 (ChibiOS tick-less) | TIM2 |

Consequences for the model:

- **Both cores are plain Cortex-M4F** — Renode's `cortex-m4f` CPU runs them as-is.
- **CAN is the only link that matters**: config, telemetry, firmware update and inter-module signals all go over
  bxCAN. Renode's STM32 bxCAN model + `CANHub` give you a bus with N modules on it.
- **USB is not needed and cannot be emulated** (no STM32 OTG FS model). The firmware must simply see "USB not
  connected" — which is also the car's normal state. Configure over CAN, as the car does.
- **The PDM config lives in an I2C FRAM** (MB85RC, a 24xx-style EEPROM protocol: 2-byte address, then data).
  Without it the PDM boots with defaults every time, so a small I2C memory model is the first thing to write.
- **The CANBoard config lives in internal flash** through ChibiOS EFL: the model needs an STM32F3 FLASH
  controller (unlock keys, page erase, half-word program) behind the memory, or the "Burn" path fails.
- **ADC values are the main stimulus**: Profet current sense (what the output "draws"), battery voltage, CANBoard
  analog inputs (rotary ladders). Renode's STM32 ADC lets a script feed samples per channel.
- **PWM pins are not waveforms** in Renode's timer model — read the duty from the firmware's own telemetry
  (PDM Msg 23 / CANBoard duty frame) or from the timer registers, not from the pin.

---

## 3. Architecture

```
 ┌──────────── Renode emulation (one process) ─────────────┐
 │  machine PDM-01   machine PDM-02 … machine CB-1  CB-2    │
 │  (dingopdm_v7.elf) (dingopdm_v7.elf)  (canboard_v2.elf)  │
 │     │ can1            │ can1            │ can           │
 │     └────────────┬────┴─────────────────┴───────┐       │
 │                  │        CANHub "vehicle"       │       │
 │                  │                               │       │
 │   stimulus scripts (Robot / Python): GPIO pins,  │       │
 │   ADC samples, "ECU" frames, time               │       │
 └──────────────────┼───────────────────────────────┼───────┘
                    │ host bridge (one of):         │
                    │  • SocketCAN (Linux/WSL2)      │
                    │  • TCP-SLCAN extension        │
                    ▼                               │
          dingoConfig (web/) ── SocketCAN or SLCAN-over-TCP adapter ── your browser
```

- One Renode emulation hosts every module. Each module is a *machine* loading its own `.elf`.
- A `CANHub` is the bus. The hub is also where a bridge to the host attaches, so dingoConfig sees the
  simulated modules exactly like a USB-CAN adapter would.
- Stimulus and assertions come from Renode's monitor/Robot scripts, the same way you would probe pins on a
  bench.

---

## 4. Build it, step by step

### 4.1 Install Renode

Windows: the portable zip from <https://builds.renode.io> (no install). Linux/WSL2/Docker: the `.deb` or the
`antmicro/renode` image. Use a current nightly rather than an old release — the STM32 bxCAN model, the CAN hub and
the SocketCAN bridge are recent additions; check `peripherals` in the Renode monitor and
<https://renode.readthedocs.io> for the exact class names your build ships (names below are from the stock
`platforms/cpus/stm32f4.repl`; verify them).

### 4.2 Platform descriptions (`sim/*.repl`)

Start from the stock STM32F4 platform and override what differs. Keep the files in this repo under `sim/`.

`sim/dingopdm_v7.repl` (sketch):

```
using "platforms/cpus/stm32f4.repl"      // F407 base: same bxCAN, GPIO, TIM, ADC, I2C blocks the F446 uses

flash: Memory.MappedMemory @ sysbus 0x08000000
    size: 0x80000                         // 512 KB — bootloader sector 0 + app from 0x08004000

sram: Memory.MappedMemory @ sysbus 0x20000000
    size: 0x20000                         // 128 KB

// I2C1: config FRAM + temperature sensor (see §4.3 for the two models)
fram: I2C.MB85RC @ i2c1 0x50
    size: 0x2000
tempSensor: I2C.MCP9808 @ i2c1 0x18

// Loads to "watch": LEDs on the Profet IN lines so output changes show up in the log / Robot tests
ledOut1: Miscellaneous.LED @ gpioPortX 0   // one per output, pins from boards/dingopdm_v7/board.h
```

`sim/canboard_v2.repl`: no stock F303 platform ships with Renode — compose it from the F4 pieces that are
register-compatible (bxCAN, GPIO, TIM2/3/15/16/17, NVIC/SysTick) plus an **STM32F3 ADC** (the F0/F3 ADC block,
not the F4 one) and an **STM32F1-style FLASH controller** for the EFL config writes. Flash 64 KB, SRAM 12 KB,
CCM 4 KB at `0x10000000`.

Memory map details you will need are all in `boards/<board>/board.h` (pin assignments, `LINE_*`), `port.h`
(ADC channel order, `CONFIG_SECTOR`) and the `.ld` files.

### 4.3 Peripheral models you have to write

Renode lets you add peripherals in Python (loaded from the `.repl`) or C# (`include @file.cs`). Keep them tiny.

1. **MB85RC FRAM (I2C, PDM)** — a 24xx-EEPROM-style device: on write, the first two bytes are the 16-bit
   address, the rest are data; a read returns bytes from the current address and auto-increments. Back it with a
   byte array and (optionally) a file so the config survives a Renode restart — that is exactly what the FRAM does
   in the car. `hardware/mb85rc.cpp` shows the access pattern the firmware expects (device-ID read on `0xF8`,
   `MB85RC_PROD_ID 0x510`). ~60 lines.
2. **MCP9808 temperature sensor (I2C, PDM)** — return a fixed ambient temperature register (0x05) and accept
   config writes. Make the value settable from the monitor so you can test the over-temperature shutdown
   (`bDeviceOverTemp` / `bDeviceCriticalTemp` in `core/device.cpp`). ~30 lines.
3. **STM32F3 FLASH controller (CANBoard)** if your Renode lacks one: KEYR unlock sequence, `PER` + `STRT` page
   erase of 2 KB pages, `PG` half-word programming, `SR.BSY`/`EOP`. Only sector 31 is ever touched
   (`core/config_handler.cpp` via `EFLD1`). Without it, "Burn" on a simulated CANBoard reports an error and
   the config is lost at reset — a loud, honest failure, so this can come second.
4. **Nothing for USB** — leave OTG FS unmapped. If the firmware's `usbStart` faults on a missing peripheral,
   map a `Python.PythonPeripheral` that returns zeros for the OTG register range; `GetUsbConnected()` then stays
   false, which is the car's normal state.

### 4.4 One script per module type, one for the vehicle (`sim/*.resc`)

```
# sim/pdm.resc — one dingoPDM; parameters: $name, $elf
mach create $name
machine LoadPlatformDescription @sim/dingopdm_v7.repl
sysbus LoadELF $elf
cpu VectorTableOffset 0x08004000          # app runs relocated, no bootloader needed for most tests
connector Connect sysbus.can1 vehicle     # the shared bus
sysbus.gpioPortA.10 Release               # DI1 (ignition) starts open; drive it from the test
```

```
# sim/car.resc — the Ford Ranchero layout: 5 PDM + 2 CANBoard on one bus
emulation CreateCANHub "vehicle"
$elf=@build/dingopdm_v7.elf
$name="PDM-01"; include @sim/pdm.resc
$name="PDM-02"; include @sim/pdm.resc
...
$elf=@build/canboard_v2.elf
$name="CB-1";   include @sim/canboard.resc
$name="CB-2";   include @sim/canboard.resc
start
```

The modules boot with **default configs** (empty FRAM / blank flash sector), i.e. base ID `0x0DE` for every
PDM and `0x640` for every CANBoard — the same collision a fresh set of boards has on a bench. Either
pre-load each module's FRAM image from a known-good config, or bring them up one at a time and re-address them
from dingoConfig as you would on the bench. A `sim/fram/<module>.bin` per module, written by the FRAM model,
makes the second start deterministic.

### 4.5 Bridge the virtual bus to dingoConfig

dingoConfig already has two adapters that fit:

- **SocketCAN** (`infrastructure/Adapters/SocketCanAdapter.cs`, Linux only). If you run Renode and the app under
  WSL2/Linux: Renode's SocketCAN bridge (recent builds) attaches the hub to a `vcan0`/`can0` interface and the
  app connects to it like any Linux CAN adapter. Zero app changes.
- **SLCAN** (`infrastructure/Adapters/SlcanAdapter.cs`, serial today). For Windows, add a transport option so the
  port name `tcp://127.0.0.1:7777` opens a TCP socket instead of `new SerialPort(…)` (one constructor branch;
  the SLCAN text protocol is unchanged), and write a ~150-line Renode C# extension that joins the `CANHub` as a
  member and speaks SLCAN (`t<id><dlc><data>\r`, `T…` for extended, `O`/`C`/`S` accepted and ignored) over that
  socket. The firmware's own `comms/usb.cpp` is the reference for the dialect dingoConfig expects, including the
  `X` filter and `I` identify extensions (answer `I` with a fixed string so the app labels the bridge).

Either way the app then behaves exactly as in the car: Add from CAN finds the modules, Read/Write/Burn work,
telemetry streams, Deploy pushes the project, the Lua uploader and the CAN flasher work.

### 4.6 Drive inputs, watch outputs

- **Digital inputs**: `sysbus.gpioPortA.10 Press` / `Release` (or connect `Miscellaneous.Button` peripherals in the
  `.repl` for named buttons: `ignition`, `door`, …).
- **Analog**: feed the ADC — `sysbus.adc1 FeedSample <raw> <channel> <repeat>` — for Profet current sense
  (`adc1_cfg` in `boards/dingopdm_v7/port.cpp` gives the channel order), battery voltage, and the CANBoard rotary
  ladders (`functions/analog_input.cpp` expects millivolts matching the calibrated positions; 15 = between detents).
- **Other ECUs**: a third machine type is not needed — send frames into the hub from a Python script
  (`emulation` scripting) or from dingoConfig's Sim replay through the bridge.
- **Outputs**: the Profet IN lines are plain GPIO → `LED` models log every change (`machine LogLevel` or Robot
  `Wait For LED State`). PWM duty comes from the telemetry frame, not the pin.
- **CAN**: dingoConfig's Logs tab, or Renode's own `can1 LogFrames` style tracing on the hub.
- **Time**: `emulation RunFor "00:00:30"` makes a 30 s sleep delay take as long as the host needs, not 30 s.

### 4.7 Validation runs worth scripting

Each of these maps to something that already bit this project:

1. **Boot and identity** — every module answers `MsgCmd::Version` on `base+1`/`base+0` with the expected build.
2. **Config persistence** — write + burn a config, `machine Reset`, read it back: FRAM model for PDM, flash sector
   for CANBoard. Also the "CONFIG_VERSION bump loads defaults" path.
3. **Output rules** — drive the ignition input / a rotary voltage, check the Profet lines; feed current sense
   above the limit and expect Overcurrent → Fault/reset per the reset mode in telemetry.
4. **Lua** — upload the project's program, confirm `setLuaOut` slots land on the outputs bound to them (the
   off-by-one in the Ranchero scripts would have shown up here in seconds).
5. **Sleep** — assert the force-sleep input, see the module stop (CPU in `WFI`, telemetry gone, outputs off),
   toggle a wake input or send a frame, see it reset and come back; test "no wake source" is refused by the tool.
6. **Fleet behaviour** — master/follower shutdown sequence across 7 machines with virtual time.
7. **Firmware update over CAN** — load `openblt` into sector 0 of one machine and let dingoConfig's CAN flasher
   push a new app; this also proves the vector-table/relocation story.

Write them as Robot Framework tests (Renode's native test runner); run them in CI with the `renode` Docker
image after `make BOARD=…`, so a broken `.elf` never reaches the `testing` prerelease. The existing
`build_firmware_testing.yml` is the place to add the job.

---

## 5. Limitations to keep in mind

- **Timing is virtual.** Renode preserves ordering and the 500 Hz control loop, not nanosecond timing. Don't
  use it to tune PWM edge timing or the Profet current-sense sampling window.
- **No USB.** SLCAN-over-USB, DFU flashing and the "USB connected blocks sleep" branch cannot be exercised.
- **Analog realism is yours.** Inrush, bulb warm-up and sensor noise only exist if the stimulus script supplies
  them.
- **Peripheral fidelity is "good enough to run", not cycle-exact.** If a test fails only in the simulator,
  suspect the model first; if it fails on the bench too, you found a bug.
- **One emulation, one process.** Seven machines run fine on a laptop; dozens would not.

---

## 6. Suggested order of work

1. Platform file + FRAM model for one PDM; boot `dingopdm_v7.elf`, see the Version reply on the hub.
2. Bridge to dingoConfig (SocketCAN under WSL2 is the shortest path; TCP-SLCAN for native Windows).
3. Read/Write/Burn/reset cycle; then outputs with scripted current sense.
4. CANBoard platform (ADC + flash controller models), rotary stimulus.
5. `car.resc` with the full project, Deploy from dingoConfig, run the Ranchero project's rules.
6. Robot tests + CI.

Steps 1–3 are a few evenings; 4–6 another few. The payoff is that every firmware change and every project
change can be exercised against the real images with the bus in the loop before a single module leaves the bench.
