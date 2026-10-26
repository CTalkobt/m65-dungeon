/* front.c — the title and the endings (the overlay "scenes") */
#pragma cc45 overlay(scenes)
#pragma cc45 noescape_strings
#include "hw.h"
#include "assets.h"
#include "video.h"
#include "rng.h"
#include "game.h"

unsigned char title(void) {
    unsigned char k;
    unsigned int t = 0;
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
    video_centre(44, "RETURN or FIRE to begin        RUN/STOP to leave", C_HILITE);
    video_centre(47, "A game for the MEGA65, written in C with cc45", C_DARK);
    video_fade(16);
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

