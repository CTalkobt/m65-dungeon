/* dungeon.c — a level: rooms cut in the rock, corridors between them,
 * doors, braziers in some rooms (the rest are dark), creatures, chests, an
 * idol, the stairs down (on the last level the lich instead)
 */
#pragma cc45 overlay(gen)
#pragma cc45 noescape_strings
#include <dma.h>
#include "game.h"
#include "rng.h"

#define MAX_ROOMS 14
static unsigned char rx[MAX_ROOMS], ry[MAX_ROOMS], rw[MAX_ROOMS], rh[MAX_ROOMS];
static unsigned char nrooms;


static unsigned char overlaps(unsigned char x, unsigned char y, unsigned char w, unsigned char h) {
    unsigned char i;
    for (i = 0; i < nrooms; i++)
        if (x < rx[i] + rw[i] + 2 && rx[i] < x + w + 2 && y < ry[i] + rh[i] + 2 && ry[i] < y + h + 2) return 1;
    return 0;
}

static void carve(unsigned char x, unsigned char y) {
    unsigned int i = AT(x, y);
    if (TERRAIN(i) == TR_ROCK) map[i] = TR_CORR;
}

/* an L-shaped corridor between two rooms' middles */
static void corridor(unsigned char a, unsigned char b) {
    unsigned char x0 = rx[a] + rw[a] / 2, y0 = ry[a] + rh[a] / 2;
    unsigned char x1 = rx[b] + rw[b] / 2, y1 = ry[b] + rh[b] / 2;
    unsigned char x = x0, y = y0;
    if (rng_range(2)) {
        while (x != x1) { carve(x, y); x += x < x1 ? 1 : -1; }
        while (y != y1) { carve(x, y); y += y < y1 ? 1 : -1; }
    } else {
        while (y != y1) { carve(x, y); y += y < y1 ? 1 : -1; }
        while (x != x1) { carve(x, y); x += x < x1 ? 1 : -1; }
    }
}

/* a free floor cell in a room (nothing there, not the player's) */
static unsigned int spot(unsigned char r) {
    unsigned char k, x, y;
    unsigned int i;
    for (k = 0; k < 40; k++) {
        x = rx[r] + rng_range(rw[r]);
        y = ry[r] + rng_range(rh[r]);
        i = AT(x, y);
        if (TERRAIN(i) == TR_FLOOR && !(map[i] & F_OBJ) && !mon_at(x, y) && !(x == pl.x && y == pl.y)) return i;
    }
    return 0;
}

/* a door or a corridor among the eight round a cell */
static unsigned char by_way_in(unsigned int i) {
    unsigned char k, t;
    for (k = 0; k < 9; k++) {
        t = TERRAIN(i + (k / 3 - 1) * MAP_W + (k % 3) - 1);
        if (t == TR_DOOR || t == TR_OPEN || t == TR_CORR) return 1;
    }
    return 0;
}

/* spot gives 0 when it finds no room */
static void put(unsigned int at, unsigned char kind) {
    if (at) obj_set(at, kind);
}

static void add_mon(unsigned char kind, unsigned int at) {
    struct mon *m;
    if (!at || nmons >= MAX_MON) return;
    m = &mons[nmons++];
    m->x = (unsigned char)(at % MAP_W);
    m->y = (unsigned char)(at / MAP_W);
    m->kind = kind;
    m->hp = FRESH;
    m->awake = rng_range(3) == 0;
    m->stun = 0;
}

