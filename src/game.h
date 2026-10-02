#ifndef NEON_GAME_H
#define NEON_GAME_H
#include "level.h"
#define FP_ONE 256L
#define PLAYER_SIZE 12
typedef struct {
    int32_t x, y, vy; /* Q24.8 */
    uint32_t ticks;
    uint8_t mode, gravity, speed;
    bool grounded, dead, won, previous_action;
    uint8_t activated[LEVEL_OBJECTS / 8];
} Game;
typedef struct { bool action; } GameInput;
void game_start(Game *game, const Level *level);
void game_step(Game *game, const Level *level, GameInput input);
uint8_t game_progress(const Game *game, const Level *level);
int32_t game_speed(uint8_t speed);
const char *game_mode_name(uint8_t mode);
#endif
