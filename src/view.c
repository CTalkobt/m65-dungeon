/* view.c — what the player sees and remembers, and the exploration screen
 *
 * Sight: every place within SEE of the player with a clear line to it
 * (a Bresenham line through nothing solid). Within the torch's reach it is
 * lit, brighter the nearer; in a room with a brazier all of it is lit;
 * further and dark, it is in shadow: only what shines by itself shows
 * (eyes). A lit place is remembered (seen[] = the turn).
 *
 * Memory: a place out of sight shows only if it is a wall, a door or the
 * stairs, as faint lines - the shape of the place - fading with the turns
 * since it was seen (pl.fade: faster under a curse), then gone.
 *
 * The screen: row 0 the level's name, rows 1-44 the map (40 x 22 tiles of
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

#define WIN (SEE * 2 + 1)

static unsigned char cam_x, cam_y;
static unsigned char win[WIN * WIN];          /* vis round the player */
static signed int win_x, win_y;              /* its top left on the map */

unsigned char vis_at(unsigned char x, unsigned char y) {
    signed int dx = (signed int)x - win_x, dy = (signed int)y - win_y;
    if (dx < 0 || dy < 0 || dx >= WIN || dy >= WIN) return 0;
    return win[(unsigned int)dy * WIN + dx];
}
static unsigned int top[COLS], bot[COLS];

/* a clear line from the player to (tx, ty): nothing solid between */
static unsigned char clear_to(unsigned char tx, unsigned char ty) {
    signed int x = pl.x, y = pl.y, dx, dy, sx, sy, err, e2, ndy;
    unsigned char t;
    dx = (signed int)tx - x;
    dy = (signed int)ty - y;
    sx = dx < 0 ? -1 : 1;
    sy = dy < 0 ? -1 : 1;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    err = dx - dy;
    ndy = -dy;                 /* (a variable: cc45 before #338 compared e2 > -dy unsigned) */
    for (;;) {
        e2 = err * 2;
        if (e2 > ndy) { err -= dy; x += sx; }
        if (e2 < dx) { err += dx; y += sy; }
        if (x == tx && y == ty) return 1;
        t = TERRAIN(AT(x, y));
        if (t == TR_ROCK || t == TR_DOOR) return 0;
    }
}

void view_fov(void) {
    signed char dx, dy;
    signed int x, y;
    unsigned int i, d2;
    unsigned char v;
    unsigned char now = epoch();
    unsigned int w = 0;
    win_x = (signed int)pl.x - SEE;
    win_y = (signed int)pl.y - SEE;
    dma_fill_far((unsigned long)(unsigned int)win, WIN * WIN, 0);
    for (dy = -SEE; dy <= SEE; dy++) {
        y = (signed int)pl.y + dy;
        for (dx = -SEE; dx <= SEE; dx++, w++) {
            x = (signed int)pl.x + dx;
            if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) continue;
            d2 = (unsigned int)((signed int)dx * dx + (signed int)dy * dy);
            if (d2 > SEE * SEE) continue;
            i = AT(x, y);
            if ((dx || dy) && !clear_to((unsigned char)x, (unsigned char)y)) continue;
            /* the torch: bright near, dim at its edge */
            if (d2 <= 3) v = 1;
            else if (d2 <= 10) v = 2;
            else if (d2 <= 22) v = 3;
            else if (d2 <= 38) v = 4;
            else v = VIS_SHADOW;
            if ((map[i] & F_LIT) && v > 2) v = 2;
            win[w] = v;
            if (v != VIS_SHADOW) seen[i] = now;
        }
    }
}

/* walls: which sides have something to walk on (N 1, E 2, S 4, W 8) */
static unsigned char open_sides(unsigned int i) {
    unsigned char m = 0;
    if (i >= MAP_W && TERRAIN(i - MAP_W) != TR_ROCK) m |= 1;
    if (TERRAIN(i + 1) != TR_ROCK) m |= 2;
    if (i + MAP_W < MAP_N && TERRAIN(i + MAP_W) != TR_ROCK) m |= 4;
    if (TERRAIN(i - 1) != TR_ROCK) m |= 8;
    return m;
}

/* remembered walls: which neighbours are remembered walls or doors too */
static unsigned char wallish(unsigned int i) {
    unsigned char t = TERRAIN(i);
    return seen[i] && ((map[i] & F_BOUND) || t == TR_DOOR || t == TR_OPEN);
}

static unsigned char outline(unsigned int i) {
    unsigned char m = 0;
    if (i >= MAP_W && wallish(i - MAP_W)) m |= 1;
    if (wallish(i + 1)) m |= 2;
    if (i + MAP_W < MAP_N && wallish(i + MAP_W)) m |= 4;
    if (wallish(i - 1)) m |= 8;
    return m;
}

static const unsigned char obj_tile[9] = {
    0, T_CHEST, T_CHEST_OPEN, T_IDOL, T_IDOL_SPENT, T_POTION, T_GOLD, T_BONES, T_BRAZIER,
};

