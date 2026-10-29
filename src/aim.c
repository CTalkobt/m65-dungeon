/* aim.c — shooting on the map (T, G), and the creatures' tricks there: an
 * archer's arrow, a thief's snatch (the overlay "aim") */
#pragma cc45 overlay(aim)
#pragma cc45 noescape_strings
#include <dma.h>
#include "hw.h"
#include "assets.h"
#include "video.h"
#include "rng.h"
#include "game.h"

/* one tile drawn over a place on the map's screen (a mark, something in
 * flight) (view.c's rows 1-44: 40 x 22 places from cam_x, cam_y),
 * as lit as the place is (glowing alone in the dark); view_unput puts the
 * place back as it was */
static unsigned int keep[8];                  /* the 4 cells' characters, then their colours */
static unsigned char kept_x, kept_y;
unsigned char view_kept;                      /* a place is kept (view_draw forgets it) */

/* place (x, y)'s first cell on the screen (and in colour RAM): an index */
static unsigned int cell_at(unsigned char x, unsigned char y) {
    return (unsigned int)(1 + (y - cam_y) * 2) * COLS + (x - cam_x) * 2;
}

/* the place's cells, 2 x 2: q 0-3 (left, right, then the row below), in
 * screen and colour RAM */
typedef unsigned int __far *cells_t;
#define CELL(q) (((q) & 2 ? COLS : 0) + ((q) & 1))

static void view_unput(void) {
    cells_t s, c;
    unsigned char q;
    if (!view_kept) return;
    s = (cells_t)SCREEN_LIN + cell_at(kept_x, kept_y);
    c = (cells_t)COLOR_LIN + cell_at(kept_x, kept_y);
    for (q = 0; q < 4; q++) {
        s[CELL(q)] = keep[q];
        c[CELL(q)] = keep[q + 4];
    }
    view_kept = 0;
}

static void view_put(unsigned char x, unsigned char y, unsigned char k) {
    cells_t s, c;
    unsigned char q, v = vis_at(x, y);
    view_unput();
    if (x < cam_x || y < cam_y || x >= cam_x + 40 || y >= cam_y + 22) return;
    v = (v && v != VIS_SHADOW ? v - 1 : TV_EYES) * 4;
    if (!tile_char[k][v]) return;
    s = (cells_t)SCREEN_LIN + cell_at(x, y);
    c = (cells_t)COLOR_LIN + cell_at(x, y);
    for (q = 0; q < 4; q++) {
        keep[q] = s[CELL(q)];
        keep[q + 4] = c[CELL(q)];
        s[CELL(q)] = tile_char[k][v + q];     /* (a full-colour tile: colour RAM 0) */
        c[CELL(q)] = 0;
    }
    kept_x = x;
    kept_y = y;
    view_kept = 1;
}

/* the creatures one could shoot at: in sight (lit, or eyes in the dark),
 * within reach, a clear line; nearest first */
static unsigned char targets[8], tdist[8], ntargets;

static void find_targets(unsigned char reach) {
    unsigned char k, j, d, e;
    ntargets = 0;
    for (k = 0; k < nmons && ntargets < 8; k++) {
        if (!mons[k].hp || !vis_at(mons[k].x, mons[k].y)) continue;
        if (!mons[k].awake && (mon_habits[mons[k].kind] & MH_MIMIC)) continue;   /* (it looks a chest) */
        d = mons[k].x > pl.x ? mons[k].x - pl.x : pl.x - mons[k].x;
        e = mons[k].y > pl.y ? mons[k].y - pl.y : pl.y - mons[k].y;
        if (e > d) d = e;
        if (d > reach || !view_line(mons[k].x, mons[k].y)) continue;
        for (j = ntargets; j > 0 && tdist[j - 1] > d; j--) {
            targets[j] = targets[j - 1];
            tdist[j] = tdist[j - 1];
        }
        targets[j] = k;
        tdist[j] = d;
        ntargets++;
    }
}

/* "the giant rat" and the rest in buf */
static void the_mon(char *buf, struct mon *m) {
    str_cat(buf, "the ");
    str_cat(buf, mon_name(m->kind));
}

