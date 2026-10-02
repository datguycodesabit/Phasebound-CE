#include "game.h"

#include <assert.h>
#include <string.h>

static bool is_activated(const Game *game, unsigned index) {
    return (game->activated[index / 8] & (1u << (index % 8))) != 0;
}

static void test_fast_portals(void) {
    Level level;
    Game game;
    int i;
    int32_t before_y;

    level_blank(&level, "Fast portal", 64);
    level.start_mode = MODE_SHIP;
    level.start_y = 100;
    level.objects[level.object_count++] = (LevelObject){48, 0, OBJ_SPEED, 2};
    level.objects[level.object_count++] = (LevelObject){64, 0, OBJ_MODE, MODE_WAVE};
    game_start(&game, &level);

    for (i = 0; i < 40 && game.mode != MODE_WAVE; ++i)
        game_step(&game, &level, (GameInput){true});

    assert(!game.dead);
    assert(game.mode == MODE_WAVE);
    assert(game.speed == 2);
    assert(game.x / FP_ONE < 80);
    assert(game.vy == 0);
    assert(is_activated(&game, 1));
    assert(is_activated(&game, 2));

    before_y = game.y;
    game_step(&game, &level, (GameInput){true});
    assert(!game.dead);
    assert(game.y == before_y - game_speed(2));
}

static void test_held_input_across_ball_portal(void) {
    Level level;
    Game game;
    int i;

    level_blank(&level, "Held portal input", 64);
    level.start_mode = MODE_SHIP;
    level.start_y = 100;
    level.objects[level.object_count++] = (LevelObject){48, 0, OBJ_MODE, MODE_BALL};
    game_start(&game, &level);
    for (i = 0; i < 40 && game.mode != MODE_BALL; ++i)
        game_step(&game, &level, (GameInput){true});
    assert(!game.dead && game.mode == MODE_BALL);
    assert(game.previous_action);
    assert(game.gravity == 0);

    for (i = 0; i < 100 && !game.grounded && !game.dead; ++i)
        game_step(&game, &level, (GameInput){true});
    assert(!game.dead && game.grounded);
    assert(game.gravity == 0); /* A held button does not create a new press. */

    game_step(&game, &level, (GameInput){false});
    game_step(&game, &level, (GameInput){true});
    assert(!game.dead && game.gravity == 1);
}

static void test_inverted_gravity_modes(void) {
    Level level;
    Game game;
    int i;

    level_blank(&level, "Inverted cube", 64);
    level.start_mode = MODE_CUBE;
    level.start_gravity = 1;
    level.start_y = 16;
    game_start(&game, &level);
    assert(game.grounded);
    game_step(&game, &level, (GameInput){true});
    assert(!game.dead && game.gravity == 1 && game.y > 16 * FP_ONE);

    level_blank(&level, "Inverted ball", 64);
    level.start_mode = MODE_BALL;
    level.start_gravity = 1;
    level.start_y = 16;
    game_start(&game, &level);
    game_step(&game, &level, (GameInput){true});
    assert(!game.dead && game.gravity == 0);
    for (i = 0; i < 100; ++i)
        game_step(&game, &level, (GameInput){true});
    assert(!game.dead && game.grounded && game.y == 212 * FP_ONE);
    assert(game.gravity == 0);

    level_blank(&level, "Inverted spider", 64);
    level.start_mode = MODE_SPIDER;
    level.start_gravity = 1;
    level.start_y = 16;
    game_start(&game, &level);
    assert(game.grounded);
    game_step(&game, &level, (GameInput){true});
    assert(!game.dead && game.grounded && game.gravity == 0);
    assert(game.y == 212 * FP_ONE);
}

