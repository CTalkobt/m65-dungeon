/* pack.c — what the player carries, used on the map: the inventory (I),
 * potions and draughts, scrolls, and the torch fed (F) and lit (L) (the
 * overlay "pack") */
#pragma cc45 overlay(pack)
#pragma cc45 noescape_strings
#include "hw.h"
#include "assets.h"
#include "video.h"
#include "rng.h"
#include "game.h"

/* a message: msg is in the overlay "walk", which takes this one's place
 * in the region while it runs, text and all; so the text goes over in a
 * buffer of its own */
static char said[64];

static void say(const char *s, unsigned char c) {
    unsigned char n = 0;
    while ((said[n] = s[n]) != 0 && n < sizeof said - 1) n++;
    said[n] = 0;
    msg(said, c);
}

/* the torch fed with fuel item k (I_RAGS ..); 1 if it was (a turn) */
static const unsigned char burns_for[4] = {80, 120, 160, 220};

static unsigned char feed(unsigned char k) {
    if (pl.fuel > TORCH_MAX - 40) {
        say("Your torch has all it can hold.", C_DIM);
        return 0;
    }
    pl.items[k]--;
    pl.fuel += burns_for[k - I_RAGS];
    if (pl.fuel > TORCH_MAX) pl.fuel = TORCH_MAX;
    say(nth("You wrap the rags round the torch's head.\0You bind the sticks to the torch.\0"
            "You wrap the fur round the torch. It stinks.\0You melt the tallow onto the torch.", k - I_RAGS), C_ORANGE);
    if (!pl.lit) say("Now it wants lighting.", C_DIM);
    return 1;
}

/* the torch lit with k (I_FLINT, I_FIRESTICK, or I_COUNT: a brazier's
 * coals); 1 if that took the turn */
unsigned char light(unsigned char k) {
    if (pl.lit) {
        say("Your torch is burning.", C_DIM);
        return 0;
    }
    if (!pl.fuel) {
        say("There is nothing on the torch to burn. Feed it.", C_DIM);
        return 0;
    }
    if (k == I_FLINT && rng_range(3)) {
        say("Sparks fly, but the torch will not catch.", C_DIM);
        return 1;
    }
    if (k == I_FIRESTICK) pl.items[I_FIRESTICK]--;
    pl.lit = 1;
    say(nth("Sparks catch. Your torch flares up.\0The firestick flares, and the torch catches.\0"
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
    say("You have nothing to feed the torch.", C_DIM);
    return 0;
}

unsigned char light_any(void) {
    if (!pl.lit && pl.fuel && !pl.items[I_FLINT] && !pl.items[I_FIRESTICK]) {
        say("You have no flint and no firesticks.", C_DIM);
        return 0;
    }
    return light(pl.items[I_FLINT] ? I_FLINT : I_FIRESTICK);
}

/* a healing potion drunk; 1 if it was (a turn) */
unsigned char drink(void) {
    unsigned char d;
    if (!pl.items[I_POTION]) say("You have no healing potions.", C_DIM);
    else if (pl.hp == pl.maxhp) say("You are unhurt.", C_DIM);
    else {
        pl.items[I_POTION]--;
        d = 15 + pl.level * 2;
        pl.hp = pl.hp + d > pl.maxhp ? pl.maxhp : pl.hp + d;
        say("You drink. Warmth spreads through you.", C_GREEN);
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
            say("Your mana is full.", C_DIM);
            return 0;
        }
        pl.items[I_DRAUGHT]--;
        d = 10 + pl.level;
        pl.mp = pl.mp + d > pl.maxmp ? pl.maxmp : pl.mp + d;
        say("The draught hums on your tongue. Mana returns.", C_BLUE);
        sfx(SFX_DRINK);
        return 1;
    case I_FIRE:
        say("A scroll of fire is for a fight.", C_DIM);
        return 0;
    case I_RECALL:
        pl.items[I_RECALL]--;
        memory_renew();
        say("The halls you have walked come back to you.", C_CYAN);
        sfx(SFX_CHIME);
        return 1;
    case I_SIGHT:
        pl.items[I_SIGHT]--;
        level_reveal();
        say("The shape of this level unfolds in your mind.", C_CYAN);
        sfx(SFX_BELL);
        return 1;
    case I_KEY:
        say("Walk into a locked door to turn the key.", C_DIM);
        return 0;
    }
    if (k >= I_FLINT) return light(k);
    return feed(k);
}

/* what the player carries, over the map; a letter uses it. 1 if that took
 * the turn */
#define INV_COL 18
#define INV_ROW 5
#define INV_W   44
#define INV_H   26

unsigned char inventory(void) {
    unsigned char k, n = 0, row = INV_ROW + 7, has[I_COUNT];
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
    video_text(INV_COL + 3, INV_ROW + 5, buf, pl.lit ? C_ORANGE : C_RED);
    for (k = 0; k < I_COUNT; k++) {
        if (!pl.items[k]) continue;
        buf[0] = (char)('a' + n);
        buf[1] = buf[2] = ' ';
        buf[3] = 0;
        str_num(buf, pl.items[k]);
        str_cat(buf, " ");
        str_cat(buf, item_name(k, pl.items[k]));
        video_text(INV_COL + 3, row++, buf, item_col[k]);
        has[n++] = k;
    }
    if (!n) video_text(INV_COL + 3, row, "Nothing else.", C_DIM);
    buf[0] = 0;
    str_cat(buf, "Gold    ");
    str_num(buf, pl.gold);
    video_text(INV_COL + 3, INV_ROW + 21, buf, C_GOLD);
    video_text(INV_COL + 3, INV_ROW + 23, n ? "A letter: use it.  Another key: back." : "Any key: back.", C_DIM);
    k = key_wait();
    view_draw();
    if (k >= 'a' && k < 'a' + n) return use(has[k - 'a']);
    return 0;
}
