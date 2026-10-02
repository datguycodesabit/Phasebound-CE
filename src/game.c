#include "game.h"
#include <string.h>

int32_t game_speed(uint8_t speed) {
    static const int32_t speeds[] = {512, 640, 768};
    return speeds[speed < 3 ? speed : 0];
}
const char *game_mode_name(uint8_t mode) {
    static const char *names[] = {"CORE", "GLIDER", "BLINK", "TUMBLER", "BEAM"};
    return mode < MODE_COUNT ? names[mode] : "?";
}
static bool overlap(int x, int y, int w, int h, int a, int b, int c, int d) {
    return x < a+c && x+w > a && y < b+d && y+h > b;
}
/* Solid collision uses the full player rectangle. Hazards use a 2px inset
   and a triangular profile, avoiding deaths on transparent spike corners. */
static bool solid(const Level *l, int32_t fx, int32_t fy) {
    int tx,ty;
    if (fx<0 || fy<0 || fx+PLAYER_SIZE*FP_ONE>(int32_t)l->width*TILE_SIZE*FP_ONE || fy+PLAYER_SIZE*FP_ONE>240*FP_ONE) return true;
    for(tx=(int)(fx/(16*FP_ONE));tx<=(int)((fx+12*FP_ONE-1)/(16*FP_ONE));tx++)
        for(ty=(int)(fy/(16*FP_ONE));ty<=(int)((fy+12*FP_ONE-1)/(16*FP_ONE));ty++)
        if(level_tile(l,tx,ty)==TILE_BLOCK) return true;
    return false;
}
static bool hazard(const Level *l, int32_t fx, int32_t fy) {
    int x=(int)(fx/FP_ONE)+2,y=(int)(fy/FP_ONE)+2,tx,ty,px;
    if(fy<0 || fy+PLAYER_SIZE*FP_ONE>240*FP_ONE) return true;
    for(tx=x/16;tx<=(x+7)/16;tx++) for(ty=y/16;ty<=(y+7)/16;ty++) {
        uint8_t tile=level_tile(l,tx,ty);
        if(tile!=TILE_SPIKE_UP && tile!=TILE_SPIKE_DOWN) continue;
        for(px=0;px<16;px++) {
            int height=px<8?px*2+1:(15-px)*2+1;
            int top=ty*16+(tile==TILE_SPIKE_UP?16-height:0);
            if(overlap(x,y,8,8,tx*16+px,top,1,height)) return true;
        }
    }
    return false;
}
static bool support(const Game *g,const Level *l) {
    return solid(l,g->x,g->y+(g->gravity?-FP_ONE:FP_ONE));
}
void game_start(Game *g, const Level *l) {
    memset(g,0,sizeof(*g));
    g->x=(int32_t)l->start_x*FP_ONE; g->y=(int32_t)l->start_y*FP_ONE;
    g->mode=l->start_mode;g->gravity=l->start_gravity;g->speed=l->start_speed;
    g->grounded=support(g,l);
}
static void spider_jump(Game *g,const Level *l) {
    int n;int32_t dy=g->gravity?-FP_ONE:FP_ONE;
    for(n=0;n<240;n++) {
        if(solid(l,g->x,g->y+dy)) {g->grounded=true;g->vy=0;return;}
        g->y+=dy;
        if(hazard(l,g->x,g->y)) {g->dead=true;return;}
    }
    g->dead=true;
}
static bool triggered(const Game *g,unsigned i) {return (g->activated[i/8] & (1u<<(i%8)))!=0;}
static void activate(Game *g,unsigned i) {g->activated[i/8]|=(uint8_t)(1u<<(i%8));}
static void objects(Game *g,const Level *l,bool pressed) {
    unsigned i;int x=(int)(g->x/FP_ONE),y=(int)(g->y/FP_ONE);
    for(i=0;i<l->object_count;i++) {
        const LevelObject *o=&l->objects[i];
        if(triggered(g,i) || x+PLAYER_SIZE<=o->x || x>=o->x+16) continue;
        if(o->type==OBJ_PAD || o->type==OBJ_ORB) {
            if(!overlap(x,y,12,12,o->x,o->y,16,16)) continue;
            if(o->type==OBJ_ORB && !pressed) continue;
            g->vy=(g->gravity?1:-1)*(o->type==OBJ_PAD?1280:1100);
            g->grounded=false;
        } else if(o->type==OBJ_GRAVITY) {
            g->gravity=o->value;g->grounded=false;g->vy=0;
        } else if(o->type==OBJ_SPEED) g->speed=o->value;
        else if(o->type==OBJ_MODE) {g->mode=o->value;g->vy=0;g->grounded=support(g,l);}
        else if(o->type==OBJ_FINISH) g->won=true;
        activate(g,i);
    }
}
void game_step(Game *g,const Level *l,GameInput input) {
    bool pressed=input.action&&!g->previous_action;
    int sign,steps,i;int32_t dx,dy,oldx,oldy,stepx,stepy;
    if(g->dead||g->won) return;
    g->previous_action=input.action;g->ticks++;
    objects(g,l,pressed);
    if(g->won) return;
    sign=g->gravity?-1:1;
    if(g->mode==MODE_CUBE) {
        if(input.action&&g->grounded) {g->vy=-960*sign;g->grounded=false;}
        g->vy+=26*sign;
    } else if(g->mode==MODE_SHIP) {
        g->vy+=(input.action?-20:20)*sign;
        if(g->vy>640) g->vy=640;
        if(g->vy< -640) g->vy=-640;
        g->grounded=false;
    } else if(g->mode==MODE_WAVE) {g->vy=(input.action?-1:1)*sign*game_speed(g->speed);g->grounded=false;}
    else {
        if(pressed&&g->grounded) {
            g->gravity=!g->gravity;sign=-sign;g->grounded=false;g->vy=0;
            if(g->mode==MODE_SPIDER) spider_jump(g,l);
        }
        g->vy+=26*sign;
    }
    if(g->dead) return;
    if(g->vy>1536) g->vy=1536;
    if(g->vy< -1536) g->vy=-1536;
    dx=game_speed(g->speed);dy=g->vy;
    /* Subdivide to <=1px per axis: fast motion cannot tunnel through tiles. */
    steps=(int)((dx>(dy<0?-dy:dy)?dx:(dy<0?-dy:dy))+255)/256;
    oldx=g->x;oldy=g->y;g->grounded=false;
    stepx=dx/steps;stepy=dy/steps;
    for(i=1;i<=steps;i++) {
        int32_t nx=i==steps?oldx+dx:g->x+stepx;
        int32_t ny=i==steps?oldy+dy:g->y+stepy;
        if(solid(l,nx,g->y)) {g->dead=true;break;}
        g->x=nx;
        if(solid(l,g->x,ny)) {
            if(g->mode==MODE_SHIP || g->mode==MODE_WAVE) {g->dead=true;break;}
            /* Snap the leading edge exactly to the solid tile boundary. */
            if(dy>0) g->y=(((ny+PLAYER_SIZE*FP_ONE-1)/(16*FP_ONE))*16-PLAYER_SIZE)*FP_ONE;
            else g->y=((ny/FP_ONE)/16+1)*16*FP_ONE;
            if((dy>0&&sign>0)||(dy<0&&sign<0)) g->grounded=true;
            g->vy=0;dy=0;stepy=0;oldy=g->y;
        } else g->y=ny;
        if(hazard(l,g->x,g->y)) {g->dead=true;break;}
    }
    if(!g->dead) {g->grounded=support(g,l);objects(g,l,pressed);}
}
uint8_t game_progress(const Game *g,const Level *l) {
    unsigned i;int32_t end=(int32_t)(l->width-1)*16;
    if(g->won) return 100;
    for(i=0;i<l->object_count;i++) if(l->objects[i].type==OBJ_FINISH && l->objects[i].x<end) end=l->objects[i].x;
    if(end<=l->start_x) return 0;
    {int32_t p=(g->x/FP_ONE-l->start_x)*100/(end-l->start_x);return (uint8_t)(p<0?0:p>99?99:p);}
}
