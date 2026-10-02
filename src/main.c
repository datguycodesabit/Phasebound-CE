#include <graphx.h>
#include <keypadc.h>
#include <fileioc.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "content.h"
#include "storage.h"
#include "editor.h"
#include "render.h"

enum { K_UP=1, K_DOWN=2, K_LEFT=4, K_RIGHT=8, K_ACTION=16, K_ENTER=32,
       K_BACK=64, K_DEL=128, K_MODE=256, K_ALPHA=512, K_UNDO=1024,
       K_TEST=2048, K_SAVE=4096, K_NAME=8192, K_EXPORT=16384, K_IMPORT=32768 };
static Level level;
static Profile profile;
static Game game,checkpoint;
static Editor editor;
static uint32_t keys,pressed,previous;
/* Exposed in linker map for emulator diagnostics. Units are 32768 Hz clocks. */
volatile uint32_t nd_frames,nd_ticks,nd_max_frame_clocks;
volatile uint8_t nd_screen;
volatile uint8_t nd_storage_ok;
static void scan(void) {
    uint32_t k=0;kb_Scan();
    if(kb_Data[7]&kb_Up) k|=K_UP;
    if(kb_Data[7]&kb_Down) k|=K_DOWN;
    if(kb_Data[7]&kb_Left) k|=K_LEFT;
    if(kb_Data[7]&kb_Right) k|=K_RIGHT;
    if((kb_Data[1]&kb_2nd)||(k&K_UP)) k|=K_ACTION;
    if(kb_Data[6]&kb_Enter) k|=K_ENTER;
    if(kb_Data[6]&kb_Clear) k|=K_BACK;
    if(kb_Data[1]&kb_Del) k|=K_DEL;
    if(kb_Data[1]&kb_Mode) k|=K_MODE;
    if(kb_Data[2]&kb_Alpha) k|=K_ALPHA;
    if(kb_Data[2]&kb_Math) k|=K_UNDO;
    if(kb_Data[1]&kb_Graph) k|=K_TEST;
    if(kb_Data[1]&kb_Yequ) k|=K_SAVE;
    if(kb_Data[1]&kb_Window) k|=K_NAME;
    if(kb_Data[1]&kb_Zoom) k|=K_EXPORT;
    if(kb_Data[1]&kb_Trace) k|=K_IMPORT;
    keys=k;pressed=k&~previous;previous=k;
}
static void release_keys(void) {do{scan();}while(keys);pressed=0;}
static void busy(const char *title) {
    nd_screen=0;render_title(title,"Please wait...");gfx_SwapDraw();
}
static void show_message(const char *title,const char *message) {
    release_keys();nd_screen=0;
    render_title(title,message);render_text(20,198,C_CYAN,"ENTER / CLEAR to continue");gfx_SwapDraw();nd_screen=6;
    do{scan();}while(!(pressed&(K_ENTER|K_BACK)));release_keys();
}
static int menu(const char *title,const char *subtitle,const char *const *items,int count,int selected) {
    bool redraw=true;release_keys();
    uint8_t screen=!strcmp(title,"PHASEBOUND")?1:!strcmp(title,"CAMPAIGN")?2:!strcmp(title,"PAUSED")?4:!strcmp(title,"CREATE")?7:!strcmp(title,"CUSTOM LEVEL")?8:!strcmp(title,"PLAY MODE")?9:10;
    for(;;) {
        if(redraw) {
            int first=selected/6*6;
            render_title(title,subtitle);
            for(int i=first;i<count&&i<first+6;i++) render_menu_item(91+(i-first)*21,items[i],i==selected);
            render_text(20,224,C_CYAN,"UP/DOWN  ENTER select  CLEAR back");gfx_SwapDraw();nd_screen=screen;redraw=false;
        }
        scan();
        if(pressed&K_BACK) return -1;
        if(pressed&K_UP) {selected=(selected+count-1)%count;redraw=true;}
        if(pressed&K_DOWN) {selected=(selected+1)%count;redraw=true;}
        if(pressed&K_ENTER) return selected;
    }
}
static bool confirm(const char *title) {const char *items[]={"Keep editing","Discard changes"};return menu(title,"Unsaved work will be lost",items,2,0)==1;}
static void save_profile(void) {busy("SAVING");if(!storage_save_profile(&profile)) {nd_storage_ok&=(uint8_t)~1;show_message("SAVE FAILED",storage_error());}else nd_storage_ok|=1;}
static void rename_level(void) {
    static const char alphabet[]=" ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-";
    char draft[LEVEL_NAME];unsigned cursor=0;bool redraw=true;
    memset(draft,' ',LEVEL_NAME-1);draft[LEVEL_NAME-1]=0;
    memcpy(draft,level.name,strlen(level.name));release_keys();
    for(;;) {
        if(redraw) {
            render_title("LEVEL NAME","LEFT/RIGHT position - UP/DOWN letter");
            render_text(20,114,C_WHITE,draft);gfx_SetColor(C_CYAN);gfx_HorizLine(20+cursor*8,128,7);
            render_text(20,188,C_CYAN,"ENTER save / CLEAR cancel");gfx_SwapDraw();redraw=false;
        }
        scan();
        if(pressed&K_BACK) return;
        if(pressed&K_LEFT) {cursor=(cursor+LEVEL_NAME-2)%(LEVEL_NAME-1);redraw=true;}
        if(pressed&K_RIGHT) {cursor=(cursor+1)%(LEVEL_NAME-1);redraw=true;}
        if(pressed&(K_UP|K_DOWN)) {
            const char *p=strchr(alphabet,draft[cursor]);int n=p?(int)(p-alphabet):0;
            n=(n+((pressed&K_UP)?1:(int)sizeof(alphabet)-2))%((int)sizeof(alphabet)-1);
            draft[cursor]=alphabet[n];redraw=true;
        }
        if(pressed&K_ENTER) {
            int n=LEVEL_NAME-2;while(n>0&&draft[n]==' ') draft[n--]=0;
            memcpy(level.name,draft,LEVEL_NAME);editor.dirty=true;return;
        }
    }
}
static void record_progress(int campaign,bool practice) {
    if(campaign>=0&&!practice) {
        uint8_t p=game_progress(&game,&level);
        if(p>profile.best[campaign]) profile.best[campaign]=p;
        if(game.won) profile.completed[campaign]=1;
    }
}
/* campaign=-1 for custom; endless sections reuse the same working Level.
   Editor playtests never write progress or mutate their source level. */
