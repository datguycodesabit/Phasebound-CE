#include "editor.h"

enum { EDITOR_TOOL_COUNT = 11, EDITOR_VIEW_COLS = 20 };

static uint16_t editor_level_width(const Level *level) {
    if (level == NULL || level->width == 0) return LEVEL_COLS;
    return level->width > LEVEL_COLS ? LEVEL_COLS : level->width;
}

static void editor_keep_cursor_visible(Editor *editor, uint16_t width) {
    uint16_t max_camera = 0;
    if (width > EDITOR_VIEW_COLS) max_camera = (uint16_t)(width - EDITOR_VIEW_COLS);
    if (editor->camera_col > max_camera) editor->camera_col = max_camera;
    if (editor->x < editor->camera_col) {
        editor->camera_col = editor->x;
    } else if ((unsigned)editor->x >= (unsigned)editor->camera_col + EDITOR_VIEW_COLS) {
        editor->camera_col = (uint16_t)(editor->x - (EDITOR_VIEW_COLS - 1));
        if (editor->camera_col > max_camera) editor->camera_col = max_camera;
    }
}

static int editor_find_object(const Level *level, uint16_t x, uint16_t y) {
    unsigned i;
    if (level == NULL || level->object_count > LEVEL_OBJECTS) return -1;
    for (i = 0; i < level->object_count; ++i) {
        if (level->objects[i].x == x && level->objects[i].y == y) return (int)i;
    }
    return -1;
}

static void editor_remove_object(Level *level, unsigned index) {
    unsigned i;
    for (i = index; i + 1 < level->object_count; ++i) {
        level->objects[i] = level->objects[i + 1];
    }
    --level->object_count;
    level->objects[level->object_count].x = 0;
    level->objects[level->object_count].y = 0;
    level->objects[level->object_count].type = 0;
    level->objects[level->object_count].value = 0;
}

static void editor_push_change(Editor *editor, const EditChange *change) {
    unsigned i;
    if (editor->undo_count == EDITOR_UNDO) {
        for (i = 1; i < EDITOR_UNDO; ++i) editor->undo[i - 1] = editor->undo[i];
        editor->undo_count = EDITOR_UNDO - 1;
    }
    editor->undo[editor->undo_count++] = *change;
}

static uint8_t editor_value_limit(uint8_t tool) {
    if (tool == 6) return 1; /* Gravity: down or up. */
    if (tool == 7) return 2; /* Speed: three settings. */
    if (tool == 8 || tool == 10) return 4; /* Player mode / spawn mode. */
    return 0;
}

void editor_init(Editor *editor) {
    if (editor == NULL) return;
    editor->x = 0;
    editor->camera_col = 0;
    editor->y = 0;
    editor->tool = 0;
    editor->value = 0;
    editor->dirty = false;
    editor->undo_count = 0;
}

void editor_move(Editor *editor, int dx, int dy, const Level *level) {
    uint16_t width;
    unsigned max_x;
    if (editor == NULL) return;

    width = editor_level_width(level);
    max_x = (unsigned)width - 1;
    if (editor->x > max_x) editor->x = (uint16_t)max_x;
    if (dx < 0) {
        if (dx <= -(int)editor->x) editor->x = 0;
        else editor->x = (uint16_t)((int)editor->x + dx);
    } else if ((unsigned)dx > max_x - editor->x) {
        editor->x = (uint16_t)max_x;
    } else {
        editor->x = (uint16_t)(editor->x + (unsigned)dx);
    }

    if (editor->y >= LEVEL_ROWS) editor->y = LEVEL_ROWS - 1;
    if (dy < 0) {
        if (dy <= -(int)editor->y) editor->y = 0;
        else editor->y = (uint8_t)((int)editor->y + dy);
    } else if ((unsigned)dy > (unsigned)(LEVEL_ROWS - 1) - (unsigned)editor->y) {
        editor->y = LEVEL_ROWS - 1;
    } else {
        editor->y = (uint8_t)(editor->y + (unsigned)dy);
    }

    editor_keep_cursor_visible(editor, width);
}

