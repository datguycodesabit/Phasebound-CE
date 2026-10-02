#include "content.h"

static void put_tile(Level *level, unsigned column, unsigned row, uint8_t tile) {
    if (column < level->width && row < LEVEL_ROWS)
        level->tiles[column * LEVEL_ROWS + row] = tile;
}

static void put_object(Level *level, unsigned x, unsigned y,
                       ObjectType type, unsigned value) {
    LevelObject *object;
    if (level->object_count >= LEVEL_OBJECTS) return;
    object = &level->objects[level->object_count++];
    object->x = (uint16_t)x;
    object->y = (uint16_t)y;
    object->type = (uint8_t)type;
    object->value = (uint8_t)value;
}

static void base_level(Level *level, const char *name, unsigned width,
                       PlayerMode mode) {
    unsigned column;
    level_blank(level, name, (uint16_t)width);
    level->object_count = 0;
    level->start_x = 32;
    level->start_y = (mode == MODE_SHIP || mode == MODE_WAVE) ? 112 : 212;
    level->start_mode = (uint8_t)mode;
    level->start_gravity = 0;
    level->start_speed = 0;
    for (column = 0; column < width; ++column) {
        put_tile(level, column, 0, TILE_BLOCK);
        put_tile(level, column, 14, TILE_BLOCK);
    }
}

static void floor_spikes(Level *level, unsigned x, unsigned count) {
    unsigned offset;
    for (offset = 0; offset < count; ++offset)
        put_tile(level, (x / TILE_SIZE) + offset, 13, TILE_SPIKE_UP);
}

static void ceiling_spikes(Level *level, unsigned x, unsigned count) {
    unsigned offset;
    for (offset = 0; offset < count; ++offset)
        put_tile(level, (x / TILE_SIZE) + offset, 1, TILE_SPIKE_DOWN);
}

static void cube_course(Level *level, unsigned first, unsigned last,
                        unsigned spacing, unsigned pair_every) {
    unsigned x;
    unsigned index = 0;
    for (x = first; x <= last && x < (unsigned)level->width * TILE_SIZE - 80;
         x += spacing, ++index) {
        unsigned count = (pair_every && index % pair_every == pair_every - 1) ? 2 : 1;
        floor_spikes(level, x, count);
    }
}

static void surface_course(Level *level, unsigned first, unsigned last,
                           unsigned spacing) {
    unsigned x;
    unsigned index = 0;
    for (x = first; x <= last && x < (unsigned)level->width * TILE_SIZE - 80;
         x += spacing, ++index) {
        if ((index & 1u) == 0) floor_spikes(level, x, 1);
        else ceiling_spikes(level, x, 1);
    }
}

static void flight_gates(Level *level, unsigned first, unsigned last,
                         unsigned spacing) {
    unsigned x;
    for (x = first; x <= last && x < (unsigned)level->width * TILE_SIZE - 80;
         x += spacing) {
        /* A broad, clear middle lane lets new pilots learn height control. */
        floor_spikes(level, x, 2);
        put_tile(level, (x / TILE_SIZE), 12, TILE_SPIKE_UP);
        put_tile(level, (x / TILE_SIZE), 11, TILE_SPIKE_UP);
        put_tile(level, (x / TILE_SIZE), 10, TILE_SPIKE_UP);
        ceiling_spikes(level, x + 32, 2);
        put_tile(level, ((x + 32) / TILE_SIZE), 2, TILE_SPIKE_DOWN);
        put_tile(level, ((x + 32) / TILE_SIZE), 3, TILE_SPIKE_DOWN);
        put_tile(level, ((x + 32) / TILE_SIZE), 4, TILE_SPIKE_DOWN);
    }
}

static void add_mode_portal(Level *level, unsigned x, PlayerMode mode) {
    put_object(level, (x / TILE_SIZE) * TILE_SIZE, 112, OBJ_MODE, (unsigned)mode);
}

static void add_finish(Level *level) {
    unsigned x = (unsigned)level->width * TILE_SIZE - 32;
    put_object(level, x, 112, OBJ_FINISH, 0);
}

