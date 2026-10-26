/* game.h — the state of the game: the level, the player, the creatures
 *
 * Strings in the game are ASCII (video.c shows them).
 */
#pragma once
#pragma encoding ascii

#define MAP_W      64
#define MAP_H      48
#define MAP_N      (MAP_W * MAP_H)
#define AT(x, y)   ((unsigned int)(y) * MAP_W + (x))
#define DEPTHS     5

/* map[]: the terrain in the low bits, flags above */
#define TR_ROCK    0
#define TR_FLOOR   1
#define TR_CORR    2
#define TR_DOOR    3          /* shut */
#define TR_OPEN    4          /* an open door */
#define TR_STAIRS  5
#define TR_MASK    0x0F
#define F_OBJ      0x10       /* something lies here (obj_at) */
#define F_BOUND    0x20       /* rock next to a way through (a wall: drawn, remembered) */
#define F_LIT      0x40       /* in a room with a brazier: seen from afar */
#define F_ROOM     0x80       /* in a room (not a corridor) */
extern unsigned char map[MAP_N];
#define TERRAIN(i) (map[i] & TR_MASK)
#define PASSABLE(t) ((t) != TR_ROCK && (t) != TR_DOOR)

/* obj[]: what lies there */
#define O_NONE        0
#define O_CHEST       1
#define O_CHEST_OPEN  2
#define O_IDOL        3
#define O_IDOL_SPENT  4
#define O_POTION      5
#define O_GOLD        6
#define O_BONES       7
#define O_BRAZIER     8
#define MAX_OBJ       40
unsigned char obj_at(unsigned int at);
void obj_set(unsigned int at, unsigned char kind);
void obj_clear(void);

/* the memory: when a place was last in sight and lit, in epochs of 4
 * turns (1-255 round and round; 0 never, or long forgotten); a byte a
 * place in colour RAM past the screen (seen.s45) */
#define SEEN_LIN 0xFF82000UL
unsigned char seen_get(unsigned int i);
void seen_set(unsigned int i, unsigned char v);
unsigned char epoch(void);                     /* now */
unsigned char epoch_age(unsigned char e);      /* epochs since e */
void memory_sweep(void);                       /* the long forgotten to 0 */
/* this turn, in a window round the player: 0 out of sight, 1-4 lit
 * (bright to dim), 5 in shadow */
#define VIS_SHADOW 5
unsigned char vis_at(unsigned char x, unsigned char y);

/* the creatures: M_*, A_* (what they do in a fight), MF_* and the tables
 * mon_tile, mon_scene, mon_flags, level_mons come from tools/monsters.py
 * through assets.h; the rest of what a creature is is in its close-up's
 * header, read when a fight starts (battle.c) */
#include "assets.h"
#define FRESH      0xFF       /* mon.hp of a creature not yet fought: its full life */

#define MAX_MON    24
struct mon {
    unsigned char x, y, kind, hp;
    unsigned char awake;      /* 0 asleep, 1 wandering, 2 hunting */
    unsigned char stun;       /* turns it will not move (after the player fled) */
};
extern struct mon mons[MAX_MON];
extern unsigned char nmons;


/* the player */
struct player {
    unsigned char x, y;
    unsigned char hp, maxhp, mp, maxmp;
    unsigned char level, wdice, wsides, wplus, def;
    unsigned char potions;
    unsigned int gold, xp;
    unsigned char depth;
    unsigned char fade;       /* how fast memories fade: 1 slow, 2 normal, 4 fast */
    unsigned char weapon, armour;   /* names: game.c */
};
extern struct player pl;
extern unsigned int turn;
const char *depth_name(unsigned char depth);   /* view.c */

/* the outcome of a fight */
#define WON    1
#define FLED   2
#define DIED   3

/* dungeon.c */
void dungeon_make(void);              /* the level for pl.depth; the player placed */
unsigned char mon_at(unsigned char x, unsigned char y);   /* its index + 1, or 0 */

/* view.c: what the player sees and remembers; the screen of the map */
void view_fov(void);
void view_draw(void);
void view_hud(void);
void view_screen(void);               /* the whole exploration screen anew */
void msg(const char *s, unsigned char colour);
void msg_clear(void);

/* explore.c */
unsigned char explore(void);          /* until the player dies (DIED) or wins (WON) */

/* battle.c */
unsigned char battle(unsigned char m, unsigned char ambush);

/* detail.c */
void chest_view(unsigned int at);
void idol_view(unsigned int at);
unsigned char scene_load(unsigned char s);
void unpack(unsigned long src);       /* a packed file (tools/pack.py) where it says */
void scene_show(unsigned char frame);
void scene_frame(void);

/* ui.c: keys, frames, bars, menus */
#define K_UP     1
#define K_DOWN   2
#define K_LEFT   3
#define K_RIGHT  4
#define K_FIRE   5
#define K_STOP   6
unsigned char key_get(void);          /* a key (ASCII lower case, or K_*), 0 none */
unsigned char key_wait(void);
void ui_frame(unsigned char col, unsigned char row, unsigned char w, unsigned char h);
void ui_hline(unsigned char col, unsigned char row, unsigned char w);
void ui_bar(unsigned char col, unsigned char row, unsigned char w, unsigned char v, unsigned char max, unsigned int bar0);
void ui_wrap(unsigned char col, unsigned char row, unsigned char w, const char *s, unsigned char colour);
void ui_stats(unsigned char col, unsigned char row);
void str_cat(char *d, const char *s);
const char *nth(const char *list, unsigned char n);   /* the nth of "a\0b\0c" */
void str_num(char *d, unsigned int v);
/* a short message to the log in the battle's and the views' text area */
void log_init(unsigned char col, unsigned char row, unsigned char w, unsigned char h);
void log_add(const char *s, unsigned char colour);

/* sfx.c */
void sfx_on(void);
void sfx_off(void);
void sfx_update(void);
#define SFX_STEP   0
#define SFX_HIT    1
#define SFX_HURT   2
#define SFX_CLANG  3
#define SFX_SPELL  4
#define SFX_CREAK  5
#define SFX_GROWL  6
#define SFX_COIN   7
#define SFX_DOOR   8
#define SFX_CHIME  9
#define SFX_DEATH  10
#define SFX_FROST  11
#define SFX_SWING  12
#define SFX_HEAVY  13
#define SFX_FIRE   14
#define SFX_HEAL   15
#define SFX_WARD   16
#define SFX_POISON 17
#define SFX_CURSE  18
#define SFX_DRINK  19
#define SFX_VICTORY 20
#define SFX_LEVEL  21
#define SFX_DEFEAT 22
#define SFX_STIR   23
#define SFX_BELL   24
#define SFX_TRAP   25
#define SFX_CHEST  26
#define SFX_COUNT  27
void sfx(unsigned char n);
void sfx_far(unsigned char n, signed char dx);   /* quieter, from that side (dx < 0 left) */

/* audio.c: the sampled sounds (sfx plays them where there is one) */
extern unsigned char audio_ok;
void audio_init(void);                /* decoded into attic RAM: at the start */
void audio_off(void);
void audio_play(unsigned char id, unsigned char left, unsigned char right);
void audio_start(unsigned char ch, unsigned long at, unsigned int len, unsigned char left, unsigned char right);
void audio_update(void);              /* once a frame */

/* front.c */
void title_show(void);                /* the title, before the sounds load */
void loading_tick(void);              /* now and then while they do */
unsigned char title(void);            /* 0: the player wants to leave */
void ending(unsigned char how);

/* game.c */
void player_new(void);
unsigned char gain_xp(unsigned int xp);   /* the levels gained */
