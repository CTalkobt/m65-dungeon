/* creatures.c — the creatures' turn on the map: asleep until they notice
 * the player, hunting while they see the player, wandering when they lose
 * track; an archer shoots, a thief snatches and runs, an eater dissolves
 * what lies in its way. One that walks into the player starts the fight
 * (explore.c fight). (The overlay "aim", with the shots and the thefts.)
 */
#pragma cc45 overlay(aim)
#pragma cc45 noescape_strings
#include "hw.h"
#include "assets.h"
#include "video.h"
#include "rng.h"
#include "game.h"

static signed char sgn(signed int v) {
    return v < 0 ? -1 : v > 0 ? 1 : 0;
}

static unsigned char free_for(struct mon *c, unsigned char x, unsigned char y) {
    unsigned char t = terrain(x, y);
    if (t == TR_ROCK || t == TR_LOCKED || t == TR_RUBBLE || mon_at(x, y)) return 0;
    t = obj_at(AT(x, y));
    if (BLOCKS(t)) return 0;
    t = terrain(x, y);
    if (t == TR_DOOR) {
        vm_set(x, y, vm_kind(x, y) + TK_DOOR_OPEN_H - TK_DOOR_H);   /* it opens the door: that is its move */
        if (vis_at(x, y)) {
            msg("A latch clicks somewhere in the dark.", C_DIM);
            sfx_far(SFX_DOOR, (signed char)((signed int)x - pl.x));
        }
        return 0;
    }
    (void)c;
    return 1;
}

/* an eater's place: what lay there dissolved (not a key: a vault would shut) */
static void devour(struct mon *c) {
    unsigned char o = obj_at(AT(c->x, c->y));
    char buf[48];
    if (!o || o == O_KEY || !(o >= O_PILE || o == O_GOLD || o == O_SCROLL || o == O_POTION || (o >= O_KEY && o <= O_SHOT))) return;
    obj_set(AT(c->x, c->y), O_NONE);
    o = vis_at(c->x, c->y);
    if (!o || o == VIS_SHADOW) return;
    buf[0] = 0;
    str_cat(buf, "The ");
    str_cat(buf, mon_name(c->kind));
    str_cat(buf, " engulfs what lay there.");
    msg(buf, C_GREEN);
}

/* creature k a step toward dx, dy (round what is in the way): 0, or the
 * outcome of the fight it starts, walking into the player */
static unsigned char go(unsigned char k, signed char dx, signed char dy) {
    struct mon *c = &mons[k];
    unsigned char x = c->x + dx, y = c->y + dy;
    if (!dx && !dy) return 0;
    if (x == pl.x && y == pl.y) {
        if (c->awake != 2) return 0;
        if ((mon_habits[c->kind] & MH_THIEF) && !c->carry && rng_range(2) == 0 && steal(k)) return 0;
        return fight(k, 1);
    }
    if (free_for(c, x, y)) {
        c->x = x;
        c->y = y;
    } else if (dx && free_for(c, c->x + dx, c->y) && !(c->x + dx == pl.x && c->y == pl.y)) c->x += dx;
    else if (dy && free_for(c, c->x, c->y + dy) && !(c->x == pl.x && c->y + dy == pl.y)) c->y += dy;
    else return 0;
    if (mon_habits[c->kind] & MH_EATER) devour(c);
    return 0;
}

/* every creature's move: 0, or the outcome of a fight one started */
unsigned char creatures_move(void) {
    unsigned char k, d, sees, y;
    signed char dx, dy;
    struct mon *c;
    /* the burning oil: its flames die down, the rooms it lit go dark */
    for (k = 0; k < MAX_OBJ; k++)
        if (thing_kind[k] == O_FLAMES && !--thing_n[k]) obj_set(thing_at[k], O_NONE);
    for (k = 0; k < lit_n; k++)
        if (lit_on[k] > 1 && --lit_on[k] == 1) {
            lit_on[k] = 0;
            if (pl.x >= lit_x0[k] && pl.x <= lit_x1[k] && pl.y >= lit_y0[k] && pl.y <= lit_y1[k])
                msg("The oil burns away, and the dark creeps back.", C_DIM);
        }
    for (k = 0; k < nmons; k++) {
        c = &mons[k];
        if (!c->hp) continue;
        if (c->stun) {
            c->stun--;
            continue;
        }
        if (mon_habits[c->kind] & MH_MIMIC) continue;   /* (a mimic waits to be walked into) */
        dx = sgn((signed int)pl.x - c->x);
        dy = sgn((signed int)pl.y - c->y);
        d = (unsigned char)(c->x > pl.x ? c->x - pl.x : pl.x - c->x);
        y = (unsigned char)(c->y > pl.y ? c->y - pl.y : pl.y - c->y);
        if (y > d) d = y;
        sees = vis_at(c->x, c->y) && d <= 10;
        if (!c->awake) {
            if (sees && d <= 6 && rng_range(3) == 0) {
                c->awake = 2;
                if (vis_at(c->x, c->y) != VIS_SHADOW) msg("Something stirs.", C_ORANGE);
                sfx(SFX_STIR);
            }
            continue;
        }
        if ((mon_flags[c->kind] & MF_SLOW) && (turn & 1)) continue;
        if (sees) c->awake = 2;
        else if (c->awake == 2 && rng_range(8) == 0) c->awake = 1;
        /* an archer shoots, from 2 to 6 places off along a clear line */
        if ((mon_habits[c->kind] & MH_ARCHER) && c->awake == 2 && sees && d >= 2 && d <= 6 &&
            rng_range(2) == 0 && view_line(c->x, c->y)) {
            if (foe_shoots(k)) return DIED;
            continue;
        }
        if (c->carry && c->awake == 2) {   /* a thief with its prize runs */
            dx = -dx;
            dy = -dy;
        }
        if ((mon_habits[c->kind] & MH_DARK) && c->awake == 2) {
            /* a ghoul shrinks from the light (but strikes what it is next
             * to), and is quick in the dark while the torch is out */
            y = vis_at(c->x, c->y);
            if (y && y != VIS_SHADOW && d > 1) {
                dx = -dx;
                dy = -dy;
            } else if (!pl.lit && (y = go(k, dx, dy)) != 0) return y;
        }
        if (mon_habits[c->kind] & MH_FLIT) {
            /* bats: two flits a turn, and one in two of them anywhere */
            for (d = 0; d < 2; d++) {
                if (c->awake == 1 || rng_range(2)) {
                    dx = (signed char)rng_range(3) - 1;
                    dy = (signed char)rng_range(3) - 1;
                } else {
                    dx = sgn((signed int)pl.x - c->x);
                    dy = sgn((signed int)pl.y - c->y);
                }
                if ((y = go(k, dx, dy)) != 0) return y;
            }
            continue;
        }
        if (c->awake == 1) {
            if (rng_range(2)) continue;
            dx = (signed char)rng_range(3) - 1;
            dy = (signed char)rng_range(3) - 1;
        }
        if ((y = go(k, dx, dy)) != 0) return y;
    }
    return 0;
}
