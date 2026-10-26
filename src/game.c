/* game.c — the creatures, the player, the names of things */
#include "game.h"
#include "assets.h"

struct player pl;
unsigned int turn;
unsigned char map[MAP_N];
struct mon mons[MAX_MON];
unsigned char nmons;

/* the things lying about: a short list, the map flagging their places */
static unsigned int thing_at[MAX_OBJ];
static unsigned char thing_kind[MAX_OBJ];

void obj_clear(void) {
    unsigned char k;
    for (k = 0; k < MAX_OBJ; k++) thing_kind[k] = O_NONE;
}

unsigned char obj_at(unsigned int at) {
    unsigned char k;
    if (!(map[at] & F_OBJ)) return O_NONE;
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
    if (kind) map[at] |= F_OBJ;
    else map[at] &= ~F_OBJ;
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
