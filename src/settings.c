/* settings.c — the music's and the effects' levels, on and off: a panel
 * over whatever is on the screen (the caller draws it again after) (the
 * overlay "scenes") */
#pragma cc45 overlay(scenes)
#pragma cc45 noescape_strings
#include "hw.h"
#include "assets.h"
#include "video.h"
#include "game.h"

#define SET_W 44
#define SET_H 10

void settings(unsigned char col, unsigned char row) {
    static unsigned char was[2] = {15, 15};      /* a level to turn back on to */
    unsigned char sel = 0, k, *v;
    video_blank(col, row, SET_W, SET_H);
    ui_frame(col, row, SET_W, SET_H);
    video_text(col + 3, row + 1, "Settings", C_GOLD);
    video_text(col + 3, row + 6, "Up, down: choose.  Left, right: level.", C_DIM);
    video_text(col + 3, row + 7, "RETURN: off or on.  RUN/STOP: done.", C_DIM);
    for (;;) {
        for (k = 0; k < 2; k++) {
            v = k ? &vol_sfx : &vol_music;
            video_text(col + 3, row + 3 + k, k == sel ? ">" : " ", C_HILITE);
            video_text(col + 5, row + 3 + k, k ? "Sounds" : "Music", k == sel ? C_HILITE : C_TEXT);
            ui_bar(col + 13, row + 3 + k, 15, *v, 15, UI_BAR_G0);
            video_blank(col + 30, row + 3 + k, 4, 1);
            if (*v) video_num(col + 30, row + 3 + k, *v, C_TEXT);
            else video_text(col + 30, row + 3 + k, "off", C_RED);
        }
        k = key_wait();
        v = sel ? &vol_sfx : &vol_music;
        if (k == K_UP || k == K_DOWN) sel ^= 1;
        else if (k == K_LEFT && *v) (*v)--;
        else if (k == K_RIGHT && *v < 15) (*v)++;
        else if (k == K_FIRE) {
            if (*v) {
                was[sel] = *v;
                *v = 0;
            } else *v = was[sel];
        } else if (k == K_STOP || k == 'o' || k == 's') break;
        else continue;
        sound_levels();
        if (sel && k != K_UP && k != K_DOWN) sfx(SFX_COIN);   /* to hear the level */
    }
}
