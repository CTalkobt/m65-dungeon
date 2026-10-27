/* detail.c — the views of a chest and an idol (the overlay "scenes") */
#pragma cc45 overlay(scenes)
#pragma cc45 noescape_strings
#include "hw.h"
#include "assets.h"
#include "video.h"
#include "rng.h"
#include "game.h"

#define TEXT_COL 3
#define TEXT_ROW 35
#define TEXT_W 74

static void pause(unsigned char frames) {
    while (frames--) {
        video_frame();
        sfx_update();
    }
}

/* the view's screen: the picture framed, a box of text below */
static void view_open(unsigned char s) {
    video_fade(0);
    v_flicker = 0;
    v_fire = 0;
    scene_load(s);
    video_clear();
    ui_frame(11, 0, 58, 32);
    ui_frame(0, 33, 80, 17);
    scene_show(0);
}

static void choices(const char *s) {
    video_blank(1, 47, 78, 1);
    video_centre(47, s, C_HILITE);
}

static unsigned char ask(const char *yes) {
    unsigned char k;
    for (;;) {
        k = key_wait();
        if (k == yes[0] || k == K_FIRE) return 1;
        if (k == 'l' || k == K_STOP) return 0;
    }
}

static void wash(unsigned char r, unsigned char g, unsigned char b, unsigned char k) {
    v_wash_r = r;
    v_wash_g = g;
    v_wash_b = b;
    for (v_wash = k; v_wash; v_wash--) pause(1);
}

static void leave(void) {
    choices("Press a key.");
    key_wait();
    video_fade(0);
}

/* one find in a chest, its line of text at line; the next line. good: the
 * weapons, the armour and the potions more likely */
static unsigned char find(unsigned char line, unsigned char good) {
    unsigned char r = rng_range(good ? 60 : 100), d, i;
    char buf[80];
    buf[0] = 0;
    if (r < 18 && pl.weapon < WEAPONS - 1) {
        weapon_take(pl.weapon + 1);
        str_cat(buf, "Wrapped in oilcloth: a ");
        str_cat(buf, weapon_name(pl.weapon));
        str_cat(buf, ". You take it up.");
    } else if (r < 30 && pl.armour < ARMOURS - 1) {
        armour_take(pl.armour + 1);
        str_cat(buf, "Folded at the bottom: ");
        str_cat(buf, armour_name(pl.armour));
        str_cat(buf, ". You put it on.");
    } else if (r < 38 && (pl.bow < RANGED_BEST || pl.gun < RANGED_BEST)) {
        /* the bow first, then a flintlock, each with something to shoot */
        i = pl.bow < RANGED_BEST ? 0 : 1;
        d = i ? ++pl.gun : ++pl.bow;
        pl.items[i ? I_SHOT : I_ARROWS] += i ? 4 : 8;
        str_cat(buf, "Under a rotten cloak: a ");
        str_cat(buf, ranged_name(i, d));
        str_cat(buf, i ? ", and powder and ball." : ", and a sheaf of arrows.");
    } else if (r < 44) {
        i = I_FLASK + rng_range(FLASKS);
        pl.items[i]++;
        str_cat(buf, "Wrapped in straw: a ");
        str_cat(buf, item_name(i, 1));
        str_cat(buf, ".");
    } else if (r < 48) {
        d = 1 + rng_range(2);
        pl.items[I_POTION] += d;
        str_cat(buf, "Stoppered flasks of red cordial: ");
        str_num(buf, d);
        str_cat(buf, " ");
        str_cat(buf, item_name(I_POTION, d));
        str_cat(buf, ".");
    } else if (r < 52) {
        pl.items[I_DRAUGHT]++;
        str_cat(buf, "A vial of blue glass, faintly humming: a mana draught.");
    } else if (r < 56) {
        pl.items[I_ANTIDOTE]++;
        str_cat(buf, "A slim vial, bitter-smelling: a vial of antidote.");
    } else if (r < 70) {
        r = rng_range(10);
        i = r < 4 ? I_FIRE : r < 7 ? I_RECALL : I_SIGHT;
        pl.items[i]++;
        str_cat(buf, "A bone scroll case, sealed with wax: a ");
        str_cat(buf, item_name(i, 1));
        str_cat(buf, ".");
    } else if (r < 78 || (r < 86 && !pl.bow && !pl.gun)) {
        r = rng_range(13);
        i = r < 3 ? I_RAGS : r < 5 ? I_STICKS : r < 7 ? I_FUR : r < 8 ? I_CANDLE : r < 9 ? I_FIRESTICK :
            r < 10 ? I_FLINT : r < 12 ? I_OIL : I_FOOD;
        d = i == I_FLINT ? 1 : 1 + rng_range(2);
        pl.items[i] += d;
        str_cat(buf, "Dry, wrapped in waxed cloth: ");
        str_num(buf, d);
        str_cat(buf, " ");
        str_cat(buf, item_name(i, d));
        str_cat(buf, ".");
    } else if (r < 86) {
        i = pl.gun && rng_range(2) ? I_SHOT : I_ARROWS;
        d = i == I_SHOT ? 3 + rng_range(3) : 5 + rng_range(6);
        pl.items[i] += d;
        str_cat(buf, "Bundled in oilskin: ");
        str_num(buf, d);
        str_cat(buf, " ");
        str_cat(buf, item_name(i, d));
        str_cat(buf, ".");
    } else {
        d = (10 + rng_range(30)) * pl.depth;
        pl.gold += d;
        str_cat(buf, "Old coins, cold to the touch: ");
        str_num(buf, d);
        str_cat(buf, " gold.");
    }
    ui_wrap(TEXT_COL, line, TEXT_W, buf, C_GOLD);
    return line + 2;
}

