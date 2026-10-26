/* ui.c — keys and joysticks, the panels' frames, bars, wrapped text, the
 * log of a battle or a view, the player's numbers
 */
#include <input.h>
#include "hw.h"
#include "assets.h"
#include "video.h"
#include "game.h"

/* ---------------------------------------------------------------- keys */

static unsigned char joy_last, joy_hold;

/* a key as ASCII lower case (letters either way), or K_*; a joystick in
 * either port as K_* (held: repeating) */
#ifdef SCRIPT
/* the scripted keys (tests): U D L R the cursor keys, F RETURN, X RUN/STOP,
 * _ a pause; one every 6 frames, then the keyboard */
static const char *script = SCRIPT;
static unsigned char script_wait = 120;

static unsigned char scripted(void) {
    unsigned char c;
    if (!*script) return 0;
    if (script_wait) {
        script_wait--;
        return 0xFF;
    }
    c = (unsigned char)*script++;
    script_wait = 6;
    switch (c) {
    case 'U': return K_UP;
    case 'D': return K_DOWN;
    case 'L': return K_LEFT;
    case 'R': return K_RIGHT;
    case 'F': return K_FIRE;
    case 'X': return K_STOP;
    case '_': script_wait = 60; return 0xFF;
    }
    return c;
}
#endif

unsigned char key_get(void) {
    unsigned char k, j, d;
#ifdef SCRIPT
    k = scripted();
    if (k == 0xFF) return 0;
    if (k) return k;
#endif
    k = input_key();
    if (k) {
        if (k >= 0x41 && k <= 0x5A) return k + 0x20;      /* PETSCII a-z */
        if (k >= 0xC1 && k <= 0xDA) return k - 0x60;      /* PETSCII A-Z */
        switch (k) {
        case CH_CURSOR_UP: return K_UP;
        case CH_CURSOR_DOWN: return K_DOWN;
        case CH_CURSOR_LEFT: return K_LEFT;
        case CH_CURSOR_RIGHT: return K_RIGHT;
        case CH_RETURN: return K_FIRE;
        case CH_STOP:
        case CH_ESC: return K_STOP;
        }
        return k;
    }
    j = input_joy(1) | input_joy(2);
    if (!j) {
        joy_last = 0;
        return 0;
    }
    d = j & JOY_FIRE ? K_FIRE : j & JOY_UP ? K_UP : j & JOY_DOWN ? K_DOWN : j & JOY_LEFT ? K_LEFT : j & JOY_RIGHT ? K_RIGHT : 0;
    if (j != joy_last) {
        joy_last = j;
        joy_hold = 18;
        return d;
    }
    if (d == K_FIRE) return 0;                     /* fire does not repeat */
    if (--joy_hold == 0) {
        joy_hold = 7;
        return d;
    }
    return 0;
}

unsigned char key_wait(void) {
    unsigned char k;
    for (;;) {
        video_frame();
        sfx_update();
        k = key_get();
        if (k) return k;
    }
}

/* ---------------------------------------------------------------- strings */

void str_cat(char *d, const char *s) {
    while (*d) d++;
    while (*s) *d++ = *s++;
    *d = 0;
}

/* the nth (0, 1, ...) of strings one after another in a literal, each
 * ending in its NUL: a table of strings that stays with the code that has
 * the literal (an overlay's, under noescape_strings), not in bank 0 */
const char *nth(const char *list, unsigned char n) {
    while (n--)
        while (*list++) {}
    return list;
}

void str_num(char *d, unsigned int v) {
    char buf[6];
    unsigned char i = 5;
    buf[5] = 0;
    do {
        buf[--i] = (char)(0x30 + v % 10);
        v /= 10;
    } while (v);
    str_cat(d, buf + i);
}

/* ---------------------------------------------------------------- frames */

static unsigned int line[COLS];

void ui_hline(unsigned char col, unsigned char row, unsigned char w) {
    unsigned char i;
    for (i = 0; i < w; i++) line[i] = UI_F_H;
    video_cells(col, row, line, w);
}

void ui_frame(unsigned char col, unsigned char row, unsigned char w, unsigned char h) {
    unsigned char i;
    unsigned int c;
    line[0] = UI_F_TL;
    for (i = 1; i < w - 1; i++) line[i] = UI_F_H;
    line[w - 1] = UI_F_TR;
    video_cells(col, row, line, w);
    line[0] = UI_F_BL;
    line[w - 1] = UI_F_BR;
    video_cells(col, row + h - 1, line, w);
    c = UI_F_V;
    for (i = 1; i < h - 1; i++) {
        video_cells(col, row + i, &c, 1);
        video_cells(col + w - 1, row + i, &c, 1);
    }
}

