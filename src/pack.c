/* pack.c — what the player carries, used on the map: the inventory (I),
 * potions and draughts, scrolls, and the torch fed (F) and lit (L) (the
 * overlay "pack") */
#pragma cc45 overlay(pack)
#pragma cc45 noescape_strings
#include <dma.h>
#include "hw.h"
#include "assets.h"
#include "video.h"
#include "rng.h"
#include "game.h"

/* the torch fed with fuel item k (I_RAGS ..); 1 if it was (a turn) */
static const unsigned char burns_for[4] = {80, 120, 160, 220};

static unsigned char feed(unsigned char k) {
    if (pl.fuel > TORCH_MAX - 40) {
        msg("Your torch has all it can hold.", C_DIM);
        return 0;
    }
    pl.items[k]--;
    pl.fuel += burns_for[k - I_RAGS];
    if (pl.fuel > TORCH_MAX) pl.fuel = TORCH_MAX;
    msg(nth("You wrap the rags round the torch's head.\0You bind the sticks to the torch.\0"
            "You wrap the fur round the torch. It stinks.\0You melt the tallow onto the torch.", k - I_RAGS), C_ORANGE);
    if (!pl.lit) msg("Now it wants lighting.", C_DIM);
    return 1;
}

/* the torch lit with k (I_FLINT, I_FIRESTICK, or I_COUNT: a brazier's
 * coals); 1 if that took the turn */
unsigned char light(unsigned char k) {
    if (pl.lit) {
        msg("Your torch is burning.", C_DIM);
        return 0;
    }
    if (!pl.fuel) {
        msg("There is nothing on the torch to burn. Feed it.", C_DIM);
        return 0;
    }
    if (k == I_FLINT && rng_range(3)) {
        msg("Sparks fly, but the torch will not catch.", C_DIM);
        return 1;
    }
    if (k == I_FIRESTICK) pl.items[I_FIRESTICK]--;
    pl.lit = 1;
    msg(nth("Sparks catch. Your torch flares up.\0The firestick flares, and the torch catches.\0"
            "You hold the torch to the coals. It catches.", k == I_FLINT ? 0 : k == I_FIRESTICK ? 1 : 2), C_ORANGE);
    sfx(SFX_FIRE);
    return 1;
}

/* F: the torch fed with the least fuel there is; L: lit with flint, or a
 * firestick */
unsigned char feed_any(void) {
    unsigned char k;
    for (k = I_RAGS; k <= I_CANDLE; k++)
        if (pl.items[k]) return feed(k);
    msg("You have nothing to feed the torch.", C_DIM);
    return 0;
}

unsigned char light_any(void) {
    if (!pl.lit && pl.fuel && !pl.items[I_FLINT] && !pl.items[I_FIRESTICK]) {
        msg("You have no flint and no firesticks.", C_DIM);
        return 0;
    }
    return light(pl.items[I_FLINT] ? I_FLINT : I_FIRESTICK);
}

/* the things on the floor one carries: their item (0xFF: not carried) */
static const unsigned char obj_item[18] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, I_POTION, 0xFF, 0xFF, 0xFF,
    0xFF, I_KEY, I_DRAUGHT, I_RAGS, I_STICKS, I_FUR, I_CANDLE, I_FLINT, I_FIRESTICK,
};

/* what lay at i taken: one more of item */
static void take(unsigned int i, unsigned char item, const char *s, unsigned char c) {
    pl.items[item]++;
    obj_set(i, O_NONE);
    msg(s, c);
    sfx(SFX_CHIME);
}

/* what lies at i (n: O_*) picked up as the player walks over it */
void pick_up(unsigned int i, unsigned char n) {
    unsigned char k;
    char buf[64];
    if (n >= O_PILE) {
        /* a pile put down (or arrows shot): all of it */
        k = n - O_PILE;
        n = obj_count(i);
        pl.items[k] += n;
        obj_set(i, O_NONE);
        buf[0] = 0;
        if (n == 1) {
            str_cat(buf, "You pick up a ");
            str_cat(buf, item_name(k, 1));
        } else {
            str_cat(buf, "You gather ");
            str_num(buf, n);
            str_cat(buf, " ");
            str_cat(buf, item_name(k, n));
        }
        str_cat(buf, ".");
        msg(buf, item_col[k]);
        sfx(SFX_CHIME);
        return;
    }
    switch (n) {
    case O_GOLD:
        n = (2 + rng_range(10)) * pl.depth;
        pl.gold += n;
        obj_set(i, O_NONE);
        buf[0] = 0;
        str_cat(buf, "You gather ");
        str_num(buf, n);
        str_cat(buf, " gold coins.");
        msg(buf, C_GOLD);
        sfx(SFX_COIN);
        break;
    case O_ARROWS:
    case O_SHOT:
        k = n == O_ARROWS ? I_ARROWS : I_SHOT;
        n = n == O_ARROWS ? 3 + rng_range(4) : 2 + rng_range(3);
        pl.items[k] += n;
        obj_set(i, O_NONE);
        buf[0] = 0;
        str_cat(buf, "You gather ");
        str_num(buf, n);
        str_cat(buf, " ");
        str_cat(buf, item_name(k, n));
        str_cat(buf, ".");
        msg(buf, item_col[k]);
        sfx(SFX_CHIME);
        break;
    case O_SCROLL:
        n = rng_range(10);
        n = n < 4 ? I_FIRE : n < 7 ? I_RECALL : I_SIGHT;
        buf[0] = 0;
        str_cat(buf, "You take up a ");
        str_cat(buf, item_name(n, 1));
        str_cat(buf, ".");
        take(i, n, buf, C_BONE);
        break;
    default:
        buf[0] = 0;
        str_cat(buf, "You pick up a ");
        str_cat(buf, item_name(obj_item[n], 1));
        str_cat(buf, ".");
        take(i, obj_item[n], buf, item_col[obj_item[n]]);
        break;
    }
}