bool editor_place(Editor *editor, Level *level) {
    EditChange change;
    uint16_t width;
    uint16_t pixel_x, pixel_y;
    uint8_t tile;
    int object_index;

    if (editor == NULL || level == NULL || level->object_count > LEVEL_OBJECTS) return false;
    width = editor_level_width(level);
    if (editor->x >= width || editor->x >= LEVEL_COLS || editor->y >= LEVEL_ROWS) return false;
    pixel_x = (uint16_t)(editor->x * TILE_SIZE);
    pixel_y = (uint16_t)(editor->y * TILE_SIZE);
    tile = level->tiles[(unsigned)editor->x * LEVEL_ROWS + editor->y];

    change.x = editor->x;
    change.y = editor->y;
    change.old_tile = tile;
    change.object_edit = false;
    change.had_object = false;
    change.spawn_edit = false;
    change.start_x = level->start_x;
    change.start_y = level->start_y;
    change.start_mode = level->start_mode;
    change.start_gravity = level->start_gravity;
    change.start_speed = level->start_speed;
    change.old_object.x = 0;
    change.old_object.y = 0;
    change.old_object.type = 0;
    change.old_object.value = 0;

    if (editor->tool == 0) {
        object_index = editor_find_object(level, pixel_x, pixel_y);
        if (object_index >= 0) {
            change.object_edit = true;
            change.had_object = true;
            change.old_object = level->objects[object_index];
            editor_push_change(editor, &change);
            editor_remove_object(level, (unsigned)object_index);
            editor->dirty = true;
            return true;
        }
        if (tile == TILE_EMPTY) return true;
        editor_push_change(editor, &change);
        level->tiles[(unsigned)editor->x * LEVEL_ROWS + editor->y] = TILE_EMPTY;
        editor->dirty = true;
        return true;
    }

    if (editor->tool >= 1 && editor->tool <= 3) {
        if (tile == editor->tool) return true;
        editor_push_change(editor, &change);
        level->tiles[(unsigned)editor->x * LEVEL_ROWS + editor->y] = editor->tool;
        editor->dirty = true;
        return true;
    }

    if (editor->tool >= 4 && editor->tool <= 9) {
        uint8_t type = (uint8_t)(editor->tool - 4);
        uint8_t value = editor->value;
        uint8_t limit = editor_value_limit(editor->tool);
        LevelObject next_object;

        if (value > limit) value = 0;
        if (type == OBJ_PAD || type == OBJ_ORB || type == OBJ_FINISH) value = 0;
        next_object.x = pixel_x;
        next_object.y = pixel_y;
        next_object.type = type;
        next_object.value = value;

        object_index = editor_find_object(level, pixel_x, pixel_y);
        change.object_edit = true;
        if (object_index >= 0) {
            LevelObject *old_object = &level->objects[object_index];
            if (old_object->type == next_object.type && old_object->value == next_object.value) return true;
            change.had_object = true;
            change.old_object = *old_object;
            editor_push_change(editor, &change);
            *old_object = next_object;
        } else {
            if (level->object_count >= LEVEL_OBJECTS) return false;
            editor_push_change(editor, &change);
            level->objects[level->object_count++] = next_object;
        }
        editor->dirty = true;
        return true;
    }

    if (editor->tool == 10) {
        uint8_t mode = editor->value;
        if (mode > MODE_WAVE) mode = MODE_CUBE;
        if (level->start_x == pixel_x && level->start_y == pixel_y && level->start_mode == mode) return true;
        change.spawn_edit = true;
        editor_push_change(editor, &change);
        level->start_x = pixel_x;
        level->start_y = pixel_y;
        level->start_mode = mode;
        editor->dirty = true;
        return true;
    }

    return false;
}

bool editor_undo(Editor *editor, Level *level) {
    EditChange *change;
    int object_index;
    if (editor == NULL || level == NULL || editor->undo_count == 0) return false;
    change = &editor->undo[editor->undo_count - 1];

    if (change->spawn_edit) {
        level->start_x = change->start_x;
        level->start_y = change->start_y;
        level->start_mode = change->start_mode;
        level->start_gravity = change->start_gravity;
        level->start_speed = change->start_speed;
    } else if (change->object_edit) {
        if (level->object_count > LEVEL_OBJECTS) return false;
        object_index = editor_find_object(level,
                                          (uint16_t)(change->x * TILE_SIZE),
                                          (uint16_t)(change->y * TILE_SIZE));
        if (change->had_object) {
            if (object_index >= 0) {
                level->objects[object_index] = change->old_object;
            } else {
                if (level->object_count >= LEVEL_OBJECTS) return false;
                level->objects[level->object_count++] = change->old_object;
            }
        } else {
            if (object_index < 0) return false;
            editor_remove_object(level, (unsigned)object_index);
        }
    } else {
        if (change->x >= LEVEL_COLS || change->y >= LEVEL_ROWS) return false;
        level->tiles[(unsigned)change->x * LEVEL_ROWS + change->y] = change->old_tile;
    }

    --editor->undo_count;
    editor->dirty = true;
    return true;
}

void editor_cycle_tool(Editor *editor, int delta) {
    int step, tool;
    if (editor == NULL) return;
    tool = editor->tool % EDITOR_TOOL_COUNT;
    step = delta % EDITOR_TOOL_COUNT;
    tool = (tool + step) % EDITOR_TOOL_COUNT;
    if (tool < 0) tool += EDITOR_TOOL_COUNT;
    editor->tool = (uint8_t)tool;
    editor->value = 0;
}

void editor_cycle_value(Editor *editor) {
    uint8_t limit;
    if (editor == NULL) return;
    limit = editor_value_limit(editor->tool);
    if (editor->value > limit) editor->value = 0;
    else editor->value = editor->value == limit ? 0 : (uint8_t)(editor->value + 1);
}

const char *editor_tool_name(const Editor *editor) {
    static const char *const names[EDITOR_TOOL_COUNT] = {
        "Erase", "Block", "Spike Up", "Spike Down", "Pad", "Orb",
        "Gravity", "Speed", "Mode", "Finish", "Spawn"
    };
    if (editor == NULL || editor->tool >= EDITOR_TOOL_COUNT) return "?";
    return names[editor->tool];
}
