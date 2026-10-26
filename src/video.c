/* video.c — the display: VIC-IV 80 x 50 (640 x 400) with 16-bit characters
 *
 * Characters above $FF are full colour (FCM: 64 bytes, a byte per pixel,
 * the byte is the palette entry): the tiles (bank 4) and the scene (bank 5).
 * Characters below $100 are text from the ROM's lower-case set, in their
 * colour RAM colour. The screen is at the top of bank 5; cells are written
 * by DMA.
 *
 * The screen gets a palette bank of its own (not the ROM's), so all 256
 * entries are ours and the ROM's colours are as they were at the end.
 * $D070: MAPEDPAL (the bank at $D100-$D3FF) bits 7-6, BTPALSEL (the
 * screen's) 5-4, SPRPALSEL 3-2, ABTPALSEL 1-0. Palette registers take
 * their value nibble-swapped.
 *
 * The master palette (pal_r/g/b) is what the art asked for; what is shown
 * is it dimmed (v_bright, the fades), the tiles' part flickering like a
 * torch (v_flicker), the scene's washed toward a colour (v_wash: hits,
 * spells) and its glow (247-254) pulsing; or, while the map is shown
 * (v_map), 128-255 are the map's palette banks and flicker with the tiles.
 *
 * Colour RAM: a cell's first byte its attributes (0 for text and the
 * full-colour characters; view.c sets the map's 16-colour ones), the
 * second its colour.
 */
#include <dma.h>
#include <audio_tune.h>
#include "hw.h"
#include "rng.h"
#include "video.h"
#include "assets.h"

unsigned char pal_r[256], pal_g[256], pal_b[256];
unsigned char v_bright = 16, v_flicker, v_fire, v_music, v_map;
unsigned char v_wash, v_wash_r, v_wash_g, v_wash_b;

static unsigned char pal_bank, saved[18];
static unsigned int textx, charptr;
static unsigned int row_buf[COLS];

/* the text colours (0-15): parchment and ink */
static const unsigned char text_r[16] = {0, 232, 140, 240, 220, 110, 110, 250, 120, 255, 170, 60, 255, 150, 120, 220};
static const unsigned char text_g[16] = {0, 220, 128, 196, 50, 200, 160, 140, 120, 255, 110, 56, 236, 20, 220, 210};
static const unsigned char text_b[16] = {0, 190, 108, 80, 40, 100, 250, 40, 128, 255, 220, 64, 160, 20, 230, 180};

#define PAL_R(i) REG(0xD100 + (i))
#define PAL_G(i) REG(0xD200 + (i))
#define PAL_B(i) REG(0xD300 + (i))

static unsigned char nib(unsigned char v) {
    return (unsigned char)(v << 4 | v >> 4);
}

/* entries first..last at scale / 256, washed toward the wash colour by
 * wash / 16 */
static void put_range(unsigned char first, unsigned char last, unsigned int scale, unsigned char wash) {
    unsigned char i = first, r, g, b;
    for (;;) {
        r = (unsigned char)((pal_r[i] * scale) >> 8);
        g = (unsigned char)((pal_g[i] * scale) >> 8);
        b = (unsigned char)((pal_b[i] * scale) >> 8);
        if (wash) {
            r = (unsigned char)((r * (16 - wash) + v_wash_r * wash) >> 4);
            g = (unsigned char)((g * (16 - wash) + v_wash_g * wash) >> 4);
            b = (unsigned char)((b * (16 - wash) + v_wash_b * wash) >> 4);
        }
        PAL_R(i) = nib(r);
        PAL_G(i) = nib(g);
        PAL_B(i) = nib(b);
        if (i == last) break;
        i++;
    }
}

static unsigned char flick = 240, glow_phase;

/* the fire (the braziers, the player's torch on the map): its 4 entries
 * turn round a ramp from deep red to pale gold, each a little brighter or
 * dimmer at random, every few frames */
static const unsigned char fire_r[4] = {150, 235, 255, 255};
static const unsigned char fire_g[4] = {30, 100, 190, 240};
static const unsigned char fire_b[4] = {4, 14, 50, 160};
static unsigned char fire_phase, fire_k[4] = {255, 255, 255, 255};

static void fire_show(unsigned int base) {
    unsigned char i, j;
    unsigned int sc;
    for (i = 0; i < 4; i++) {
        j = (i + fire_phase) & 3;
        sc = (base * fire_k[i]) >> 8;
        PAL_R(PAL_FIRE + i) = nib((unsigned char)((fire_r[j] * sc) >> 8));
        PAL_G(PAL_FIRE + i) = nib((unsigned char)((fire_g[j] * sc) >> 8));
        PAL_B(PAL_FIRE + i) = nib((unsigned char)((fire_b[j] * sc) >> 8));
    }
}

static void fire_step(void) {
    unsigned char i;
    if (rng_range(3)) fire_phase++;
    for (i = 0; i < 4; i++) fire_k[i] = 190 + rng_range(66);
}
/* the glow's brightness through a pulse: a slow triangle, 160-256 */
static unsigned int glow_scale(void) {
    unsigned char p = glow_phase & 63;
    if (p >= 32) p = 63 - p;
    return 160 + p * 3;
}