/* a healing potion drunk; 1 if it was (a turn) */
unsigned char drink(void) {
    unsigned char d;
    if (!pl.items[I_POTION]) msg("You have no healing potions.", C_DIM);
    else if (pl.hp == pl.maxhp) msg("You are unhurt.", C_DIM);
    else {
        pl.items[I_POTION]--;
        d = 15 + pl.level * 2;
        pl.hp = pl.hp + d > pl.maxhp ? pl.maxhp : pl.hp + d;
        msg("You drink. Warmth spreads through you.", C_GREEN);
        sfx(SFX_DRINK);
        return 1;
    }
    return 0;
}

/* an item used on the map; 1 if that took the turn */
static unsigned char use(unsigned char k) {
    unsigned char d;
    switch (k) {
    case I_POTION:
        return drink();
    case I_DRAUGHT:
        if (pl.mp == pl.maxmp) {
            msg("Your mana is full.", C_DIM);
            return 0;
        }
        pl.items[I_DRAUGHT]--;
        d = 10 + pl.level;
        pl.mp = pl.mp + d > pl.maxmp ? pl.maxmp : pl.mp + d;
        msg("The draught hums on your tongue. Mana returns.", C_BLUE);
        sfx(SFX_DRINK);
        return 1;
    case I_FIRE:
        msg("A scroll of fire is for a fight.", C_DIM);
        return 0;
    case I_RECALL:
        pl.items[I_RECALL]--;
        memory_renew();
        msg("The halls you have walked come back to you.", C_CYAN);
        sfx(SFX_CHIME);
        return 1;
    case I_SIGHT:
        pl.items[I_SIGHT]--;
        level_reveal();
        msg("The shape of this level unfolds in your mind.", C_CYAN);
        sfx(SFX_BELL);
        return 1;
    case I_KEY:
        msg("Walk into a locked door to turn the key.", C_DIM);
        return 0;
    case I_ARROWS:
        return shoot(0);
    case I_SHOT:
        return shoot(1);
    }
    if (k >= I_FLINT) return light(k);
    return feed(k);
}

/* n of item put down at (x, y), or on a free place next to it (a floor,
 * nothing else lying there; a pile of the same grows); 0 if there is none */
static const signed char near_x[9] = {0, 1, -1, 0, 0, 1, 1, -1, -1};
static const signed char near_y[9] = {0, 0, 0, 1, -1, 1, -1, 1, -1};

static unsigned char put_down(unsigned char x, unsigned char y, unsigned char item, unsigned char n) {
    unsigned char k, t, o, px, py;
    for (k = 0; k < 9; k++) {
        px = x + near_x[k];
        py = y + near_y[k];
        t = terrain(px, py);
        if (t != TR_FLOOR && t != TR_CORR && t != TR_OPEN) continue;
        o = obj_at(AT(px, py));
        if (o && o != O_PILE + item) continue;
        return obj_add(AT(px, py), O_PILE + item, n);
    }
    return 0;
}

/* what the player carries, over the map. Up and down choose; RETURN (or
 * U) uses it, D puts all of it down; RUN/STOP or I: back. 1 if that took
 * the turn */
#define INV_COL 18
#define INV_ROW 5
#define INV_W   44
#define INV_H   28

