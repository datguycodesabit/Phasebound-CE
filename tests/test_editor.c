#include <assert.h>
#include "../src/editor.h"

static void make_level(Level *level, uint16_t width) {
    level_blank(level, "editor-test", width);
}

static void test_cursor_bounds(void) {
    Level level;
    Editor editor;
    make_level(&level, 32);
    editor_init(&editor);

    editor_move(&editor, -10, -10, &level);
    assert(editor.x == 0 && editor.y == 0 && editor.camera_col == 0);

    editor_move(&editor, 5000, 5000, &level);
    assert(editor.x == 31 && editor.y == LEVEL_ROWS - 1);
    assert(editor.camera_col == 12);
    assert(editor.x >= editor.camera_col);
    assert(editor.x < editor.camera_col + 20);

    editor_move(&editor, -26, -20, &level);
    assert(editor.x == 5 && editor.y == 0);
    assert(editor.camera_col == 5);

    make_level(&level, 32);
    editor_init(&editor);
    editor_move(&editor, 100, 100, &level);
    assert(editor.x == 31 && editor.y == LEVEL_ROWS - 1);
    editor_move(&editor, -100, -100, &level);
    assert(editor.x == 0 && editor.y == 0 && editor.camera_col == 0);
}

static void test_tile_edits_and_undo(void) {
    Level level;
    Editor editor;
    unsigned tile_index = 3 * LEVEL_ROWS + 4;
    make_level(&level, 32);
    editor_init(&editor);
    editor_move(&editor, 3, 4, &level);

    editor.tool = TILE_BLOCK;
    assert(editor_place(&editor, &level));
    assert(level.tiles[tile_index] == TILE_BLOCK);
    assert(editor.dirty && editor.undo_count == 1);
    assert(editor_place(&editor, &level));
    assert(editor.undo_count == 1);

    editor.tool = 0;
    assert(editor_place(&editor, &level));
    assert(level.tiles[tile_index] == TILE_EMPTY);
    assert(editor_undo(&editor, &level));
    assert(level.tiles[tile_index] == TILE_BLOCK);
    assert(editor_undo(&editor, &level));
    assert(level.tiles[tile_index] == TILE_EMPTY);
    assert(editor_place(&editor, &level)); /* Erasing an empty cell is a harmless no-op. */
    assert(editor.undo_count == 0);
    assert(!editor_undo(&editor, &level));
}

static void test_object_replace_erase_and_undo(void) {
    Level level;
    Editor editor;
    make_level(&level, 32);
    editor_init(&editor);
    level.object_count = 0;
    level.tiles[0] = TILE_BLOCK;

    editor.tool = 4;
    assert(editor_place(&editor, &level));
    assert(level.object_count == 1);
    assert(level.objects[0].x == 0 && level.objects[0].y == 0);
    assert(level.objects[0].type == OBJ_PAD);

    editor.tool = 5;
    assert(editor_place(&editor, &level));
    assert(level.object_count == 1); /* The same position is replaced. */
    assert(level.objects[0].type == OBJ_ORB);
    assert(editor_place(&editor, &level)); /* Repeating the same object is accepted without history. */
    assert(editor.undo_count == 2);
    assert(editor_undo(&editor, &level));
    assert(level.object_count == 1 && level.objects[0].type == OBJ_PAD);

    editor.tool = 0;
    assert(editor_place(&editor, &level));
    assert(level.object_count == 0);
    assert(level.tiles[0] == TILE_BLOCK); /* Erase removes the object first. */
    assert(editor_undo(&editor, &level));
    assert(level.object_count == 1 && level.objects[0].type == OBJ_PAD);

    assert(editor_place(&editor, &level));
    assert(level.object_count == 0);
    assert(editor_place(&editor, &level));
    assert(level.tiles[0] == TILE_EMPTY); /* Next erase removes the tile. */
    assert(editor_undo(&editor, &level));
    assert(level.tiles[0] == TILE_BLOCK);
    assert(editor_undo(&editor, &level));
    assert(level.object_count == 1);
    assert(editor_undo(&editor, &level));
    assert(level.object_count == 0);
}