void ui_bar(unsigned char col, unsigned char row, unsigned char w, unsigned char v, unsigned char max, unsigned int bar0) {
    unsigned int fill = max ? (unsigned int)((unsigned long)v * w * 8 / max) : 0;
    unsigned char i;
    if (v && !fill) fill = 1;
    for (i = 0; i < w; i++) {
        line[i] = bar0 + (fill >= 8 ? 8 : fill);
        fill = fill >= 8 ? fill - 8 : 0;
    }
    video_cells(col, row, line, w);
}

/* text in a box w wide from (col, row), broken between words; \n starts a
 * new line */
void ui_wrap(unsigned char col, unsigned char row, unsigned char w, const char *s, unsigned char colour) {
    char buf[COLS + 1];
    unsigned char n, brk;
    while (*s) {
        n = 0;
        brk = 0;
        while (s[n] && s[n] != '\n' && n < w) {
            if (s[n] == ' ') brk = n;
            n++;
        }
        if (s[n] && s[n] != '\n' && s[n] != ' ' && brk) n = brk;
        for (brk = 0; brk < n; brk++) buf[brk] = s[brk];
        buf[n] = 0;
        video_text(col, row++, buf, colour);
        s += n;
        while (*s == ' ' || *s == '\n') {
            if (*s == '\n' && s[1] == '\n') row++;
            s++;
        }
    }
}

/* the player's numbers, 4 rows from (col, row), 22 wide */
void ui_stats(unsigned char col, unsigned char row) {
    char buf[24];
    video_blank(col, row, 23, 4);
    video_text(col, row, "Life", C_TEXT);
    ui_bar(col + 5, row, 10, pl.hp, pl.maxhp, UI_BAR_R0);
    buf[0] = 0;
    str_num(buf, pl.hp);
    str_cat(buf, "/");
    str_num(buf, pl.maxhp);
    video_text(col + 16, row, buf, pl.hp * 4 < pl.maxhp ? C_RED : C_TEXT);
    video_text(col, row + 1, "Mana", C_TEXT);
    ui_bar(col + 5, row + 1, 10, pl.mp, pl.maxmp, UI_BAR_B0);
    buf[0] = 0;
    str_num(buf, pl.mp);
    str_cat(buf, "/");
    str_num(buf, pl.maxmp);
    video_text(col + 16, row + 1, buf, C_BLUE);
    buf[0] = 0;
    str_cat(buf, "Gold ");
    str_num(buf, pl.gold);
    video_text(col, row + 2, buf, C_GOLD);
    buf[0] = 0;
    str_cat(buf, "Potions ");
    str_num(buf, pl.potions);
    video_text(col + 12, row + 2, buf, C_RED);
    buf[0] = 0;
    str_cat(buf, "Level ");
    str_num(buf, pl.level);
    str_cat(buf, "  XP ");
    str_num(buf, pl.xp);
    video_text(col, row + 3, buf, C_DIM);
}

/* ---------------------------------------------------------------- the log */

#define LOG_MAX 12
#define LOG_W   52
static unsigned char log_c, log_r, log_w, log_h;
static char logs[LOG_MAX][LOG_W + 1];
static unsigned char log_cols[LOG_MAX];

void log_init(unsigned char col, unsigned char row, unsigned char w, unsigned char h) {
    unsigned char k;
    log_c = col;
    log_r = row;
    log_w = w > LOG_W ? LOG_W : w;
    log_h = h > LOG_MAX ? LOG_MAX : h;
    for (k = 0; k < LOG_MAX; k++) logs[k][0] = 0;
}

static void log_line(const char *s, unsigned char n, unsigned char colour) {
    unsigned char k, i;
    for (k = 0; k < log_h - 1; k++) {
        for (i = 0; i <= LOG_W; i++) logs[k][i] = logs[k + 1][i];
        log_cols[k] = log_cols[k + 1];
        if (log_cols[k] != C_DARK && k < log_h - 3) log_cols[k] = C_DIM;
    }
    for (i = 0; i < n; i++) logs[log_h - 1][i] = s[i];
    logs[log_h - 1][n] = 0;
    log_cols[log_h - 1] = colour;
}

/* a message, broken between words into lines as wide as the log */
void log_add(const char *s, unsigned char colour) {
    unsigned char n, brk, k;
    while (*s) {
        n = 0;
        brk = 0;
        while (s[n] && n < log_w) {
            if (s[n] == ' ') brk = n;
            n++;
        }
        if (s[n] && s[n] != ' ' && brk) n = brk;
        log_line(s, n, colour);
        s += n;
        while (*s == ' ') s++;
    }
    video_blank(log_c, log_r, log_w, log_h);
    for (k = 0; k < log_h; k++)
        if (logs[k][0]) video_text(log_c, log_r + k, logs[k], log_cols[k]);
}
