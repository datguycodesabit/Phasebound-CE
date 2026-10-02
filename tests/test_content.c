#include "content.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define REPLAY_CAPACITY 4400

static int next_hazard_distance(const Level *level, const Game *game,
                                int allow_blocks) {
    int player_x = (int)(game->x / FP_ONE);
    int right = player_x + PLAYER_SIZE;
    int row = game->gravity ? 1 : 13;
    int first_column = right / TILE_SIZE;
    int column;
    for (column = first_column; column < level->width; ++column) {
        uint8_t tile = level_tile(level, column, row);
        if (tile == TILE_SPIKE_UP || tile == TILE_SPIKE_DOWN
            || (allow_blocks && tile == TILE_BLOCK)) {
            int distance = column * TILE_SIZE - right;
            if (distance >= 0) return distance;
        }
    }
    return -1;
}

static GameInput replay_policy(const Level *level, const Game *game) {
    GameInput input;
    int y = (int)(game->y / FP_ONE);
    int distance;
    int want_up;
    input.action = false;

    switch (game->mode) {
    case MODE_CUBE:
        distance = next_hazard_distance(level, game, 1);
        if (game->grounded && distance >= 0 && distance <= 40)
            input.action = true;
        break;
    case MODE_SPIDER:
    case MODE_BALL:
        distance = next_hazard_distance(level, game, 0);
        if (game->grounded && distance >= 0 && distance <= 56)
            input.action = true;
        break;
    case MODE_SHIP:
        {
            int32_t velocity = game->vy;
            int32_t speed = velocity < 0 ? -velocity : velocity;
            int stopping_distance = (int)(speed * speed / (40 * FP_ONE));
            int error = y - 116;
            if (velocity < 0) want_up = error > stopping_distance;
            else if (velocity > 0) want_up = error + stopping_distance > 0;
            else want_up = error > 0;
            input.action = (bool)(want_up ^ (game->gravity != 0));
        }
        break;
    case MODE_WAVE:
        want_up = y > 116;
        input.action = (bool)(want_up ^ (game->gravity != 0));
        break;
    default:
        break;
    }
    return input;
}

static void assert_same_run(const Game *a, const Game *b) {
    assert(a->x == b->x);
    assert(a->y == b->y);
    assert(a->vy == b->vy);
    assert(a->ticks == b->ticks);
    assert(a->mode == b->mode);
    assert(a->gravity == b->gravity);
    assert(a->speed == b->speed);
    assert(a->grounded == b->grounded);
    assert(a->dead == b->dead);
    assert(a->won == b->won);
    assert(a->previous_action == b->previous_action);
    assert(memcmp(a->activated, b->activated, sizeof(a->activated)) == 0);
}

static void validate_level(const Level *level) {
    char error[96];
    if (!level_validate(level, error, sizeof(error))) {
        fprintf(stderr, "Invalid level '%s': %s\n", level->name, error);
        assert(0);
    }
}

static Game run_and_replay(const Level *level, unsigned tick_limit) {
    Game first, second;
    GameInput inputs[REPLAY_CAPACITY];
    unsigned count = 0;
    unsigned i;

    game_start(&first, level);
    while (!first.won && !first.dead && count < tick_limit
           && count < REPLAY_CAPACITY) {
        inputs[count] = replay_policy(level, &first);
        game_step(&first, level, inputs[count]);
        ++count;
    }
    if (first.dead || !first.won)
        fprintf(stderr, "Replay failed: %s at tick %lu x=%ld y=%ld mode=%u gravity=%u\n",
                level->name, (unsigned long)first.ticks,
                (long)(first.x / FP_ONE), (long)(first.y / FP_ONE),
                (unsigned)first.mode, (unsigned)first.gravity);
    assert(!first.dead);
    assert(first.won);
    assert(first.ticks == count);

    game_start(&second, level);
    for (i = 0; i < count; ++i) game_step(&second, level, inputs[i]);
    assert(second.won);
    assert(!second.dead);
    assert_same_run(&first, &second);
    return first;
}

static unsigned count_objects(const Level *level, ObjectType type) {
    unsigned count = 0;
    unsigned i;
    for (i = 0; i < level->object_count; ++i)
        if (level->objects[i].type == type) ++count;
    return count;
}

static unsigned count_endless_hazards(const Level *level) {
    unsigned count = 0;
    unsigned column;
    for (column = 0; column < level->width; ++column) {
        uint8_t tile = level_tile(level, (int)column, 13);
        if (tile == TILE_SPIKE_UP || tile == TILE_BLOCK) ++count;
    }
    return count;
}

static uint32_t content_hash(const Level *level) {
    uint32_t hash = 2166136261u;
    unsigned i;
    for (i = 0; i < (unsigned)level->width * LEVEL_ROWS; ++i) {
        hash ^= level->tiles[i];
        hash *= 16777619u;
    }
    for (i = 0; i < level->object_count; ++i) {
        hash ^= level->objects[i].x & 0xffu;
        hash *= 16777619u;
        hash ^= level->objects[i].x >> 8;
        hash *= 16777619u;
        hash ^= level->objects[i].type;
        hash *= 16777619u;
    }
    return hash;
}

void test_content(void) {
    static const uint8_t intro_modes[5] = {
        MODE_CUBE, MODE_SHIP, MODE_SPIDER, MODE_BALL, MODE_WAVE
    };
    static const uint32_t seeds[] = {
        1u, 2u, 0x12345678u, 0x9e3779b9u, 0xf00dcafeu, 0u
    };
    static const unsigned difficulties[] = { 0, 0, 1, 2, 5, 99 };
    Level level;
    unsigned i;

    for (i = 0; i < CAMPAIGN_COUNT; ++i) {
        Game result;
        campaign_load(i, &level);
        validate_level(&level);
        assert(level.start_x == 32);
        assert(level.start_y == ((level.start_mode == MODE_SHIP || level.start_mode == MODE_WAVE) ? 112 : 212));
        assert(level.start_speed == 0);
        assert(level.width >= 344 && level.width <= LEVEL_COLS);
        assert(count_objects(&level, OBJ_FINISH) == 1);
        if (i < 5) assert(level.start_mode == intro_modes[i]);
        else assert(count_objects(&level, OBJ_MODE) >= 2);

        result = run_and_replay(&level, REPLAY_CAPACITY);
        assert((double)result.ticks / 60.0 >= 45.0);
        assert((double)result.ticks / 60.0 <= 90.0);
    }

    for (i = 0; i < sizeof(seeds) / sizeof(seeds[0]); ++i) {
        Game result;
        endless_load(seeds[i], difficulties[i], &level);
        validate_level(&level);
        assert(level.width == 96);
        assert(level.start_mode == MODE_CUBE);
        assert(level.start_x == 32 && level.start_y == 212);
        assert(count_objects(&level, OBJ_FINISH) == 1);
        assert(count_endless_hazards(&level) >= 4);
        assert(count_endless_hazards(&level) <= 18);
        result = run_and_replay(&level, 1000);
        assert((double)result.ticks / 60.0 > 10.0);
        assert((double)result.ticks / 60.0 < 20.0);
    }

    {
        Level first, repeat, other_seed;
        endless_load(0x12345678u, 0, &first);
        endless_load(0x12345678u, 0, &repeat);
        endless_load(0x87654321u, 0, &other_seed);
        assert(content_hash(&first) == content_hash(&repeat));
        assert(content_hash(&first) != content_hash(&other_seed));
    }
}
