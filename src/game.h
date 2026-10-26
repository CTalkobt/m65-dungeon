/* game.h — the state of the game: the level, the player, the creatures
 *
 * Strings in the game are ASCII (video.c shows them).
 */
#pragma once
#pragma encoding ascii
#include "assets.h"

#define MAP_W      64
#define MAP_H      48
#define MAP_N      (MAP_W * MAP_H)
#define AT(x, y)   ((unsigned int)(y) * MAP_W + (x))
#define DEPTHS     5

/* memory for what is only ever in use in one place at a time: the map's
 * drawing (view.c), or a close-up's log (ui.c) and its creature's words
 * (battle.c) */
#define SCRATCH_SIZE 1045
extern unsigned char scratch[SCRATCH_SIZE];

/* the level's map: each place's terrain tile (assets.h TK_*) drawn once
 * as 2 x 2 16-colour characters, 128 x 96 of them at MAP_LIN (vmap.s45),
 * the way the screen shows them; the view copies the part in sight to the
 * screen (view.c). What a place is, is read back from its character. */
unsigned char vm_kind(unsigned char x, unsigned char y);
void vm_set(unsigned char x, unsigned char y, unsigned char kind);
#define IS_WALL(k)  ((k) >= TK_WALL0 && (k) <= TK_WALL15)

/* what a place is (terrain): walls are rock */
#define TR_ROCK    0
#define TR_FLOOR   1
#define TR_CORR    2
#define TR_DOOR    3          /* shut */
#define TR_OPEN    4          /* an open door */
#define TR_STAIRS  5
#define TR_LOCKED  6          /* shut, and an iron key's to open */
#define SOLID(t)   ((t) == TR_ROCK || (t) == TR_DOOR || (t) == TR_LOCKED)   /* nothing seen through */
unsigned char terrain(unsigned char x, unsigned char y);

/* the rooms with a brazier (with their walls): while it burns, the room
 * is lit, seen from afar; a cold one (on 0) is lit by the player's torch */
#define MAX_LIT    14
unsigned char lit_at(unsigned char x, unsigned char y);
void lit_add(unsigned char x0, unsigned char y0, unsigned char x1, unsigned char y1, unsigned char on);
void lit_light(unsigned char x, unsigned char y);   /* the room round (x, y) lit from now on */
extern unsigned char lit_n;

/* the vault: a room behind locked doors, its chests the richer (none: x1 0) */
void vault_set(unsigned char x0, unsigned char y0, unsigned char x1, unsigned char y1);
unsigned char vault_at(unsigned char x, unsigned char y);

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
#define O_SCROLL      9       /* which, when it is picked up */
#define O_KEY         10
#define O_DRAUGHT     11
#define O_RAGS        12      /* O_RAGS + n: the fuel, as I_RAGS + n */
#define O_STICKS      13
#define O_FUR         14
#define O_CANDLE      15
#define O_FLINT       16
#define O_FIRESTICK   17
#define O_ARROWS      18      /* a few arrows */
#define O_SHOT        19      /* a few cartridges */
#define O_TRAP        20      /* a pressure plate, hidden (found: the tile TK_TRAP) */
#define O_BRAZIER_COLD 21     /* a brazier not burning: the torch lights it */
#define O_PILE        32      /* O_PILE + I_*: so many (thing_n) of a thing carried, put down */
#define MAX_OBJ       56
extern unsigned int thing_at[MAX_OBJ];       /* where (AT) */
extern unsigned char thing_kind[MAX_OBJ];    /* O_NONE: a free entry */
extern unsigned char thing_n[MAX_OBJ];       /* a pile's count */
unsigned char obj_at(unsigned int at);
void obj_set(unsigned int at, unsigned char kind);
unsigned char obj_add(unsigned int at, unsigned char kind, unsigned char n);   /* a pile; 0: no room */
unsigned char obj_count(unsigned int at);
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
/* trap.c: what is hidden, found and sprung */
void memory_renew(void);                       /* all remembered as if just seen */
void level_reveal(void);                       /* the walls, doors, stairs remembered; the hidden found */
unsigned char search(unsigned char odds);      /* the places round the player: each hidden thing 1 in odds */
unsigned char trap_spring(void);               /* the hidden trap underfoot: DIED if it killed */
/* this turn, in a window round the player: 0 out of sight, 1-4 lit
 * (bright to dim), 5 in shadow */
