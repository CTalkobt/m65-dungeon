/* scene.c — the art's packed files, the close-ups and their frames
 *
 * A close-up is two files: DG<NAME>A into bank 5 (its palette, a header,
 * two frames' maps, the first characters) and DG<NAME>B into bank 1 (the
 * rest of the characters). A frame is shown by copying its map, a row at a
 * time, into the screen's cells (rows 1-30, columns 12-67).
 *
 * The files are packed (tools/pack.py): each loads near the end of the
 * area it unpacks to and unpacks in place, by DMA: a copy of the bytes as
 * they are, a fill, or a copy of what is unpacked already.
 *
 * Once unpacked, a close-up is kept in attic RAM (128 KB each from
 * CACHE_LIN; the code overlays are below, at $8000000): the next time it
 * is two DMA copies, not the disk.
 *
 * In the overlay "scenes" (with the chest and the idol): the title and the
 * fights call into it for a frame now and then.
 */
#pragma cc45 overlay(scenes)
#include <dma.h>
#include "hw.h"
#include "assets.h"
#include "video.h"
#include "load.h"
#include "game.h"

#define SC_COL 12
#define SC_ROW 1
#define BANK1_LIN 0x0012000UL
#define BANK5_LEN 0xE000                 /* $50000-$5DFFF */
#define BANK1_LEN 0xD800                 /* $12000-$1F7FF */
#define CACHE_LIN 0x8100000UL

static unsigned int cached;              /* a bit a close-up */

static unsigned char loaded = 0xFF;

/* the packed stream at src unpacked to where it says */
void unpack(unsigned long src) {
    unsigned char h[3], t, n;
    unsigned int off;
    unsigned long dst;
    dma_copy_far((unsigned long)(unsigned int)h, src, 3);
    dst = h[0] | (unsigned long)h[1] << 8 | (unsigned long)h[2] << 16;
    src += 3;
    for (;;) {
        dma_copy_far((unsigned long)(unsigned int)h, src, 3);
        t = h[0];
        n = t & 0x3F;
        if (t < 0x40) {                       /* bytes as they are */
            n++;
            dma_copy_far(dst, src + 1, n);
            src += 1 + n;
        } else if (t < 0x80) {                /* one byte, n + 3 times */
            n += 3;
            dma_fill_far(dst, n, h[1]);
            src += 2;
        } else {                              /* a copy from off back */
            if (t < 0xC0) {
                off = h[1] + 1;
                src += 2;
            } else {
                off = h[1] | (unsigned int)h[2] << 8;
                if (!off) return;
                src += 3;
            }
            n += 3;
            dma_copy_far(dst, dst - off, n);
        }
        dst += n;
    }
}

unsigned char scene_load(unsigned char s) {
    char name[16];
    unsigned char keep[8];
    unsigned char n = 0, ok;
    unsigned long at = CACHE_LIN + ((unsigned long)s << 17);
    v_map = 0;                                /* 128-255 the scene's again */
    if (loaded == s) {
        video_pal_load(SCENE_LIN, PAL_SCENE, 255 - PAL_SCENE);   /* (the map's banks were there) */
        return 1;
    }
    if (cached & (1U << s)) {
        dma_copy_far(SCENE_LIN, at, BANK5_LEN);
        dma_copy_far(BANK1_LIN, at + 0x10000UL, BANK1_LEN);
        loaded = s;
        video_pal_load(SCENE_LIN, PAL_SCENE, 255 - PAL_SCENE);
        return 1;
    }
    while (scene_file[s][n]) {
        name[n] = scene_file[s][n];
        n++;
    }
    name[n + 1] = 0;
    name[n] = 'A';
    /* (the packed file's last few bytes lie on the screen's first cells,
     * past the close-up's area: they are put back) */
    dma_copy_far((unsigned long)(unsigned int)keep, SCREEN_LIN, sizeof keep);
    ok = load_file(name, 5);
    if (ok) unpack(SCENE_LIN + scene_skip[s][0]);
    dma_copy_far(SCREEN_LIN, (unsigned long)(unsigned int)keep, sizeof keep);
    name[n] = 'B';
    if (ok) ok = load_file(name, 1);
    if (ok) unpack(BANK1_LIN + scene_skip[s][1]);
    loaded = ok ? s : 0xFF;
    if (ok) {
        dma_copy_far(at, SCENE_LIN, BANK5_LEN);
        dma_copy_far(at + 0x10000UL, BANK1_LIN, BANK1_LEN);
        cached |= 1U << s;
    }
    video_pal_load(SCENE_LIN, PAL_SCENE, 255 - PAL_SCENE);
    return ok;
}

void scene_forget(void) {
    loaded = 0xFF;
}

void scene_show(unsigned char frame) {
    unsigned char r;
    unsigned long src = SCENE_HDR + SCENE_HDR_SIZE + (unsigned long)frame * (SCENE_W * SCENE_H * 2);
    for (r = 0; r < SCENE_H; r++, src += SCENE_W * 2)
        dma_copy_far(SCREEN_LIN + ((unsigned int)(SC_ROW + r) * COLS + SC_COL) * 2, src, SCENE_W * 2);
}

