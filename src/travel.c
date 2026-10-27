/* travel.c — the way to the stairs, once found (explore.c travel walks it)
 *
 * A search out from the stairs over the places remembered and walked on:
 * way[] holds each place's distance from them, mod 3, plus 1 (2 bits a
 * place; 0 not reached). From any place reached, a neighbour one step
 * nearer is the one with the distance before (neighbours differ by one
 * step at most). (The overlay "aim": out of the walk's way.)
 */
#pragma cc45 overlay(aim)
#include "hw.h"
#include "assets.h"
#include "video.h"
#include "game.h"

static unsigned char way[MAP_N / 4];
#define WAY_Q    ((unsigned int *)scratch)      /* the search's queue (view_draw's scratch) */
#define WAY_QN   512
typedef char way_q_fits[WAY_QN * 2 <= SCRATCH_SIZE ? 1 : -1];
static const signed char dir_x[8] = {0, 1, 0, -1, 1, 1, -1, -1}, dir_y[8] = {-1, 0, 1, 0, -1, 1, 1, -1};

static unsigned char way_get(unsigned int i) {
    return (way[i >> 2] >> ((i & 3) * 2)) & 3;
}

/* a place the way may go through: remembered, walked on (a shut door is
 * opened), nothing in the way, nothing that would stop the walk */
static unsigned char way_open(unsigned char x, unsigned char y) {
    unsigned char t;
    if (!seen_get(AT(x, y)) || vm_kind(x, y) == TK_TRAP) return 0;   /* (round a found plate) */
    t = terrain(x, y);
    if (t != TR_FLOOR && t != TR_CORR && t != TR_OPEN && t != TR_STAIRS && t != TR_DOOR) return 0;
    t = obj_at(AT(x, y));
    return !BLOCKS(t) && t != O_CHEST && t != O_IDOL;
}

unsigned char way_find(void) {
    unsigned int i, j, head = 0, tail = 0;
    unsigned char x, y, d, k, nx, ny;
    for (y = 0; y < MAP_H; y++)
        for (x = 0; x < MAP_W; x++)
            if (vm_kind(x, y) == TK_STAIRS) goto found;
    return 0;
found:
    if (!seen_get(AT(x, y))) return 0;
    for (i = 0; i < sizeof way; i++) way[i] = 0;
    i = AT(x, y);
    way[i >> 2] |= 1 << ((i & 3) * 2);
    WAY_Q[tail++] = i;
    while (head != tail) {
        video_poll();
        i = WAY_Q[head];
        head = (head + 1) & (WAY_QN - 1);
        d = way_get(i) % 3 + 1;
        x = (unsigned char)(i % MAP_W);
        y = (unsigned char)(i / MAP_W);
        for (k = 0; k < 8; k++) {
            nx = x + dir_x[k];
            ny = y + dir_y[k];
            if (nx >= MAP_W || ny >= MAP_H) continue;      /* (0 - 1: 255) */
            j = AT(nx, ny);
            if (way_get(j) || !way_open(nx, ny)) continue;
            way[j >> 2] |= d << ((j & 3) * 2);
            WAY_Q[tail] = j;
            tail = (tail + 1) & (WAY_QN - 1);
        }
    }
    return 1;
}

unsigned char way_next(void) {
    unsigned char d = way_get(AT(pl.x, pl.y)), k;
    if (!d) return 0xFF;
    d = d == 1 ? 3 : d - 1;                       /* the distance one step nearer */
    for (k = 0; k < 8; k++)
        if (way_get(AT(pl.x + dir_x[k], pl.y + dir_y[k])) == d)
            return (unsigned char)((dir_x[k] + 1) | (dir_y[k] + 1) << 2);
    return 0xFF;
}