#define VIS_SHADOW 5
unsigned char vis_at(unsigned char x, unsigned char y);

/* the creatures: M_*, A_* (what they do in a fight), MF_* and the tables
 * mon_tile, mon_scene, mon_flags, level_mons come from tools/monsters.py
 * through assets.h; the rest of what a creature is is in its close-up's
 * header, read when a fight starts (battle.c) */
#define FRESH      0xFF       /* mon.hp of a creature not yet fought: its full life */

#define MAX_MON    24
struct mon {
    unsigned char x, y, kind, hp;
    unsigned char awake;      /* 0 asleep, 1 wandering, 2 hunting */
    unsigned char stun;       /* turns it will not move (after the player fled) */
    unsigned char carry;      /* what it ran off with: I_* + 1, 0 nothing */
};
extern struct mon mons[MAX_MON];
extern unsigned char nmons;


/* what the player carries: so many of each */
#define I_POTION   0          /* healing */
#define I_DRAUGHT  1          /* mana */
#define I_FIRE     2          /* scroll of fire: for a fight */
#define I_RECALL   3          /* scroll of recall: what was seen, fresh again */
#define I_SIGHT    4          /* scroll of far sight: the level's shape */
#define I_KEY      5          /* iron key: opens a locked door */
#define I_RAGS     6          /* the torch's fuel: rags, sticks, fur, tallow */
#define I_STICKS   7
#define I_FUR      8
#define I_CANDLE   9
#define I_FLINT    10         /* lights the torch, now and then; kept */
#define I_FIRESTICK 11        /* lights the torch, once */
#define I_ARROWS   12         /* the bow's */
#define I_SHOT     13         /* the flintlock's: paper cartridges, powder and ball */
#define I_COUNT    14
const char *item_name(unsigned char i, unsigned char n);   /* game.c: as for n of them */
extern const unsigned char item_col[I_COUNT];               /* game.c: its colour in text */
extern const unsigned char item_tile[I_COUNT];              /* game.c: its tile, put down (T_*) */
extern unsigned char arrows_spent;    /* arrows shot in this fight (battle.c; explore.c gathers them) */
/* pack.c: the things carried, used on the map; 1 if that took the turn */
unsigned char inventory(void);
unsigned char drink(void);
unsigned char feed_any(void);
unsigned char light_any(void);
unsigned char light(unsigned char k);   /* I_FLINT, I_FIRESTICK, I_COUNT: a brazier's coals */
unsigned char shoot(unsigned char gun); /* the bow (0) or the flintlock (1) at a creature in sight */
void pick_up(unsigned int i, unsigned char n);   /* what lies at i (O_*), as the player walks over it */
unsigned char foe_shoots(unsigned char k);       /* creature k's arrow at the player: 1 if it killed */
unsigned char steal(unsigned char k);            /* creature k snatches something: 1 if it did */
void drop_carry(unsigned char k);                /* what creature k ran off with, where it lies */
/* pack.c: n of item put down at (x, y) or beside it; 0 if there is no room */
unsigned char put_down(unsigned char x, unsigned char y, unsigned char item, unsigned char n);

/* the weapons and the armour, worst to best: their names, what they do */
#define WEAPONS    5
#define ARMOURS    5
const char *weapon_name(unsigned char w);
const char *armour_name(unsigned char a);
void weapon_take(unsigned char w);
void armour_take(unsigned char a);

/* the weapons that reach: a bow (pl.bow: 0 none, 1 short bow, 2 longbow)
 * and a flintlock (pl.gun: 0 none, 1 pistol, 2 musket); r = gun * 2 + w - 1 */