static void play(int campaign,bool practice,bool endless,bool playtest) {
    bool has_checkpoint=false;uint32_t attempts=1,distance=0,section=0;
    bool queued_action=false;
    uint32_t accumulator=0;clock_t last=clock(),last_draw=last;
    char label[64];unsigned death_ticks=0;
    if(endless) endless_load(0x4e454f4e,0,&level);
    game_start(&game,&level);release_keys();last=clock();nd_screen=3;
    nd_frames=nd_ticks=nd_max_frame_clocks=0;
    if(campaign>=0) profile.attempts[campaign]++;
    for(;;) {
        clock_t now=clock();uint32_t delta=(uint32_t)(now-last);last=now;
        if(delta>CLOCKS_PER_SEC/4) delta=CLOCKS_PER_SEC/4;
        accumulator+=delta*60;
        scan();
        if(pressed&K_ACTION) queued_action=true;
        if(pressed&K_BACK) {
            const char *items[]={"Resume","Restart","Exit to menu"};
            int choice=menu("PAUSED",practice?"Practice - records disabled":"Run paused",items,3,0);
            if(choice==2) {record_progress(campaign,practice);break;}
            if(choice==1) {has_checkpoint=false;game_start(&game,&level);attempts++;if(campaign>=0)profile.attempts[campaign]++;}
            release_keys();last=clock();last_draw=last;accumulator=0;queued_action=false;nd_screen=3;
        }
        if(pressed&K_DEL) {
            record_progress(campaign,practice);
            if(has_checkpoint) game=checkpoint;else game_start(&game,&level);
            death_ticks=0;attempts++;if(campaign>=0)profile.attempts[campaign]++;
        }
        if(practice && !game.dead && !game.won && (pressed&K_ALPHA)) {checkpoint=game;has_checkpoint=true;}
        if(practice && (pressed&K_MODE)) has_checkpoint=false;
        /* Catch up simulation without discarding ticks when drawing is slow. */
        for(unsigned n=0;accumulator>=CLOCKS_PER_SEC&&n<8;n++) {
            accumulator-=CLOCKS_PER_SEC;nd_ticks++;
            if(game.dead) {
                if(!death_ticks) record_progress(campaign,practice);
                if(++death_ticks>=12) {
                    if(endless) {uint32_t score=distance+(uint32_t)(game.x/FP_ONE)/16;if(score>profile.endless_best)profile.endless_best=score;distance=0;section=0;endless_load(0x4e454f4e,0,&level);}
                    if(has_checkpoint) game=checkpoint;else game_start(&game,&level);
                    attempts++;if(campaign>=0)profile.attempts[campaign]++;death_ticks=0;
                }
            } else {game_step(&game,&level,(GameInput){(keys&K_ACTION)!=0||queued_action});queued_action=false;}
        }
        if(game.won) {
            if(endless) {
                distance+=(uint32_t)(game.x/FP_ONE)/16;section++;
                if(distance>profile.endless_best)profile.endless_best=distance;
                endless_load(0x4e454f4eUL + section*7919,section,&level);game_start(&game,&level);
            } else {
                record_progress(campaign,practice);
                snprintf(label,sizeof(label),"%lu attempts%s",(unsigned long)attempts,practice?" - practice clear":" - complete!");
                show_message(playtest?"TEST COMPLETE":"LEVEL CLEAR",label);break;
            }
        }
        if((uint32_t)(now-last_draw)>=CLOCKS_PER_SEC/30) {
            clock_t began=clock();last_draw+=CLOCKS_PER_SEC/30;
            render_world(&level,&game,NULL,render_player_color(profile.color),!profile.reduced_effects);
            if(game.dead) render_text(112,112,C_RED,"RETRYING");
            if(practice) render_text(4,228,C_YELLOW,has_checkpoint?"PRACTICE  checkpoint set  MODE clears":"PRACTICE  ALPHA sets checkpoint");
            else if(endless) {snprintf(label,sizeof(label),"ENDLESS %lu  BEST %lu",(unsigned long)(distance+game.x/FP_ONE/16),(unsigned long)profile.endless_best);render_text(4,228,C_WHITE,label);}
            else {snprintf(label,sizeof(label),"ATTEMPT %lu   CLEAR pause / DEL retry",(unsigned long)attempts);render_text(4,228,C_WHITE,label);}
            gfx_SwapDraw();nd_frames++;
            uint32_t elapsed=(uint32_t)(clock()-began);if(elapsed>nd_max_frame_clocks)nd_max_frame_clocks=elapsed;
        }
    }
    if(!playtest) save_profile();
}
static void edit_level(unsigned slot) {
    char error[64];bool redraw=true;clock_t repeat=0;editor_init(&editor);release_keys();
    for(;;) {
        if(redraw){nd_screen=5;render_world(&level,NULL,&editor,C_CYAN,false);gfx_SwapDraw();redraw=false;}
        scan();
        uint32_t move=pressed;
        if(keys&(K_UP|K_DOWN|K_LEFT|K_RIGHT)) {
            if((uint32_t)(clock()-repeat)>CLOCKS_PER_SEC/8){move|=keys;repeat=clock();}
        } else repeat=clock();
        if(move&K_LEFT){editor_move(&editor,-1,0,&level);redraw=true;}
        if(move&K_RIGHT){editor_move(&editor,1,0,&level);redraw=true;}
        if(move&K_UP){editor_move(&editor,0,-1,&level);redraw=true;}
        if(move&K_DOWN){editor_move(&editor,0,1,&level);redraw=true;}
        /* Up is action during play, but only 2nd/Enter places in the editor. */
        if((pressed&K_ENTER)||((pressed&K_ACTION)&&!(keys&K_UP))) {if(!editor_place(&editor,&level))show_message("CANNOT PLACE","Object limit or invalid placement");redraw=true;}
        if(pressed&K_DEL){uint8_t tool=editor.tool;editor.tool=0;editor_place(&editor,&level);editor.tool=tool;redraw=true;}
        if(pressed&K_MODE){editor_cycle_tool(&editor,1);redraw=true;}
        if(pressed&K_ALPHA){editor_cycle_value(&editor);redraw=true;}
        if(pressed&K_UNDO){editor_undo(&editor,&level);redraw=true;}
        if(pressed&K_NAME){rename_level();redraw=true;release_keys();}
        if(pressed&K_SAVE){
            busy("SAVING");
            if(!level_validate(&level,error,sizeof(error)))show_message("INVALID LEVEL",error);
            else if(storage_save_level(slot,&level)){nd_storage_ok|=2;editor.dirty=false;show_message("SAVED",level.name);}
            else show_message("SAVE FAILED",storage_error());
            redraw=true;
        }
        if(pressed&K_EXPORT){
            busy("EXPORTING");
            if(storage_export_level(slot,&level)){nd_storage_ok|=4;show_message("EXPORTED","Transfer NDX00..NDX09 via TI Connect");}
            else show_message("EXPORT FAILED",storage_error());
            redraw=true;
        }
        if(pressed&K_IMPORT){
            if(!editor.dirty||confirm("IMPORT LEVEL?")) {
                busy("IMPORTING");
                if(storage_import_level(slot,&level)){nd_storage_ok|=8;editor_init(&editor);show_message("IMPORTED","Level saved to this custom slot");}
                else show_message("IMPORT FAILED",storage_error());
            }
            redraw=true;
        }
        if(pressed&K_TEST){
            if(level_validate(&level,error,sizeof(error)))play(-1,false,false,true);else show_message("INVALID LEVEL",error);
            redraw=true;release_keys();
        }
        if(pressed&K_BACK){if(!editor.dirty||confirm("LEAVE EDITOR?"))return;redraw=true;release_keys();}
    }
}
static void campaign_menu(void) {
    static char names[10][48];const char *items[10];int selected=0;
    for(;;) {
        for(unsigned i=0;i<10;i++) {campaign_load(i,&level);snprintf(names[i],sizeof(names[i]),"%02u %s %s%u%%",i+1,level.name,i&&!profile.completed[i-1]?"LOCK ":"",profile.best[i]);items[i]=names[i];}
        selected=menu("CAMPAIGN","Ten routes. Five ways to move.",items,10,selected);
        if(selected<0)return;
        if(selected&&!profile.completed[selected-1]) {show_message("LOCKED","Complete the previous level first");continue;}
        const char *modes[]={"Normal - earn completion","Practice - manual checkpoints"};
        int choice=menu("PLAY MODE","ALPHA checkpoint / MODE remove",modes,2,0);
        if(choice>=0){campaign_load((unsigned)selected,&level);play(selected,choice==1,false,false);}
    }
}
static void custom_menu(void) {
    static char names[10][40];const char *items[10];int selected=0;
    for(;;) {
        busy("LOADING");
        for(unsigned i=0;i<10;i++){bool ok=storage_load_level(i,&level);snprintf(names[i],sizeof(names[i]),"%02u %s",i+1,ok?level.name:"New level");items[i]=names[i];}
        selected=menu("CREATE","Ten independent custom slots",items,10,selected);
        if(selected<0)return;
        if(!storage_load_level((unsigned)selected,&level))level_blank(&level,"UNTITLED",512);
        const char *actions[]={"Edit level","Play level","Practice level"};
        int action=menu("CUSTOM LEVEL",level.name,actions,3,0);
        if(action==0)edit_level((unsigned)selected);
        else if(action>0)play(-1,action==2,false,false);
    }
}
static void settings(void) {
    int selected=0;
    for(;;){
        char effects[40],color[40];unsigned unlocked=1;
        for(unsigned i=0;i<10;i++)if(profile.completed[i])unlocked++;
        snprintf(effects,sizeof(effects),"Effects: %s",profile.reduced_effects?"reduced":"full");
        snprintf(color,sizeof(color),"Color: %u / %u unlocked",profile.color+1,unlocked);
        const char *items[]={effects,color,"Back"};
        selected=menu("SETTINGS","Colors unlock through campaign clears",items,3,selected);
        if(selected<0||selected==2){save_profile();return;}
        if(selected==0)profile.reduced_effects=!profile.reduced_effects;
        if(selected==1)profile.color=(profile.color+1)%unlocked;
    }
}
static void help(void) {
    const char *pages[]={
        "2nd/UP act. CLEAR pause. DEL retry.",
        "Core: hold to jump again on landing.",
        "Glider: hold rise; release fall.",
        "Blink: press to teleport to ceiling.",
        "Tumbler: press on a surface to flip.",
        "Beam: hold up / release down.",
        "Yellow pads bounce; press near orbs.",
        "Portals change mode, speed or gravity.",
        "Practice: ALPHA checkpoint, MODE clear.",
        "Editor: MODE tool, ALPHA tool value.",
        "2nd place, DEL erase, MATH undo.",
        "Y= save, WINDOW name, GRAPH playtest.",
        "ZOOM export, TRACE import NDX slot.",
        "Original game/code/art. Casio idea: fXXa."
    };
    for(unsigned i=0;i<sizeof(pages)/sizeof(pages[0]);i++)show_message("HOW TO PLAY",pages[i]);
}
int main(void) {
    const char *items[]={"Campaign","Endless","Create / custom levels","Settings","How to play / credits","Exit"};int selected=0;
    storage_load_profile(&profile);render_begin();ti_SetGCBehavior(gfx_End,render_begin);
    for(;;){
        selected=menu("PHASEBOUND","Shift form. Defy gravity.",items,6,selected);
        if(selected<0||selected==5)break;
        if(selected==0)campaign_menu();
        else if(selected==1)play(-1,false,true,false);
        else if(selected==2)custom_menu();
        else if(selected==3)settings();
        else if(selected==4)help();
    }
    save_profile();ti_SetGCBehavior(NULL,NULL);gfx_End();return 0;
}
