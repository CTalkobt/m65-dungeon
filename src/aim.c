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
static unsigned int keep[8];
static unsigned char kept_x, kept_y;
unsigned char view_kept;                      /* a place is kept (view_draw forgets it) */

static unsigned long cell_at(unsigned char x, unsigned char y) {
    return ((unsigned int)(1 + (y - cam_y) * 2) * COLS + (x - cam_x) * 2) * 2;
}

static void view_unput(void) {
    unsigned long c;
    if (!view_kept) return;
    c = cell_at(kept_x, kept_y);
    dma_copy_far(SCREEN_LIN + c, (unsigned long)(unsigned int)keep, 4);
    dma_copy_far(SCREEN_LIN + c + COLS * 2, (unsigned long)(unsigned int)(keep + 2), 4);
    dma_copy_far(COLOR_LIN + c, (unsigned long)(unsigned int)(keep + 4), 4);
    dma_copy_far(COLOR_LIN + c + COLS * 2, (unsigned long)(unsigned int)(keep + 6), 4);
    view_kept = 0;
}

static void view_put(unsigned char x, unsigned char y, unsigned char k) {
    unsigned long c;
    unsigned int ch, cells[2], zero[2] = {0, 0};
    unsigned char v = vis_at(x, y);
    view_unput();
    if (x < cam_x || y < cam_y || x >= cam_x + 40 || y >= cam_y + 22) return;
    ch = tile_char[k][v && v != VIS_SHADOW ? v - 1 : TV_EYES];
    if (!ch) return;
    c = cell_at(x, y);
    dma_copy_far((unsigned long)(unsigned int)keep, SCREEN_LIN + c, 4);
    dma_copy_far((unsigned long)(unsigned int)(keep + 2), SCREEN_LIN + c + COLS * 2, 4);
    dma_copy_far((unsigned long)(unsigned int)(keep + 4), COLOR_LIN + c, 4);
    dma_copy_far((unsigned long)(unsigned int)(keep + 6), COLOR_LIN + c + COLS * 2, 4);
    kept_x = x;
    kept_y = y;
    view_kept = 1;
    cells[0] = ch;
    cells[1] = ch + 1;
    dma_copy_far(SCREEN_LIN + c, (unsigned long)(unsigned int)cells, 4);
    dma_copy_far(COLOR_LIN + c, (unsigned long)(unsigned int)zero, 4);
    cells[0] = ch + 2;
    cells[1] = ch + 3;
    dma_copy_far(SCREEN_LIN + c + COLS * 2, (unsigned long)(unsigned int)cells, 4);
    dma_copy_far(COLOR_LIN + c + COLS * 2, (unsigned long)(unsigned int)zero, 4);
}


/* the creatures one could shoot at: in sight (lit, or eyes in the dark),
 * within reach, a clear line; nearest first */
static unsigned char targets[8], tdist[8], ntargets;

static void find_targets(unsigned char reach) {
    unsigned char k, j, d, e;
    ntargets = 0;
    for (k = 0; k < nmons && ntargets < 8; k++) {
        if (!mons[k].hp || !vis_at(mons[k].x, mons[k].y)) continue;
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
    str_cat(buf, nth(mon_names, m->kind));
}

/* the shot's flight from the player to (tx, ty), a place a frame or two */
static void fly(unsigned char fx, unsigned char fy, unsigned char tx, unsigned char ty) {
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
        str_cat(buf, ". RETURN: shoot. RUN/STOP: no.");
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
    if (m->hp == FRESH) m->hp = mon_hp[m->kind];
    d = rng_dice(r_dice[r], r_sides[r]) + r_plus[r] + (pl.level - 1) / 2 + pl.aim;
    d = d > mon_def[m->kind] ? d - mon_def[m->kind] : 1;
    sfx(SFX_HIT);
    str_cat(buf, gun ? "The ball strikes " : "The arrow strikes ");
    the_mon(buf, m);
    str_cat(buf, " (");
    str_num(buf, d);
    str_cat(buf, ").");
    msg(buf, C_TEXT);
    if (m->hp > d) {
        m->hp -= d;
        m->stun = 0;
        return 1;
    }
    /* it falls where it stood */
    m->hp = 0;
    buf[0] = 0;
    str_cat(buf, "The ");
    str_cat(buf, nth(mon_names, m->kind));
    str_cat(buf, " falls.");
    msg(buf, C_HILITE);
    sfx(SFX_DEATH);
    if (gain_xp(mon_xp[m->kind])) {
        msg("You have grown stronger!", C_GOLD);
        sfx(SFX_LEVEL);
    }
    drop_carry(targets[t]);
    if (!obj_at(AT(m->x, m->y)) && rng_range(3) == 0) obj_set(AT(m->x, m->y), O_GOLD);
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
        str_cat(buf, nth(mon_names, m->kind));
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
    str_cat(buf, nth(mon_names, m->kind));
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
