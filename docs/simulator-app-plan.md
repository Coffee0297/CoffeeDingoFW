# CoffeeDingoSim — implementation plan

Companion to [`simulator.md`](simulator.md) (the "why Renode / what the firmware needs" guide). This is the build
plan for **a standalone vehicle simulator**: a graphical canvas where you drop bulbs, pumps, fans, switches, knobs,
keypads, an engine and a battery, wire them to the modules of a dingoConfig project *by name*, and the **real
firmware images** run in Renode on a virtual CAN bus that any dingoConfig (original or fork) connects to as if it
were a USB-CAN stick. Every component has a best-guess current curve that scales with its rated current; you watch
model vs. measured on live graphs, break things with one click, record and replay the session, and the same
scenes run headless in CI against every new firmware build.

Decisions below were taken with the owner on 2026-10-06 (four question rounds). Reference project:
`C:\Users\tlm\Documents\dingoConfig\FordRanchero_CANBoards.json`. Verified against Renode `master` and firmware
`e680ef4`+. Nothing is built yet.

---

## 0. Decisions

| Topic | Decision | Notes |
|---|---|---|
| Product shape | **Standalone app, new repo `CoffeeDingoSim`** | Must work with the *original* dingoConfig (corygrant, Blazor Server; adapters USB / SLCAN / PCAN / SocketCAN / Simulated) as well as the fork. |
| Stack | **Node 24 + Svelte 5 + Svelte Flow (`@xyflow/svelte`)**, UI in the browser | One language for library, UI and backend. Node serves the SPA, spawns Renode, owns the TCP links and the COM port, exposes WebSocket + MCP. |
| Load model | **C# peripheral inside Renode (`ProfetLoadBank`) that plays current tables** on the virtual clock | Shape math lives once, in JS; the bank sums, applies faults/voltage/noise, clamps, feeds the ADC. |
| Bridge to dingoConfig | **Virtual COM pair (com0com) speaking SLCAN** on Windows | Plan B transports kept trivial (the relay is a stream): TCP SLCAN for the fork, SocketCAN `vcan` on Linux/CI, a real USB-CAN stick in loopback for mixed real/virtual benches. com0com has Windows 11 signing caveats (§8.4). |
| Project import | **Fork and original project JSON** (same `ConfigFile` schema, fork adds fields), **live config read from modules**, **DBC files** for other ECUs | Names only exist in the project file; limits/inrush/PWM can also be read live. |
| Stimulus | **Full**: switches → digital inputs, rotary knobs → CANBoard analog ladders, Blink Marine / Grayhill keypad nodes, CAN signal generator, **engine model**, **battery node** (Voc + Ri + alternator + crank), **wiper with park-switch feedback** | The canvas is the whole vehicle, not just a load bench. |
| Hardware in v1 | **Full vehicle**: 5 × dingoPDM + 2 × CANBoard (Ranchero), PDM-Max platform included | `car.resc` generated from the project. |
| Renode control | **The sim app launches Renode**, picks firmware per module type (GitHub release tag, `testing-latest`, or a local `build/` folder), optional OpenBLT in sector 0; Start / Stop / Pause / Run-for / Reset module / Sleep-wake; Renode log in the UI | Monitor over telnet (`renode -P <port> --disable-xwt`). |
| Fidelity extras | **Paired/follower outputs, PWM-aware loads, board temperature (MCP9808) + over-temp, sleep/wake** | All in scope; PWM-aware and sleep/wake are the two with sharp edges (§8). |
| Scenarios | **Record & replay** the interactive session (vehicle time), **golden-run regression** (diff a replay against a recorded good run), **CI gate** on firmware builds, **MCP server** in the app | Assertions come from golden runs, not hand-written checks. |
| Packaging | **Clone + `npm install` + `npm start`**; the app downloads the Renode portable zip on first run and links the signed com0com installer | Packaged exe later if wanted. |

---

## 1. Architecture

```
                         CoffeeDingoSim (Node 24 process)                      browser
 ┌────────────────────────────────────────────────────────────────────┐   ┌──────────────┐
 │ project import (ConfigFile JSON, DBC) ─► scene model ─► components.js│   │ Svelte Flow  │
 │ renode.js: download, generate .repl/.resc, spawn, telnet monitor    │◄─►│ canvas + side │
 │ bank.js:   NDJSON ↔ ProfetLoadBank (scene, stimulus, traces)        │ws │ charts, log,  │
 │ bus.js:    SLCAN ↔ SlcanTcpBridge; DBC decode of module telemetry   │   │ Renode ctrl   │
 │ bridge.js: COMx (com0com) ⇄ bus  |  tcp://  |  vcan  |  USB stick    │   └──────────────┘
 │ record.js: session log, replay, golden diff   mcp.js: MCP server     │
 └──────┬──────────────────────┬──────────────────────────────┬─────────┘
        │ spawn + telnet :1234 │ TCP :7800 NDJSON             │ TCP :7777 SLCAN
 ┌──────▼──────────────────────▼──────────────────────────────▼─────────┐
 │ Renode (virtual time)                                                │
 │  PDM-01..05 (dingopdm_v7.elf)  CB-1, CB-2 (canboard_v2.elf)          │
 │   each: loadBank (ADC feed, GPIO in/out, timers, temp, wake pulse)   │
 │  CANHub "vehicle" ── slcanBridge (ICAN)                              │
 └──────────────────────────────────────────────────────────────────────┘
        ▲ COM5 ⇄ COM6 (com0com)                      ▲ Linux CI: vcan0
   original or fork dingoConfig, "SLCAN, COM6"   headless replay + golden diff
```

