/* game.c — the creatures, the player, the names of things */
#include "hw.h"
#include "game.h"
#include "assets.h"

struct player pl;
unsigned int turn;
struct mon mons[MAX_MON];
unsigned char nmons;

/* the things lying about: a short list, the map flagging their places */
unsigned int thing_at[MAX_OBJ];
unsigned char thing_kind[MAX_OBJ];

void obj_clear(void) {
    unsigned char k;
    for (k = 0; k < MAX_OBJ; k++) thing_kind[k] = O_NONE;
}

unsigned char obj_at(unsigned int at) {
    unsigned char k;
    for (k = 0; k < MAX_OBJ; k++)
        if (thing_kind[k] && thing_at[k] == at) return thing_kind[k];
    return O_NONE;
}

void obj_set(unsigned int at, unsigned char kind) {
    unsigned char k, f = 0xFF;
    for (k = 0; k < MAX_OBJ; k++) {
        if (thing_kind[k] && thing_at[k] == at) break;
        if (!thing_kind[k] && f == 0xFF) f = k;
    }
    if (k == MAX_OBJ) {
        if (!kind || f == 0xFF) return;
        k = f;
    }
    thing_at[k] = at;
    thing_kind[k] = kind;
}

/* the terrain from the tile kind: TK_ROCK, FLOOR0-3, CORR0-1, WALL0-15,
 * DOOR_H/V, DOOR_OPEN_H/V, STAIRS, DOOR_LOCKED_H/V (gen_art.py's order) */
unsigned char terrain(unsigned char x, unsigned char y) {
    unsigned char k = vm_kind(x, y);
    if (k < TK_FLOOR0) return TR_ROCK;
    if (k < TK_CORR0) return TR_FLOOR;
    if (k < TK_WALL0) return TR_CORR;
    if (k < TK_DOOR_H) return TR_ROCK;
    if (k < TK_DOOR_OPEN_H) return TR_DOOR;
    if (k < TK_STAIRS) return TR_OPEN;
    if (k == TK_STAIRS) return TR_STAIRS;
    return TR_LOCKED;
}

static unsigned char lit_x0[MAX_LIT], lit_y0[MAX_LIT], lit_x1[MAX_LIT], lit_y1[MAX_LIT];
unsigned char lit_n;

void lit_add(unsigned char x0, unsigned char y0, unsigned char x1, unsigned char y1) {
    if (lit_n >= MAX_LIT) return;
    lit_x0[lit_n] = x0;
    lit_y0[lit_n] = y0;
    lit_x1[lit_n] = x1;
    lit_y1[lit_n] = y1;
    lit_n++;
}

/* the vault: a room behind locked doors (none: x1 0) */
static unsigned char vault_x0, vault_y0, vault_x1, vault_y1;

void vault_set(unsigned char x0, unsigned char y0, unsigned char x1, unsigned char y1) {
    vault_x0 = x0;
    vault_y0 = y0;
    vault_x1 = x1;
    vault_y1 = y1;
}

unsigned char vault_at(unsigned char x, unsigned char y) {
    return x >= vault_x0 && x <= vault_x1 && y >= vault_y0 && y <= vault_y1;
}

/* every place remembered, as if just seen (an idol, a scroll) */
void memory_renew(void) {
    unsigned int i;
    unsigned char now = epoch();
    for (i = 0; i < MAP_N; i++)
        if (seen_get(i)) seen_set(i, now);
}

/* the level's walls, doors and stairs remembered, seen or not */
void level_reveal(void) {
    unsigned char x, y, k, now = epoch();
    for (y = 0; y < MAP_H; y++)
        for (x = 0; x < MAP_W; x++) {
            k = vm_kind(x, y);
            if (IS_WALL(k) || k >= TK_DOOR_H) seen_set(AT(x, y), now);   /* the doors, the stairs */
        }
}

unsigned char lit_at(unsigned char x, unsigned char y) {
    unsigned char k;
    for (k = 0; k < lit_n; k++)
        if (x >= lit_x0[k] && x <= lit_x1[k] && y >= lit_y0[k] && y <= lit_y1[k]) return 1;
    return 0;
}