static void test_spider_path_hazards(void) {
    Level level;
    Game game;
    int i;

    level_blank(&level, "Clear spider path", 64);
    level.start_mode = MODE_SPIDER;
    level.tiles[4 * LEVEL_ROWS + 13] = TILE_SPIKE_UP;
    game_start(&game, &level);
    game_step(&game, &level, (GameInput){true});
    assert(!game.dead && game.gravity == 1 && game.y == 16 * FP_ONE);
    for (i = 0; i < 20; ++i)
        game_step(&game, &level, (GameInput){false});
    assert(!game.dead); /* A floor spike is safe after teleporting to the ceiling. */

    level_blank(&level, "Spider path spike", 64);
    level.start_mode = MODE_SPIDER;
    level.tiles[2 * LEVEL_ROWS + 7] = TILE_SPIKE_UP;
    game_start(&game, &level);
    game_step(&game, &level, (GameInput){true});
    assert(game.dead); /* Teleporting through a spike column remains lethal. */

    level_blank(&level, "Spider path spike down", 64);
    level.start_mode = MODE_SPIDER;
    level.start_gravity = 1;
    level.start_y = 16;
    level.tiles[2 * LEVEL_ROWS + 7] = TILE_SPIKE_DOWN;
    game_start(&game, &level);
    game_step(&game, &level, (GameInput){true});
    assert(game.dead);

    level_blank(&level, "Ceiling landing spike", 64);
    level.start_mode = MODE_SPIDER;
    level.tiles[2 * LEVEL_ROWS + 1] = TILE_SPIKE_DOWN;
    game_start(&game, &level);
    game_step(&game, &level, (GameInput){true});
    assert(game.dead);
}

static void test_level_bounds(void) {
    Level level;
    Game game;
    unsigned mode;

    for (mode = 0; mode < MODE_COUNT; ++mode) {
        level_blank(&level, "Right edge", 32);
        level.start_mode = (uint8_t)mode;
        level.start_x = 500;
        level.start_y = 100;
        level.start_speed = 2;
        level.object_count = 0;
        game_start(&game, &level);
        game_step(&game, &level, (GameInput){false});
        assert(game.dead);
        assert(game.x == 500 * FP_ONE);
    }

    level_blank(&level, "Top edge", 64);
    level.start_mode = MODE_WAVE;
    level.start_y = 16;
    game_start(&game, &level);
    game_step(&game, &level, (GameInput){true});
    assert(game.dead);

    level_blank(&level, "Bottom edge", 64);
    level.start_mode = MODE_WAVE;
    level.start_y = 212;
    game_start(&game, &level);
    game_step(&game, &level, (GameInput){false});
    assert(game.dead);
}

static void test_fractional_right_bound(void) {
    Level level;
    Game game;
    int32_t maximum_x;

    level_blank(&level, "Fractional edge", 32);
    level.start_mode = MODE_CUBE;
    level.start_x = 499;
    level.start_y = 100;
    level.start_speed = 1; /* 2.5 px/tick crosses the edge between pixels. */
    level.object_count = 0;
    game_start(&game, &level);
    game_step(&game, &level, (GameInput){false});
    maximum_x = (int32_t)level.width * TILE_SIZE - PLAYER_SIZE;
    assert(game.dead);
    assert(game.x <= maximum_x * FP_ONE);
}

static void test_same_tick_replay_all_modes(void) {
    Level level;
    Game first, replay;
    unsigned mode, tick;
    for (mode = 0; mode < MODE_COUNT; ++mode) {
        level_blank(&level, "Tick replay", 64);
        level.start_mode = (uint8_t)mode;
        level.start_speed = 1;
        if (mode == MODE_SPIDER || mode == MODE_BALL) {
            level.start_gravity = 1;
            level.start_y = 16;
        } else {
            level.start_y = 100;
        }
        game_start(&first, &level);
        replay = first;
        for (tick = 0; tick < 120; ++tick) {
            GameInput input;
            input.action = ((tick * 7u + tick / 3u) % 11u) < 5u;
            game_step(&first, &level, input);
            game_step(&replay, &level, input);
            assert(memcmp(&first, &replay, sizeof(first)) == 0);
        }
    }
}

void test_regression(void) {
    test_fast_portals();
    test_held_input_across_ball_portal();
    test_inverted_gravity_modes();
    test_spider_path_hazards();
    test_level_bounds();
    test_same_tick_replay_all_modes();
    test_fractional_right_bound();
}