void dungeon_make(void) {
    unsigned int i;
    unsigned char k, t, x, y, w, h, r, last, d = pl.depth - 1;

    for (i = 0; i < MAP_N; i++) map[i] = TR_ROCK;
    dma_fill_far(SEEN_LIN, MAP_N, 0);
    obj_clear();
    nmons = 0;
    nrooms = 0;
    /* rooms: tries at random places, those that fit */
    for (k = 0; k < 120 && nrooms < MAX_ROOMS; k++) {
        w = 4 + rng_range(9);
        h = 3 + rng_range(6);
        x = 1 + rng_range(MAP_W - w - 2);
        y = 1 + rng_range(MAP_H - h - 2);
        if (overlaps(x, y, w, h)) continue;
        rx[nrooms] = x; ry[nrooms] = y; rw[nrooms] = w; rh[nrooms] = h;
        nrooms++;
    }
    /* order them left to right, so the corridors wander less */
    for (k = 1; k < nrooms; k++)
        for (r = k; r > 0 && rx[r - 1] > rx[r]; r--) {
            t = rx[r]; rx[r] = rx[r - 1]; rx[r - 1] = t;
            t = ry[r]; ry[r] = ry[r - 1]; ry[r - 1] = t;
            t = rw[r]; rw[r] = rw[r - 1]; rw[r - 1] = t;
            t = rh[r]; rh[r] = rh[r - 1]; rh[r - 1] = t;
        }
    for (r = 0; r < nrooms; r++)
        for (y = ry[r]; y < ry[r] + rh[r]; y++)
            for (x = rx[r]; x < rx[r] + rw[r]; x++) map[AT(x, y)] = TR_FLOOR | F_ROOM;
    for (r = 1; r < nrooms; r++) corridor(r - 1, r);
    for (k = 0; k < 3; k++) corridor(rng_range(nrooms), rng_range(nrooms));

    /* doors: where a corridor meets a room, between two walls */
    for (y = 1; y < MAP_H - 1; y++)
        for (x = 1; x < MAP_W - 1; x++) {
            i = AT(x, y);
            if (TERRAIN(i) != TR_CORR) continue;
            if ((TERRAIN(i - 1) == TR_ROCK && TERRAIN(i + 1) == TR_ROCK &&
                 ((map[i - MAP_W] & F_ROOM) || (map[i + MAP_W] & F_ROOM))) ||
                (TERRAIN(i - MAP_W) == TR_ROCK && TERRAIN(i + MAP_W) == TR_ROCK &&
                 ((map[i - 1] & F_ROOM) || (map[i + 1] & F_ROOM)))) {
                k = rng_range(10);
                if (k < 5) map[i] = TR_DOOR;
                else if (k < 8) map[i] = TR_OPEN;
            }
        }

    /* the walls: rock next to anything one can walk on */
    for (y = 1; y < MAP_H - 1; y++)
        for (x = 1; x < MAP_W - 1; x++) {
            i = AT(x, y);
            if (TERRAIN(i) != TR_ROCK) continue;
            for (k = 0; k < 9; k++) {
                t = TERRAIN(i + (k / 3 - 1) * MAP_W + (k % 3) - 1);
                if (t != TR_ROCK) {
                    map[i] |= F_BOUND;
                    break;
                }
            }
        }

    /* the player in the first room, the way down in the last */
    pl.x = rx[0] + rw[0] / 2;
    pl.y = ry[0] + rh[0] / 2;
    last = nrooms - 1;
    if (pl.depth < DEPTHS) {
        map[AT(rx[last] + rw[last] / 2, ry[last] + rh[last] / 2)] = TR_STAIRS | F_ROOM;
    } else {
        add_mon(M_BOSS, AT(rx[last] + rw[last] / 2, ry[last] + rh[last] / 2));
        mons[nmons - 1].awake = 0;
    }

    /* braziers: some rooms are lit (always the first and the last) */
    for (r = 0; r < nrooms; r++) {
        if (r != 0 && r != last && rng_range(5) >= 2) continue;
        /* in a corner, but never one next to a way in (a brazier there
         * would leave only a diagonal step into the room) */
        t = rng_range(4);
        for (k = 0; k < 4; k++, t = (t + 1) & 3) {
            x = rx[r] + (t & 1 ? rw[r] - 1 : 0);
            y = ry[r] + (t & 2 ? rh[r] - 1 : 0);
            i = AT(x, y);
            if (TERRAIN(i) == TR_FLOOR && !(map[i] & F_OBJ) && !by_way_in(i)) {
                obj_set(i, O_BRAZIER);
                break;
            }
        }
        for (y = ry[r] - 1; y <= ry[r] + rh[r]; y++)
            for (x = rx[r] - 1; x <= rx[r] + rw[r]; x++) map[AT(x, y)] |= F_LIT;
    }

    /* things to find */
    for (k = 0; k < 2 + rng_range(2); k++) put(spot(1 + rng_range(last)), O_CHEST);
    if (rng_range(4)) put(spot(1 + rng_range(last)), O_IDOL);
    for (k = 0; k < 2; k++) put(spot(rng_range(nrooms)), O_POTION);
    for (k = 0; k < 3; k++) put(spot(rng_range(nrooms)), O_GOLD);
    for (k = 0; k < 5; k++) put(spot(rng_range(nrooms)), O_BONES);

    /* creatures: more, and worse, deeper down; none in the first room */
    for (k = 0; k < 6 + d * 2; k++)
        add_mon(level_mons[d][rng_range(SPAWN)], spot(1 + rng_range(last)));
}
