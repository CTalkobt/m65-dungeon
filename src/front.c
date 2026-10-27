/* front.c — the title and the endings (the overlay "front") */
#pragma cc45 overlay(front)
#pragma cc45 noescape_strings
#include "hw.h"
#include "assets.h"
#include "video.h"
#include "rng.h"
#include "game.h"
#include "load.h"
#include <dma.h>
#include <audio_tune.h>
#include "music.h"

static unsigned char shown;                   /* the title is on the screen */

/* the title screen, without the prompt (title() adds it) */
/* the box of words under the picture */
static void title_text(void) {
    video_blank(0, 33, 80, 17);
    ui_frame(0, 33, 80, 17);
    video_centre(35, "T H E    F A D I N G    D E E P", C_GOLD);
    video_centre(37, "Beneath the ruined abbey the old halls go down, and down.", C_TEXT);
    video_centre(38, "At the bottom the lich who emptied the valley keeps its vigil.", C_TEXT);
    video_centre(40, "Take your torch. Mind the dark. And remember -", C_DIM);
    video_centre(41, "the deep makes you forget.", C_DIM);
    video_centre(47, "A game for the MEGA65, written in C with cc45", C_DARK);
}

void title_show(void) {
    video_fade(0);
    v_flicker = 0;
    v_fire = 0;
    video_clear();
    ui_frame(11, 0, 58, 32);
    title_text();
    scene_load(S_LICH);
    v_wash_r = v_wash_g = v_wash_b = 0;
    v_wash = 16;
    scene_show(0);
    if (v_bright != 16) video_fade(16);
    for (; v_wash; v_wash--) video_frames(2);
    shown = 1;
}

/* while the sounds load (audio.c calls it often): a line of nonsense on
 * the title, another every second and a half or so. (One literal in the
 * code, so it stays in the overlay: bank 0 is full.) */
static unsigned char quip, quip_at = 0xFF;

void loading_tick(void) {
    const char *s = "Reticulating splines...\0"
                    "Sharpening the skeleton's sword...\0"
                    "Teaching the rats to squeak...\0"
                    "Filling the braziers...\0"
                    "Untangling the cobwebs...\0"
                    "Tuning the dripping water...\0"
                    "Hiding the treasure, badly...\0"
                    "Polishing the lich's crown...\0"
                    "Waking the spiders. Sorry...\0"
                    "Misplacing the stairs...\0"
                    "Feeding the ooze...\0"
                    "Rattling distant chains...\0"
                    "Remembering what you will soon forget...\0";
    const char *q = s;
    unsigned char f = FRAMECOUNT, k;
    if (quip_at != 0xFF && (unsigned char)(f - quip_at) < 80) return;
    quip_at = f;
    for (k = 0; k < quip; k++)
        while (*q++) {}
    if (!*q) {                                /* past the last (the two NULs): round again */
        quip = 0;
        q = s;
    }
    quip++;
    video_blank(1, 44, 78, 1);
    video_centre(44, q, C_DIM);
}

static void fame_show(void);

/* the prompt under the words: with Continue when there is a saved descent */
static unsigned char saved;

static void prompt(void) {
    video_blank(1, 44, 78, 1);
    video_centre(44, saved ? "RETURN: begin  C: continue  S: settings  F: the fallen  STOP: leave"
                           : "RETURN: begin    S: settings    F: the fallen    RUN/STOP: leave", C_HILITE);
}

/* ---------------------------------------------------------------- who goes down */

static unsigned char chosen;

