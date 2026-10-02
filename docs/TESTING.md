# Build and test

## Windows build

Install [CEdev v15](https://github.com/CE-Programming/toolchain/releases/tag/v15.0), extract its Windows package into `.tools\CEdev`, and confirm that `.tools\CEdev\bin\cedev-config.exe` exists. The project build helper uses that path by default. CEdev's official Windows guidance asks for a path without spaces; `tools/build.ps1` maps the repository and the relative CEdev directory to a temporary drive letter from Q through V before invoking `make`.

From PowerShell 7 in the repository root, run:

```powershell
pwsh -NoProfile -File .\tools\build.ps1
```

The output is `bin\PHASEBND.8xp`. If all letters Q through V are occupied, close the temporary mapped drive or change the helper before rebuilding. The application dynamically loads the official CE runtime libraries; transfer the v15 `clibs.8xg` group file to the calculator or emulator if they are not already present. See [installation](INSTALL.md).

## Host tests

The portable engine, level, editor, storage, and campaign tests run with Microsoft's 64-bit C++ Build Tools. From PowerShell 7 at the repository root, run:

```powershell
pwsh -NoProfile -File .\tools\test.ps1
```

The script compiles the C sources and test entry point with MSVC, then runs `build\tests\tests.exe`. These tests do not exercise calculator graphics, calculator key scanning, runtime CE libraries, or hardware timing.

## Emulator and calculator checklist

Use a matching TI-84 Plus CE OS and the launch route described in [the install guide](INSTALL.md). For CEmu, use a ROM image from your own calculator; the project does not include a ROM. Record the model, OS version, launch path, build revision, and result for every run.

| Target | Setup | Checks | Current status |
| --- | --- | --- | --- |
| Windows host | Visual Studio C++ Build Tools; `tools/test.ps1` | Game physics, level validation and wire format, editor undo/capacity, storage recovery, campaign generation | PASS: five suites, including all ten campaign completion replays and six seeded endless scenarios |
| CEmu | Local TI-84 Plus CE ROM, `PHASEBND.8xp`, and CE libraries | Cold launch, menus, play, editor, save/import/export, return from playtest | PASS: 25 state/storage/timing checks |
| TI-84 Plus CE, OS 5.4 or earlier | `Asm(prgmPHASEBND)` or compatible shell | Cold launch, input, save data, and full campaign | Pending hardware run |
| TI-84 Plus CE, OS 5.5–5.8.4 | arTIfiCE v2.1 or an installed compatible Cesium shell | Cold launch, input, save data, and full campaign | Pending hardware run |
| TI-84 Plus CE, OS 5.8.5 | arTIfiCE v3 / AsmHook2 or an installed compatible Cesium shell | Cold launch, input, save data, and full campaign | Pending hardware run |

Run the following checks on a real calculator and record observed values; they are targets, not results:

| Measurement | Target | How to check |
| --- | --- | --- |
| Gameplay rendering | 30 frames per second | Play several sections in each form and check for sustained smooth motion. |
| Action response | No more than 2 simulation ticks | Tap and hold the action key in all five forms, including near pads, orbs, and form portals. |
| Manual restart | No more than 0.5 seconds | Press Del during a run and time from keypress to resumed gameplay. |
| Automatic death restart | No more than 0.5 seconds | Trigger repeated deaths in ordinary and practice runs and check return to the start/checkpoint. |

Also verify that all ten campaign entries unlock and save progress as intended; endless sections continue after a finish; practice checkpoints set and clear; invalid levels are rejected on save/playtest; custom levels survive a calculator restart; and exported `NDXnn` AppVars transfer to another calculator and import into the matching slot. Test an edited level at the object limit and test a level with an invalid spawn before considering editor/storage coverage complete.

## Current verification state

The portable suites and actual CE binary smoke checks pass. A short Core-run benchmark recorded 99 rendered frames over 198 simulation ticks: 30.0 FPS, with a maximum measured draw interval of 28.63 ms. This is an emulator sample, not proof of sustained performance across all levels or on physical hardware. Hardware input latency, restart timing, archive garbage-collection prompts, and transfer between two real calculators remain unverified.

Reproduce the emulator checks with a local ROM:

```powershell
python tools/emulator_test.py --rom C:/path/to/your-calculator.rom
python tools/emulator_test.py --rom C:/path/to/your-calculator.rom --measure
```

The script generates its configuration from the current linker map, reads diagnostic counters, and checks that actual profile/custom saves, export, and import succeed. It writes logs and timing measurements under `build/`. The measurement command intentionally probes unknown counter hashes, decodes the returned values, and checks the resulting timing limits; those diagnostic hash mismatches are not product failures. Archive operations may take several seconds and have longer test timeouts than gameplay transitions.

Run `python tools/package.py` after verification to assemble the release ZIP. The packager includes only an explicit file list, verifies TI container checksums, and never includes ROMs, user saves, compiler binaries, or emulator state.
