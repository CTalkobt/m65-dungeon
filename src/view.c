/* view.c — what the player sees and remembers, and the exploration screen
 *
 * Sight: every place within SEE of the player with a clear line to it
 * (a Bresenham line through nothing solid). Within the torch's reach it is
 * lit, brighter the nearer; in a room with a brazier all of it is lit;
 * further and dark, it is in shadow: only what shines by itself shows
 * (eyes). A lit place is remembered (seen_set: the epoch).
 *
 * Memory: a place out of sight shows only its edges - the walls' faces,
 * the doors, the stairs - as faint lines, the shape of the place, fading
 * with the turns since it was seen (pl.fade: faster under a curse), then
 * gone.
 *
 * The map (game.h vm_*) holds every place's terrain as the screen draws
 * it: 16-colour characters, the same however it is lit. What the player
 * sees is in the colour RAM: each character's palette bank (gen_art.py
 * map_palette) is one of the torch's 4 levels (MAP_LIT), the memory's 3
 * (MAP_MEM: only the edges have colour) or dark. Each turn the rows in view
 * are copied from the map, the things, the creatures and the player put
 * over them as full-colour tiles where they are seen, and the colours
 * worked out, a character (a quarter of a place) at a time.
 *
 * The screen: row 0 the level's name, rows 1-44 the map (40 x 22 places of
 * 2 x 2 characters), row 45 a frame, rows 46-49 messages and the player.
 */
#pragma cc45 overlay(walk)
#pragma cc45 noescape_strings
#include <dma.h>
#include "hw.h"
#include "assets.h"
#include "video.h"
#include "game.h"

#define VIEW_W   40
#define VIEW_H   22
#define VIEW_ROW 1
#define SEE      11
#define MSG_ROW  46
#define MSG_W    53
#define STAT_COL 57

/* memory ages (turns x pl.fade) where it dims, and where it is gone */
#define MEM1 200
#define MEM2 500
#define MEM3 900

/* the map's palette banks (PAL_MAP on: gen_art.py map_palette) */
#define MAP_LIT  (PAL_MAP >> 4)          /* + 0-3: the torch, bright to dim */
#define MAP_MEM  (MAP_LIT + 4)           /* + 0-2: remembered, fading */
#define MAP_DARK (MAP_LIT + 7)
/* colour RAM's first byte for the map: 16 colours (bit 3), shown 8 pixels
 * wide (bit 2: 8 of the 16 trimmed); the bank in the second's high nibble */
#define NCM      0x0C
#define NCM_BANK(b) (NCM | (unsigned int)(b) << 12)

#define WIN (SEE * 2 + 1)

static unsigned char cam_x, cam_y;
static unsigned char win[WIN * WIN];          /* vis round the player */
static signed int win_x, win_y;              /* its top left on the map */

unsigned char vis_at(unsigned char x, unsigned char y) {
    signed int dx = (signed int)x - win_x, dy = (signed int)y - win_y;
    if (dx < 0 || dy < 0 || dx >= WIN || dy >= WIN) return 0;
    return win[(unsigned int)dy * WIN + dx];
}
static unsigned int top[COLS], bot[COLS];     /* a row of places: its characters */
static unsigned int ctop[COLS], cbot[COLS];   /* and their colour RAM */

/* a clear line from the player to (tx, ty): nothing solid between */
static unsigned char clear_to(unsigned char tx, unsigned char ty) {
    signed int x = pl.x, y = pl.y, dx, dy, sx, sy, err, e2;
    unsigned char t;
    dx = (signed int)tx - x;
    dy = (signed int)ty - y;
    sx = dx < 0 ? -1 : 1;
    sy = dy < 0 ? -1 : 1;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    err = dx - dy;
    for (;;) {
        e2 = err * 2;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx) { err += dx; y += sy; }
        if (x == tx && y == ty) return 1;
        t = terrain((unsigned char)x, (unsigned char)y);
        if (t == TR_ROCK || t == TR_DOOR) return 0;
    }
}

void view_fov(void) {
    signed char dx, dy;
    signed int x, y;
    unsigned int d2;
    unsigned char v;
    unsigned char now = epoch();
    unsigned int w = 0;
    win_x = (signed int)pl.x - SEE;
    win_y = (signed int)pl.y - SEE;
    dma_fill_far((unsigned long)(unsigned int)win, WIN * WIN, 0);
    for (dy = -SEE; dy <= SEE; dy++) {
        video_poll();
        y = (signed int)pl.y + dy;
        for (dx = -SEE; dx <= SEE; dx++, w++) {
            x = (signed int)pl.x + dx;
            if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) continue;
            d2 = (unsigned int)((signed int)dx * dx + (signed int)dy * dy);
            if (d2 > SEE * SEE) continue;
            if ((dx || dy) && !clear_to((unsigned char)x, (unsigned char)y)) continue;
            /* the torch: bright near, dim at its edge */
            if (d2 <= 3) v = 1;
            else if (d2 <= 10) v = 2;
            else if (d2 <= 22) v = 3;
            else if (d2 <= 38) v = 4;
            else v = VIS_SHADOW;
            if (v > 2 && lit_at((unsigned char)x, (unsigned char)y)) v = 2;
            win[w] = v;
            if (v != VIS_SHADOW) seen_set(AT(x, y), now);
        }
    }
}

