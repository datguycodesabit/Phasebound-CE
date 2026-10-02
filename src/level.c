#include "checksum.h"
#include "level.h"

#include <string.h>

#define LEVEL_PLAYER_SIZE 12
#define LEVEL_WIRE_VERSION 1u
#define LEVEL_WIRE_HEADER_SIZE 42u
#define LEVEL_WIRE_CHECKSUM_SIZE 4u

static void set_error(char *error, size_t size, const char *message)
{
    if (error && size) {
        size_t n = strlen(message);
        if (n >= size) n = size - 1;
        memcpy(error, message, n);
        error[n] = '\0';
    }
}

static void put_u16(uint8_t *out, size_t *at, uint16_t value)
{
    out[(*at)++] = (uint8_t)(value & 0xffu);
    out[(*at)++] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t *out, size_t *at, uint32_t value)
{
    out[(*at)++] = (uint8_t)(value & 0xffu);
    out[(*at)++] = (uint8_t)((value >> 8) & 0xffu);
    out[(*at)++] = (uint8_t)((value >> 16) & 0xffu);
    out[(*at)++] = (uint8_t)(value >> 24);
}

static uint16_t get_u16(const uint8_t *data, size_t *at)
{
    uint16_t value = (uint16_t)data[*at] | ((uint16_t)data[*at + 1] << 8);
    *at += 2;
    return value;
}