/* the shot's flight from the player to (tx, ty), a place a frame or two */
void fly(unsigned char fx, unsigned char fy, unsigned char tx, unsigned char ty) {
    signed int x = fx, y = fy, dx, dy, sx, sy, err, e2;
    dx = (signed int)tx - x;
    dy = (signed int)ty - y;
    sx = dx < 0 ? -1 : 1;
    sy = dy < 0 ? -1 : 1;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    err = dx - dy;
    for (;;) {
        e2 = err * 2;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx) { err += dx; y += sy; }
        if (x == tx && y == ty) break;
        view_put((unsigned char)x, (unsigned char)y, T_MISSILE);
        video_frames(2);
    }
    view_unput();
}

/* choosing the target: the reticle blinks on it; again (key) or left /
 * right: the next; RETURN: shoot. Its index in targets, or 0xFF */
static unsigned char aim(unsigned char key) {
    unsigned char t = 0, k, f = 0;
    char buf[64];
    for (;;) {
        buf[0] = 0;
        str_cat(buf, "Aim at ");
        the_mon(buf, &mons[targets[t]]);
        str_cat(buf, ". RETURN: loose. RUN/STOP: no.");
        msg(buf, C_HILITE);
        for (;;) {
            if ((f & 15) == 0) view_put(mons[targets[t]].x, mons[targets[t]].y, T_AIM);
            else if ((f & 15) == 8) view_unput();
            f++;
            video_frame();
            sfx_update();
            k = key_get();
            if (k) break;
        }
        view_unput();
        if (k == K_FIRE) return t;
        if (k == K_STOP) return 0xFF;
        if (k == key || k == K_RIGHT || k == K_DOWN) t = t + 1 < ntargets ? t + 1 : 0;
        else if (k == K_LEFT || k == K_UP) t = t ? t - 1 : ntargets - 1;
    }
}

/* creature k hurt d: less life, or its fall (where it stood:
 * the experience, what it carried, now and then gold) */
void mon_hurt(unsigned char k, unsigned char d) {
    struct mon *m = &mons[k];
    if (m->hp == FRESH) m->hp = mon_hp[m->kind];
    char buf[48];
    if (m->hp > d) {
        m->hp -= d;
        m->stun = 0;
        return;
    }
    m->hp = 0;
    buf[0] = 0;
    str_cat(buf, "The ");
    str_cat(buf, mon_name(m->kind));
    str_cat(buf, " falls.");
    msg(buf, C_HILITE);
    sfx(SFX_DEATH);
    pl.kills++;
    if (gain_xp(mon_xp[m->kind])) {
        msg("You have grown stronger!", C_GOLD);
        sfx(SFX_LEVEL);
    }
    drop_carry(k);
    if (!obj_at(AT(m->x, m->y)) && rng_range(3) == 0) obj_set(AT(m->x, m->y), O_GOLD);
}