/* a door's tile: across a corridor running east-west it stands upright */
static unsigned char door_tile(unsigned int i, unsigned char open) {
    unsigned char vert = TERRAIN(i - MAP_W) == TR_ROCK;
    if (open) return vert ? T_DOOR_OPEN_V : T_DOOR_OPEN_H;
    return vert ? T_DOOR_V : T_DOOR_H;
}

/* what a map cell shows: the tile kind (NONE: black) and either its
 * variant (cell_var) or, when cell_lit, the torchlight decides for each
 * quarter (view_draw) */
#define NONE 0xFF
static unsigned char cell_var, cell_lit, cell_room;

static unsigned char cell(unsigned char x, unsigned char y) {
    unsigned int i = AT(x, y), age;
    unsigned char v = vis_at(x, y), t = TERRAIN(i), m;
    cell_lit = 0;
    if (x == pl.x && y == pl.y) {
        cell_var = 0;
        return T_PLAYER;
    }
    if (v && v != VIS_SHADOW) {
        cell_lit = 1;
        cell_room = (map[i] & F_LIT) != 0;
        m = mon_at(x, y);
        if (m) return montypes[mons[m - 1].kind].tile;
        if (map[i] & F_OBJ) return obj_tile[obj_at(i)];
        switch (t) {
        case TR_FLOOR: return T_FLOOR0 + (((x * 7) ^ (y * 13)) & 3);
        case TR_CORR: return T_CORR0 + ((x ^ y) & 1);
        case TR_DOOR: return door_tile(i, 0);
        case TR_OPEN: return door_tile(i, 1);
        case TR_STAIRS: return T_STAIRS;
        }
        if (!(map[i] & F_BOUND)) return NONE;
        return T_WALL0 + open_sides(i);
    }
    if (v == VIS_SHADOW) {
        m = mon_at(x, y);
        if (m) {
            cell_var = TV_EYES;
            return montypes[mons[m - 1].kind].tile;
        }
    }
    /* remembered: walls, doors and the stairs, fading */
    if (!seen[i]) return NONE;
    age = (unsigned int)epoch_age(seen[i]) * 4 * pl.fade;
    if (age < MEM1) cell_var = TV_MEM;
    else if (age < MEM2) cell_var = TV_MEM + 1;
    else if (age < MEM3) cell_var = TV_MEM + 2;
    else return NONE;
    if (t == TR_DOOR || t == TR_OPEN) return door_tile(i, 0);
    if (t == TR_STAIRS) return T_STAIRS;
    if (map[i] & F_BOUND) return T_WMEM0 + outline(i);
    return NONE;
}

/* the torch's light on a quarter of a tile (a character): its centre's
 * distance from the player's, in quarter tiles, squared; 0 bright .. 3
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

void view_draw(void) {
    unsigned char tx, ty, c, k, q, lv;
    unsigned int ch;
    signed int dx, dy;
    signed int cx = (signed int)pl.x - VIEW_W / 2, cy = (signed int)pl.y - VIEW_H / 2;
    unsigned int *row;
    if (!sq[1])
        for (k = 0; k < 96; k++) sq[k] = (unsigned int)k * k;
    if (cx < 0) cx = 0;
    if (cx > MAP_W - VIEW_W) cx = MAP_W - VIEW_W;
    if (cy < 0) cy = 0;
    if (cy > MAP_H - VIEW_H) cy = MAP_H - VIEW_H;
    cam_x = (unsigned char)cx;
    cam_y = (unsigned char)cy;
    for (ty = 0; ty < VIEW_H; ty++) {
        dy = (signed int)(cam_y + ty) - pl.y;
        for (tx = 0, c = 0; tx < VIEW_W; tx++, c += 2) {
            k = cell(cam_x + tx, cam_y + ty);
            dx = (signed int)(cam_x + tx) - pl.x;
            for (q = 0; q < 4; q++) {
                row = q & 2 ? bot : top;
                ch = 0x20;
                if (k != NONE) {
                    lv = cell_var;
                    if (cell_lit) {
                        lv = torch(dx * 4 + (q & 1) * 2 - 1, dy * 4 + (q >> 1) * 2 - 1,
                                   bayer[(((ty * 2 + (q >> 1)) & 3) << 2) | ((c + (q & 1)) & 3)]);
                        if (cell_room && lv > 1) lv = 1;
                        if (lv > 3) lv = 0xFF;          /* beyond the light: dark */
                    }
                    if (lv < 8 && tile_char[k][lv]) ch = tile_char[k][lv] + q;
                }
                row[c + (q & 1)] = ch;
            }
        }
        video_cells(0, VIEW_ROW + ty * 2, top, COLS);
        video_cells(0, VIEW_ROW + ty * 2 + 1, bot, COLS);
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

void view_hud(void) {
    char buf[40];
    video_blank(0, 0, COLS, 1);
    buf[0] = 0;
    str_cat(buf, "Depth ");
    str_num(buf, pl.depth);
    str_cat(buf, ": ");
    str_cat(buf, depth_name[pl.depth]);
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