static void test_object_capacity(void) {
    Level level;
    Editor editor;
    unsigned i;
    make_level(&level, LEVEL_COLS);
    editor_init(&editor);
    editor.tool = 4;
    editor_move(&editor, 500, 0, &level);

    for (i = 0; i < LEVEL_OBJECTS; ++i) {
        level.objects[i].x = (uint16_t)(i + 1);
        level.objects[i].y = 1000;
        level.objects[i].type = OBJ_PAD;
        level.objects[i].value = 0;
    }
    level.object_count = LEVEL_OBJECTS - 1;
    assert(editor_place(&editor, &level));
    assert(level.object_count == LEVEL_OBJECTS);
    assert(level.objects[LEVEL_OBJECTS - 1].x == 500 * TILE_SIZE);

    editor_move(&editor, 1, 0, &level);
    assert(!editor_place(&editor, &level));
    assert(level.object_count == LEVEL_OBJECTS);

    /* Corrupt counts are rejected before any fixed-array access. */
    level.object_count = LEVEL_OBJECTS + 1;
    assert(!editor_place(&editor, &level));
}

static void test_spawn_edit_and_undo(void) {
    Level level;
    Editor editor;
    make_level(&level, 64);
    editor_init(&editor);
    editor_move(&editor, 12, 5, &level);
    editor.tool = 10;
    editor.value = MODE_WAVE;
    level.start_x = 24;
    level.start_y = 40;
    level.start_mode = MODE_BALL;
    level.start_gravity = 1;
    level.start_speed = 2;

    assert(editor_place(&editor, &level));
    assert(level.start_x == 12 * TILE_SIZE);
    assert(level.start_y == 5 * TILE_SIZE);
    assert(level.start_mode == MODE_WAVE);
    assert(level.start_gravity == 1 && level.start_speed == 2);
    assert(editor_place(&editor, &level));
    assert(editor.undo_count == 1);
    assert(editor_undo(&editor, &level));
    assert(level.start_x == 24 && level.start_y == 40);
    assert(level.start_mode == MODE_BALL);
    assert(level.start_gravity == 1 && level.start_speed == 2);

    editor_cycle_value(&editor);
    assert(editor.value == 0); /* Wave wraps to Cube. */
    editor.value = MODE_COUNT;
    assert(editor_place(&editor, &level)); /* Invalid mode normalizes to Cube. */
    assert(level.start_mode == MODE_CUBE);
}

static void test_value_cycles_and_tool_names(void) {
    Editor editor;
    editor_init(&editor);
    assert(editor_tool_name(&editor)[0] == 'E');

    editor_cycle_tool(&editor, 6);
    assert(editor.tool == 6 && editor_tool_name(&editor)[0] == 'G');
    editor_cycle_value(&editor);
    assert(editor.value == 1);
    editor_cycle_value(&editor);
    assert(editor.value == 0);

    editor_cycle_tool(&editor, 1);
    assert(editor.tool == 7 && editor.value == 0);
    editor_cycle_value(&editor);
    editor_cycle_value(&editor);
    assert(editor.value == 2);
    editor_cycle_value(&editor);
    assert(editor.value == 0);

    editor_cycle_tool(&editor, 1);
    assert(editor.tool == 8);
    for (unsigned i = 0; i < MODE_COUNT; ++i) editor_cycle_value(&editor);
    assert(editor.value == 0);

    editor_cycle_tool(&editor, 2);
    assert(editor.tool == 10);
    editor_cycle_value(&editor);
    assert(editor.value == 1);

    editor_cycle_tool(&editor, 1);
    assert(editor.tool == 0 && editor.value == 0);
    editor_cycle_value(&editor);
    assert(editor.value == 0);
    editor_cycle_tool(&editor, -1);
    assert(editor.tool == 10);
}

static void test_undo_keeps_latest_32_edits(void) {
    Level level;
    Editor editor;
    unsigned x;
    make_level(&level, LEVEL_COLS);
    editor_init(&editor);
    editor.tool = TILE_BLOCK;
    editor_move(&editor, 0, 5, &level);
    for (x = 0; x < EDITOR_UNDO + 1; ++x) {
        if (x != 0) editor_move(&editor, 1, 0, &level);
        assert(editor_place(&editor, &level));
    }
    assert(editor.undo_count == EDITOR_UNDO);
    for (x = 0; x < EDITOR_UNDO; ++x) assert(editor_undo(&editor, &level));
    assert(level_tile(&level, 0, 5) == TILE_BLOCK);
    for (x = 1; x <= EDITOR_UNDO; ++x) assert(level_tile(&level, (int)x, 5) == TILE_EMPTY);
    assert(!editor_undo(&editor, &level));
}

void test_editor(void) {
    test_cursor_bounds();
    test_tile_edits_and_undo();
    test_object_replace_erase_and_undo();
    test_object_capacity();
    test_spawn_edit_and_undo();
    test_value_cycles_and_tool_names();
    test_undo_keeps_latest_32_edits();
}