The Node app is the only TCP client of the bridge; it relays SLCAN lines to whatever dingoConfig is attached (COM
port, TCP or vcan) and injects its own frames (keypads, CAN generator, engine) on the same link. It decodes module
telemetry itself with the firmware DBCs (`dbc/dingoPdm_0.5.1.dbc`, `CANBoard_0.5.1.dbc`), so nothing on the canvas
depends on dingoConfig being open.

---

## 2. The canvas

### 2.1 Node kinds

| Node | Handles | Shows live |
|---|---|---|
| **PDM / PDM-Max** (one per project module, header = project name, e.g. `PDM-04`) | target per output `Fuel Pump (1)`; target per digital input `DI1`, `DI2`; source `Vbatt`, `temp` | output state colour (Off/On/Overcurrent/Fault/Warning/OpenLoad), measured A, duty; sleep badge; board temp |
| **CANBoard** (`CB-1`) | target per analog input `Headlights (AI1)`; target per digital input; source per digital output `Wiper RUN (DO1)` (no current sense) | decoded rotary position + mV; DO states |
| **Load** (palette, §3) | source `supply` → an output handle | model A sparkline, pre-check badge, fault badge |
| **Switch** (toggle / momentary / 3-position) | source `contact` → a digital-input handle; option 12 V or ground-switching | state |
| **Rotary knob** | source `wiper` → a CANBoard analog handle; positions and mV taken from the project's ladder (`rotary.points`, `positionNames`) with ±noise | position name |
| **Keypad** (Blink Marine PKP 2/4/6/8/12-key with dials; Grayhill) | bus only (node id) | buttons you click, **LED colours as the firmware commands them** |
| **CAN generator** | bus only; per-signal sliders / constants from a DBC or raw ID/byte | last frame |
| **Engine** | bus (DBC signals), source `alternator` → battery; target `fan` from a Load | state Off/Ign/Crank/Run, RPM, coolant °C, oil bar, speed |
| **Battery** | source `+12V` (implicit to every module) | V under load, total A |
| **Wiper motor** (coupled load) | source `supply` → PDM output; targets `run`, `speed` from CANBoard DO handles; source `park` → a digital input | angle, speed, park state |

Edges: load → output = "hangs on this output" (several loads on one handle **sum**); switch/knob/park → input;
DO → wiper relay. A load has one supply edge. Faults are an overlay on any load (right-click): `open`, `short`,
`stall`, `intermittent`, `high resistance`, `wrong part`, now or at *t* after turn-on; `clear`. The palette also
has pre-broken items (Burnt bulb, Blocked pump, Short, Broken wire, Loose connector).

### 2.2 Populate from the project (Ranchero as the reference)

1. One module node per `PdmDevices[]`, `PdmMaxDevices[]`, `CanboardDevices[]` with names and base IDs
   (Ranchero: PDM-01 `0x680`, PDM-02 `0x6A0`, PDM-03 `0x6C0`, PDM-04 `0x6E0`, PDM-05 `0x700`, CB-1 `0x660`, CB-2 `0x670`).
