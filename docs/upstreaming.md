# Upstreaming CoffeeDingoFW features to corygrant/dingoFW: instructions for an LLM agent

You are porting features from this fork (`Coffee0297/CoffeeDingoFW`, branch `master`) to the original
firmware (`corygrant/dingoFW`). The goal is **small, reviewable pull requests that look like the maintainer
wrote them**: one feature per PR, in upstream's code style, built on upstream's current code. A PR that is
quick to read gets merged; a 3000-line "port everything" PR does not.

The dingoConfig side of each feature has its own instructions in CoffeeDingoConfig `docs/upstreaming.md`.

## 0. Ground rules

1. **Only port what the operator asks for.** The community poll decides which features go upstream and in what
   order. Don't port anything that wasn't voted for or requested.
2. **One feature = one branch = one PR.** Never put two features in one PR, even small ones. Bug fixes found on
   the way get their own PR too.
3. **Re-implement on upstream, don't copy files.** The fork branched from upstream `master` at `06cb9e3`
   (2026-05-16); upstream `development` has ~65 commits since (params in their own thread, chunked FRAM
   writes, live CAN filter updates, mute-TX / force-sleep as inputs, wake on any digital input, PT-DPDM4
   board, NeoPixels). Copying a fork file over an upstream file would revert that work. Read the fork's diff
   to understand the feature, then write the change into the upstream file as it is today.
4. **Leave fork-only things out**: Coffee naming, fork README/CHANGELOG, `flash-dingo.ps1`, the fork's
   version numbers, simulator hooks, personal paths, anything about one specific vehicle.
5. **Nothing outward-facing without the operator.** Push branches to the fork, but ask before opening each
   upstream PR, and never push to `corygrant/*` directly. `gh` in this clone defaults to upstream, so always
   pass `-R` explicitly.

## 1. Set up

```bash
git remote -v                      # origin = Coffee0297/CoffeeDingoFW, upstream = corygrant/dingoFW
git fetch upstream
git switch -c up/<feature> upstream/development   # target branch: see below
```

**Target branch:** upstream works on `development` and merges it into `master` for releases. Base each branch
on `upstream/development` and open the PR against `development`, unless the maintainer asks otherwise (check
the latest upstream PRs or ask the operator). Rebase onto `upstream/development` right before opening the PR.

To see what the fork did for a feature:
```bash
git log --oneline 06cb9e3..master -- <paths>     # the fork commits that touch it
git show <commit> -- <paths>                     # read, don't cherry-pick
```
Several fork commits bundle more than one feature (e.g. `ac37f2c` = Lua + overload log + sleep). Split them.

## 2. Match upstream's style

Read 2–3 neighbouring upstream files before writing anything, and copy what you see:

- **Naming:** Hungarian-style prefixes on members and config fields: `bEnabled`, `nInput`, `fArg`, `eOperator`,
  `pConfig`, `pInput`. Config structs are `Config_<Function>` (`struct Config_Condition{ ... };`), classes are
  PascalCase with PascalCase methods (`SetConfig`, `Update`), each function block has
  `static const uint16_t nBaseIndex = 0x....;`.
- **Layout:** follow the file you're editing (brace placement, indentation, `#pragma once`, include order).
  Don't reformat lines you didn't change, and don't add a formatter config.
- **Comments:** upstream has few. Comment only what isn't obvious (a hardware quirk, a timing reason). No
  banners, no changelog-style comments, no fork history.
- **Structure:** a new function block goes in `functions/<name>.{h,cpp}` like `condition.*` / `counter.*`, is
  wired up where the existing blocks are (config struct, param registry, var map, `device.cpp` update loop),
  and gets its params in the same table style as its neighbours.
- **No new dependencies or build-system changes** unless the feature needs them (Lua and the OpenBLT
  bootloader do; say so in the PR).
- **Commit messages:** upstream style is a short lowercase imperative subject, no prefix:
  `add timer function`, `fix can input even start bit`. Not `feat(fw): ...`. A body is fine for the why.
  Ask the operator whether to add an AI co-author line.
- **Don't bump the firmware version** (`device_config.h` BUILD/MINOR); the maintainer does that in his own
  "bump version" commits.

## 3. Config layout, params and the var map: be careful

These are shared with dingoConfig and stored on the module, so mistakes brick configs:

- **Param indices:** use upstream's index map (`core/param_registry.*`, each block's `nBaseIndex`). The fork's
  new indices (e.g. timers, tables, sub-indices 5–7 on digital inputs for PWM) may collide with what upstream
  has added since. Pick free indices in upstream's map and **list every new index/sub-index in the PR
  description**, so the dingoConfig PR matches.
- **Var map:** append new entries at the end of the board's var map, never insert in the middle (dingoConfig
  and existing CAN outputs address it by position). Update `VAR_MAP_SIZE` in every `boards/*/port.h`,
  including **`pt-dpdm4_1`**, which the fork doesn't have.
- **Stored config:** if the PR changes the stored config struct, bump `CONFIG_VERSION` once, say so in the PR
  title or description, and give new fields safe defaults (feature off). The maintainer may renumber it on merge.
