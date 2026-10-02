#ifndef NEON_RENDER_H
#define NEON_RENDER_H
#include "game.h"
#include "editor.h"
enum { C_BG, C_PANEL, C_CYAN, C_PINK, C_WHITE, C_MUTED, C_YELLOW, C_GREEN, C_RED, C_BLUE, C_PURPLE, C_ORANGE };
void render_begin(void);
void render_title(const char *title,const char *subtitle);
void render_text(int x,int y,uint8_t color,const char *text);
void render_world(const Level *l,const Game *g,const Editor *e,uint8_t color,bool effects);
void render_menu_item(int y,const char *text,bool selected);
uint8_t render_player_color(unsigned index);
#endif
