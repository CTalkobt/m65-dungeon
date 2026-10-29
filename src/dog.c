/* dog.c — the player's dog: it keeps close, runs at any creature in sight
 * and bites it (1), and when it is hurt too badly limps off into the dark
 * and comes back DOG_BACK turns later (the overlay "aim", with the
 * creatures' turn: creatures.c calls dog_move first). It opens no doors;
 * the player walking into it trades places (explore.c); creatures cannot
 * walk through it (creatures.c free_for); in a close-up fight it worries
 * at the foe it was beside (battle.c). Its nose: a mimic still a chest it
 * does not run at, but beside one it growls (the once) and leaves it be;
 * and now and then it sniffs out a hidden plate near the player (sniff:
 * found, as the player's search finds one). Told to stay (B: explore.c)
 * it holds where it is, biting only what comes beside it, till told to
 * follow, or a new level, or it comes back from the dark. The stick
 * thrown (aim.c), it runs for it (fetch), and 17 times in 20 brings it
 * back; creatures in sight come first. */
#pragma cc45 overlay(aim)
#pragma cc45 noescape_strings
#include "hw.h"
#include "assets.h"
#include "rng.h"
#include "game.h"

#define DOG_BACK 35

/* a place the dog may stand: open ground, no one there, nothing in the way */
static unsigned char dog_free(unsigned char x, unsigned char y) {
    unsigned char t = terrain(x, y);
    if (t != TR_FLOOR && t != TR_CORR && t != TR_OPEN && t != TR_STAIRS) return 0;
    if (mon_at(x, y) || (x == pl.x && y == pl.y)) return 0;
    t = obj_at(AT(x, y));
    return !BLOCKS(t);
}

/* the dog beside the player (a new level, or back from the dark); 0: no room */
unsigned char dog_place(void) {
    unsigned char k, x, y;
    for (k = 0; k < 9; k++) {
        x = pl.x + (signed char)(k % 3) - 1;
        y = pl.y + (signed char)(k / 3) - 1;
        if (dog_free(x, y)) {
            pl.dog_x = x;
            pl.dog_y = y;
            return 1;
        }
    }
    return 0;
}

/* a step toward dx, dy, round what is in the way */
static void dog_step(signed char dx, signed char dy) {
    unsigned char x = pl.dog_x, y = pl.dog_y;
    if (dog_free(x + dx, y + dy)) {
        x += dx;
        y += dy;
    } else if (dx && dog_free(x + dx, y)) x += dx;
    else if (dy && dog_free(x, y + dy)) y += dy;
    pl.dog_x = x;
    pl.dog_y = y;
}

static signed char toward(unsigned char a, unsigned char b) {
    return a < b ? 1 : a > b ? -1 : 0;
}

static unsigned char apart(unsigned char x0, unsigned char y0, unsigned char x1, unsigned char y1) {
    unsigned char dx = x0 > x1 ? x0 - x1 : x1 - x0, dy = y0 > y1 ? y0 - y1 : y1 - y0;
    return dx > dy ? dx : dy;
}

/* a hidden plate where the player is going (two places each way, in
 * sight), found by the dog's nose ranging round (1 in 6 a turn) */
static void sniff(void) {
    char buf[64];
    unsigned char x, y, x0 = pl.x - 2, y0 = pl.y - 2;
    for (y = y0; y != (unsigned char)(y0 + 5); y++)
        for (x = x0; x != (unsigned char)(x0 + 5); x++) {
            if (x >= MAP_W || y >= MAP_H || !vis_at(x, y)) continue;
            if (obj_at(AT(x, y)) == O_TRAP && !rng_range(6)) {
                obj_set(AT(x, y), O_NONE);
                vm_set(x, y, TK_TRAP);
                msg(dog_says(buf, " sniffs at the floor, and growls: a plate!"), C_HILITE);
                sfx(SFX_BARK);
                return;
            }
        }
}

/* the stick: run for it, snatch it up (17 in 20) and bring it back; 0 if
 * there is none to see to */