/* the calling, on the title's lower panel: up, down, RETURN; 0xFF: back */
static unsigned char calling_choose(void) {
    unsigned char k, sel = chosen;
    video_blank(1, 34, 78, 15);
    video_centre(35, "Who goes down?", C_GOLD);
    video_centre(45, "Up, down: choose.   RETURN: begin.   RUN/STOP: back.", C_DIM);
    for (;;) {
        for (k = 0; k < CALLINGS; k++) {
            video_text(6, 38 + k * 2, k == sel ? ">" : " ", C_HILITE);
            video_text(8, 38 + k * 2, nth("Fighter\0Hunter\0Lamplighter", k), k == sel ? C_HILITE : C_TEXT);
            video_text(22, 38 + k * 2,
                       nth("Life 40, a long sword, studded leather. No bow.\0"
                           "A short bow, 24 arrows, a sure aim, and meat.\0"
                           "Fuel to spare, a flint, oil, and a keen memory.", k), k == sel ? C_TEXT : C_DIM);
        }
        k = key_wait();
        if (k == K_UP) sel = sel ? sel - 1 : CALLINGS - 1;
        else if (k == K_DOWN) sel = sel + 1 < CALLINGS ? sel + 1 : 0;
        else if (k == K_FIRE || k == ' ') return sel;
        else if (k == K_STOP) return 0xFF;
    }
}

void calling_take(void) {
    pl.calling = chosen;
    switch (chosen) {
    case CALL_FIGHTER:                        /* the front of the fight */
        pl.hp = pl.maxhp = 40;
        weapon_take(1);
        armour_take(1);
        pl.bow = 0;
        pl.items[I_ARROWS] = 0;
        pl.items[I_POTION]++;
        break;
    case CALL_HUNTER:                         /* from afar */
        pl.items[I_ARROWS] = 24;
        pl.aim = 1;
        pl.items[I_FOOD] = 2;
        break;
    default:                                  /* the light: more of it, longer */
        pl.hp = pl.maxhp = 26;
        pl.fuel = TORCH_MAX;
        pl.items[I_FLINT] = 1;
        pl.items[I_OIL] = 2;
        pl.items[I_RAGS] = 2;
        pl.keen = 1;
        break;
    }
}

/* 0: the player wants to leave; 1: a new descent; 2: the saved one (game_load) */
unsigned char title(void) {
    unsigned char k, r = 1;
    unsigned int t = 0;
    if (!shown) title_show();
    shown = 0;
    saved = game_peek();
    prompt();
    tune_play_far(MUSIC_LIN + TUNE_TITLE_AT);  /* the theme, now that the loading is done */
    v_music = 1;
    for (;;) {
        video_frame();
        sfx_update();
        t++;
        if ((t & 255) == 0) {
            scene_show(1);                    /* now and then the lich stirs */
            v_wash_r = 200;
            v_wash_g = 120;
            v_wash_b = 255;
            for (v_wash = 10; v_wash; v_wash--) video_frame();
            video_frames(40);
            scene_show(0);
        }
        k = key_get();
        if (k == K_FIRE || k == ' ') {
            k = calling_choose();
            if (k != 0xFF) {
                chosen = k;
                break;
            }
            title_text();
            prompt();
        }
        if (k == 'c' && saved) {
            r = 2;
            break;
        }
        if (k == 's') {
            settings(18, 36);
            title_text();
            prompt();
        }
        if (k == 'f') {
            fame_show();
            title_text();
            prompt();
        }
        if (k == K_STOP) {
            v_music = 0;
            tune_stop();
            return 0;
        }
    }
    v_music = 0;
    tune_stop();
#ifndef SEED
    rng_state ^= t * 31 + REG(0xD012);
    if (!rng_state) rng_state = 0xACE1;
#endif
    sfx(SFX_BELL);
    video_fade(0);
    return r;
}

/* ---------------------------------------------------------------- the hall of fame */

/* the five best descents, kept on the disk (DGFAME: built in and loaded
 * back into FAME_LIN, the long sounds' play slot, past keep.c's save):
 * the won first, then the deepest, the highest level, the richest */
#define FAMES    5
#define FAME_LIN 0x1C000UL
struct fame {
    unsigned char won, depth, level, cause;
    unsigned int turns, gold, kills;
};
static struct fame fame[FAMES];
static unsigned char fame_read;

static void fame_load(void) {
    const unsigned char __far *h = (const unsigned char __far *)FAME_LIN;
    fame_read = 1;
    audio_off();
    if (!load_file("DGFAME", 1)) return;
    if (h[0] == 'D' && h[1] == 'F' && h[2] == FAMES && h[3] == sizeof fame[0])
        dma_copy_far((unsigned long)(unsigned int)fame, FAME_LIN + 4, sizeof fame);
}