/* the torch's light on a quarter of a place (a character): its centre's
 * distance from the player's, in quarter places, squared; 0 bright .. 3
 * dim, 4 dark. Per character, so the edge of the light is round; and
 * where one level meets the next the boundary moves a little from one
 * character to the next (a 4 x 4 ordered dither, b 0-15), so the levels
 * shade into each other instead of making rings. */
static unsigned int sq[96];
static const unsigned int edge[4] = {52, 170, 360, 600};
static const unsigned char bayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};

static unsigned char torch(signed int dxq, signed int dyq, unsigned char b) {
    unsigned int d2, t;
    unsigned char k;
    if (dxq < 0) dxq = -dxq;
    if (dyq < 0) dyq = -dyq;
    d2 = sq[dxq] + sq[dyq];
    for (k = 0; k < 4; k++) {
        t = edge[k];
        if (d2 <= t + (t >> 5) * b - (t >> 2)) return k;      /* t * (b / 32 - 1/4) either way */
    }
    return 4;
}

/* what is put over the terrain this turn, in the window: the player, the
 * creatures, the things (the first at a place wins); the full-colour
 * tile, and its variant (OV_TORCH: as the torch lights each quarter) */
#define MAX_OV   (1 + MAX_MON + MAX_OBJ)
#define OV_TORCH 0xFF
static unsigned char ov_n, ov_x[MAX_OV], ov_y[MAX_OV], ov_k[MAX_OV], ov_v[MAX_OV];
static unsigned char row_ov[MAX_OV];

static const unsigned char obj_tile[9] = {
    0, T_CHEST, T_CHEST_OPEN, T_IDOL, T_IDOL_SPENT, T_POTION, T_GOLD, T_BONES, T_BRAZIER,
};

static void ov_add(unsigned char x, unsigned char y, unsigned char k, unsigned char v) {
    if (x < cam_x || y < cam_y || x >= cam_x + VIEW_W || y >= cam_y + VIEW_H) return;
    ov_x[ov_n] = x - cam_x;
    ov_y[ov_n] = y - cam_y;
    ov_k[ov_n] = k;
    ov_v[ov_n] = v;
    ov_n++;
}

static void ov_gather(void) {
    unsigned char k, v, x, y;
    ov_n = 0;
    ov_add(pl.x, pl.y, T_PLAYER, 0);
    for (k = 0; k < nmons; k++) {
        if (!mons[k].hp) continue;
        v = vis_at(mons[k].x, mons[k].y);
        if (v == VIS_SHADOW) ov_add(mons[k].x, mons[k].y, mon_tile[mons[k].kind], TV_EYES);
        else if (v) ov_add(mons[k].x, mons[k].y, mon_tile[mons[k].kind], OV_TORCH);
    }
    for (k = 0; k < MAX_OBJ; k++) {
        if (!thing_kind[k]) continue;
        x = (unsigned char)(thing_at[k] % MAP_W);
        y = (unsigned char)(thing_at[k] / MAP_W);
        v = vis_at(x, y);
        if (v && v != VIS_SHADOW) ov_add(x, y, obj_tile[thing_kind[k]], OV_TORCH);
    }
}