void chest_view(unsigned int at) {
    unsigned char d, good, line = TEXT_ROW + 3;
    view_open(S_CHEST);
    ui_wrap(TEXT_COL, TEXT_ROW, TEXT_W,
            "An iron-bound chest, its wood swollen black with damp. The lock is rusted, but it holds. "
            "Something shifts inside when you nudge it.", C_TEXT);
    choices("O: open it      L: leave it");
    video_fade(16);
    if (!ask("o")) {
        video_fade(0);
        return;
    }
    sfx(SFX_CHEST);
    if (rng_range(5) == 0) {
        d = 2 + rng_range(3) * pl.depth;
        pl.hp = pl.hp > d ? pl.hp - d : 1;
        sfx(SFX_TRAP);
        ui_wrap(TEXT_COL, line, TEXT_W, "A needle springs from the lock and bites your hand!", C_RED);
        line += 2;
        REG(0xD020) = C_BLOOD;
        video_shake(8);
        REG(0xD020) = 0;
    }
    pause(10);
    scene_show(1);
    sfx(SFX_COIN);
    wash(255, 220, 120, 14);
    obj_set(at, O_CHEST_OPEN);
    /* a vault's chests hold two finds, and the better ones */
    good = vault_at((unsigned char)(at % MAP_W), (unsigned char)(at / MAP_W));
    line = find(line, good);
    if (good) line = find(line, good);
    else if (rng_range(4) == 0) {
        pl.items[I_POTION]++;
        ui_wrap(TEXT_COL, line, TEXT_W, "And beneath it all, a healing potion.", C_GOLD);
    }
    leave();
}

void idol_view(unsigned int at) {
    unsigned char r;
    const char *s;
    unsigned char c = C_GREEN;
    view_open(S_IDOL);
    ui_wrap(TEXT_COL, TEXT_ROW, TEXT_W,
            "A squat idol of green-veined stone, four-armed and three-eyed. Guttering candles and "
            "old offerings lie at its feet. Its eyes seem to follow you.", C_TEXT);
    choices("P: pray to it      L: leave it be");
    video_fade(16);
    if (!ask("p")) {
        video_fade(0);
        return;
    }
    sfx(SFX_BELL);
    pause(20);
    scene_show(1);
    wash(140, 255, 190, 16);
    obj_set(at, O_IDOL_SPENT);
    r = rng_range(7);
    switch (r) {
    case 0:
        memory_renew();
        s = "The halls you have walked blaze again in your mind.";
        break;
    case 1:
        level_reveal();
        s = "The shape of this level unfolds behind your eyes.";
        break;
    case 2:
        pl.maxhp += 5;
        pl.hp = pl.maxhp;
        s = "Strength flows into you. Your wounds close.";
        break;
    case 3:
        pl.maxmp += 3;
        pl.mp = pl.maxmp;
        s = "Your mind clears; power gathers in your hands.";
        c = C_BLUE;
        break;
    case 4:
        pl.fade = 1;
        s = "Your memory grows keen. What you see, you will not soon forget.";
        c = C_CYAN;
        break;
    case 5:
        pl.fade = 4;
        s = "Its eyes flare. Your memories begin to slip away...";
        c = C_PURPLE;
        break;
    default:
        pl.hp = pl.hp > pl.maxhp / 4 ? pl.hp - pl.maxhp / 4 : 1;
        s = "It drinks from you. You feel cold and thin.";
        c = C_RED;
        REG(0xD020) = C_BLOOD;
        video_shake(8);
        REG(0xD020) = 0;
    }
    ui_wrap(TEXT_COL, TEXT_ROW + 4, TEXT_W, s, c);
    leave();
}
