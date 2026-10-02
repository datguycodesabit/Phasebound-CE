# Neon Dash

Neon Dash is a native C game for the TI-84 Plus CE family, built as `NEONDASH.8xp`. It is a compact side-scrolling platform challenge with five movement modes: Cube, Ship, Spider, Ball, and Wave.

The project includes a ten-route campaign, seeded endless sections, practice checkpoints, profile and settings saves, and ten custom level slots. The in-calculator editor supports tile and object placement, a 32-edit undo history, playtesting, and level exchange through calculator AppVars.

## Install and run

Start with [installation and sharing instructions](docs/INSTALL.md). The build output is `bin/NEONDASH.8xp`; a calculator also needs the CE runtime libraries used by the program. On OS 5.5 and later, use the compatible arTIfiCE or installed Cesium launch route for the calculator's current OS. No OS update or downgrade is needed.

## Controls

In menus, use Up/Down to move, Enter to select, and Clear to go back.

During a run, Up or 2nd is the action input. Clear pauses; Del restarts from the beginning or the current practice checkpoint. In practice, Alpha sets a checkpoint and Mode clears it. Cube jumps when grounded, Ship rises while the action is held, Wave moves up while held and down when released, and Spider/Ball reverse gravity on a grounded press. Pads bounce automatically; an Orb needs a press.

In the editor, arrows move, 2nd/Enter places the selected tool, Del erases, Mode selects the next tool, Alpha cycles its value, Math undoes, and Graph starts a playtest. Y= saves, Window edits the name, Zoom exports the current slot, Trace imports its matching exchange file, and Clear returns after an unsaved-change prompt. See [editor details](docs/EDITOR.md).

Zoom exports a level as the archived AppVar `NDX00` through `NDX09`. Transfer that `.8xv` file to another calculator, then use Trace while editing the matching slot to import it.

## Build and verify

On Windows, install CEdev v15 and follow [the build and test guide](docs/TESTING.md). The helper script maps the project to a temporary drive letter so CEdev can build even when the checkout path contains spaces.

The host suites pass, including deterministic completion replays of all ten levels, editor limits/undo, malformed level rejection, and save-failure recovery. The actual calculator binary passes 25 CEmu UI/storage checks. A short cube-run benchmark measured approximately 30 FPS; sustained performance across every mode on physical hardware remains unverified. No calculator ROM is included.

The ready-to-transfer bundle is `dist/NeonDash-CE.zip`. It contains the game, four required runtime AppVars, instructions, licenses, and source. Saves/imports may take several seconds; a working screen is shown while they complete.

## Project notes

See [credits and third-party notices](docs/CREDITS.md) and the [MIT license](LICENSE).