void video_pal_show(void) {
    unsigned int base = (unsigned int)v_bright * 16;
    REG(0xD070) = (unsigned char)(pal_bank << 6 | pal_bank << 4 | pal_bank << 2 | pal_bank);
    put_range(0, 15, v_bright == 16 ? 256 : base, 0);
    put_range(PAL_TILES, PAL_SCENE - 1, v_flicker ? (base * flick) >> 8 : base, 0);
    if (v_fire) fire_show(base);
    if (v_map) {
        put_range(PAL_SCENE, 255, v_flicker ? (base * flick) >> 8 : base, 0);
        return;
    }
    put_range(PAL_SCENE, PAL_GLOW - 1, base, v_wash);
    put_range(PAL_GLOW, PAL_GLOW + 7, (base * glow_scale()) >> 8, v_wash);
}

/* the torch: a slow wander with now and then a dip */
static void flicker_step(void) {
    unsigned char want = 226 + rng_range(30);
    if (rng_range(24) == 0) want = 200;
    flick = (unsigned char)((flick * 3 + want) >> 2);
}

/* the music on a frame, once a frame: from video_frame, and from long work
 * (drawing the map, making a level) so the tune keeps its time */
static unsigned char music_at;

void video_poll(void) {
    unsigned char f = FRAMECOUNT;
    if (v_music && f != music_at) {
        music_at = f;
        tune_update();
    }
}

void video_frame(void) {
    unsigned char f = FRAMECOUNT;
    while (FRAMECOUNT == f) {}
    video_poll();
    glow_phase++;
    if (v_flicker && (glow_phase & 3) == 0) flicker_step();
    if (v_fire && (glow_phase & 3) == 2) fire_step();
    if ((glow_phase & 1) == 0) video_pal_show();
}

void video_frames(unsigned char n) {
    while (n--) video_frame();
}

void video_fade(unsigned char to) {
    while (v_bright != to) {
        if (to > v_bright) v_bright = v_bright + 2 > to ? to : v_bright + 2;
        else v_bright = v_bright < to + 2 ? to : v_bright - 2;
        video_frame();
        video_pal_show();
    }
}

void video_pal_load(unsigned long lin, unsigned char first, unsigned char n) {
    dma_copy_far((unsigned long)(unsigned int)(pal_r + first), lin + first, n);
    dma_copy_far((unsigned long)(unsigned int)(pal_g + first), lin + 256 + first, n);
    dma_copy_far((unsigned long)(unsigned int)(pal_b + first), lin + 512 + first, n);
}

static void pal_text(void) {
    unsigned char i;
    for (i = 0; i < 16; i++) {
        pal_r[i] = text_r[i];
        pal_g[i] = text_g[i];
        pal_b[i] = text_b[i];
    }
    pal_r[255] = pal_g[255] = pal_b[255] = 0;
}

void video_sway(signed char dx) {
    unsigned int x = textx + dx;
    REG(0xD04C) = (unsigned char)x;
    REG(0xD04D) = (REG(0xD04D) & 0xF0) | (unsigned char)((x >> 8) & 0x0F);
}

void video_shake(unsigned char frames) {
    static const signed char d[8] = {6, -5, 4, -4, 3, -2, 2, -1};
    unsigned char i;
    for (i = 0; i < frames; i++) {
        video_sway(d[i & 7]);
        video_frame();
    }
    video_sway(0);
}

/* cells */
void video_cells(unsigned char col, unsigned char row, const unsigned int *cells, unsigned char n) {
    dma_copy_far(SCREEN_LIN + ((unsigned int)row * COLS + col) * 2, (unsigned long)(unsigned int)cells, (unsigned int)n * 2);
}

void video_block(unsigned char col, unsigned char row, unsigned char w, unsigned char h, unsigned int first) {
    unsigned char r, c;
    for (r = 0; r < h; r++) {
        for (c = 0; c < w; c++) row_buf[c] = first++;
        video_cells(col, row + r, row_buf, w);
    }
}

/* ASCII to the lower-case set's screen codes: a-z 1-26, A-Z 65-90, @ 0 */
static unsigned int scr(unsigned char c) {
    if (c >= 0x61 && c <= 0x7A) return c - 0x60;
    if (c == 0x40) return 0;
    if (c >= 0x5B && c <= 0x5F) return c - 0x40;
    return c;
}

static void colour(unsigned char col, unsigned char row, unsigned char n, unsigned char c) {
    dma_fill_far_step(COLOR_LIN + ((unsigned int)row * COLS + col) * 2 + 1, 2, n, c);
}

void video_text(unsigned char col, unsigned char row, const char *s, unsigned char c) {
    unsigned char n = 0;
    while (s[n] && col + n < COLS) {
        row_buf[n] = scr((unsigned char)s[n]);
        n++;
    }
    if (!n) return;
    video_cells(col, row, row_buf, n);
    colour(col, row, n, c);
}

