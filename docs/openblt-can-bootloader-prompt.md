# Implementation prompt — OpenBLT CAN bootloader for dingoFW + dingoConfig

> Hand this to an implementing agent. It encodes the codebase facts so you don't rediscover them.
> Work in **phases**, stop at each **GATE** for human confirmation, and never break the existing
> SWD/DFU flash paths. Cite `file:line` for every change.

## Goal

Add an **OpenBLT** (Feaser, https://www.feaser.com/openblt/doku.php?id=manual:can_demo) CAN bootloader
to **every dingoFW board** (`canboard_v2`, `dingopdm_v7`, `dingopdmmax_v1`) so that, after a **one-time
SWD flash of the bootloader**, the **application firmware can be reflashed over CAN** from the
**dingoConfig** tool using a dingoPDM or any SLCAN CAN probe as the bus interface. The CAN bootloader is
a standard part of all three boards.

**The CANBoard (STM32F303K8T6) is the first test/validation target** — prove the whole pipeline there,
end to end, before porting: it has no USB so CAN is its only field-update path, and its 64 KB flash is
the tightest fit, so if it works there it works everywhere. The **dingoPDM / -Max (STM32F446)** get the
**same** CAN bootloader (not an optional add-on) — and **keep** their existing USB-DFU path; CAN becomes
an additional update channel, DFU is not removed.

Suggested order of work: full canboard pipeline (Phases 1→3) end-to-end first, then replicate Phases 1–2
to the two F446 boards (the host/UI in Phase 3 is board-agnostic once it works).

Transport: OpenBLT's **XCP over CAN**. Host side: a XCP-over-CAN master in dingoConfig driving the
existing SLCAN adapter (see `infrastructure/Adapters/SlcanAdapter.cs`).

## Repos

- **Firmware:** `CoffeeDingoFW` (ChibiOS, C/C++). Build `make BOARD=<board>` (`canboard_v2`,
  `dingopdm_v7`, `dingopdmmax_v1`). Per-board `boards/<board>/board.mk`, linker
  `boards/<board>/<board>.ld`. Toolchain at `C:\dingo-tools\arm\...\bin` (arm-none-eabi-gcc 13.3).
- **Config tool:** `CoffeeDingoConfig` (.NET 10 + Svelte). `domain` / `infrastructure` / `application`
  / `web`. SLCAN in `infrastructure/Adapters/SlcanAdapter.cs`. UI flows in `web/clientapp/src`.
  Existing DFU flasher: `web/clientapp/src/lib/SystemView.svelte` ("Flash new module over USB DFU",
  via dfu-util) — **the OpenBLT-CAN flow lives alongside it, not replacing it.**

## Codebase facts you must build on (verified)

### Firmware
- **Existing bootloader trigger:** `MsgCmd::Bootloader = 33` (`core/enums.h`). `comms/request_msg.cpp`
  (~line 81) matches it on the config-RX frame and calls `RequestBootloader()`. dingoConfig sends it
  via the **`bootloader` device action**.
- **Current bootloader impl is system-DFU, F4 only:** `boards/cortex-m4/mcu_utils.cpp::RequestBootloader()`
  sets a RAM magic and resets; `boards/cortex-m4/enter_bootloader.S` overrides the reset handler to jump
  to the STM32 **system** bootloader. The **CANBoard uses `MCUDIR = boards/cortex-m3`** (see
  `boards/canboard_v2/board.mk`) precisely because the cortex-m4 path hardcodes an F4 RAM magic address
  wrong for the F303 — so the CANBoard has **no working bootloader today**.
- **CANBoard flash map** (`boards/canboard_v2/canboard_v2.ld`):
  - `flash0 (rx) : org = 0x08000000, len = 62k`  ← app + vectors
  - `flash_config (r) : org = 0x0800F800, len = 2k` ← persistent config (last sector, `CONFIG_SECTOR 31`)
  - 64 KB total. App currently ≈ **37.6 KB** (`text`); RAM 12 KB + 4 KB CCM.
  - `REGION_ALIAS("VECTORS_FLASH", flash0)` — ChibiOS crt0 sets `VTOR = _vectors`, so **moving `flash0`'s
    origin relocates the vector table automatically** (no manual VTOR code needed).
- **PDM/-Max flash:** STM32F446, 512 KB — ample room.
- **CAN:** `comms/can.cpp`; bitrate configs in `boards/<board>/port.cpp`. Param protocol on
  `base+0` (TX to tool) / `base+1` (RX from tool). Cyclic broadcasts `base+2 …`. Default base IDs:
  CANBoard `0x640`, PDM/-Max `0x0DE`. Per-type CAN-ID footprint helper: dingoConfig
  `web/clientapp/src/lib/canids.js` (`spanAfter`) and `tools/canfree.py`.
- Config is stored by the app in the **last 2 KB sector** — the bootloader and its flash driver must
  treat that sector as off-limits (don't erase it on app update) so settings survive a reflash.

### dingoConfig
- SLCAN serial CAN adapter already implemented; the tool owns the COM port while connected.
- Existing **DFU** flasher shells out to `dfu-util` from `SystemView.svelte`. Mirror that UX.
- MCP server (`web/Api/McpServer.cs`) exposes device actions; add the CAN-flash as a tool too.

## GATE 0 — Feasibility (do this FIRST, report numbers, then stop)

The CANBoard is the binding constraint: **64 KB flash**. Budget:
`bootloader + app + 2 KB config ≤ 64 KB`.
- A CAN-only OpenBLT for Cortex-M4 is typically **~13–16 KB**. Reserve a sector-aligned **16 KB**
  region (F303 has 2 KB sectors → 16 KB = 8 sectors): bootloader `0x08000000–0x08004000`, app
  `0x08004000–0x0800F800` (**46 KB**), config `0x0800F800` (2 KB).
- App is 37.6 KB now → fits in 46 KB with ~8 KB headroom. **Confirm** by building OpenBLT and checking
  its size, and re-confirm the app still fits after relocation. If OpenBLT > ~16 KB, trim its features
  (CAN only; disable UART/USB/file-system/checksum-hook bloat) before widening the region.
- Report: measured OpenBLT size, resulting app free space. **Do not proceed past this gate without
  a confirmed fit.**

## Architecture decisions (make these explicit, justify, get sign-off at GATE 1)

1. **OpenBLT integration style.** OpenBLT is bare-metal with its own startup — it is a *separate*
   project from the ChibiOS app, not a library linked in. Recommended: vendor OpenBLT under
   `bootloader/` with a port per MCU (`Target/Source/ARMCM4_STM32F3`, `..._STM32F4`), its own
   `blt_conf.h`, and a build that emits `bootloader/build/<board>_blt.elf/.hex`. Keep it out of the
   app's `make` so the app build is unchanged except for the linker origin.
2. **XCP CAN identifiers.** Decide whether IDs are fixed (OpenBLT default 0x667 cmd / 0x7E1 resp) or
   **derived per-module from the base ID** so several modules can coexist and be flashed individually.
   Strongly prefer per-module IDs (e.g. a dedicated offset pair outside the cyclic range). Document the
   chosen IDs and update `canids.js` / `canfree.py` / `docs/can-frame-map.md` if the footprint grows.
3. **Two distinct "enter bootloader" actions — keep them separate.** There are now two field-update
   channels and they must not be conflated:
   - **CAN / OpenBLT entry** (all boards): activate OpenBLT — write its shared magic
     (RAM/backup-register "SharedParams") + `NVIC_SystemReset()`; OpenBLT reads it at reset and stays
     in CAN-update mode instead of starting the app.
   - **USB-DFU entry** (PDM/-Max only): the **existing** STM32 ROM-system-bootloader path that
     dfu-util uses today — **must be preserved**.
   On the **CANBoard** (no USB) reuse `MsgCmd::Bootloader (33)` → OpenBLT. On the **PDMs**, keep
   `MsgCmd::Bootloader (33)` → USB-DFU (existing behaviour) **and add a new command/sub-action** for
   OpenBLT-CAN entry (don't overload 33 to mean different things per board in a way that breaks the
   tool). See decision #6 for the USB+CAN coexistence mechanics.
4. **App-valid check + backdoor.** OpenBLT jumps to the app only if a valid app is present (checksum /
   first-vector sanity) else stays in the bootloader; keep a short startup backdoor window so a bricked
   app is still recoverable over CAN. Confirm the F303 RAM magic address used for SharedParams is valid
   for 12 KB SRAM (the bug that forced the CANBoard onto `cortex-m3` MCUDIR — do not repeat it).
5. **Firmware file format.** OpenBLT consumes **S-record (.srec/.s19)**. Add an `objcopy` step to emit
   `<board>.srec` next to the existing `.hex/.bin`.
6. **PDM/-Max: USB *and* CAN must both flash the app (DFU is NOT removed).** With OpenBLT at
   `0x08000000` and the app relocated above it, the existing dfu-util/ROM-bootloader USB flow needs
   three adjustments so it keeps working and never clobbers OpenBLT:
   - **dfu-util targets the relocated app base**, not `0x08000000`. Update dingoConfig's DFU flasher
     (the dfu-util invocation in `SystemView.svelte`) and any `.dfu`/address metadata to the app origin
     (e.g. F446 sector 1 `0x08004000`). The bootloader region (sector 0) is never written over USB.
   - **USB-DFU entry can no longer rely on the app's reset-handler override** (`enter_bootloader.S`):
     OpenBLT now owns the reset vector. Instead, on the USB-DFU command the **app jumps directly to the
     STM32 system bootloader** (deinit clocks/peripherals, set MSP, branch to `0x1FFF0000`) — the
     standard "jump to system memory" sequence — independent of OpenBLT. Verify dfu-util still
     enumerates and flashes after this change.
   - Both bootloaders (ROM-system for USB, OpenBLT for CAN) flash the **same relocated app region**;
     neither touches the other's region or the config sector. A `.srec` is for OpenBLT/CAN, the
     `.bin`/`.dfu` at the relocated base is for USB — same app image, two containers.
   - The CANBoard is unaffected (no USB) — `MsgCmd::Bootloader (33)` → OpenBLT, single channel.

## Phase 1 — OpenBLT bootloader port (firmware)

Per target board:
- Vendor OpenBLT; create the MCU port (clock = the board's actual config; CAN pins = the board's, see
  `boards/<board>/board.h` line assignments and `port.cpp` bitrate tables).
- `blt_conf.h`: enable **CAN only**; set CAN baudrate to match the board default and the dingoConfig
  default (500 kbit); set the XCP CAN IDs per decision #2; set the flash layout so the bootloader owns
  its region, the app region is programmable, and the **config sector is excluded**.
- Flash driver layout table must list the app sectors only (never the bootloader's own, never the
  config sector).
- Build → record size (GATE 0). Output `.hex` for SWD programming.

GATE 1: bootloader builds, fits, flashes via SWD (use the picoprobe/ST-Link launch configs already in
`.vscode/launch.json`), and on power-up sits waiting on CAN (verify with `candump`/dingoConfig raw log).

## Phase 2 — Application relocation + activation (firmware)

- Move `flash0` origin in `boards/<board>/<board>.ld` to the app base (CANBoard `0x08004000`), reduce
  `len` accordingly; leave `flash_config` untouched. Rebuild; confirm VTOR is right (ChibiOS sets it
  from `_vectors`) by halting and reading `SCB->VTOR == app_base`.
- Implement OpenBLT activation in `mcu_utils` (decision #3): on `MsgCmd::Bootloader`, set SharedParams
  magic + reset. Keep `CheckBootloaderRequest()` semantics.
- Verify the relocated app still boots and runs (SWD: PC reaches `main`, idle thread runs — same check
  used to validate the current build).

GATE 2: bootloader + relocated app both on the chip via SWD; app runs normally; sending the bootloader
command drops it into OpenBLT (app stops, bootloader answers XCP).

## Phase 3 — dingoConfig CAN-flash host

- Add an **XCP-over-CAN master** that drives the existing SLCAN connection (do **not** require releasing
  the port to an external exe if avoidable; if you instead shell out to OpenBLT's `BootCommander` with
  its Lawicel/SLCAN driver, you must hand off the COM port cleanly and restore the live connection
  after). Recommended: implement the XCP master in C# in `infrastructure`/`application`, reusing
  `SlcanAdapter`, so the tool stays in control of the bus and UX.
- Sequence: (a) send the module's **OpenBLT-CAN entry** command (the new command from decision #3 — on
  PDMs this is NOT the USB-DFU command 33); (b) wait for the module's OpenBLT to appear (XCP connect on
  the module's IDs); (c) program the `.srec`; (d) reset/run; (e) re-discover the module and confirm the
  new version.
- UI: a **"Update firmware over CAN"** action in `SystemView.svelte` next to the DFU flasher — file
  picker (.srec), progress bar, honest success/fail (mirror the DFU flow's reporting). Also expose as
  an MCP tool in `web/Api/McpServer.cs` (e.g. `flash_firmware_can`) and document it.
- **Keep USB-DFU working on the PDMs (decision #6):** retarget the existing dfu-util flasher to the
  relocated app base (not `0x08000000`) so it never overwrites OpenBLT, and confirm the USB-DFU button
  still flashes a PDM end-to-end. The PDM UI should offer **both** "Update over USB (DFU)" and "Update
  over CAN"; the CANBoard shows only the CAN option.
- Update `docs/can-frame-map.md` (in CoffeeDingoConfig) and `canids.js`/`canfree.py` if the XCP IDs
  enlarge any module's footprint.

GATE 3: from dingoConfig, select a connected CANBoard, pick a new `.srec`, flash over CAN end-to-end,
module reboots into the new app, version reads back updated. Repeat with a second module on the bus to
prove per-module targeting (decision #2).

## Phase 4 — Hardening & docs

- Brown-out / interrupted-flash recovery: pull power mid-flash → module must still be re-flashable over
  CAN (bootloader intact, app invalid → bootloader waits). Verify.
- Wrong-board guard: refuse to flash a `.srec` whose target doesn't match the module type.
- Update both repos' `CHANGELOG.md` and the firmware `README` (bootloader region, the new SWD-once /
  CAN-after workflow, the reserved flash map).

## Constraints / do-not-break

- Don't move or erase the **config sector** on app update — settings must persist across CAN reflashes.
- Don't remove the **USB-DFU** path on the PDMs or the **SWD** launch configs.
- Keep the CAN **base-ID / footprint** model coherent: any new permanent IDs go through
  `canids.js`, `tools/canfree.py`, and `docs/can-frame-map.md`.
- The **first** bootloader install is SWD-only (expected). Everything after is CAN.
- Re-verify the **F303 flash budget** after every size-affecting change.

## Acceptance criteria

1. **All three boards** build an OpenBLT bootloader + a relocated app, both SWD-installable, and the
   relocated app runs normally (reaches `main`, idle thread runs, SWD debug via both launch configs works).
2. **CANBoard (proven end-to-end):** SWD-flash bootloader once → thereafter flash the app purely over CAN
   from dingoConfig with an SLCAN probe; settings survive; version reads back correct.
3. **dingoPDM and dingoPDM-Max:** same CAN reflash works over CAN, **and** USB-DFU still works (not removed).
4. A mid-flash power cut leaves the module recoverable over CAN (bootloader intact, invalid app → waits).
5. Two modules on one bus are individually flashable (per-module XCP IDs, decision #2).
6. App still fits the F303 with headroom; CAN footprint stays coherent (`canids.js`/`canfree.py`/frame map).
