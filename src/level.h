#ifndef NEON_LEVEL_H
#define NEON_LEVEL_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#define LEVEL_COLS 512
#define LEVEL_ROWS 15
#define LEVEL_OBJECTS 256
#define TILE_SIZE 16
#define LEVEL_NAME 24
#define CAMPAIGN_COUNT 10
#define CUSTOM_SLOTS 10
typedef enum { MODE_CUBE, MODE_SHIP, MODE_SPIDER, MODE_BALL, MODE_WAVE, MODE_COUNT } PlayerMode;
typedef enum { TILE_EMPTY, TILE_BLOCK, TILE_SPIKE_UP, TILE_SPIKE_DOWN, TILE_COUNT } Tile;
typedef enum { OBJ_PAD, OBJ_ORB, OBJ_GRAVITY, OBJ_SPEED, OBJ_MODE, OBJ_FINISH, OBJ_COUNT } ObjectType;
/* Pixel coordinates, integer values: gravity 0=down/1=up; speed 0..2;
   mode 0..4. Pad/orb/finish value must be 0. Object spans one 16px tile. */
typedef struct { uint16_t x, y; uint8_t type, value; } LevelObject;
typedef struct {
    char name[LEVEL_NAME];
    uint16_t width, start_x, start_y, object_count;
    uint8_t start_mode, start_gravity, start_speed;
    uint8_t tiles[LEVEL_COLS * LEVEL_ROWS]; /* column-major: x*15+y */
    LevelObject objects[LEVEL_OBJECTS];
} Level;
void level_blank(Level *level, const char *name, uint16_t width);
uint8_t level_tile(const Level *level, int x, int y);
bool level_validate(const Level *level, char *error, size_t error_size);
/* Explicit little-endian versioned wire format; no struct dumps. */
size_t level_encode(const Level *level, uint8_t *out, size_t capacity);
bool level_decode(Level *level, const uint8_t *data, size_t size, char *error, size_t error_size);
#define LEVEL_WIRE_MAX 9400
#endif
