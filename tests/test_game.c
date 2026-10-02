#include "game.h"
#include <assert.h>
#include <string.h>
static Level l;
static void blank(void){level_blank(&l,"TEST",64);}
void test_game(void) {
    Game g,a,b;
    blank();game_start(&g,&l);assert(g.grounded);
    game_step(&g,&l,(GameInput){true});assert(g.y<212*FP_ONE&&!g.dead);
    for(int i=0;i<100;i++)game_step(&g,&l,(GameInput){false});
    assert(g.y==212*FP_ONE&&g.grounded&&!g.dead);
    blank();l.tiles[6*15+13]=TILE_SPIKE_UP;game_start(&g,&l);
    for(int i=0;i<60;i++)game_step(&g,&l,(GameInput){false});assert(g.dead);
    blank();l.tiles[6*15+13]=TILE_BLOCK;game_start(&g,&l);g.speed=2;
    for(int i=0;i<60;i++)game_step(&g,&l,(GameInput){false});assert(g.dead);
    blank();l.start_mode=MODE_SHIP;l.start_y=110;game_start(&g,&l);
    game_step(&g,&l,(GameInput){true});assert(g.vy<0);
    for(int i=0;i<100;i++)game_step(&g,&l,(GameInput){true});assert(g.dead);
    blank();l.start_mode=MODE_WAVE;l.start_y=110;game_start(&g,&l);
    game_step(&g,&l,(GameInput){true});assert(g.y==108*FP_ONE);
    game_step(&g,&l,(GameInput){false});assert(g.y==110*FP_ONE);
    blank();l.start_mode=MODE_BALL;game_start(&g,&l);
    game_step(&g,&l,(GameInput){true});assert(g.gravity==1);
    for(int i=0;i<100;i++)game_step(&g,&l,(GameInput){true});assert(g.gravity==1&&g.grounded&&g.y==16*FP_ONE);
    game_step(&g,&l,(GameInput){false});game_step(&g,&l,(GameInput){true});assert(g.gravity==0);
    blank();l.start_mode=MODE_SPIDER;game_start(&g,&l);
    game_step(&g,&l,(GameInput){true});assert(g.y==16*FP_ONE&&g.grounded&&!g.dead);
    blank();l.start_mode=MODE_SPIDER;l.tiles[2*15+7]=TILE_SPIKE_UP;game_start(&g,&l);
    game_step(&g,&l,(GameInput){true});assert(g.dead);
    blank();l.objects[l.object_count++]=(LevelObject){64,112,OBJ_MODE,MODE_BALL};
    l.objects[l.object_count++]=(LevelObject){96,112,OBJ_GRAVITY,1};game_start(&g,&l);
    for(int i=0;i<36;i++)game_step(&g,&l,(GameInput){false});assert(g.mode==MODE_BALL&&g.gravity==1&&!g.dead);
    blank();l.objects[l.object_count++]=(LevelObject){48,208,OBJ_PAD,0};game_start(&g,&l);
    for(int i=0;i<8;i++)game_step(&g,&l,(GameInput){false});assert(g.vy<0);
    blank();l.objects[l.object_count++]=(LevelObject){32,208,OBJ_ORB,0};game_start(&g,&l);
    game_step(&g,&l,(GameInput){true});assert(g.vy< -1000);
    blank();game_start(&a,&l);b=a;
    for(int i=0;i<200;i++){bool action=i%70<20;game_step(&a,&l,(GameInput){action});game_step(&b,&l,(GameInput){action});}
    assert(memcmp(&a,&b,sizeof(a))==0);
    /* Checkpoint is a full deterministic state, including trigger history. */
    a=g;b=a;for(int i=0;i<30;i++)game_step(&a,&l,(GameInput){false});
    g=b;for(int i=0;i<30;i++)game_step(&g,&l,(GameInput){false});assert(memcmp(&a,&g,sizeof(a))==0);
}
