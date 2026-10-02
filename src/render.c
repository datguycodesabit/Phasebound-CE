#include "render.h"
#include <graphx.h>
#include <stdio.h>
#include <string.h>

static const uint16_t palette[]={
    gfx_RGBTo1555(8,12,25),gfx_RGBTo1555(18,28,46),gfx_RGBTo1555(40,225,232),
    gfx_RGBTo1555(255,71,156),gfx_RGBTo1555(234,248,255),gfx_RGBTo1555(112,137,162),
    gfx_RGBTo1555(255,213,76),gfx_RGBTo1555(94,242,155),gfx_RGBTo1555(255,92,93),
    gfx_RGBTo1555(79,135,255),gfx_RGBTo1555(179,122,255),gfx_RGBTo1555(255,159,77)
};
uint8_t render_player_color(unsigned i) {static const uint8_t colors[]={C_CYAN,C_GREEN,C_PINK,C_YELLOW,C_BLUE,C_PURPLE,C_ORANGE,C_WHITE,C_RED,C_MUTED,C_CYAN};return colors[i%11];}
void render_begin(void) {gfx_Begin();gfx_SetPalette(palette,sizeof(palette),0);gfx_SetDrawBuffer();gfx_SetTextConfig(gfx_text_clip);gfx_SetTextBGColor(C_BG);gfx_SetTextTransparentColor(C_BG);}
void render_text(int x,int y,uint8_t c,const char *s) {
    gfx_SetTextConfig(x>=0 && y>=0 && y<=232 && x+(int)strlen(s)*8<=320 ? gfx_text_noclip : gfx_text_clip);
    gfx_SetTextScale(1,1);gfx_SetTextFGColor(c);gfx_PrintStringXY(s,x,y);
}
void render_title(const char *title,const char *subtitle) {
    gfx_FillScreen(C_BG);gfx_SetColor(C_PANEL);
    for(int x=0;x<320;x+=16) gfx_VertLine(x,0,240);
    gfx_SetColor(C_CYAN);gfx_FillRectangle(20,25,28,3);
    gfx_SetTextConfig(gfx_text_noclip);gfx_SetTextFGColor(C_WHITE);gfx_SetTextScale(2,2);gfx_PrintStringXY(title,20,40);gfx_SetTextConfig(gfx_text_clip);
    render_text(20,65,C_MUTED,subtitle);
}
void render_menu_item(int y,const char *s,bool selected) {
    if(selected){gfx_SetColor(C_PANEL);gfx_FillRectangle(16,y-6,288,22);gfx_SetColor(C_CYAN);gfx_FillRectangle(16,y-6,3,22);}
    render_text(28,y,selected?C_WHITE:C_MUTED,s);
}
static void player(int x,int y,uint8_t mode,uint8_t c,uint32_t ticks) {
    gfx_SetColor(c);
    if(mode==MODE_SHIP) {gfx_FillTriangle(x,y,x,y+12,x+15,y+6);gfx_SetColor(C_BG);gfx_FillRectangle(x+2,y+4,5,4);}
    else if(mode==MODE_WAVE) {gfx_FillTriangle(x,y+12,x+7,y,x+14,y+12);}
    else if(mode==MODE_BALL) {gfx_FillCircle(x+6,y+6,6);gfx_SetColor(C_BG);gfx_FillCircle(x+6,y+6,2);}
    else if(mode==MODE_SPIDER) {gfx_FillRectangle(x+1,y+3,10,6);gfx_Line(x,y,x+3,y+4);gfx_Line(x+12,y,x+9,y+4);gfx_Line(x,y+12,x+3,y+8);gfx_Line(x+12,y+12,x+9,y+8);}
    else {gfx_FillRectangle(x,y,12,12);gfx_SetColor(C_BG);gfx_Rectangle(x+2,y+2,8,8);gfx_SetColor(C_WHITE);gfx_FillRectangle(x+3+(ticks/12%2),y+4,2,2);}
}
void render_world(const Level *l,const Game *g,const Editor *e,uint8_t color,bool effects) {
    int cam=e?(int)e->camera_col*16:(int)(g->x/FP_ONE)-72;
    char text[48];unsigned i;int col,row;
    if(cam<0) cam=0;
    if(cam>(int)l->width*16-320) cam=(int)l->width*16-320;
    gfx_FillScreen(C_BG);gfx_SetColor(C_PANEL);
    for(col=-(cam/3%32);col<320;col+=32) gfx_VertLine(col,16,208);
    for(row=32;row<224;row+=32) gfx_HorizLine(0,row,320);
    for(col=cam/16;col<cam/16+21 && col<l->width;col++) for(row=0;row<15;row++) {
        int x=col*16-cam,y=row*16;uint8_t tile=level_tile(l,col,row);
        if(tile==TILE_BLOCK) {gfx_SetColor(C_PANEL);gfx_FillRectangle(x,y,16,16);gfx_SetColor(C_BLUE);gfx_Rectangle(x,y,16,16);}
        else if(tile==TILE_SPIKE_UP) {gfx_SetColor(C_PINK);gfx_FillTriangle(x,y+15,x+8,y,x+15,y+15);}
        else if(tile==TILE_SPIKE_DOWN) {gfx_SetColor(C_PINK);gfx_FillTriangle(x,y,x+8,y+15,x+15,y);}
    }
    for(i=0;i<l->object_count;i++) {
        const LevelObject *o=&l->objects[i];int x=o->x-cam,y=o->y;
        if(x< -16||x>320) continue;
        if(o->type==OBJ_PAD) {gfx_SetColor(C_YELLOW);gfx_FillRectangle(x,y+11,16,5);}
        else if(o->type==OBJ_ORB) {gfx_SetColor(C_YELLOW);gfx_Circle(x+8,y+8,7);gfx_FillCircle(x+8,y+8,3);}
        else {
            uint8_t c=o->type==OBJ_FINISH?C_GREEN:o->type==OBJ_MODE?C_CYAN:o->type==OBJ_SPEED?C_ORANGE:C_PURPLE;
            gfx_SetColor(c);gfx_VertLine(x+7,16,208);
            gfx_Rectangle(x+2,92,12,48);
            if(o->type==OBJ_MODE) player(x+2,110,o->value,c,0);
            else render_text(x,112,c,o->type==OBJ_FINISH?"F":o->type==OBJ_SPEED?">":"G");
        }
    }
    if(e) {
        int sx=l->start_x-cam;
        if(sx>=0&&sx<320) player(sx,l->start_y,l->start_mode,C_GREEN,0);
        gfx_SetColor(C_WHITE);gfx_Rectangle(e->x*16-cam,e->y*16,16,16);
        gfx_SetColor(C_BG);gfx_FillRectangle(0,0,320,16);gfx_FillRectangle(0,224,320,16);
        snprintf(text,sizeof(text),"%s %u [%u,%u]%s",editor_tool_name(e),e->value,e->x,e->y,e->dirty?" *":"");
        render_text(4,4,C_WHITE,text);render_text(4,228,C_MUTED,"MODE tool  ALPHA value  GRAPH test");
        gfx_SetColor(C_WHITE);gfx_Rectangle(e->x*16-cam,e->y*16,16,16);
    } else {
        int x=(int)(g->x/FP_ONE)-cam,y=(int)(g->y/FP_ONE);
        if(effects) {gfx_SetColor(C_MUTED);for(i=1;i<5;i++) gfx_FillRectangle(x-(int)i*7,y+5+(int)(i%2)*2,3,3);}
        player(x,y,g->mode,color,g->ticks);
        gfx_SetColor(C_BG);gfx_FillRectangle(0,0,320,16);
        snprintf(text,sizeof(text),"%s   %u%%",game_mode_name(g->mode),game_progress(g,l));render_text(5,4,C_WHITE,text);
        gfx_SetColor(C_CYAN);gfx_FillRectangle(160,6,game_progress(g,l)*154/100,4);
        gfx_SetColor(C_BG);gfx_FillRectangle(0,224,320,16);
        gfx_SetColor(C_BLUE);gfx_HorizLine(0,223,320);
    }
}