unsigned char inventory(void) {
    unsigned char k, n, sel = 0, has[I_COUNT];
    char buf[48];
    video_blank(INV_COL, INV_ROW, INV_W, INV_H);
    ui_frame(INV_COL, INV_ROW, INV_W, INV_H);
    video_text(INV_COL + 3, INV_ROW + 1, "You carry", C_GOLD);
    buf[0] = 0;
    str_cat(buf, "Weapon  ");
    str_cat(buf, weapon_name(pl.weapon));
    str_cat(buf, "  ");
    str_num(buf, pl.wdice);
    str_cat(buf, "d");
    str_num(buf, pl.wsides);
    str_cat(buf, "+");
    str_num(buf, pl.wplus);
    video_text(INV_COL + 3, INV_ROW + 3, buf, C_TEXT);
    buf[0] = 0;
    str_cat(buf, "Armour  ");
    str_cat(buf, armour_name(pl.armour));
    str_cat(buf, "  +");
    str_num(buf, pl.def);
    video_text(INV_COL + 3, INV_ROW + 4, buf, C_TEXT);
    buf[0] = 0;
    str_cat(buf, "Torch   ");
    str_cat(buf, pl.lit ? "burning, " : "unlit, ");
    str_num(buf, pl.fuel);
    str_cat(buf, " turns of fuel");
    video_text(INV_COL + 3, INV_ROW + 7, buf, pl.lit ? C_ORANGE : C_RED);
    for (k = 0; k < 2; k++) {
        buf[0] = 0;
        str_cat(buf, k ? "Gun     " : "Bow     ");
        n = k ? pl.gun : pl.bow;
        if (n) {
            str_cat(buf, ranged_name(k, n));
            str_cat(buf, "  ");
            n = k * 2 + n - 1;
            str_num(buf, r_dice[n]);
            str_cat(buf, "d");
            str_num(buf, r_sides[n]);
            str_cat(buf, "+");
            str_num(buf, r_plus[n]);
            str_cat(buf, ", reach ");
            str_num(buf, r_reach[n]);
        } else str_cat(buf, "none");
        video_text(INV_COL + 3, INV_ROW + 5 + k, buf, C_TEXT);
    }
    buf[0] = 0;
    str_cat(buf, "Gold    ");
    str_num(buf, pl.gold);
    video_text(INV_COL + 3, INV_ROW + 24, buf, C_GOLD);
    for (k = 0, n = 0; k < I_COUNT; k++)
        if (pl.items[k]) has[n++] = k;
    if (!n) {
        video_text(INV_COL + 3, INV_ROW + 9, "Nothing else.", C_DIM);
        video_text(INV_COL + 3, INV_ROW + 26, "Any key: back.", C_DIM);
        key_wait();
        view_draw();
        return 0;
    }
    video_text(INV_COL + 3, INV_ROW + 26, "RETURN: use.  D: put down.  I: back.", C_DIM);
    for (;;) {
        for (k = 0; k < n; k++) {
            buf[0] = k == sel ? '>' : ' ';
            buf[1] = ' ';
            buf[2] = 0;
            str_num(buf, pl.items[has[k]]);
            str_cat(buf, " ");
            str_cat(buf, item_name(has[k], pl.items[has[k]]));
            video_blank(INV_COL + 2, INV_ROW + 9 + k, INV_W - 4, 1);
            video_text(INV_COL + 2, INV_ROW + 9 + k, buf, k == sel ? C_HILITE : item_col[has[k]]);
        }
        k = key_wait();
        if (k == K_UP) sel = sel ? sel - 1 : n - 1;
        else if (k == K_DOWN) sel = sel + 1 < n ? sel + 1 : 0;
        else if (k == K_FIRE || k == 'u') {
            view_draw();
            return use(has[sel]);
        } else if (k == 'd') {
            k = has[sel];
            view_draw();
            if (!put_down(pl.x, pl.y, k, pl.items[k])) {
                msg("There is no room here to put it down.", C_DIM);
                return 0;
            }
            buf[0] = 0;
            str_cat(buf, "You put down ");
            if (pl.items[k] == 1) str_cat(buf, "a ");
            else {
                str_num(buf, pl.items[k]);
                str_cat(buf, " ");
            }
            str_cat(buf, item_name(k, pl.items[k]));
            str_cat(buf, ".");
            msg(buf, C_DIM);
            pl.items[k] = 0;
            return 0;
        } else if (k == K_STOP || k == 'i') {
            view_draw();
            return 0;
        }
    }
}

/* ---------------------------------------------------------------- shooting */

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
static void fly(unsigned char tx, unsigned char ty) {
    signed int x = pl.x, y = pl.y, dx, dy, sx, sy, err, e2;
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
    fly(m->x, m->y);
    m->awake = 2;
    buf[0] = 0;
    chance = 80 + pl.level * 2 - tdist[t] * 4;
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
    d = rng_dice(r_dice[r], r_sides[r]) + r_plus[r] + (pl.level - 1) / 2;
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
    if (!obj_at(AT(m->x, m->y)) && rng_range(3) == 0) obj_set(AT(m->x, m->y), O_GOLD);
    return 1;
}
