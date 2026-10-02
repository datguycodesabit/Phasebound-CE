# Level editor model

The portable editor model keeps the cursor on the level grid, tracks a 20-column viewport, edits tiles and objects, and stores the 32 latest successful edits for undo. It uses fixed-size level and undo arrays, so editing does not allocate memory. The model owns edit state; the calculator UI draws it and maps key presses to these operations.

## Editing behavior

The cursor is bounded by the level width and the 15 available rows. The level format supports at most 512 columns. `camera_col` is the leftmost visible column; moving the cursor keeps it inside the 20-column view.

Tiles use the column-major `Level.tiles` array. Object placement converts the cursor cell to pixel coordinates by multiplying each grid coordinate by `TILE_SIZE` (16). Placing an object on a position that already has one replaces it, so editor actions do not create duplicate objects at that position. If all 256 object slots are occupied, placing a new object fails without changing the level.

Tool 0 erases an object at the cursor first. If there is no object there, it erases the tile. Tools 1–3 paint the matching tile ID. Tools 4–9 place the corresponding object type. Tool 10 sets the spawn cell and starting player mode; it preserves the spawn's gravity and speed settings.

Undo records only successful changes. `editor_place` returns success for a valid no-op, so repeating an edit that already matches the level does not consume an undo slot or trigger an error in the UI. It returns failure for an invalid level/cursor or a full object table. After 32 edits, the oldest undo entry is discarded as the newest edit is recorded. Undo restores the prior tile, object (including a replaced or deleted object), or all spawn settings.

The value selector cycles by tool: gravity objects use 0–1, speed objects use 0–2, mode objects and the spawn tool use 0–4. Other tools keep value 0. Values map to the level enums: gravity 0=down/1=up, speed 0–2, and modes Cube, Ship, Spider, Ball, Wave in enum order.

## Calculator controls

The calculator key mapping is:

| Key | Action |
| --- | --- |
| Arrow keys | Move the cursor |
| 2nd / Enter | Place the selected tool |
| Del | Erase at the cursor, keeping the selected tool |
| Mode | Select the next tool |
| Alpha | Cycle the selected tool's value |
| Math | Undo |
| Graph | Playtest |
| Y= | Save |
| Window | Edit the level name |
| Zoom | Export |
| Trace | Import |
| Clear | Return, prompting before discarding unsaved edits |

The model API is in `src/editor.h`. `editor_move`, `editor_place`, `editor_undo`, `editor_cycle_tool`, and `editor_cycle_value` provide the operations for the UI; `editor_tool_name` supplies the selected tool label.
