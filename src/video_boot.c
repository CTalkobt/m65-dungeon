/* video_boot.c — the display set up at the start, and put back as it was
 * at the end (the overlay "front": only main calls them) */
#pragma cc45 overlay(front)
#include <dma.h>
#include "hw.h"
#include "video.h"

extern unsigned char pal_bank, saved[18];    /* video.c */
extern unsigned int textx, charptr;

/* the text colours (0-15): parchment and ink */
static const unsigned char text_r[16] __noescape = {0, 232, 140, 240, 220, 110, 110, 250, 120, 255, 170, 60, 255, 150, 120, 220};
static const unsigned char text_g[16] __noescape = {0, 220, 128, 196, 50, 200, 160, 140, 120, 255, 110, 56, 236, 20, 220, 210};
static const unsigned char text_b[16] __noescape = {0, 190, 108, 80, 40, 100, 250, 40, 128, 255, 220, 64, 160, 20, 230, 180};

static void pal_text(void) {
    unsigned char i;
    for (i = 0; i < 16; i++) {
        pal_r[i] = text_r[i];
        pal_g[i] = text_g[i];
        pal_b[i] = text_b[i];
    }
    pal_r[255] = pal_g[255] = pal_b[255] = 0;
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