void campaign_load(unsigned index, Level *level) {
    static const uint16_t widths[CAMPAIGN_COUNT] = {
        344, 360, 376, 392, 408, 424, 440, 456, 472, 508
    };
    static const char *const names[CAMPAIGN_COUNT] = {
        "Pulse Start", "Open Skies", "Mirror Steps", "Polarity Run",
        "Signal Flow", "Switchback", "Airlock", "Twin Orbit",
        "Phase Circuit", "Last Signal"
    };
    static const PlayerMode start_modes[CAMPAIGN_COUNT] = {
        MODE_CUBE, MODE_SHIP, MODE_SPIDER, MODE_BALL, MODE_WAVE,
        MODE_CUBE, MODE_SHIP, MODE_SPIDER, MODE_BALL, MODE_WAVE
    };
    if (level == NULL) return;
    if (index >= CAMPAIGN_COUNT) index = CAMPAIGN_COUNT - 1;

    base_level(level, names[index], widths[index], start_modes[index]);

    switch (index) {
    case 0: /* Cube introduction: long approaches, then an occasional double. */
        put_object(level, 160, 208, OBJ_PAD, 0);
        cube_course(level, 480, 5240, 460, 4);
        break;
    case 1: /* Ship introduction: a centered spawn gives time to learn input. */
        flight_gates(level, 480, 5200, 470);
        break;
    case 2: /* Spider introduction: each teleport alternates the safe surface. */
        surface_course(level, 420, 5620, 420);
        break;
    case 3: /* Ball introduction: reverse gravity at each widely spaced spike. */
        surface_course(level, 420, 5900, 420);
        break;
    case 4: /* Wave introduction: steer into the open lane before the first gate. */
        flight_gates(level, 400, 6000, 500);
        break;
    case 5: /* Cube -> ship -> cube, with quiet runways around both portals. */
        cube_course(level, 280, 760, 480, 0);
        add_mode_portal(level, 1040, MODE_SHIP);
        flight_gates(level, 1450, 2770, 440);
        add_mode_portal(level, 2960, MODE_CUBE);
        cube_course(level, 3380, 6600, 460, 4);
        break;
    case 6: /* Ship -> wave -> cube. */
        flight_gates(level, 420, 1830, 470);
        add_mode_portal(level, 2180, MODE_WAVE);
        flight_gates(level, 2580, 3970, 470);
        add_mode_portal(level, 4310, MODE_CUBE);
        cube_course(level, 4730, 6840, 430, 4);
        break;
    case 7: /* Spider -> ball -> wave; paired reversals leave each run grounded. */
        surface_course(level, 400, 1600, 400);
        add_mode_portal(level, 1920, MODE_BALL);
        surface_course(level, 2320, 3680, 400);
        add_mode_portal(level, 4140, MODE_WAVE);
        flight_gates(level, 4540, 7160, 500);
        break;
    case 8: /* Ball -> cube -> ship. */
        surface_course(level, 420, 2520, 420);
        add_mode_portal(level, 2920, MODE_CUBE);
        cube_course(level, 3340, 5740, 400, 3);
        add_mode_portal(level, 6120, MODE_SHIP);
        flight_gates(level, 6540, 7480, 470);
        break;
    default: /* Wave -> spider -> cube -> ship, with generous transition space. */
        flight_gates(level, 380, 1460, 500);
        add_mode_portal(level, 1900, MODE_SPIDER);
        surface_course(level, 2300, 3500, 400);
        add_mode_portal(level, 4100, MODE_CUBE);
        cube_course(level, 4500, 6200, 420, 4);
        add_mode_portal(level, 6660, MODE_SHIP);
        flight_gates(level, 7080, 7980, 470);
        break;
    }

    add_finish(level);
}

static uint32_t next_random(uint32_t *state) {
    uint32_t value = *state;
    if (value == 0) value = 0x6d2b79f5u;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

void endless_load(uint32_t seed, unsigned difficulty, Level *level) {
    unsigned count, i, start_column, span_columns, max_cluster;
    unsigned jitter_range, previous_column = 0;
    uint32_t random_state = seed ? seed : 0x6d2b79f5u;
    if (level == NULL) return;
    if (difficulty > 5) difficulty = 5;
    count = 4 + (difficulty < 2 ? difficulty : 2);
    max_cluster = 1 + difficulty / 2;
    if (max_cluster > 3) max_cluster = 3;
    jitter_range = difficulty == 0 ? 2 : (difficulty == 1 ? 1 : 0);

    base_level(level, "Endless", 96, MODE_CUBE);
    start_column = 14;
    span_columns = 70;
    for (i = 0; i < count; ++i) {
        unsigned column = start_column + (i * span_columns) / (count - 1);
        unsigned cluster, tile_index;
        if (i > 0 && i + 1 < count && jitter_range > 0) {
            int jitter = (int)(next_random(&random_state) % (2 * jitter_range + 1))
                - (int)jitter_range;
            int adjusted = (int)column + jitter;
            unsigned minimum = previous_column + (difficulty == 0 ? 19u : 15u);
            if (adjusted < (int)minimum) adjusted = (int)minimum;
            column = (unsigned)adjusted;
        }
        previous_column = column;
        cluster = 1 + next_random(&random_state) % max_cluster;
        for (tile_index = 0; tile_index < cluster; ++tile_index) {
            unsigned tile = (next_random(&random_state) % 5 == 0 && difficulty >= 3)
                ? TILE_BLOCK : TILE_SPIKE_UP;
            put_tile(level, column + tile_index, 13, (uint8_t)tile);
        }
    }
    add_finish(level);
}