static unsigned char fetch(void) {
    char buf[64];
    unsigned char x = pl.dog_x, y = pl.dog_y, n;
    unsigned int at = pl.dog_fetch;
    if (pl.dog_carry) {
        if (apart(pl.x, pl.y, x, y) > 1) {
            dog_step(toward(x, pl.x), toward(y, pl.y));
            if (apart(pl.x, pl.y, pl.dog_x, pl.dog_y) > 1) return 1;
        }
        pl.dog_carry = 0;
        pl.items[I_STICK]++;
        msg(dog_says(buf, " drops the stick at your feet, tail wagging."), C_GOLD);
        return 1;
    }
    if (!at) return 0;
    if (obj_at(at) != O_PILE + I_STICK) {
        pl.dog_fetch = 0;                          /* (picked up meanwhile) */
        return 0;
    }
    if (AT(x, y) != at) {
        dog_step(toward(x, at % MAP_W), toward(y, at / MAP_W));
        if (pl.dog_x == x && pl.dog_y == y) {
            pl.dog_fetch = 0;                      /* (no way to it) */
            msg(dog_says(buf, " cannot get to the stick."), C_DIM);
        }
        return 1;
    }
    pl.dog_fetch = 0;
#ifdef TEST_FETCH_MISS
    if (1) {                                       /* (tests: it always loses interest) */
#else
    if (rng_range(20) >= 17) {
#endif
        msg(dog_says(buf, " sniffs the stick, and wanders off."), C_DIM);
        return 1;
    }
    n = obj_count(at);
    obj_set(at, O_NONE);
    if (n > 1) obj_add(at, O_PILE + I_STICK, n - 1);
    pl.dog_carry = 1;
    msg(dog_says(buf, " snatches up the stick."), C_TEXT);
    return 1;
}

void dog_move(void) {
    unsigned char k, t = 0xFF, d, best = 9;
    struct mon *c;
    char buf[64];
    if (!pl.dog_hp) {
        /* gone off to lick its wounds: back after a while */
        if (pl.dog_down && !--pl.dog_down && dog_place()) {
            pl.dog_hp = DOG_MAX;
            pl.dog_stay = 0;
            msg(dog_says(buf, " comes bounding back out of the dark."), C_GOLD);
            sfx(SFX_BARK);
        } else if (!pl.dog_down) pl.dog_down = 1;   /* (no room beside you: next turn) */
        return;
    }
    /* the nearest creature in sight (not the warded lich) */
    for (k = 0; k < nmons; k++) {
        c = &mons[k];
        if (!c->hp || (mon_flags[c->kind] & MF_BOSS)) continue;
        d = vis_at(c->x, c->y);
        if (!d || d == VIS_SHADOW) continue;
        d = apart(c->x, c->y, pl.dog_x, pl.dog_y);
        if (!c->awake && (mon_habits[c->kind] & MH_MIMIC)) {
            /* a chest that smells wrong, beside it: growled at, the once,
             * and left be (not run at) */
            if (d <= 1 && dog_warned != AT(c->x, c->y)) {
                dog_warned = AT(c->x, c->y);
                msg(dog_says(buf, " growls at the chest, hackles up."), C_HILITE);
                sfx(SFX_BARK);
            }
            continue;
        }
        if (d < best) {
            best = d;
            t = k;
        }
    }
    if (t == 0xFF) {
        /* nothing to chase: the stick (fetch), or keep close (and catch up
         * when left far behind), nose down; told to stay, it holds */
        if (fetch()) return;
        if (pl.dog_stay) return;
        sniff();
        d = apart(pl.x, pl.y, pl.dog_x, pl.dog_y);
        if (d > 10) dog_place();
        else if (d > 2) dog_step(toward(pl.dog_x, pl.x), toward(pl.dog_y, pl.y));
        return;
    }
    c = &mons[t];
    if (best > 1) {
        if (best == 9 || pl.dog_stay) return;          /* (staying: it bites only what comes to it) */
        dog_step(toward(pl.dog_x, c->x), toward(pl.dog_y, c->y));
        return;
    }
    /* beside it: a bite, and now and then it bites back */
    dog_says(buf, " bites the ");
    str_cat(buf, mon_name(c->kind));
    str_cat(buf, ".");
    msg(buf, C_TEXT);
    sfx(SFX_BARK);
    mon_hurt(t, 1);
    if (!c->hp) return;
    c->awake = 2;
    if (rng_range(3)) return;
    d = 1 + rng_range(3) + pl.depth / 2;
    if (pl.dog_hp > d) {
        pl.dog_hp -= d;
        return;
    }
    pl.dog_hp = 0;
    pl.dog_down = DOG_BACK;
    if (pl.dog_carry) put_down(pl.dog_x, pl.dog_y, I_STICK, 1);   /* (the stick falls where it was) */
    pl.dog_carry = 0;
    pl.dog_fetch = 0;
    msg(dog_says(buf, " yelps, and limps off into the dark."), C_RED);
    sfx(SFX_YELP);
}