unsigned char mon_at(unsigned char x, unsigned char y) {
    unsigned char i;
    for (i = 0; i < nmons; i++)
        if (mons[i].hp && mons[i].x == x && mons[i].y == y) return i + 1;
    return 0;
}

/* epochs: 4 turns each, 1-255 round and round */
unsigned char epoch(void) {
    return (unsigned char)((turn >> 2) % 255 + 1);
}

unsigned char epoch_age(unsigned char e) {
    unsigned char now = epoch();
    return now >= e ? now - e : (unsigned char)(255 - e + now);
}

/* every 64 turns: the memories older than the slowest fading keeps (225
 * epochs, 900 turns) are gone for good, before their epochs come round */
void memory_sweep(void) {
    unsigned int i;
    unsigned char e;
    for (i = 0; i < MAP_N; i++)
        if ((e = seen_get(i)) != 0 && epoch_age(e) >= 225) seen_set(i, 0);
}



/* each item's colour in lists and messages */
const unsigned char item_col[I_COUNT] = {
    C_RED, C_BLUE, C_BONE, C_BONE, C_BONE, C_GOLD, C_ORANGE, C_ORANGE, C_ORANGE, C_ORANGE, C_GREY, C_ORANGE,
};

/* the things one carries, one and many */
const char *item_name(unsigned char i, unsigned char n) {
    return nth("healing potion\0healing potions\0mana draught\0mana draughts\0"
               "scroll of fire\0scrolls of fire\0scroll of recall\0scrolls of recall\0"
               "scroll of far sight\0scrolls of far sight\0iron key\0iron keys\0"
               "twist of rags\0twists of rags\0bundle of sticks\0bundles of sticks\0"
               "scrap of fur\0scraps of fur\0tallow candle\0tallow candles\0"
               "flint\0flints\0firestick\0firesticks", i * 2 + (n != 1));
}

/* the weapons: dice, sides, plus */
static const unsigned char w_dice[WEAPONS] = {1, 1, 1, 1, 2};
static const unsigned char w_sides[WEAPONS] = {8, 8, 10, 10, 10};
static const unsigned char w_plus[WEAPONS] = {0, 2, 4, 6, 8};

const char *weapon_name(unsigned char w) {
    return nth("short sword\0long sword\0war axe\0rune blade\0dragonbone sword", w);
}

const char *armour_name(unsigned char a) {
    return nth("leather jerkin\0studded leather\0mail shirt\0plate\0rune plate", a);
}

void weapon_take(unsigned char w) {
    pl.weapon = w;
    pl.wdice = w_dice[w];
    pl.wsides = w_sides[w];
    pl.wplus = w_plus[w];
}

void armour_take(unsigned char a) {
    pl.armour = a;
    pl.def = a + 1;
}

void player_new(void) {
    unsigned char k;
    pl.hp = pl.maxhp = 30;
    pl.mp = pl.maxmp = 12;
    pl.level = 1;
    for (k = 0; k < I_COUNT; k++) pl.items[k] = 0;
    pl.items[I_POTION] = 2;
    pl.items[I_STICKS] = 1;
    pl.items[I_FIRESTICK] = 2;
    pl.fuel = 400;
    pl.lit = 1;
    pl.gold = 0;
    pl.xp = 0;
    pl.depth = 1;
    pl.fade = 2;
    weapon_take(0);
    armour_take(0);
    turn = 1;
}

/* the experience to reach each level after the first */
static const unsigned int next_level[] = {20, 50, 100, 170, 260, 380, 530, 700, 900, 65535};

unsigned char gain_xp(unsigned int xp) {
    unsigned char up = 0;
    pl.xp += xp;
    while (pl.level < 10 && pl.xp >= next_level[pl.level - 1]) {
        pl.level++;
        pl.maxhp += 7;
        pl.maxmp += 3;
        pl.hp = pl.maxhp;
        pl.mp = pl.maxmp;
        up++;
    }
    return up;
}
