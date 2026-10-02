#ifndef NEON_CONTENT_H
#define NEON_CONTENT_H
#include "game.h"
void campaign_load(unsigned index, Level *level);
/* Sections are 96 columns long, grounded cube entry/exit. Caller advances
   seed and difficulty between sections, preserving total distance. */
void endless_load(uint32_t seed, unsigned difficulty, Level *level);
#endif