#define RANGED_BEST 2
const char *ranged_name(unsigned char gun, unsigned char w);
extern const unsigned char r_dice[4], r_sides[4], r_plus[4], r_reach[4];

/* the player */
struct player {
    unsigned char x, y;
    unsigned char hp, maxhp, mp, maxmp;
    unsigned char level, wdice, wsides, wplus, def;
    unsigned char items[I_COUNT];
    unsigned int gold, xp;
    unsigned char depth;
    unsigned char fade;       /* how fast memories fade: 1 slow, 2 normal, 4 fast */
    unsigned char weapon, armour;   /* 0 .. WEAPONS - 1, ARMOURS - 1 */
    unsigned char bow, gun;         /* 0 none .. RANGED_BEST */
    unsigned int fuel;        /* the torch: turns of burning left (TORCH_MAX at most) */
    unsigned char lit;        /* 1 while it burns */
    unsigned char ups;        /* levels gained whose gift is yet to be chosen (pack.c level_up) */
    unsigned char keen, steady, aim, wary;   /* the gifts chosen, 0-3 each (GIFT_MAX) */
    unsigned int kills;       /* creatures slain */
    unsigned char cause;      /* what ended the descent (CAUSE_*) */
};
#define CAUSE_QUIT  1         /* given up */
#define CAUSE_DART  2         /* a trap's dart */
#define CAUSE_SLAIN 16        /* + the creature's kind: in a fight */
#define CAUSE_SHOT  32        /* + the archer's kind */
#define GIFT_MAX   3
#define TORCH_MAX  600
#define TORCH_LOW  150        /* below: it burns lower (the light shorter) */
#define TORCH_GUTTER 60       /* below: it gutters (shorter still) */
#define TORCH_OUT  0xFF       /* torch_dim: no light of one's own */
unsigned char torch_dim(void);        /* view.c: how much shorter the light is: 0, 1, 2, TORCH_OUT */
extern struct player pl;
extern unsigned int turn;
const char *depth_name(unsigned char depth);   /* game.c */

/* the outcome of a fight */
#define WON    1
#define FLED   2
#define DIED   3

/* dungeon.c */
void dungeon_make(void);              /* the level for pl.depth; the player placed */
unsigned char mon_at(unsigned char x, unsigned char y);   /* its index + 1, or 0 */

/* view.c: what the player sees and remembers; the screen of the map (msg,
 * msg_clear, msg_show: ui.c, resident) */
void view_fov(void);
void view_draw(void);
void view_hud(void);
void view_screen(void);               /* the whole exploration screen anew */
unsigned char view_line(unsigned char x, unsigned char y);   /* a clear line from the player */
extern unsigned char cam_x, cam_y;    /* the map's place at the view's top left */
extern unsigned char view_kept;       /* pack.c: a place kept under a tile (view_draw: 0) */
void msg(const char *s, unsigned char colour);
void msg_clear(void);
void msg_show(void);                  /* the messages drawn again */

/* explore.c */
unsigned char explore(unsigned char resume);   /* until DIED, WON or SAVED; resume: a saved game, loaded */

/* keep.c: the game on the disk (DGSAVE) */
#define SAVED  4              /* explore: the player saved and stopped */
unsigned char game_save(void);        /* 1 if it was written */
unsigned char game_load(void);        /* 1 if there was one, and it is back */
unsigned char game_peek(void);        /* 1 if there is one */
void game_unsave(void);               /* the journey is over: no more of it */

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
extern unsigned char vol_music, vol_sfx;         /* 0 (off) - 15 */
void sound_levels(void);                         /* vol_music to the SIDs */
void settings(unsigned char col, unsigned char row);   /* settings.c: the panel there (44 x 10) */
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
unsigned char gain_xp(unsigned int xp);   /* the levels gained (each a gift to choose: pl.ups) */
void level_up(void);                       /* pack.c: the gifts chosen, over the map */