2. For every **enabled output**, parse the name:
   - **multiplier** `x2`, `×2`, `2x` → that many instances (`Right Low Beam x2` → two H4 low-beam filaments on one
     handle, so "one of two bulbs burnt" is a one-click test);
   - **composites** `+`, `&`, `and`, `L+R` → one instance per part (`Left Rear Position + Plate` → 5 W position bulb +
     5 W licence bulb; `Seat Heating L+R` → two seat heaters; `Dash Pi + Radio` → Pi electronics + head unit;
     `ECU + O2 Relay Triggers` → ECU + two relay coils);
   - **keyword → component**: `fuel|pump → Fuel pump`, `washer → Washer pump`, `fan|rad|coolant → Radiator fan`,
     `blower|hvac → Blower`, `low beam|high beam|head|spot|fog|driving|aux → Halogen headlight` (LED if `led|bar`),
     `position|park|tail|plate|licen|marker|sidemarker|interior|dome → small bulb` (LED if `led`), `brake|stop →
     P21W`, `indicator|turn|blink|hazard → PY21W`, `reverse → P21W`, `horn`, `wiper → Wiper motor`, `heat|seat|
     defrost|demist|mirror → Heater`, `ecu|pcm|ems|efi|pi|dash|gauge|radio|stereo|gps|cam|usb → Electronics`,
     `amp → Amplifier`, `ign|coil|inj → Pulsed`, `starter|sol → Starter solenoid`, `relay|trigger → Relay coil`,
     `lock|window|actuator|locker → Actuator`, `compressor|air → Compressor`, `glow → Glow plug`, `beacon|strobe →
     Strobe`. No match → `Generic resistive` with an orange **guess** badge.
   - **size**: preset default when the keyword implies one (H4 low 55 W, P21W 21 W, W5W 5 W …), else 60 % of
     `currentLimit`.
3. For every enabled **CANBoard analog input** with `rotary.enabled`: a Rotary knob node with that ladder
   (Ranchero: Headlights OFF/Park/Low/High/Showroom, Wiper OFF/INT/Low/High, Indicators OFF/Left/Right/Hazard,
   Gear P/R/N/D/2/1).
4. For every enabled **digital input** (PDM or CANBoard): a Switch node, type guessed from the input `mode`/`pull`.
5. **Keypads** from `BlinkMarineKeypads[]` / `GrayhillKeypads[]` (node id, key count) — Ranchero has none.
6. **DbcDevices[]** → CAN generator nodes with the DBC's signals (Ranchero has none → the Engine node offers a
   built-in `SimEngine.dbc`: RPM, CLT, OilP, TPS, Speed, Gear).
7. Wiper: if a `Wiper motor` load exists and CANBoard DOs are named `wiper|R_MODE|R_SPEED`, wire them to its
   `run`/`speed` targets (Ranchero CB-2 DO1/DO2).
8. Battery and Engine nodes are always added. Layout: modules in a row, stimulus column left, loads column right.

The populate pass also lists **config observations** it noticed (not errors, just facts the sim will make visible):
Ranchero has `resetMode = None` on every output (first overcurrent → permanent `Fault`), and inrush limits of
50 A / 1000 ms on 2 A outputs — above the 16.4 A sense saturation of outputs 3–8, so the inrush limit can never
trip there and the steady limit does all the work after 1 s.

### 2.3 Scene file (`scenes/<name>.sim.json`)

Everything on the canvas + layout + the project path it was built from + firmware choice per module type. The
rendered **bank scene** (tables per load, §4.3) is derived from it on load and on every change.

---

## 3. Component library (`lib/components/*.json` + `lib/components.js`)

Each component = a **per-unit shape** `i_pu(t)` (1.0 = rated steady) + rating-dependent parameters + a voltage
exponent + optional ripple/events. Instances carry `ratedA` (or W at 13.8 V). Shapes are best guesses from
datasheets and bench lore; every entry has a `source` note and can be replaced by a measured `table`.

### 3.1 Shape families

| Family | `i_pu(t)` | Scaling | `vExp` | PWM-aware behaviour |
|---|---|---|---|---|
| `filament` | `1 + (kCold−1)·e^(−t/τ)`, `kCold` 10 | `τ = 10 ms + 0.6 ms/W` | +0.55 | filament temperature tracks duty: on-phase `i = steady · (1/duty)^0.45` (a dimmed bulb draws more per pulse), capped at `kCold` |
| `led` | spike 6 for 1 ms, then 1 | fixed | −1 | driver sees chopped supply: on-phase = steady, re-spike after > 20 ms off |
| `hid` | 2.5 → 1 over 3 s | fixed | −1 | not PWM-able (badge warns) |
| `motor` | `1 + (kLR−1)·e^(−t/τ)` + ripple | `kLR` 4–6; `τ = 80 ms + 1.5 ms/W` | +0.5 fans / −0.3 pumps | speed follows duty: on-phase current `≈ steady·(1 + (kLR−1)(1−duty))` |
| `actuator` | motor start, then **stall** at `kLR` after `travelMs` until off | preset travel | +0.5 | — |
| `coil` | `1 − e^(−t/τ)` | τ = L/R 5–30 ms | +1 | average = duty, on-phase rises each pulse |
| `solenoid2` | `kPull` 4 for 80 ms, then 1 | fixed | +1 | — |
| `heater` | 1 → 0.75 over `τth` 60–180 s | fixed | +1 | element temperature follows duty → on-phase `1/duty^0.1` |
| `ptc` | 2.5 → 1 over 60 s | fixed | +1 | as heater |
| `glow` | 2 → 1, τ 3 s | fixed | +1 | — |
| `electronics` | spike 8 for 1 ms, then 1 | fixed | −1 | — |
| `amplifier` | electronics + 0.3–1.0 random walk at 2 Hz | fixed | −1 | — |
| `pulsed` | `0.2 ↔ 1` at `rateHz` (from Engine RPM when linked) | — | −1 | — |
| `strobe` | 1 for 50 ms every 500 ms | fixed | −1 | — |
| `compressor` | motor start, 1 → 1.4 over 60 s | τ 500 ms, kLR 6 | +0.5 | — |
| `resistive` | 1 | — | +1 | on-phase = steady |
| `wiper` (coupled) | motor family, two speeds from `run`/`speed` relays, angle integrates at 0.7 / 1.2 rev/s, **park** output true within ±10° of home, load ripple ±35 % per sweep | — | +0.5 | — |
| `table` | user CSV `t_ms,A` | — | 0 | — |

