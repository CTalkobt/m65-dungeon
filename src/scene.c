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
 */
#include <dma.h>
#include "hw.h"
#include "assets.h"
#include "video.h"
#include "load.h"
#include "game.h"

#define SC_COL 12
#define SC_ROW 1

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
    unsigned char n = 0, ok;
    if (loaded == s) return 1;
    while (scene_file[s][n]) {
        name[n] = scene_file[s][n];
        n++;
    }
    name[n + 1] = 0;
    name[n] = 'A';
    ok = load_file(name, 5);
    if (ok) unpack(SCENE_LIN + scene_skip[s][0]);
    name[n] = 'B';
    if (ok) ok = load_file(name, 1);
    if (ok) unpack(0x12000UL + scene_skip[s][1]);
    loaded = ok ? s : 0xFF;
    video_pal_load(SCENE_LIN, PAL_SCENE, 255 - PAL_SCENE);
    return ok;
}

void scene_show(unsigned char frame) {
    unsigned char r;
    unsigned long src = SCENE_HDR + 64 + (unsigned long)frame * (SCENE_W * SCENE_H * 2);
    for (r = 0; r < SCENE_H; r++, src += SCENE_W * 2)
        dma_copy_far(SCREEN_LIN + ((unsigned int)(SC_ROW + r) * COLS + SC_COL) * 2, src, SCENE_W * 2);
}