static void fame_save(void) {
    unsigned char __far *h = (unsigned char __far *)FAME_LIN;
    audio_off();
    h[0] = 'D';
    h[1] = 'F';
    h[2] = FAMES;
    h[3] = sizeof fame[0];
    dma_copy_far(FAME_LIN + 4, (unsigned long)(unsigned int)fame, sizeof fame);
    save_file("DGFAME", 1, (unsigned int)FAME_LIN, (unsigned int)FAME_LIN + 4 + sizeof fame);
}

/* 1 if a ranks above b */
static unsigned char better(struct fame *a, struct fame *b) {
    if (a->won != b->won) return a->won > b->won;
    if (a->depth != b->depth) return a->depth > b->depth;
    if (a->level != b->level) return a->level > b->level;
    return a->gold > b->gold;
}

/* this descent into the hall: its place (1-5), 0 if it is not good enough */
static unsigned char fame_add(unsigned char won) {
    struct fame f;
    unsigned char k, j;
    if (!fame_read) fame_load();
    f.won = won;
    f.depth = pl.depth;
    f.level = pl.level;
    f.cause = pl.cause;
    f.turns = turn;
    f.gold = pl.gold;
    f.kills = pl.kills;
    for (k = 0; k < FAMES; k++)
        if (!fame[k].depth || better(&f, &fame[k])) break;
    if (k == FAMES) return 0;
    for (j = FAMES - 1; j > k; j--) fame[j] = fame[j - 1];
    fame[k] = f;
    fame_save();
    return k + 1;
}

/* "a cave spider", "an ooze", "the lich" */
static void a_mon(char *buf, unsigned char kind) {
    const char *n = nth(mon_names, kind);
    if (mon_flags[kind] & MF_BOSS) str_cat(buf, "the ");
    else str_cat(buf, *n == 'a' || *n == 'e' || *n == 'i' || *n == 'o' || *n == 'u' ? "an " : "a ");
    str_cat(buf, n);
}

/* what ended it, in a few words */
static void cause_text(char *buf, unsigned char won, unsigned char cause) {
    if (won) str_cat(buf, "destroyed the lich");
    else if (cause >= CAUSE_SHOT) {
        str_cat(buf, "shot by ");
        a_mon(buf, cause - CAUSE_SHOT);
    } else if (cause >= CAUSE_SLAIN) {
        str_cat(buf, "slain by ");
        a_mon(buf, cause - CAUSE_SLAIN);
    } else str_cat(buf, cause == CAUSE_DART ? "a poisoned dart" : "gave up");
}

/* the hall, in the title's box of words (F) */
static void fame_show(void) {
    unsigned char k;
    char buf[80];
    if (!fame_read) fame_load();
    video_blank(1, 34, 78, 15);
    video_centre(35, "The Hall of the Deep", C_GOLD);
    for (k = 0; k < FAMES; k++) {
        buf[0] = 0;
        if (!fame[k].depth) {
            if (!k) video_centre(38, "No one has gone down yet.", C_DIM);
            break;
        }
        buf[0] = (char)('1' + k);
        buf[1] = '.';
        buf[2] = ' ';
        buf[3] = 0;
        if (fame[k].won) str_cat(buf, "The lich's bane");
        else {
            str_cat(buf, "Depth ");
            str_num(buf, fame[k].depth);
        }
        str_cat(buf, ", level ");
        str_num(buf, fame[k].level);
        str_cat(buf, ", ");
        str_num(buf, fame[k].turns);
        str_cat(buf, " turns, ");
        str_num(buf, fame[k].gold);
        str_cat(buf, " gold: ");
        cause_text(buf, fame[k].won, fame[k].cause);
        video_text(3, 37 + k * 2, buf, k ? C_TEXT : C_HILITE);
    }
    video_centre(47, "Press a key.", C_DARK);
    key_wait();
}

/* ---------------------------------------------------------------- the end */