unsigned char shoot(unsigned char gun) {
    unsigned char w = gun ? pl.gun : pl.bow, ammo = gun ? I_SHOT : I_ARROWS, r, t, d, chance, k;
    struct mon *m;
    char buf[64];
    if (!w) {
        msg(gun ? "You have no flintlock." : "You have no bow.", C_DIM);
        return 0;
    }
    if (!pl.items[ammo]) {
        msg(gun ? "You have no cartridges." : "You have no arrows.", C_DIM);
        return 0;
    }
    r = gun * 2 + w - 1;
    find_targets(r_reach[r]);
    if (!ntargets) {
        msg("Nothing in sight and in reach to shoot at.", C_DIM);
        return 0;
    }
    t = aim(gun ? 'g' : 't');
    if (t == 0xFF) {
        msg("You lower your aim.", C_DIM);
        return 0;
    }
    m = &mons[targets[t]];
    pl.items[ammo]--;
    if (gun) {
        if (rng_range(6) == 0) {
            msg("The powder flashes in the pan, and no more.", C_DIM);
            sfx(SFX_CLANG);
            return 1;
        }
        sfx(SFX_HEAVY);
        video_shake(4);
        /* the shot is heard far off: what is near wakes, and comes */
        for (k = 0; k < nmons; k++) {
            if (!mons[k].hp) continue;
            d = mons[k].x > pl.x ? mons[k].x - pl.x : pl.x - mons[k].x;
            chance = mons[k].y > pl.y ? mons[k].y - pl.y : pl.y - mons[k].y;
            if (d < 14 && chance < 14) mons[k].awake = 2;
        }
    } else sfx(SFX_SWING);
    fly(pl.x, pl.y, m->x, m->y);
    m->awake = 2;
    buf[0] = 0;
    chance = 80 + pl.level * 2 + pl.aim * 8 - tdist[t] * 4;
    if (vis_at(m->x, m->y) == VIS_SHADOW) chance -= 20;
    k = rng_range(100) < chance;
    /* the arrow lies where it ended, unless it broke (a hit breaks one in
     * two, a miss one in four) */
    if (!gun && rng_range(k ? 2 : 4)) put_down(m->x, m->y, I_ARROWS, 1);
    if (!k) {
        str_cat(buf, gun ? "The shot misses " : "The arrow misses ");
        the_mon(buf, m);
        str_cat(buf, ".");
        msg(buf, C_DIM);
        return 1;
    }
    if (mon_flags[m->kind] & MF_BOSS) {
        msg("A cold ward turns it aside. It must be faced.", C_PURPLE);
        return 1;
    }
    d = rng_dice(r_dice[r], r_sides[r]) + r_bonus(gun) + (pl.level - 1) / 2 + pl.aim;
    d = d > mon_def[m->kind] ? d - mon_def[m->kind] : 1;
    sfx(SFX_HIT);
    str_cat(buf, gun ? "The ball strikes " : "The arrow strikes ");
    the_mon(buf, m);
    str_cat(buf, " (");
    str_num(buf, d);
    str_cat(buf, ").");
    msg(buf, C_TEXT);
    mon_hurt(targets[t], d);
    return 1;
}

/* ---------------------------------------------------------------- throwing */

#define THROW_REACH 5
#define FLAME_TURNS 12

/* a flask of oil or a strip of meat at a creature in sight: the oil bursts
 * into fire where it lands (the flames burn a while, and light the room
 * they are in), and burns it, the more if fire is its bane; the meat is
 * fallen on by any creature that hungers, and keeps it busy a while */
unsigned char throw_it(unsigned char i) {
    struct item_info in;
    unsigned char t, d, k;
    unsigned int at;
    struct mon *m;
    char buf[64];
    if (i == I_COUNT) {
        /* what is carried that is thrown by a key (items.py); more than
         * one, the key chooses */
        for (k = d = 0; k < I_COUNT; k++)
            if (pl.items[k] && (item_use[k] == U_THROW || item_use[k] == U_STICK)) {
                d++;
                i = k;
            }
        if (d > 1) {
            msg(item_throw_what, C_HILITE);
            d = key_wait();
            for (k = 0, i = I_COUNT; k < I_COUNT; k++) {
                item_info(k, &in);
                if (pl.items[k] && in.key == d) i = k;
            }
            if (i == I_COUNT) {
                msg("You think better of it.", C_DIM);
                return 0;
            }
        }
    }
    if (i == I_COUNT || !pl.items[i]) {
        msg("You have nothing to throw.", C_DIM);
        return 0;
    }
    if (item_use[i] == U_STICK) return throw_stick();
    find_targets(THROW_REACH);
    if (!ntargets) {
        msg("Nothing in sight and in reach to throw at.", C_DIM);
        return 0;
    }
    t = aim('x');
    if (t == 0xFF) {
        msg("You lower your arm.", C_DIM);
        return 0;
    }
    m = &mons[targets[t]];
    pl.items[i]--;
    sfx(SFX_SWING);
    fly(pl.x, pl.y, m->x, m->y);
    buf[0] = 0;
    str_cat(buf, "The ");
    str_cat(buf, mon_name(m->kind));
    if (i == I_FOOD) {
        if (mon_flags[m->kind] & MF_UNDEAD) {
            str_cat(buf, " pays the meat no heed.");
            msg(buf, C_DIM);
            put_down(m->x, m->y, I_FOOD, 1);
            return 1;
        }
        str_cat(buf, " falls on the meat.");
        msg(buf, C_TEXT);
        m->stun = 6 + rng_range(5);
        if (m->awake == 2) m->awake = 1;         /* (it loses track of the player) */
        return 1;
    }
    /* the oil: fire where it bursts, the room it is in lit a while */
    sfx(SFX_FIRE);
    at = AT(m->x, m->y);
    if (!obj_at(at)) obj_add(at, O_FLAMES, FLAME_TURNS);
    for (k = 0; k < lit_n; k++)
        if (!lit_on[k] && m->x >= lit_x0[k] && m->x <= lit_x1[k] && m->y >= lit_y0[k] && m->y <= lit_y1[k]) {
            lit_on[k] = OIL_LIGHT + 1;
            break;
        }
    m->awake = 2;
    if (mon_flags[m->kind] & MF_BOSS) {
        str_cat(buf, " stands in the flames, untouched.");
        msg(buf, C_PURPLE);
        return 1;
    }
    d = rng_dice(2, 6) + 2 + pl.aim;             /* (fire: no armour turns it) */
    if (mon_flags[m->kind] & MF_FIREWEAK) d *= 2;
    str_cat(buf, mon_flags[m->kind] & MF_FIREWEAK ? " shrieks in the bursting fire (" : " is caught in the bursting fire (");
    str_num(buf, d);
    str_cat(buf, ").");
    msg(buf, C_ORANGE);
    mon_hurt(targets[t], d);
    return 1;
}

