/* help.c — every key, on a panel over the map (H or ? on the map). (The
 * overlay "scenes": it has the room.)
 */
#pragma cc45 overlay(scenes)
#pragma cc45 noescape_strings
#include "hw.h"
#include "assets.h"
#include "video.h"
#include <dma.h>
#include "game.h"

#define HP_COL 1
#define HP_ROW 1
#define HP_W   78
#define HP_H   44
#define KEY_COL (HP_COL + 5)
#define WHAT_COL (HP_COL + 23)        /* (the longest what: 51, to the frame) */

/* the lines: a heading (its key empty), or a key and what it does; each a
 * key, then what, one after another in the literal */
static const char __noescape lines[] =
    "\0On the map\0"
    "Cursor keys, WASD\0move\0"
    "Q E Z C, 7 9 1 3\0move diagonally\0"
    "SPACE, 5, .\0rest a turn, searching round about\0"
    "RETURN\0go down the stairs; off them, walk to them\0"
    "(walk into)\0open doors, chests; fountains; foes; disarm plates\0"
    "T / G\0shoot the bow / fire the flintlock (RETURN: loose)\0"
    "X\0throw a flask of oil or meat (O / M)\0"
    "I\0what you carry: RETURN uses it, D puts it down\0"
    "P\0drink a healing potion\0"
    "F / L\0feed the torch / light it\0"
    "M\0the level as you remember it, and notes on it\0"
    "O\0settings: the music's and the sounds' levels\0"
    "K\0shut a door beside you; shut, wedge it with sticks\0"
    "J\0the messages so far, to read again\0"
    "H, ?\0this help\0"
    "RUN/STOP\0save and stop (S), or give up (Y)\0"
    "\0In a fight\0"
    "S P C A R I T\0strike, parry, cast, advance, retreat, item, shoot\0"
    "Up, down, RETURN\0choose an action, and do it\0"
    "F I H W\0(casting) firebolt, frost, heal, ward\0"
    "H M F A\0(items) potion, draught, fire scroll, antidote\0"
    "B G\0(shooting) the bow, the flintlock\0"
    "RUN/STOP\0back out of the spells, items or shots\0"
    "\0On the title\0"
    "RETURN\0begin: then choose who goes down\0"
    "C, D, S, F\0continue; the day's descent; settings; the fallen\0";
#define LINES 27

void help_screen(void) {
    unsigned char k, row = HP_ROW + 3;
    const char *key, *what;
    video_blank(HP_COL, HP_ROW, HP_W, HP_H);
    ui_frame(HP_COL, HP_ROW, HP_W, HP_H);
    video_text(HP_COL + 3, HP_ROW + 1, "The keys", C_GOLD);
    for (k = 0; k < LINES; k++) {
        key = nth(lines, k * 2);
        what = nth(lines, k * 2 + 1);
        if (!*key) {
            row++;
            video_text(KEY_COL, row++, what, C_GOLD);
        } else {
            video_text(KEY_COL, row, key, C_HILITE);
            video_text(WHAT_COL, row++, what, C_TEXT);
        }
    }
    video_text(HP_COL + 3, HP_ROW + HP_H - 2, "Press a key.", C_DARK);
    key_wait();
}

/* the messages so far, oldest first, on the same panel (J on the map) */
void history_screen(void) {
    unsigned char k, i;
    char line[HIST_STRIDE];
    video_blank(HP_COL, HP_ROW, HP_W, HP_H);
    ui_frame(HP_COL, HP_ROW, HP_W, HP_H);
    video_text(HP_COL + 3, HP_ROW + 1, "What has happened", C_GOLD);
    if (!hist_n) video_text(KEY_COL, HP_ROW + 4, "Nothing yet.", C_DIM);
    for (k = 0; k < hist_n; k++) {
        i = (unsigned char)((hist_at + HIST_N - hist_n + k) % HIST_N);
        dma_copy_far((unsigned long)(unsigned int)line, HIST_LIN + (unsigned int)i * HIST_STRIDE, HIST_STRIDE);
        line[HIST_STRIDE - 1] = 0;
        video_text(KEY_COL, HP_ROW + 3 + k, line, k + 4 < hist_n ? (hist_col[i] == C_DARK ? C_DARK : C_DIM) : hist_col[i]);
    }
    video_text(HP_COL + 3, HP_ROW + HP_H - 2, "Press a key.", C_DARK);
    key_wait();
}
