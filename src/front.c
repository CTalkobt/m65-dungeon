/* front.c — the title and the endings (the overlay "scenes") */
#pragma cc45 overlay(scenes)
#pragma cc45 noescape_strings
#include "hw.h"
#include "assets.h"
#include "video.h"
#include "rng.h"
#include "game.h"

static unsigned char shown;                   /* the title is on the screen */

/* the title screen, without the prompt (title() adds it) */
void title_show(void) {
    video_fade(0);
    v_flicker = 0;
    v_fire = 0;
    scene_load(S_LICH);
    video_clear();
    ui_frame(11, 0, 58, 32);
    ui_frame(0, 33, 80, 17);
    scene_show(0);
    video_centre(35, "T H E    F A D I N G    D E E P", C_GOLD);
    video_centre(37, "Beneath the ruined abbey the old halls go down, and down.", C_TEXT);
    video_centre(38, "At the bottom the lich who emptied the valley keeps its vigil.", C_TEXT);
    video_centre(40, "Take your torch. Mind the dark. And remember -", C_DIM);
    video_centre(41, "the deep makes you forget.", C_DIM);
    video_centre(47, "A game for the MEGA65, written in C with cc45", C_DARK);
    video_fade(16);
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

unsigned char title(void) {
    unsigned char k;
    unsigned int t = 0;
    if (!shown) title_show();
    shown = 0;
    video_blank(1, 44, 78, 1);
    video_centre(44, "RETURN or FIRE to begin        RUN/STOP to leave", C_HILITE);
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
        if (k == K_FIRE || k == ' ') break;
        if (k == K_STOP) return 0;
    }
#ifndef SEED
    rng_state ^= t * 31 + REG(0xD012);
    if (!rng_state) rng_state = 0xACE1;
#endif
    sfx(SFX_BELL);
    video_fade(0);
    return 1;
}

/* the end: fallen, or the lich destroyed */
void ending(unsigned char how) {
    char buf[64];
    video_fade(0);
    v_flicker = 0;
    v_fire = 0;
    if (how == WON) {
        scene_load(S_CHEST);
        video_clear();
        ui_frame(11, 0, 58, 32);
        ui_frame(0, 33, 80, 17);
        scene_show(1);
        video_centre(35, "The lich is destroyed.", C_GOLD);
        video_centre(37, "Its hoard lies open before you. Far above, dawn comes to the valley,", C_TEXT);
        video_centre(38, "and for the first time in a hundred years, the abbey bells ring.", C_TEXT);
    } else {
        scene_load(S_SKELETON);
        video_clear();
        ui_frame(11, 0, 58, 32);
        ui_frame(0, 33, 80, 17);
        scene_show(0);
        video_centre(35, "Your torch gutters out.", C_RED);
        video_centre(37, "In time your bones will join the others in the dark,", C_TEXT);
        video_centre(38, "and something will rise to guard them.", C_TEXT);
    }
    buf[0] = 0;
    str_cat(buf, "Depth ");
    str_num(buf, pl.depth);
    str_cat(buf, "   Level ");
    str_num(buf, pl.level);
    str_cat(buf, "   Gold ");
    str_num(buf, pl.gold);
    str_cat(buf, "   Turns ");
    str_num(buf, turn);
    video_centre(42, buf, C_BONE);
    video_centre(46, "Press a key.", C_DARK);
    video_fade(16);
    sfx(how == WON ? SFX_VICTORY : SFX_DEFEAT);
    video_frames(50);
    while (key_get()) {}
    key_wait();
}

