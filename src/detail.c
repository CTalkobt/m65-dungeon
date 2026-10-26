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

void chest_view(unsigned int at) {
    unsigned char r, d, line = TEXT_ROW + 3;
    char buf[64];
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
    r = rng_range(100);
    buf[0] = 0;
    if (r < 22 && pl.weapon < 4) {
        pl.weapon++;
        pl.wplus += 2;
        if (pl.weapon == 2) pl.wsides = 10;
        if (pl.weapon == 4) pl.wdice = 2;
        str_cat(buf, "Wrapped in oilcloth: a ");
        str_cat(buf, weapon_name[pl.weapon]);
        str_cat(buf, ". You take it up.");
    } else if (r < 40 && pl.armour < 4) {
        pl.armour++;
        pl.def += 1;
        str_cat(buf, "Folded at the bottom: ");
        str_cat(buf, armour_name[pl.armour]);
        str_cat(buf, ". You put it on.");
    } else if (r < 62) {
        d = 1 + rng_range(2);
        pl.potions += d;
        str_cat(buf, "Stoppered flasks of red cordial: ");
        str_num(buf, d);
        str_cat(buf, d > 1 ? " healing potions." : " healing potion.");
    } else {
        d = (10 + rng_range(30)) * pl.depth;
        pl.gold += d;
        str_cat(buf, "Old coins, cold to the touch: ");
        str_num(buf, d);
        str_cat(buf, " gold.");
    }
    ui_wrap(TEXT_COL, line, TEXT_W, buf, C_GOLD);
    if (r >= 62 && rng_range(3) == 0) {
        pl.potions++;
        ui_wrap(TEXT_COL, line + 2, TEXT_W, "And beneath them, a healing potion.", C_GOLD);
    }
    leave();
}

void idol_view(unsigned int at) {
    unsigned int i;
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
        for (i = 0; i < MAP_N; i++)
            if (seen[i]) seen[i] = epoch();
        s = "The halls you have walked blaze again in your mind.";
        break;
    case 1:
        for (i = 0; i < MAP_N; i++)
            if ((map[i] & F_BOUND) || TERRAIN(i) == TR_STAIRS || TERRAIN(i) == TR_DOOR) seen[i] = epoch();
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