/* ---------------------------------------------------------------- the creatures' tricks */

unsigned char foe_shoots(unsigned char k) {
    struct mon *m = &mons[k];
    unsigned char d, e, seen = vis_at(m->x, m->y) != VIS_SHADOW;
    char buf[64];
    sfx(SFX_SWING);
    fly(m->x, m->y, pl.x, pl.y);
    d = m->x > pl.x ? m->x - pl.x : pl.x - m->x;
    e = m->y > pl.y ? m->y - pl.y : pl.y - m->y;
    if (e > d) d = e;
    buf[0] = 0;
    if (seen) {
        str_cat(buf, "The ");
        str_cat(buf, mon_name(m->kind));
        str_cat(buf, "'s arrow ");
    } else str_cat(buf, "An arrow from the dark ");
    if (rng_range(100) >= 75 - d * 5) {
        str_cat(buf, "whistles past you.");
        msg(buf, C_DIM);
        if (rng_range(4)) put_down(pl.x, pl.y, I_ARROWS, 1);     /* (one to keep) */
        return 0;
    }
    d = rng_dice(1, 6) + pl.depth / 2;
    d = d > pl.def ? d - pl.def : 1;
    str_cat(buf, "strikes you (");
    str_num(buf, d);
    str_cat(buf, ").");
    msg(buf, C_RED);
    sfx(SFX_HURT);
    if (rng_range(4) == 0) put_down(pl.x, pl.y, I_ARROWS, 1);
    if (pl.hp <= d) {
        pl.hp = 0;
        pl.cause = CAUSE_SHOT + m->kind;
        return 1;
    }
    pl.hp -= d;
    return 0;
}

/* a thief's pick: fuel first, then arrows */
static const unsigned char worth[5] = {I_CANDLE, I_FUR, I_STICKS, I_RAGS, I_ARROWS};

unsigned char steal(unsigned char k) {
    struct mon *m = &mons[k];
    unsigned char j, i;
    char buf[64];
    for (j = 0; j < 5; j++)
        if (pl.items[worth[j]]) break;
    if (j == 5) return 0;
    i = worth[j];
    pl.items[i]--;
    m->carry = i + 1;
    buf[0] = 0;
    str_cat(buf, "The ");
    str_cat(buf, mon_name(m->kind));
    str_cat(buf, " snatches a");
    str_cat(buf, i == I_ARROWS ? "n " : " ");
    str_cat(buf, item_name(i, 1));
    str_cat(buf, " and flees!");
    msg(buf, C_ORANGE);
    sfx(SFX_STIR);
    return 1;
}

void drop_carry(unsigned char k) {
    struct mon *m = &mons[k];
    if (!m->carry) return;
    put_down(m->x, m->y, m->carry - 1, 1);
    m->carry = 0;
}