Cool-down: after `coolDownMs` off (filament 1.5 s, motor 2 s, heater 60 s) a curve restarts cold; quicker
re-toggles do not re-inrush. Paired outputs (`nPrimaryOutput` in the module config, or two edges from one load to
two handles): the current is split 50/50 across both sense channels, as the hardware does.

### 3.2 Palette (first cut — each row is a JSON entry with presets; add rows freely)

| Group | Component (family) | Presets (W @ 13.8 V unless A) |
|---|---|---|
| Lighting | Halogen headlight (`filament`) | H1 55, H3 55, H4 60/55, H7 55, H11 55, 100 W aux spot |
| | Halogen signal bulb (`filament`) | P21W 21, P21/5W 21+5, PY21W 21, W5W 5, R5W 5, C5W 5, H21W 21, licence 5 |
| | LED pod / light bar (`led`) | 10, 20, 40, 72, 126, 180, 300 |
| | LED signal lamp (`led`) | tail 2, indicator 3, licence 1, interior 1.5 |
| | HID headlight (`hid`) | 35 |
| | Rotating beacon (`motor`) / LED strobe (`strobe`) | 55 / 12 |
| | Dash & gauge lights (`filament`) | 10 |
| Motors | Radiator fan (`motor`, kLR 5) | 80, 120, 180, 2×180 |
| | HVAC blower (`motor`, kLR 5) | 150, 250 |
| | Fuel pump (`motor`, kLR 4) | 255 lph 60, 340 lph 110, 450 lph 160, lift pump 40 |
| | Electric water pump (`motor`, kLR 4) | 80, 150, 300 |
| | Wiper motor (`wiper`) | 60, 90 |
| | Washer pump (`motor`, kLR 3) | 30 |
| | Window / seat / sunroof (`actuator`, travel 3–8 s) | 60, 90 |
| | Door lock / tailgate (`actuator`, travel 0.3 s) | 40 |
| | Mirror adjust / fold (`actuator`, travel 2 s) | 10 |
| | Diff locker / 4WD actuator (`actuator`) | 50 |
| | Air compressor (`compressor`) | 150, 300, 450 |
| | Power-steering pump (`motor`, PDM-Max) | 600, 900 |
| Coils | Relay coil (`coil`, τ 5 ms) | 1.5 |
| | Solenoid valve / purge / boost control (`coil`, τ 15 ms) | 10, 20 |
| | A/C compressor clutch (`coil`, τ 30 ms) | 45 |
| | Starter solenoid (`solenoid2`) | pull 35 A / hold 8 A |
| | Horn (`resistive`, ±25 % buzz) | 40, 2×40 |
| Heaters | Seat heater (`heater`) | 50, 90 |
| | Rear-window defrost (`heater`) | 150, 250 |
| | Mirror / grip / nozzle heater (`heater`) | 20 |
| | PTC cabin heater (`ptc`) | 300, 600 |
| | Glow plugs (`glow`) | 100 per plug, ×4 |
| | Lambda sensor heater (`ptc`) | 12 |
| Electronics | ECU / PCM (`electronics`) | 20, 40 |
| | Dash / gauges / Raspberry Pi display (`electronics`) | 15 |
| | Head unit / radio / GPS / cameras (`electronics`) | 30, 10, 5 |
| | Amplifier (`amplifier`) | 300, 600 |
| | USB charger / phone (`electronics`) | 15, 30 |
| | Another dingoPDM / CANBoard (`electronics`) | 1.5 |
| | Ignition coils / injectors supply (`pulsed`) | 60, 120 |
| Faults | Burnt bulb, Blocked pump, Short circuit, Broken wire, Loose connector, Wrong bulb (×2) | a component with the fault pre-set |
| Other | Generic resistive, Custom curve (CSV) | any |

### 3.3 Sense-side facts the renderer, pre-check and bank share