/* the end: fallen, or the lich destroyed; then the tally, and the hall */
void ending(unsigned char how) {
    char buf[80];
    unsigned char k;
    game_unsave();                            /* the descent is over */
    video_fade(0);
    v_flicker = 0;
    v_fire = 0;
    if (how == WON) {
        /* the lich comes apart: it shudders, pales to dust, is gone */
        scene_load(S_LICH);
        video_clear();
        ui_frame(11, 0, 58, 32);
        ui_frame(0, 33, 80, 17);
        scene_show(1);
        video_centre(40, "The lich shrieks, and the shriek goes on, and on...", C_PURPLE);
        video_fade(16);
        sfx(SFX_CURSE);
        video_shake(24);
        v_wash_r = 200;
        v_wash_g = 190;
        v_wash_b = 170;
        for (v_wash = 0; v_wash < 16; v_wash++) video_frames(4);
        sfx(SFX_DEATH);
        video_frames(30);
        video_fade(0);
        v_wash = 0;
        scene_load(S_CHEST);
        video_clear();
        ui_frame(11, 0, 58, 32);
        ui_frame(0, 33, 80, 17);
        scene_show(1);
        video_centre(35, "The lich is dust.", C_GOLD);
        video_centre(37, "Its hoard lies open before you. Far above, dawn comes to the valley,", C_TEXT);
        video_centre(38, "and for the first time in a hundred years, the abbey bells ring.", C_TEXT);
    } else {
        scene_load(S_SKELETON);
        video_clear();
        ui_frame(11, 0, 58, 32);
        ui_frame(0, 33, 80, 17);
        scene_show(0);
        buf[0] = 0;
        if (pl.cause == CAUSE_QUIT) str_cat(buf, "You turn back from ");
        else {
            cause_text(buf, 0, pl.cause);
            buf[0] -= 0x20;                   /* (Slain, Shot, A poisoned dart) */
            str_cat(buf, pl.cause == CAUSE_DART ? " in " : ", in ");
        }
        str_cat(buf, depth_name(pl.depth));
        str_cat(buf, ".");
        video_centre(35, buf, C_RED);
        if (pl.cause == CAUSE_QUIT) {
            video_centre(37, "The dark keeps what it has. Perhaps another day.", C_TEXT);
        } else {
            video_centre(37, "In time your bones will join the others in the dark,", C_TEXT);
            video_centre(38, "and something will rise to guard them.", C_TEXT);
        }
    }
    /* the tally */
    buf[0] = 0;
    str_cat(buf, nth("The fighter.\0The hunter.\0The lamplighter.", pl.calling));
    str_cat(buf, "   Depth ");
    str_num(buf, pl.depth);
    str_cat(buf, "   Level ");
    str_num(buf, pl.level);
    str_cat(buf, "   Turns ");
    str_num(buf, turn);
    str_cat(buf, "   Gold ");
    str_num(buf, pl.gold);
    str_cat(buf, "   Slain ");
    str_num(buf, pl.kills);
    video_centre(41, buf, C_BONE);
    buf[0] = 0;
    for (k = 0; k < 4; k++) {
        unsigned char g = k == 0 ? pl.keen : k == 1 ? pl.steady : k == 2 ? pl.aim : pl.wary;
        if (!g) continue;
        str_cat(buf, buf[0] ? ", " : "Gifts: ");
        str_cat(buf, nth("keen memory\0steady torch\0sure aim\0wary feet", k));
        if (g > 1) {
            str_cat(buf, " x");
            str_num(buf, g);
        }
    }
    if (buf[0]) video_centre(42, buf, C_DIM);
    video_fade(16);
    sfx(how == WON ? SFX_VICTORY : SFX_DEFEAT);
    k = fame_add(how == WON);
    if (k) {
        buf[0] = 0;
        str_cat(buf, "Your descent is written in the Hall of the Deep: ");
        str_num(buf, k);
        str_cat(buf, k == 1 ? "st." : k == 2 ? "nd." : k == 3 ? "rd." : "th.");
        video_centre(44, buf, C_GOLD);
    }
    video_centre(47, "Press a key.", C_DARK);
    video_frames(50);
    while (key_get()) {}
    key_wait();
}
