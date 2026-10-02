# Install and share levels

## Transfer the program

The target is the TI-84 Plus CE family, including the Python Edition. Build the program with CEdev or use a project release. The executable is `bin/NEONDASH.8xp`.

The release ZIP includes `NEONDASH.8xp` and the four required v15 runtime AppVars: `libload.8xv`, `graphx.8xv`, `keypadc.8xv`, and `fileioc.8xv`. Transfer all five files from its `calculator` folder. Alternatively, obtain the official `clibs.8xg` group from the [CE Libraries v15 release](https://github.com/CE-Programming/libraries/releases/tag/v15.0).

Use [TI Connect CE](https://education.ti.com/en/products/computer-software/ti-connect-ce-sw) or another compatible transfer program to send `clibs.8xg` when needed, then send `NEONDASH.8xp` to the calculator. Keep a local copy of the program file so it can be transferred again later.

## Launch for your OS

Choose the launch route for the OS already installed on the calculator. You do not need to update or downgrade the OS to use Neon Dash.

| Calculator OS | Launch route |
| --- | --- |
| 5.4 or earlier | Run `Asm(prgmNEONDASH)` from the home screen, or select the game in an installed compatible shell. arTIfiCE is not needed. |
| 5.5 through 5.8.4 | Use [arTIfiCE v2](https://yvantt.github.io/arTIfiCE/) (the official instructions provide the compatible v2.1 program) to launch assembly programs, or launch Neon Dash from Cesium if Cesium is already installed. |
| 5.8.5 | Use [arTIfiCE v3](https://yvantt.github.io/arTIfiCE/), which supports 5.8.5 only, and follow its installer steps to enable AsmHook2. Then run `NEONDASH` from PRGM. If you already use a compatible Cesium setup, it can also be used as the launcher. |

On OS 5.5 and later, arTIfiCE restores assembly-program launching. The current official instructions distinguish v2 for OS versions through 5.8.4 and v3 for 5.8.5; they also note that arTIfiCE is unnecessary on OS 5.4 or earlier. Read the linked instructions for the exact first-run steps. [Cesium](https://github.com/mateoconlechuga/cesium) is an optional shell; its installer is run as a calculator program and then creates an app in the APPS menu.

## Save files and level exchange

Profile/settings and each custom level are saved in archived calculator variables. Use Y= in the editor to validate and save the current slot.

To export a level, open its custom slot in the editor and press Zoom. The calculator writes the standalone AppVar `NDXnn`, where `nn` is the two-digit slot number. Use TI Connect CE to copy that AppVar from the calculator to your computer; it is an `.8xv` file and can be sent to another calculator with the same transfer tool. TI's [file-transfer documentation](https://education.ti.com/en/customer-support/knowledge-base/ti-83-84-plus-family/product-usage/29430) lists `.8xv` as the TI-84 Plus CE AppVar format.

To import a shared level, transfer `NDXnn.8xv` to the receiving calculator, open the matching custom slot, and press Trace. Import validates and saves the level into that slot; confirm if the current editor has unsaved changes. Slot 1 corresponds to `NDX00`, through slot 10 / `NDX09`. RAM and archived exchange AppVars are accepted.

Saves and imports can take several seconds. The two alternating journal copies (`NDS00A/B` through `NDS09A/B`, and `NDPROFA/B`) preserve the previous valid save if a write fails. Keep both copies when backing up progress. Exchange files `NDXnn` are separate: a failed export can require exporting again; it does not replace the saved custom level. If the calculator requests archive garbage collection, the game temporarily restores the OS display for that prompt.

The editor also supports changing the level name, checking a level with Graph, and undoing recent edits with Math. See [the editor guide](EDITOR.md) for tool values and placement behavior.

## Emulator use

[CEmu](https://github.com/CE-Programming/CEmu) can load the built `.8xp` for development. CEmu requires a ROM image obtained from the user's own calculator; this project does not distribute a ROM or OS image. Transfer the required CE library AppVars into the emulator as well as the game program.