void view_draw(void) {
    unsigned char tx, ty, c, k, q, lv, mem, lit, room, o, nr, v, x, y, b;
    unsigned int ch, age;
    unsigned long at;
    signed int dx, dy;
    signed int cx = (signed int)pl.x - VIEW_W / 2, cy = (signed int)pl.y - VIEW_H / 2;
    unsigned int *row, *crow;
    if (!sq[1])
        for (k = 0; k < 96; k++) sq[k] = (unsigned int)k * k;
    if (cx < 0) cx = 0;
    if (cx > MAP_W - VIEW_W) cx = MAP_W - VIEW_W;
    if (cy < 0) cy = 0;
    if (cy > MAP_H - VIEW_H) cy = MAP_H - VIEW_H;
    cam_x = (unsigned char)cx;
    cam_y = (unsigned char)cy;
    ov_gather();
    for (ty = 0; ty < VIEW_H; ty++) {
        video_poll();
        y = cam_y + ty;
        dy = (signed int)y - pl.y;
        at = MAP_LIN + ((unsigned int)y << 9) + ((unsigned int)cam_x << 2);
        dma_copy_far((unsigned long)(unsigned int)top, at, COLS * 2);
        dma_copy_far((unsigned long)(unsigned int)bot, at + 256, COLS * 2);
        for (k = 0, nr = 0; k < ov_n; k++)
            if (ov_y[k] == ty) row_ov[nr++] = k;
        for (tx = 0, c = 0; tx < VIEW_W; tx++, c += 2) {
            x = cam_x + tx;
            v = vis_at(x, y);
            lit = v && v != VIS_SHADOW;
            room = lit && lit_at(x, y);
            /* out of the light: remembered, or dark */
            mem = MAP_DARK;
            k = seen_get(AT(x, y));
            if (k) {
                age = (unsigned int)epoch_age(k) * 4 * pl.fade;
                if (age < MEM1) mem = MAP_MEM;
                else if (age < MEM2) mem = MAP_MEM + 1;
                else if (age < MEM3) mem = MAP_MEM + 2;
            }
            o = 0xFF;
            for (k = 0; k < nr; k++)
                if (ov_x[row_ov[k]] == tx) {
                    o = row_ov[k];
                    break;
                }
            dx = (signed int)x - pl.x;
            for (q = 0; q < 4; q++) {
                row = q & 2 ? bot : top;
                crow = q & 2 ? cbot : ctop;
                b = c + (q & 1);
                lv = 0xFF;
                if (lit) {
                    lv = torch(dx * 4 + (q & 1) * 2 - 1, dy * 4 + (q >> 1) * 2 - 1,
                               bayer[(((ty * 2 + (q >> 1)) & 3) << 2) | ((c + (q & 1)) & 3)]);
                    if (room && lv > 1) lv = 1;
                    if (lv > 3) lv = 0xFF;          /* beyond the light: as remembered */
                }
                if (o != 0xFF) {
                    k = ov_v[o] == OV_TORCH ? lv : ov_v[o];
                    if (k < TV_EYES + 1 && (ch = tile_char[ov_k[o]][k]) != 0) {
                        row[b] = ch + q;
                        crow[b] = 0;
                        continue;
                    }
                }
                crow[b] = NCM_BANK(lv <= 3 ? MAP_LIT + lv : mem);
            }
        }
        video_cells(0, VIEW_ROW + ty * 2, top, COLS);
        video_cells(0, VIEW_ROW + ty * 2 + 1, bot, COLS);
        dma_copy_far(COLOR_LIN + (unsigned int)(VIEW_ROW + ty * 2) * (COLS * 2), (unsigned long)(unsigned int)ctop, COLS * 2);
        dma_copy_far(COLOR_LIN + (unsigned int)(VIEW_ROW + ty * 2 + 1) * (COLS * 2), (unsigned long)(unsigned int)cbot, COLS * 2);
    }
}

/* ---------------------------------------------------------------- messages */

#define NMSG 4
static char msgs[NMSG][MSG_W + 1];
static unsigned char msg_col[NMSG];

static void msg_show(void) {
    unsigned char k;
    video_blank(1, MSG_ROW, MSG_W, NMSG);
    for (k = 0; k < NMSG; k++)
        if (msgs[k][0]) video_text(1, MSG_ROW + k, msgs[k], k == NMSG - 1 ? msg_col[k] : (k == NMSG - 2 ? C_DIM : C_DARK));
}

void msg(const char *s, unsigned char colour) {
    unsigned char k, n;
    for (k = 0; k < NMSG - 1; k++) {
        for (n = 0; n <= MSG_W; n++) msgs[k][n] = msgs[k + 1][n];
        msg_col[k] = msg_col[k + 1];
    }
    for (n = 0; s[n] && n < MSG_W; n++) msgs[NMSG - 1][n] = s[n];
    msgs[NMSG - 1][n] = 0;
    msg_col[NMSG - 1] = colour;
    msg_show();
}

void msg_clear(void) {
    unsigned char k;
    for (k = 0; k < NMSG; k++) msgs[k][0] = 0;
}

const char *depth_name(unsigned char depth) {
    return nth("\0The Sunken Cellars\0The Weeping Halls\0The Bone Galleries\0The Web-Choked Deep\0The Lich's Sanctum", depth);
}

void view_hud(void) {
    char buf[40];
    video_blank(0, 0, COLS, 1);
    buf[0] = 0;
    str_cat(buf, "Depth ");
    str_num(buf, pl.depth);
    str_cat(buf, ": ");
    str_cat(buf, depth_name(pl.depth));
    video_text(2, 0, buf, C_GOLD);
    buf[0] = 0;
    str_cat(buf, "Turn ");
    str_num(buf, turn);
    video_text(68, 0, buf, C_DARK);
    if (pl.fade > 2) video_text(44, 0, "your memory slips", C_PURPLE);
    else if (pl.fade < 2) video_text(44, 0, "your memory is keen", C_CYAN);
    ui_stats(STAT_COL, MSG_ROW);
}

void view_screen(void) {
    unsigned char r;
    video_clear();
    ui_hline(0, 45, COLS);
    {
        unsigned int t = UI_F_T;
        video_cells(STAT_COL - 2, 45, &t, 1);
        t = UI_F_V;
        for (r = MSG_ROW; r < ROWS; r++) video_cells(STAT_COL - 2, r, &t, 1);
    }
    view_hud();
    msg_show();
    view_draw();
}
