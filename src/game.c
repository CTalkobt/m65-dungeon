/* game.c — the creatures, the player, the names of things */
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
 * DOOR_H/V, DOOR_OPEN_H/V, STAIRS (gen_art.py's order) */
unsigned char terrain(unsigned char x, unsigned char y) {
    unsigned char k = vm_kind(x, y);
    if (k < TK_FLOOR0) return TR_ROCK;
    if (k < TK_CORR0) return TR_FLOOR;
    if (k < TK_WALL0) return TR_CORR;
    if (k < TK_DOOR_H) return TR_ROCK;
    if (k < TK_DOOR_OPEN_H) return TR_DOOR;
    if (k < TK_STAIRS) return TR_OPEN;
    return TR_STAIRS;
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



void player_new(void) {
    pl.hp = pl.maxhp = 30;
    pl.mp = pl.maxmp = 12;
    pl.level = 1;
    pl.wdice = 1;
    pl.wsides = 8;
    pl.wplus = 0;
    pl.def = 1;
    pl.potions = 2;
    pl.gold = 0;
    pl.xp = 0;
    pl.depth = 1;
    pl.fade = 2;
    pl.weapon = 0;
    pl.armour = 0;
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