void video_centre(unsigned char row, const char *s, unsigned char c) {
    unsigned char n = 0;
    while (s[n]) n++;
    video_text((unsigned char)((COLS - n) / 2), row, s, c);
}

void video_num(unsigned char col, unsigned char row, unsigned int v, unsigned char c) {
    char buf[6];
    unsigned char i = 5;
    buf[5] = 0;
    do {
        buf[--i] = (char)(0x30 + v % 10);
        v /= 10;
    } while (v);
    video_text(col, row, buf + i, c);
}

void video_fill(unsigned char col, unsigned char row, unsigned char n, unsigned char ch, unsigned char c) {
    unsigned char i;
    for (i = 0; i < n; i++) row_buf[i] = ch;
    video_cells(col, row, row_buf, n);
    colour(col, row, n, c);
}

void video_blank(unsigned char col, unsigned char row, unsigned char w, unsigned char h) {
    while (h--) video_fill(col, row++, w, 0x20, C_TEXT);
}

void video_clear(void) {
    dma_fill_far(COLOR_LIN, COLS * ROWS * 2, 0);   /* the map's 16-colour attributes off */
    video_blank(0, 0, COLS, ROWS);
}

/* the display set up, and put back as it was at the end */
void video_on(void) {
    unsigned char rom;
    /* the VIC-IV's registers unlocked (the DOS locks them when it loads) */
    REG(0xD02F) = 0x47;
    REG(0xD02F) = 0x53;
    saved[0] = REG(0xD031); saved[1] = REG(0xD054); saved[2] = REG(0xD058); saved[3] = REG(0xD059);
    saved[4] = REG(0xD05D); saved[5] = REG(0xD05E); saved[6] = REG(0xD060); saved[7] = REG(0xD061);
    saved[8] = REG(0xD062); saved[9] = REG(0xD063); saved[10] = REG(0xD064); saved[11] = REG(0xD065);
    saved[12] = REG(0xD020); saved[13] = REG(0xD021); saved[14] = REG(0xD070); saved[15] = REG(0xD07B);
    saved[16] = REG(0xD068); saved[17] = REG(0xD069);
    textx = REG(0xD04C) | (unsigned int)(REG(0xD04D) & 0x0F) << 8;
    charptr = saved[16] | (unsigned int)saved[17] << 8;
    rom = (saved[14] >> 4) & 3;
    pal_bank = (rom + 2) & 3;

    v_bright = 0;
    pal_text();
    video_pal_show();                         /* black while it is set up */
    dma_fill_far(COLOR_LIN, COLS * ROWS * 2, 0);
    video_clear();

    REG(0xD020) = 0;
    REG(0xD021) = 0;
    REG(0xD05D) = REG(0xD05D) & 0x7F;         /* hot registers off */
    REG(0xD031) = REG(0xD031) | 0xC8;         /* H640 + V400, FAST */
    REG(0xD054) = REG(0xD054) | 0x45;         /* CHR16 + FCLRHI: characters > $FF are FCM; VFAST: 40 MHz */
    REG(0xD058) = (unsigned char)(COLS * 2);  /* bytes per row */
    REG(0xD059) = 0;
    REG(0xD05E) = COLS;
    REG(0xD07B) = ROWS;
    REG(0xD060) = (unsigned char)SCREEN_LIN;
    REG(0xD061) = (unsigned char)(SCREEN_LIN >> 8);
    REG(0xD062) = (unsigned char)(SCREEN_LIN >> 16);
    REG(0xD063) = REG(0xD063) & 0xF0;
    REG(0xD064) = 0;                          /* colour RAM from its start */
    REG(0xD065) = 0;
    /* the lower-case half of the ROM's character set */
    REG(0xD069) = (unsigned char)(((charptr & 0xF000) | 0x0800) >> 8);
}

/* the VIC-IV's registers unlocked again after a load (the DOS locks them) */
void video_unlock(void) {
    REG(0xD02F) = 0x47;
    REG(0xD02F) = 0x53;
    REG(0xD05D) = REG(0xD05D) & 0x7F;
    REG(0xD031) = REG(0xD031) | 0xC8;
    REG(0xD054) = REG(0xD054) | 0x45;
}

void video_off(void) {
    video_sway(0);
    REG(0xD070) = saved[14];
    REG(0xD054) = saved[1];
    REG(0xD058) = saved[2]; REG(0xD059) = saved[3];
    REG(0xD05E) = saved[5];
    REG(0xD060) = saved[6]; REG(0xD061) = saved[7]; REG(0xD062) = saved[8]; REG(0xD063) = saved[9];
    REG(0xD064) = saved[10]; REG(0xD065) = saved[11];
    REG(0xD07B) = saved[15];
    REG(0xD068) = saved[16]; REG(0xD069) = saved[17];
    REG(0xD05D) = saved[4];
    REG(0xD031) = saved[0];
    REG(0xD020) = saved[12]; REG(0xD021) = saved[13];
}
