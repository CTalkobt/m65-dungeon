/* levelmap.c — the level as remembered, a place a character, and the
 * notes the player puts on it (M on the map)
 *
 * The whole level (64 x 48) fills the screen: what is remembered (a place
 * seen and not yet forgotten) as small glyphs, the player, the notes. A
 * cursor moves over it; RETURN puts a note on its place, or the next kind,
 * or takes it off again. The notes stay however the memory fades, and are
 * shown on the map's view too (view.c). (The overlay "scenes": it has the
 * room.)
 */
#pragma cc45 overlay(scenes)
#pragma cc45 noescape_strings
#include "hw.h"
#include "assets.h"
#include "video.h"
#include "game.h"

#define LM_COL 8
#define LM_ROW 1

unsigned char note_kind[MAX_NOTES];          /* game.h */
unsigned int note_at[MAX_NOTES];

static unsigned char note_find(unsigned int at) {
    unsigned char k;
    for (k = 0; k < MAX_NOTES; k++)
        if (note_kind[k] && note_at[k] == at) return k;
    return 0xFF;
}

/* a place's glyph: its note, the player, what is remembered, or nothing */
static unsigned int glyph(unsigned char x, unsigned char y) {
    unsigned int at = AT(x, y);
    unsigned char k = note_find(at);
    if (k != 0xFF) return '!';               /* (drawn as text: note_draw) */
    if (x == pl.x && y == pl.y) return UI_MAP_PLAYER;
    if (!seen_get(at)) return ' ';
    k = vm_kind(x, y);
    if (k == TK_TRAP) return UI_MAP_TRAP;
    switch (terrain(x, y)) {
    case TR_FLOOR: return UI_MAP_FLOOR;
    case TR_CORR: return UI_MAP_CORR;
    case TR_DOOR: return UI_MAP_DOOR;
    case TR_OPEN: return UI_MAP_OPEN;
    case TR_STAIRS: return UI_MAP_STAIRS;
    case TR_LOCKED: return UI_MAP_LOCKED;
    case TR_RUBBLE: return UI_MAP_RUBBLE;
    }
    return IS_WALL(k) || k == TK_SECRET_H || k == TK_SECRET_V ? UI_MAP_WALL : ' ';
}

static const unsigned char note_col[5] = {0, C_RED, C_GOLD, C_CYAN, C_PURPLE};

static void draw_at(unsigned char x, unsigned char y) {
    unsigned int g = glyph(x, y);
    char s[2];
    if (g == '!') {
        g = note_kind[note_find(AT(x, y))];
        s[0] = nth("!\0$\0*\0?", (unsigned char)(g - 1))[0];
        s[1] = 0;
        video_text(LM_COL + x, LM_ROW + y, s, note_col[g]);
    } else if (g == ' ') video_blank(LM_COL + x, LM_ROW + y, 1, 1);
    else video_cells(LM_COL + x, LM_ROW + y, &g, 1);
}

void level_map(void) {
    unsigned char x, y, cx = pl.x, cy = pl.y, k, f = 0, i;
    unsigned int cur = UI_MAP_CURSOR;
    char buf[64];
    video_clear();
    buf[0] = 0;
    str_cat(buf, depth_name(pl.depth));
    str_cat(buf, ", as you remember it");
    video_text(LM_COL, 0, buf, C_GOLD);
    video_text(LM_COL, LM_ROW + MAP_H, "Cursor: move.  RETURN: a note (! $ * ?).  M: back.", C_DIM);
    for (y = 0; y < MAP_H; y++) {
        video_poll();
        for (x = 0; x < MAP_W; x++) draw_at(x, y);
    }
    for (;;) {
        /* the cursor blinks on its place */
        for (;;) {
            if ((f & 15) == 0) video_cells(LM_COL + cx, LM_ROW + cy, &cur, 1);
            else if ((f & 15) == 8) draw_at(cx, cy);
            f++;
            video_frame();
            sfx_update();
            k = key_get();
            if (k) break;
        }
        draw_at(cx, cy);
        f = 0;
        if ((k == K_UP || k == 'w') && cy) cy--;
        else if ((k == K_DOWN || k == 's') && cy < MAP_H - 1) cy++;
        else if ((k == K_LEFT || k == 'a') && cx) cx--;
        else if ((k == K_RIGHT || k == 'd') && cx < MAP_W - 1) cx++;
        else if (k == K_FIRE) {
            /* a note here: the next kind, or none; else a new one */
            i = note_find(AT(cx, cy));
            if (i != 0xFF) {
                if (++note_kind[i] > NOTE_KINDS) note_kind[i] = 0;
            } else {
                for (i = 0; i < MAX_NOTES && note_kind[i]; i++) {}
                if (i < MAX_NOTES) {
                    note_kind[i] = 1;
                    note_at[i] = AT(cx, cy);
                } else video_text(LM_COL, 0, "No room for more notes: take one off first.", C_RED);
            }
            sfx(SFX_COIN);
        } else if (k == 'm' || k == K_STOP) return;
    }
}
