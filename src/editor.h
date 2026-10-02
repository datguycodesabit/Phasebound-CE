#ifndef NEON_EDITOR_H
#define NEON_EDITOR_H
#include "level.h"
#define EDITOR_UNDO 32
typedef struct {
    uint16_t x, y;
    uint8_t old_tile;
    bool object_edit, had_object;
    bool spawn_edit;
    uint16_t start_x, start_y;
    uint8_t start_mode, start_gravity, start_speed;
    LevelObject old_object;
} EditChange;
typedef struct {
    uint16_t x, camera_col;
    uint8_t y, tool, value;
    bool dirty;
    unsigned undo_count;
    EditChange undo[EDITOR_UNDO];
} Editor;
/* Tools 0..3 tile IDs; 4..9 = OBJ_PAD..OBJ_FINISH; 10 sets spawn. */
void editor_init(Editor *editor);
void editor_move(Editor *editor, int dx, int dy, const Level *level);
bool editor_place(Editor *editor, Level *level);
bool editor_undo(Editor *editor, Level *level);
void editor_cycle_tool(Editor *editor, int delta);
void editor_cycle_value(Editor *editor);
const char *editor_tool_name(const Editor *editor);
#endif