- **Flash size:** the CANBoard (STM32F303) is close to full on upstream's build. Report the size before and after
  (`arm-none-eabi-size build/*.elf`) for every board in the PR.

## 4. Each PR: checklist

1. Branch fresh from `upstream/development`.
2. Implement the one feature in upstream style.
3. Build **every** board upstream builds (see `.github/workflows/build_firmware.yml`), from bash, not cmd.exe:
   ```bash
   for b in dingopdm_v7 dingopdmmax_v1 canboard_v2 pt-dpdm4_1; do make clean && make BOARD=$b || break; done
   ```
   No new warnings.
4. Test it: on hardware, or run the built `.elf` in CoffeeDingoSim. Write down what you did; that goes in the PR.
5. Keep the diff small: aim for under ~400 changed lines. If it's bigger, split it into PRs that each build and
   leave the firmware working (e.g. config + params first, then the logic, then the CAN broadcast).
6. Read your own diff: no unrelated whitespace, no leftover debug code, no fork-only references.
7. Push to the fork and draft the PR text (template below), then **ask the operator** before opening it:
   ```bash
   git push origin up/<feature>
   gh pr create -R corygrant/dingoFW --base development --head Coffee0297:up/<feature> --title "..." --body-file pr.md
   ```
8. When dingoConfig needs a matching change, open that PR right after and link the two.

**PR description template**

```markdown
## What
One paragraph: what the feature does for the user.

## Why
The use case (link the Discord poll / issue).

## Changes
- Config: new fields / CONFIG_VERSION change (or "none")
- Params: new indices / sub-indices
- Var map: new entries (position, name)
- CAN: new or changed frames (or "none")

## Testing
Boards built, flash size before/after, what was tested on hardware or in the simulator.

## dingoConfig
Link to the matching dingoConfig PR (or "not needed").
```

## 5. Suggested order and how to split

Small, self-contained ones first: they build trust and keep each review short. Skip anything upstream already
has (check `upstream/development` first: e.g. it already clamps PWM duty, has mute-TX / force-sleep as inputs
and wakes on any digital input).

| # | Feature (fork commits to read) | Split into | Notes |
|---|---|---|---|
| 1 | Reply to a refused single param write (`c13b5f5`) | 1 PR | Tiny, protocol-only |
| 2 | CAN RX FIFO drain on every wake-up, deeper CANBoard mailbox (`ea4d3b5`) | 1 PR | Bug fix, measurable frame loss |
| 3 | No-ACK retransmit back-off (part of `9e982c5`) | 1 PR | Bug fix; separate it from the bootloader work in that commit |
| 4 | Condition hysteresis (part of `ea87220`) | 1 PR | Config change |
| 5 | Timers: on-delay / off-delay / pulse (`functions/timer.*`) | 1 PR | New block, new base index |
| 6 | Lookup tables (`functions/table.*`) | 1 PR | PDMs only; check the CANBoard still fits |
| 7 | PWM frequency from a signal (part of `ea87220`), duty slew (`59b8bf8`) | 2 PRs | |
| 8 | CANBoard DO1–DO4 PWM (part of `2d86890`) | 1 PR | |
| 9 | Analog calibrated multi-position switch (`cab310f`), linear sensor scaling (`8ba7f2a`) | 2 PRs | Replaces the uniform rotary: keep the old mode working, or discuss first |
| 10 | Warning limit / open-load detection | 1 PR | |
| 11 | Output bench test | 1 PR | |
| 12 | Trip log with current waveform (`functions/overload_log.*`, from `ac37f2c`) | 1–2 PRs | |
| 13 | PWM input on digital inputs (`2f7401f`), glitch filter + per-board cap (`8ec2bf5`) | 2 PRs | Needs `PAL_USE_CALLBACKS` on the CANBoard |
| 14 | Sleep: per-pin wake sources, outputs off on sleep (from `ac37f2c`) | 1–2 PRs | Upstream has partly done this differently; port only what's missing |
| 15 | Embedded Lua (`lua/`, from `ac37f2c`) | Discuss first, then several PRs | Large: adds Lua 5.5 to the build, RAM/flash budget, CAN upload protocol |
| 16 | OpenBLT CAN bootloader (`27a7007`, `93331ed`, `7af5d0f`, part of `9e982c5`) | Discuss first, then several PRs | Moves the app address and changes the linker scripts; affects every user's flashing |
| 17 | CANBoard Cortex-M4F / `-Os` build | Discuss first | Build change, frees flash |
| 18 | Host self-test (`tests/host_selftest.cpp`) | Separate PR, optional | Only if the maintainer wants tests in the repo |

**"Discuss first"** means: open an upstream issue (or ask the operator to raise it on Discord) describing the
plan and the split, and wait for the maintainer's OK before writing the code.

## 6. When to stop and ask the operator

- The upstream code differs so much that the feature needs a design decision.
- A param index, var map position or `CONFIG_VERSION` choice could clash with upstream's plans.
- Builds fail for a board you can't fix within the feature.
- The diff won't go under ~400 lines and there's no clean split.
- Before opening any PR or issue on `corygrant/*`.
