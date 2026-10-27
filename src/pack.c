/* pack.c — what the player carries, used on the map: the inventory (I),
 * potions and draughts, scrolls, the torch fed (F) and lit (L), what is
 * picked up and put down (the overlay "pack"; shooting is aim.c) */
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

/* the flask of colour c drunk: what it does, and from now on its name */
unsigned char quaff(unsigned char c) {
    unsigned char d;
    char buf[64];
    pl.items[I_FLASK + c]--;
    buf[0] = 0;
    str_cat(buf, "You drink the ");
    str_cat(buf, item_name(I_FLASK + c, 1));
    str_cat(buf, ".");
    msg(buf, item_col[I_FLASK + c]);
    pl.known |= 1 << c;
    sfx(SFX_DRINK);
    switch (pl.flask[c]) {
    case FX_SIGHT:
        pl.sight = SIGHT_TURNS;
        msg("The dark thins. Shapes stand out in the shadows.", C_CYAN);
        break;
    case FX_MEMORY:
        pl.fade = 1;
        msg("Your mind clears. What you see, you will keep.", C_CYAN);
        break;
    case FX_VIGOUR:
        pl.maxhp += 3;
        pl.hp = pl.maxhp;
        msg("Warmth runs through you, and stays. (+3 life)", C_GREEN);
        sfx(SFX_HEAL);
        break;
    default:
        d = 4 + pl.depth * 2;
        pl.hp = pl.hp > d ? pl.hp - d : 1;
        buf[0] = 0;
        str_cat(buf, "Bitter! Poison burns in your veins. (");
        str_num(buf, d);
        str_cat(buf, ")");
        msg(buf, C_RED);
        sfx(SFX_POISON);
    }
    buf[0] = 0;
    str_cat(buf, "It was a ");
    str_cat(buf, item_name(I_FLASK + c, 1));
    str_cat(buf, ".");
    msg(buf, C_DIM);
    return 1;
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
    case I_OIL:
    case I_FOOD:
        return throw_it(k);
    }
    if (k >= I_FLASK) return quaff(k - I_FLASK);
    if (k >= I_FLINT) return light(k);
    return feed(k);
}

/* n of item put down at (x, y), or on a free place next to it (a floor,
 * nothing else lying there; a pile of the same grows); 0 if there is none */
static const signed char near_x[9] = {0, 1, -1, 0, 0, 1, 1, -1, -1};
static const signed char near_y[9] = {0, 0, 0, 1, -1, 1, -1, 1, -1};

unsigned char put_down(unsigned char x, unsigned char y, unsigned char item, unsigned char n) {
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
#define INV_H   30
#define INV_ROWS 16                   /* the items' rows (more scroll) */

unsigned char inventory(void) {
    unsigned char k, n, sel = 0, top = 0, has[I_COUNT];
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
    video_text(INV_COL + 3, INV_ROW + 26, buf, C_GOLD);
    for (k = 0, n = 0; k < I_COUNT; k++)
        if (pl.items[k]) has[n++] = k;
    if (!n) {
        video_text(INV_COL + 3, INV_ROW + 9, "Nothing else.", C_DIM);
        video_text(INV_COL + 3, INV_ROW + 28, "Any key: back.", C_DIM);
        key_wait();
        view_draw();
        return 0;
    }
    video_text(INV_COL + 3, INV_ROW + 28, "RETURN: use.  D: put down.  I: back.", C_DIM);
    for (;;) {
        /* (more than fit: the list scrolls with the choice) */
        if (sel < top) top = sel;
        if (sel >= top + INV_ROWS) top = sel - INV_ROWS + 1;
        for (k = top; k < n && k < top + INV_ROWS; k++) {
            buf[0] = k == sel ? '>' : ' ';
            buf[1] = ' ';
            buf[2] = 0;
            str_num(buf, pl.items[has[k]]);
            str_cat(buf, " ");
            str_cat(buf, item_name(has[k], pl.items[has[k]]));
            video_blank(INV_COL + 2, INV_ROW + 9 + k - top, INV_W - 4, 1);
            video_text(INV_COL + 2, INV_ROW + 9 + k - top, buf, k == sel ? C_HILITE : item_col[has[k]]);
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

/* ---------------------------------------------------------------- growing */

/* a level's gift: three of these offered (the last four, up to GIFT_MAX
 * each) */
#define GIFTS 6

static unsigned char *gift_of(unsigned char g) {
    return g == 2 ? &pl.keen : g == 3 ? &pl.steady : g == 4 ? &pl.aim : &pl.wary;
}

static void gift_take(unsigned char g) {
    if (g == 0) {
        pl.maxhp += 6;
        pl.hp = pl.maxhp;
    } else if (g == 1) {
        pl.maxmp += 4;
        pl.mp = pl.maxmp;
    } else (*gift_of(g))++;
}

#define UP_COL 14
#define UP_ROW 12
#define UP_W   52
#define UP_H   12

void level_up(void) {
    unsigned char k, n, j, sel, offer[3], can[GIFTS];
    char buf[64];
    while (pl.ups) {
        /* three different gifts of those still to be had */
        for (k = 0, n = 0; k < GIFTS; k++)
            if (k < 2 || *gift_of(k) < GIFT_MAX) can[n++] = k;
        for (k = 0; k < 3; k++) {
            j = k + rng_range(n - k);
            offer[k] = can[j];
            can[j] = can[k];
            can[k] = offer[k];
        }
        video_blank(UP_COL, UP_ROW, UP_W, UP_H);
        ui_frame(UP_COL, UP_ROW, UP_W, UP_H);
        buf[0] = 0;
        str_cat(buf, "You have grown. Choose a gift:");
        video_text(UP_COL + 3, UP_ROW + 1, buf, C_GOLD);
        video_text(UP_COL + 3, UP_ROW + 10, "Up, down: choose.  RETURN: take it.", C_DIM);
        sel = 0;
        for (;;) {
            for (k = 0; k < 3; k++) {
                video_blank(UP_COL + 2, UP_ROW + 3 + k * 2, UP_W - 4, 1);
                video_text(UP_COL + 2, UP_ROW + 3 + k * 2, k == sel ? ">" : " ", C_HILITE);
                video_text(UP_COL + 4, UP_ROW + 3 + k * 2,
                           nth("Hardy\0Deep well\0Keen memory\0Steady torch\0Sure aim\0Wary feet", offer[k]),
                           k == sel ? C_HILITE : C_TEXT);
                video_text(UP_COL + 18, UP_ROW + 3 + k * 2,
                           nth("six more life\0four more mana\0what you see fades slower\0"
                               "your torch burns slower\0your shots fly truer and harder\0"
                               "you find and dodge traps", offer[k]), C_DIM);
            }
            k = key_wait();
            if (k == K_UP) sel = sel ? sel - 1 : 2;
            else if (k == K_DOWN) sel = sel < 2 ? sel + 1 : 0;
            else if (k == K_FIRE) break;
        }
        gift_take(offer[sel]);
        pl.ups--;
        buf[0] = 0;
        str_cat(buf, nth("Hardy\0Deep well\0Keen memory\0Steady torch\0Sure aim\0Wary feet", offer[sel]));
        str_cat(buf, ": ");
        str_cat(buf, nth("you are hardier.\0your mana runs deeper.\0your memory sharpens.\0"
                         "you tend your torch better.\0your aim steadies.\0your feet grow wary.", offer[sel]));
        msg(buf, C_GOLD);
        sfx(SFX_LEVEL);
    }
    view_draw();
    view_hud();
}