`functions/profet.cpp::CalculateCurrent`: `I = raw · (VDDA/4095) / 1200 · kILIS`, inverted
`raw = clamp(I · 1200 · 4095 / (3.3 · kILIS), 0, 4095)`.

| Profet | Outputs | kILIS | counts per A | saturates at | firmware noise floor |
|---|---|---|---|---|---|
| BTS7002-1EPP | PDM out 1–2 | 22950 | 64.9 | 63 A | 0.5 A |
| BTS7008-2EPA (dual, DSEL) | PDM out 3–8 | 5950 | 250.3 | 16.4 A | 0.2 A |
| BTS70012-1ESP | PDM-Max out 1–4 | 35000 | 42.6 | 96 A | 1.0 A |

The **pre-check badge** on a load uses these plus the output config: predicted peak vs `inrushCurrentLimit` /
`inrushTime`, steady vs `currentLimit`, open-load floor vs `fOpenLoadLimit`, and saturation ("a 180 W fan reads
16.4 A here, not 65 A").

> Side finding: `if (nIS > 30000) eState = Fault` in `profet.cpp` can never fire on a 12-bit ADC (max 4095).
> Hardware faults therefore surface as `Overcurrent`, never `Fault`, unless the reset mode escalates. Leave as-is or
> fix separately; the sim will show it.

---

## 4. Stimulus, supply and the bus

### 4.1 Battery node
`V = Voc − Ri · ΣI` with `ΣI` = all simulated loads (plus a 150 A pulse during `crank`). Engine `Run` →
alternator holds 14.2 V (Ri drops to the alternator's). Defaults: Voc 12.6 V, Ri 15 mΩ. Every module's
`BattVolt` ADC channel follows it, so under-voltage and sag-dependent currents are real across all seven modules.

### 4.2 Engine node
States Off → Ign → Crank (1 s, battery dip) → Run. RPM from a throttle slider (idle 800), coolant °C rises toward
95 °C while running and is pulled toward 85 °C by a linked fan load that is On, oil pressure `f(RPM)`, speed from
a slider, gear from a linked rotary (Ranchero CB-1 `Gear`). Broadcast as DBC frames at the DBC's cycle times:
the project's `DbcDevices` if present, else the built-in `SimEngine.dbc`. The `pulsed` family takes `rateHz` from
RPM when linked.

### 4.3 How stimulus reaches the firmware (bank protocol additions)

- **Digital inputs** → `{"machine":"PDM-01","gpio":{"DI1":1}}` → bank drives the pin (also an EXTI wake source).
- **Rotary / analog** → `{"machine":"CB-1","adc_mV":{"1":1500}}` → bank converts to raw for the CANBoard's
  `STM32F3_ADC` channel (12-bit over 3.3 V; the firmware's ladder math is in `functions/analog_input.cpp`).
- **CANBoard digital outputs** are GPIO *outputs* the bank watches and reports (`"do":[1,0,0,0]`) → feed the
  wiper relays and any DO-driven node.
- **Keypads** and **CAN generator / Engine** frames come from the Node app through the bridge. Blink Marine PKP:
  buttons PDO at `nodeId+0x180`, LED/colour/backlight commands from the firmware at `nodeId+0x200/0x300/0x400/0x500`,
  NMT at `0x000` (`functions/keypad/blink/`). Grayhill: buttons at `nodeId+0x180`, LEDs at `+0x200/+0x300`
  (`functions/keypad/grayhill/`). The keypad node renders the colours the firmware commands.
- **Board temperature** → `{"machine":"PDM-03","tempC":85}` → MCP9808 model register; the over-temp path in
  `core/device.cpp` is then exercised.
- **Wake pulse**: when any frame hits the hub, the bank toggles the sleeping module's `LINE_CAN_RX` (PB8) once so the
  EXTI wake works as it does on hardware (the bxCAN model does not wiggle the pin, §8.3).

### 4.4 Bridge and telemetry

`SlcanTcpBridge.cs` is an `ICAN` on the hub and a TCP server. The Node app is its single client and:
- relays SLCAN lines to the attached dingoConfig transport — **com0com COM port** (via `serialport`), or
  `tcp://` for the fork, or `vcan0` on Linux (via Renode's own SocketCAN bridge), or a real USB-CAN stick;
- answers the fork's `I` identify and honours its `X` filter itself; the original dingoConfig speaks plain SLCAN;
- decodes every frame with the firmware DBCs (per module base ID from the project) → output states, currents, duty,
  CANBoard positions, sleep state → canvas and charts;
- injects keypad / generator / engine frames.

### 4.5 Charts
Click a load, an output handle, the battery or the engine → side chart (30 s canvas, zoom to the recording):
model current (bank trace at 10 Hz virtual) vs measured (Msg 1/2, 0.1 A/bit, firmware peak-hold) with a state band
(Msg 3); battery V and total A; engine signals. Palette thumbnails show each component's rendered curve.

---

## 5. Renode side (`renode/` in the new repo)

| File | Lines (est.) | Notes |
|---|---|---|
| `platforms/dingopdm_v7.repl` | 80 | from stock `stm32f4.repl`; flash 512 KB, sram 128 KB, `cortex-m4f`, nvic systick 180 MHz, **all timers 90 MHz**, `fram: I2C.MB85RC1MT @ i2c1 0x50`, `tempSensor: I2C.MCP9808 @ i2c1 0x18`, `loadBank`, `otgStub`, GPIO wiring (IN/DSEL → bank; bank → DI pins) |
| `platforms/dingopdmmax_v1.repl` | 40 | 4 outputs, BTS70012 |
| `platforms/canboard_v2.repl` | 90 | composed: bxCAN, GPIO, TIM2/3/15/16/17, NVIC + `Analog.STM32F3_ADC` ×2 + `MTD.STM32F0_FlashController` (2 KB pages ×32) + 64 KB flash, 12 KB SRAM, 4 KB CCM |
| `models/ProfetLoadBank.cs` | 250 | `IGPIOReceiver` + GPIO outputs, `ClockEntry` 1 kHz, table player, faults, voltage/noise, paired split, PWM-aware on-phase, DSEL re-feed, timer register reads, ADC mV feed (CANBoard), DO watch, temp set, wake pulse, static NDJSON listener |
| `models/SlcanTcpBridge.cs` | 150 | `ICAN` + `TcpListener`; `t T r R` both ways; `O C S L V N` no-ops; frame log |
| `models/Mcp9808.cs` | 40 | ambient 0x05, limits, manuf `0x0054` / device `0x0400`; `SetTemperature` |
| `models/OtgFsStub.cs` | 15 | `0x50000000`: GRSTCTL (0x10) reads `0x80000000`, rest 0 |
| `templates/*.resc.hbs` | 60 | generated per scene by `renode.js`: hub, one `mach create` per module, ROM cal words, FRAM/flash image paths, bridge + bank ports, monitor port |

**Firmware**: `renode.js` fetches `*_FW_v*.elf` from a GitHub release tag or `testing-latest` into
`cache/firmware/<tag>/`, or uses a local folder; OpenBLT `dingopdm_blt.elf` optional in sector 0.

**Bring-up without ID collisions**: every module boots with default base IDs (`0x0DE` PDM, `0x640` CANBoard).
First run of a scene: the app starts machines **one at a time**, and for each sends the minimal param-protocol
sequence *set base ID + burn* (subset of `core/param_protocol.cpp`; verify the exact command bytes), then starts the
next. FRAM/flash images are written by the models to `scenes/<name>/nv/<module>.bin`, so every later start boots
all seven with their project IDs. Full config deploy stays dingoConfig's job (original or fork), exactly as on the
bench.

**Time control**: `renode -P <port> --disable-xwt --console` and monitor commands over telnet: `pause`, `start`,
`emulation RunFor "0:0:60"`, `mach set "PDM-01"; machine Reset`, `sysbus.loadBank Fault …`. Virtual time is shown
in the UI; recordings are stamped with it.

---

## 6. Record, replay, golden runs, CI, MCP

- **Recording**: every user action (switch, knob, keypad, fault, battery/engine change, Renode control) is logged
  with virtual time into `scenes/<name>/runs/<timestamp>.run.json`, together with the decoded telemetry stream.
- **Replay**: the app re-issues the actions at the same virtual times (Renode is paced by `RunFor` steps, so a
  replay is deterministic regardless of host speed) and records again.
- **Golden run**: mark a run as known-good; a later replay diffs output states, overcurrent counts, sleep events
  and currents (tolerance %) per module and shows the differences on the canvas and as a table.
- **CI gate** (`CoffeeDingoFW/.github/workflows/build_firmware_testing.yml` adds a job): Linux runner, Renode Docker
  image, `vcan0`; clones `CoffeeDingoSim`, `npm run ci -- --scene scenes/ranchero --firmware build/ --golden
  runs/golden.run.json`; a diff fails the `testing` prerelease.
- **MCP server** (`@modelcontextprotocol/sdk`, stdio + HTTP): `sim.load_project`, `sim.populate`, `sim.place_load`,
  `sim.connect`, `sim.set_fault`, `sim.switch`, `sim.rotary`, `sim.keypad_press`, `sim.engine`, `sim.battery`,
  `sim.renode(start|stop|pause|run_for|reset)`, `sim.state(module)`, `sim.trace(module, output, since)`,
  `sim.record(start|stop)`, `sim.replay`, `sim.golden_diff`. Enough for an agent to run fault campaigns.

---

## 7. Repo layout (`CoffeeDingoSim`)

```
package.json            svelte, vite, @xyflow/svelte, serialport, ws, @modelcontextprotocol/sdk
server/                 index.js (http+ws), renode.js, bank.js, bus.js (SLCAN + DBC decode), bridge.js,
                        project.js (ConfigFile import), record.js, mcp.js, firmware.js (GitHub releases)
app/                    Svelte 5 SPA: Canvas.svelte (Svelte Flow), nodes/*.svelte, Palette, SideChart (canvas),
                        RenodePanel (start/stop/time/log/firmware picker), RunsPanel
lib/                    components.js + components/*.json, shapes.js, precheck.js, dbc.js (minimal parser),
                        slcan.js, paramproto.js (set-base-id + burn), engine.js, battery.js
renode/                 platforms/*.repl, models/*.cs, templates/*.resc.hbs, dbc/ (copied firmware DBCs + SimEngine.dbc)
scenes/ranchero/        scene.sim.json, nv/*.bin, runs/*.run.json
tools/                  install-com0com.md, ci.sh
```

---

## 8. Sharp edges (verified in source unless marked *verify*)

### 8.1 Platform
- **Factory calibration words are zero in Renode.** `GetVDDA()` divides by `*(0x1FFF7A2A)`; stock `rom1` is plain
  memory → 0 V → 0 A forever. `.resc` writes `0x1FFF7A2A = 1500`, `0x1FFF7A2C = 943`, `0x1FFF7A2E = 1194`
  before `start`. CANBoard (F303): same at `0x1FFFF7B8/0x1FFFF7C2` (`boards/cortex-m3/mcu_utils.h`).
- **Timer clocks.** Stock repl: every `STM32_Timer` at 10 MHz. PDM runs 180 MHz with APB ÷4 → timers **90 MHz**;
  ChibiOS's TIM2 tick-less timer at 10 kHz derives from it. Leave 10 MHz and `SYS_TIME` runs 9× slow. CANBoard:
  72 MHz, check its `mcuconf.h`.
- **USB OTG FS.** `InitUsb()` always calls `usbStart`; `otg_core_reset()` spins on `GRSTCTL.AHBIDL`/`CSRST`
  (`hal_usb_lld.c:152-164`). Unmapped → bus fault; zero-memory → boot hang. The stub fixes it; `GetUsbConnected()`
  stays false.
- **FRAM model.** Renode's `MB85RC1MT` is the 1 Mbit part (17-bit address); the board's MB85RC256V/128A use a
  2-byte address (`hardware/mb85rc.cpp`). Try, else fork to `Mb85rc256.cs` (~60 lines). File-backed per module.
- **MCP9808.** Not in Renode; `Device::Init` checks manufacturer/device IDs (`mcp9808.cpp::CheckId`).
- **ADC via DMA.** PDM ADC1 runs circular continuous scan through DMA2; `STM32_ADC.DMARequest` must be wired to
  `dma2` (stock repl does). `FeedSample(raw, channel, -1)` holds a value; channel = ADC input number (IS1 0, IS2 12,
  IS3_4 13, IS5_6 1, IS7_8 2, BattVolt 3, temp 16, VREFINT 17), not the sequence slot.
- **F446 vs F407 base**: same bxCAN/GPIO/TIM/ADC/I2C/DMA blocks; RCC differs but Renode fakes ready bits. A hang in
  `halInit` → look at `RCC->CR` polling.
- **Renode nightly needs a .NET runtime**; .NET 10 preview SDK is present, .NET 8 runtime may be needed.

### 8.2 Load bank
- Feed **on-phase** current for PWM outputs (firmware samples inside the on window); duty only for display and the
  PWM-aware shapes.
- DSEL re-feed in the GPIO callback, not the next tick, or ch 3/5/7 bleed into 4/6/8.
- Tables are per load, summed per output at tick time; each load keeps its own on-clock and cool-down.

### 8.3 Sleep / wake *(verify in Renode)*
- `EnterStopMode()` = PWR/SCB deep-sleep + `WFI`, wake → `NVIC_SystemReset()`. Renode's Cortex-M handles `WFI`;
  confirm it honours `AIRCR.SYSRESETREQ` as a machine reset (else map it to `machine Reset` from the bank).
- Wake sources are **EXTI pins**: configured digital inputs, `LINE_CAN_RX` PB8, USB D+/D−. The bxCAN model does not
  toggle PB8 on a received frame → the bank pulses it on hub traffic (§4.3), otherwise "wake on CAN" never fires.
- A sleeping module's telemetry stops; the app marks it asleep from silence + the last state frame.

### 8.4 Bridge
- **com0com on Windows 11** needs a properly signed build: the SourceForge 3.0.0.0 package is reported not to load
  on Windows 10 1607+/11 with Secure Boot; use the signed build from the BrickBot archive, or disable Secure Boot,
  or fall back to TCP (fork) / a real USB-CAN stick in loopback / Linux `vcan`. The relay is a stream, so each
  transport is ~20 lines. Document the install in `tools/install-com0com.md`; the app tests the pair on start.
- Original dingoConfig speaks plain SLCAN at 115200-equivalent framing; keep `I`/`X` handling in the app, never
  assume them.
- **Default base IDs collide** at first boot (§5 bring-up sequence). Scenes key modules by **project name**; the app
  maps name → base ID from the project, so re-addressing never orphans canvas edges.

### 8.5 Protocol subset *(verify)*
- Set-base-ID + burn via `core/param_protocol.cpp` / `comms/request_msg.cpp` (`MsgCmd` family, full-config
  `CheckCrc` from commit `1c70811`). Read the firmware before implementing `lib/paramproto.js`; if the subset turns
  out larger than ~100 lines, fall back to "first bring-up through dingoConfig, then images persist".

---

## 9. Phases and exit criteria

| # | Work | Exit criterion | Effort |
|---|---|---|---|
| 0 | New repo, Node skeleton, Renode download, `dingopdm_v7.repl` + cal words + OTG stub + timer clocks + FRAM/MCP9808, generated `.resc`, bridge | one PDM boots, status LED in the log, `Version` reply seen in the app's frame log | 2 evenings |
| 1 | `canboard_v2.repl` (F3 ADC ×2, F0 flash), Ranchero `car.resc` (7 machines), bring-up sequencer (set-base-ID + burn) or documented manual re-addressing, persisted NV images | all 7 modules on the hub with project IDs; original dingoConfig on com0com and the fork on `tcp://` both list them; Read/Write/Burn/reset persists on PDM (FRAM) and CANBoard (sector 31) | 3 evenings |
| 2 | `ProfetLoadBank.cs` table player (+ DSEL, timers, paired split, PWM-aware), `components.js` with the §3.2 palette, Node bank link, DBC telemetry decode | hand-built scene: 55 W bulb on PDM-01 out 1 shows 36 A peak → 4 A in the app's chart and in dingoConfig; stall on the fuel pump → `Overcurrent` → `Fault` (Ranchero reset mode None); burnt bulb → `OpenLoad`; short → inrush trip | 3 evenings |
| 3 | Canvas: module/load nodes, palette, edges, faults, pre-check badges, **Populate from project** (multipliers, composites, keywords, rotaries, switches), battery node, scene save/load | open Ranchero → Populate → every enabled output has sized loads wired by name, both headlight filaments present, "Position + Plate" split; badges flag the saturation facts of §2.2; two bulbs on one output double the model line | 3 evenings |
| 4 | Stimulus: switch, rotary knob (ladder from project), keypad nodes (Blink/Grayhill with LED feedback), CAN generator + DBC import, Engine node + `SimEngine.dbc`, wiper park coupling, board temperature slider, wake pulse | turn CB-1 Headlights knob to `Low` → PDM-01/02 low beams on with inrush; Indicators `Left` → flashers blink; Engine `Run` → alternator 14.2 V, fan cools coolant; wiper park switch stops the motor at home; temp 90 °C → over-temp path; ignition off → modules sleep, a frame wakes them | 4 evenings |
| 5 | Renode panel: start/stop/pause/run-for/reset/sleep-wake, firmware picker (releases, `testing-latest`, local), OpenBLT option, log view | pick `v5.5.106` for PDMs and a local build for CANBoards, run 60 s of vehicle time in a few host seconds, reset PDM-03 alone | 2 evenings |
| 6 | Record/replay, golden runs, MCP server | record a 2-minute session, replay it deterministically, mark golden, replay against another firmware tag and read the diff; drive the same from an MCP client | 3 evenings |
| 7 | CI gate on Linux (Renode Docker + `vcan0`, headless app), firmware workflow job | a deliberately broken `.elf` fails the `testing` prerelease with a readable diff | 2 evenings |

Roughly 22 evenings end to end; phases 0–3 (about 11) give the usable load bench with the full Ranchero on the bus.

---

## 10. Not doing (on purpose)

- No Python / External-Control harness, no WSL requirement; Windows-native with com0com, Linux-native with vcan.
- No thermal model of the PDM board itself (temperature is a slider), no wire-gauge voltage drop per feed, no
  cycle-accurate PWM edges, no full dingoConfig protocol reimplementation (config deploy stays in dingoConfig).
- No packaged installer yet; `npm start`. A Node single-executable build can come once the tool settles.
- Component curves are not calibrated measurements; each entry carries a `source` note, and any curve can be
  replaced by a bench-logged `table`.