static int intersects(int ax, int ay, int aw, int ah,
                      int bx, int by, int bw, int bh)
{
    return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

uint8_t level_tile(const Level *level, int x, int y)
{
    if (!level || x < 0 || y < 0 || x >= level->width || y >= LEVEL_ROWS)
        return TILE_EMPTY;
    return level->tiles[(size_t)x * LEVEL_ROWS + (size_t)y];
}

void level_blank(Level *level, const char *name, uint16_t width)
{
    unsigned x;
    if (!level) return;
    memset(level, 0, sizeof(*level));
    if (width < 32) width = 32;
    if (width > LEVEL_COLS) width = LEVEL_COLS;
    level->width = width;
    level->start_x = 32;
    level->start_y = 212;
    level->start_mode = MODE_CUBE;
    level->start_gravity = 0;
    level->start_speed = 0;
    if (name) {
        size_t n = 0;
        while (n < LEVEL_NAME - 1 && name[n]) {
            level->name[n] = name[n];
            ++n;
        }
        level->name[n] = '\0';
    } else {
        memcpy(level->name, "Untitled", sizeof("Untitled"));
    }
    for (x = 0; x < width; ++x) {
        level->tiles[(size_t)x * LEVEL_ROWS] = TILE_BLOCK;
        level->tiles[(size_t)x * LEVEL_ROWS + (LEVEL_ROWS - 1)] = TILE_BLOCK;
    }
    level->object_count = 1;
    level->objects[0].x = (uint16_t)((width - 1u) * TILE_SIZE);
    level->objects[0].y = (uint16_t)((LEVEL_ROWS - 2u) * TILE_SIZE);
    level->objects[0].type = OBJ_FINISH;
    level->objects[0].value = 0;
}

bool level_validate(const Level *level, char *error, size_t error_size)
{
    size_t i;
    int player_x, player_y;
    int tile_x0, tile_y0, tile_x1, tile_y1;
    int found_finish = 0;
    if (!level) {
        set_error(error, error_size, "Missing level");
        return false;
    }
    if (level->width < 32 || level->width > LEVEL_COLS) {
        set_error(error, error_size, "Level width must be 32..512");
        return false;
    }
    if (!memchr(level->name, '\0', LEVEL_NAME)) {
        set_error(error, error_size, "Level name is not terminated");
        return false;
    }
    if (level->start_mode >= MODE_COUNT || level->start_gravity > 1 || level->start_speed > 2) {
        set_error(error, error_size, "Invalid start state");
        return false;
    }
    if (level->object_count > LEVEL_OBJECTS) {
        set_error(error, error_size, "Too many level objects");
        return false;
    }

    for (i = 0; i < (size_t)level->width * LEVEL_ROWS; ++i) {
        if (level->tiles[i] >= TILE_COUNT) {
            set_error(error, error_size, "Invalid tile type");
            return false;
        }
    }

    player_x = level->start_x;
    player_y = level->start_y;
    if (player_x < 0 || player_y < 0
        || player_x + LEVEL_PLAYER_SIZE > (int)level->width * TILE_SIZE
        || player_y + LEVEL_PLAYER_SIZE > LEVEL_ROWS * TILE_SIZE) {
        set_error(error, error_size, "Start position is outside the level");
        return false;
    }
    tile_x0 = player_x / TILE_SIZE;
    tile_y0 = player_y / TILE_SIZE;
    tile_x1 = (player_x + LEVEL_PLAYER_SIZE - 1) / TILE_SIZE;
    tile_y1 = (player_y + LEVEL_PLAYER_SIZE - 1) / TILE_SIZE;
    for (i = (size_t)tile_x0; i <= (size_t)tile_x1; ++i) {
        int y;
        for (y = tile_y0; y <= tile_y1; ++y) {
            if (level_tile(level, (int)i, y) != TILE_EMPTY) {
                set_error(error, error_size, "Start overlaps a tile");
                return false;
            }
        }
    }

    for (i = 0; i < level->object_count; ++i) {
        const LevelObject *object = &level->objects[i];
        if (object->type >= OBJ_COUNT) {
            set_error(error, error_size, "Invalid object type");
            return false;
        }
        if (object->x % TILE_SIZE || object->y % TILE_SIZE
            || object->x >= (uint32_t)level->width * TILE_SIZE
            || object->y >= LEVEL_ROWS * TILE_SIZE) {
            set_error(error, error_size, "Object is outside the grid");
            return false;
        }
        switch (object->type) {
        case OBJ_GRAVITY:
            if (object->value > 1) {
                set_error(error, error_size, "Invalid gravity value");
                return false;
            }
            break;
        case OBJ_SPEED:
            if (object->value > 2) {
                set_error(error, error_size, "Invalid speed value");
                return false;
            }
            break;
        case OBJ_MODE:
            if (object->value >= MODE_COUNT) {
                set_error(error, error_size, "Invalid mode value");
                return false;
            }
            break;
        default:
            if (object->value != 0) {
                set_error(error, error_size, "Unexpected object value");
                return false;
            }
            break;
        }
        if (object->type == OBJ_FINISH) found_finish = 1;
        if (intersects(player_x, player_y, LEVEL_PLAYER_SIZE, LEVEL_PLAYER_SIZE,
                       object->x, object->y, TILE_SIZE, TILE_SIZE)) {
            set_error(error, error_size, "Start overlaps an object");
            return false;
        }
    }
    if (!found_finish) {
        set_error(error, error_size, "Level has no finish object");
        return false;
    }
    set_error(error, error_size, "");
    return true;
}

size_t level_encode(const Level *level, uint8_t *out, size_t capacity)
{
    size_t size, at = 0, i;
    uint32_t sum;
    if (!out || !level_validate(level, NULL, 0)) return 0;
    size = LEVEL_WIRE_HEADER_SIZE + (size_t)level->width * LEVEL_ROWS
         + (size_t)level->object_count * 6u + LEVEL_WIRE_CHECKSUM_SIZE;
    if (size > capacity || size > LEVEL_WIRE_MAX) return 0;

    out[at++] = 'N'; out[at++] = 'D'; out[at++] = 'L'; out[at++] = 'V';
    put_u16(out, &at, LEVEL_WIRE_VERSION);
    memcpy(out + at, level->name, LEVEL_NAME); at += LEVEL_NAME;
    put_u16(out, &at, level->width);
    put_u16(out, &at, level->start_x);
    put_u16(out, &at, level->start_y);
    put_u16(out, &at, level->object_count);
    out[at++] = level->start_mode;
    out[at++] = level->start_gravity;
    out[at++] = level->start_speed;
    out[at++] = 0;
    for (i = 0; i < (size_t)level->width * LEVEL_ROWS; ++i) out[at++] = level->tiles[i];
    for (i = 0; i < level->object_count; ++i) {
        const LevelObject *object = &level->objects[i];
        put_u16(out, &at, object->x);
        put_u16(out, &at, object->y);
        out[at++] = object->type;
        out[at++] = object->value;
    }
    sum = checksum32(out, at);
    put_u32(out, &at, sum);
    return at;
}

bool level_decode(Level *level, const uint8_t *data, size_t size, char *error, size_t error_size)
{
    size_t at = 0, expected, i;
    uint16_t version;
    uint16_t width, start_x, start_y, object_count;
    uint8_t start_mode, start_gravity, start_speed;
    char name[LEVEL_NAME];
    uint32_t stored_sum;
    if (!level || !data || size < LEVEL_WIRE_HEADER_SIZE + LEVEL_WIRE_CHECKSUM_SIZE) {
        set_error(error, error_size, "Level data is too short");
        return false;
    }
    if (data[0] != 'N' || data[1] != 'D' || data[2] != 'L' || data[3] != 'V') {
        set_error(error, error_size, "Invalid level signature");
        return false;
    }
    at = 4;
    version = get_u16(data, &at);
    if (version != LEVEL_WIRE_VERSION) {
        set_error(error, error_size, "Unsupported level version");
        return false;
    }
    memcpy(name, data + at, LEVEL_NAME); at += LEVEL_NAME;
    width = get_u16(data, &at);
    start_x = get_u16(data, &at);
    start_y = get_u16(data, &at);
    object_count = get_u16(data, &at);
    start_mode = data[at++];
    start_gravity = data[at++];
    start_speed = data[at++];
    if (data[at++] != 0) {
        set_error(error, error_size, "Invalid reserved field");
        return false;
    }
    if (width < 32 || width > LEVEL_COLS || object_count > LEVEL_OBJECTS) {
        set_error(error, error_size, "Level dimensions are invalid");
        return false;
    }
    expected = LEVEL_WIRE_HEADER_SIZE + (size_t)width * LEVEL_ROWS
             + (size_t)object_count * 6u + LEVEL_WIRE_CHECKSUM_SIZE;
    if (size != expected || size > LEVEL_WIRE_MAX) {
        set_error(error, error_size, "Level data size does not match its header");
        return false;
    }
    stored_sum = (uint32_t)data[size - 4]
               | ((uint32_t)data[size - 3] << 8)
               | ((uint32_t)data[size - 2] << 16)
               | ((uint32_t)data[size - 1] << 24);
    if (checksum32(data, size - 4) != stored_sum) {
        set_error(error, error_size, "Level checksum mismatch");
        return false;
    }

    /* Fill only after the fixed header and checksum pass. This keeps the
       portable codec's stack footprint small on the calculator. */
    memset(level, 0, sizeof(*level));
    memcpy(level->name, name, LEVEL_NAME);
    level->width = width;
    level->start_x = start_x;
    level->start_y = start_y;
    level->object_count = object_count;
    level->start_mode = start_mode;
    level->start_gravity = start_gravity;
    level->start_speed = start_speed;
    for (i = 0; i < (size_t)width * LEVEL_ROWS; ++i) level->tiles[i] = data[at++];
    for (i = 0; i < object_count; ++i) {
        level->objects[i].x = get_u16(data, &at);
        level->objects[i].y = get_u16(data, &at);
        level->objects[i].type = data[at++];
        level->objects[i].value = data[at++];
    }
    if (!level_validate(level, error, error_size)) return false;
    return true;
}
