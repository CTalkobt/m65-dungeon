/* trap.c — what is hidden on a level, found and sprung, and what the
 * idols and the scrolls reveal (the overlay "gen")
 *
 * A secret door is a shut door drawn as the wall it hides in (TK_SECRET_*:
 * terrain() calls it rock); found, it is a door. A trap is a hidden thing
 * on the floor (O_TRAP); found or sprung, it is gone and the floor shows
 * its plate (TK_TRAP), harmless. Waiting searches the eight places round
 * the player (search(2)); walking past finds things now and then
 * (search(12)).
 */
#pragma cc45 overlay(gen)
#pragma cc45 noescape_strings
#include <dma.h>
#include "hw.h"
#include "assets.h"
#include "video.h"
#include "rng.h"
#include "game.h"

static const signed char round_x[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
static const signed char round_y[8] = {-1, -1, -1, 0, 0, 1, 1, 1};

/* what hides at (x, y) shown: 1 a door, 2 a trap, 0 nothing was there */
static unsigned char reveal(unsigned char x, unsigned char y) {
    unsigned char k = vm_kind(x, y);
    if (k == TK_SECRET_H || k == TK_SECRET_V) {
        vm_set(x, y, k - TK_SECRET_H + TK_DOOR_H);
        return 1;
    }
    if (obj_at(AT(x, y)) == O_TRAP) {
        obj_set(AT(x, y), O_NONE);
        vm_set(x, y, TK_TRAP);
        return 2;
    }
    return 0;
}

static unsigned char hidden(unsigned char x, unsigned char y) {
    unsigned char k = vm_kind(x, y);
    return k == TK_SECRET_H || k == TK_SECRET_V || obj_at(AT(x, y)) == O_TRAP;
}

unsigned char search(unsigned char odds) {
    unsigned char k, x, y, any = 0;
    for (k = 0; k < 8; k++) {
        x = pl.x + round_x[k];
        y = pl.y + round_y[k];
        if (!hidden(x, y) || rng_range(odds)) continue;
        msg(reveal(x, y) == 1 ? "You find a door hidden in the wall!" : "You find a pressure plate in the floor.", C_HILITE);
        sfx(SFX_CHIME);
        any = 1;
    }
    return any;
}

unsigned char trap_spring(void) {
    unsigned char d, k;
    char buf[48];
    reveal(pl.x, pl.y);
    if (rng_range(8) < 2 + pl.wary) {         /* (wary feet: 3, 4, 5 in 8) */
        msg("A plate shifts underfoot. You step off in time.", C_DIM);
        return 0;
    }
    sfx(SFX_TRAP);
    k = rng_range(4);
    if (k == 1 && !pl.lit) k = 0;
    switch (k) {
    case 0:
        d = 2 + rng_range(4) + pl.depth;
        buf[0] = 0;
        str_cat(buf, "A dart flies from the wall! (");
        str_num(buf, d);
        str_cat(buf, ")");
        msg(buf, C_RED);
        video_shake(4);
        if (pl.hp <= d) {
            pl.hp = 0;
            msg("Its poison is quick. You fall...", C_BLOOD);
            pl.cause = CAUSE_DART;
            return DIED;
        }
        pl.hp -= d;
        break;
    case 1:
        pl.lit = 0;
        msg("A gust from a vent snuffs out your torch!", C_RED);
        break;
    case 2:
        dma_fill_far(SEEN_LIN, MAP_N, 0);
        msg("A chime rings. What you knew of here slips away.", C_PURPLE);
        break;
    default:
        for (k = 0; k < nmons; k++)
            if (mons[k].hp) mons[k].awake = 2;
        msg("A bell clangs! Things stir in the dark.", C_RED);
        sfx(SFX_BELL);
        break;
    }
    return 0;
}

/* every place remembered, as if just seen (an idol, a scroll) */
void memory_renew(void) {
    unsigned int i;
    unsigned char now = epoch();
    for (i = 0; i < MAP_N; i++)
        if (seen_get(i)) seen_set(i, now);
}

/* the level's walls, doors and stairs remembered, seen or not; what hides
 * is found */
void level_reveal(void) {
    unsigned char x, y, k, now = epoch();
    for (y = 0; y < MAP_H; y++) {
        video_poll();
        for (x = 0; x < MAP_W; x++) {
            reveal(x, y);
            k = vm_kind(x, y);
            if (IS_WALL(k) || (k >= TK_DOOR_H && k != TK_SECRET_H && k != TK_SECRET_V)) seen_set(AT(x, y), now);
        }
    }
}
