/* audio_boot.c — the sampled sounds decoded into attic RAM, once, at the
 * start (the overlay "front", with the title they load under; audio.c
 * plays them)
 */
#pragma cc45 overlay(front)
#include <dma.h>
#include "hw.h"
#include "load.h"
#include "video.h"
#include "game.h"
#include "sfxdata.h"

#define SND_ATTIC 0x8080000UL
#define SLOT_LONG 0x0019800UL
#define CH(ch, r) REG(0xD720 + (ch) * 16 + (r))


/* one sound: n bytes of blocks at src (the play slots) to dst (attic RAM) */
static unsigned char quips;                   /* loading_tick while decoding */

static void decode(unsigned long src, unsigned int n, unsigned long dst) {
    /* (locals: their storage is shared with functions not running now) */
    unsigned char buf[16 * 16];               /* decoded samples on their way to attic RAM */
    unsigned char blk[9 * 16];                /* coded blocks on their way from the file */
    signed int s1 = 0, s2 = 0, p, x, r;
    unsigned char k, b, f, sh, i, *o;
    unsigned int chunk;
    while (n) {
        chunk = n > sizeof blk ? sizeof blk : n;
        dma_copy_far((unsigned long)(unsigned int)blk, src, chunk);
        src += chunk;
        n -= chunk;
        o = buf;
        for (b = 0; b < chunk; b += 9) {
            f = blk[b] >> 4;
            sh = blk[b] & 15;
            for (k = 0; k < 16; k++) {
                i = blk[b + 1 + (k >> 1)];
                r = k & 1 ? i >> 4 : i & 15;
                if (r >= 8) r -= 16;
                switch (f) {
                case 0: p = 0; break;
                case 1: p = s1 - (s1 >> 4); break;
                case 2:
                    x = 2 * s1 - s2;
                    p = x - (x >> 4);
                    if (p > 16383) p = 16383;
                    if (p < -16384) p = -16384;
                    break;
                default:
                    p = s1 + ((s1 - s2) >> 1);
                    if (p > 16383) p = 16383;
                    if (p < -16384) p = -16384;
                }
                x = p + (r << sh);
                if (x > 8191) x = 8191;
                if (x < -8192) x = -8192;
                s2 = s1;
                s1 = x;
                *o++ = (unsigned char)((unsigned int)(x + 8192) >> 6);
            }
        }
        dma_copy_far(dst, (unsigned long)(unsigned int)buf, (unsigned int)(o - buf));
        dst += o - buf;
        if (quips) loading_tick();
    }
}

void audio_init(void) {
#if SND_COUNT
    quips = 1;
    char name[8] = "DGSFX1";
    unsigned char f, n, k;
    const unsigned char __far *e;             /* a sound's header in the file */
    unsigned long src;
    unsigned int len;
    for (f = 0; f < SND_FILES; f++) {
        name[5] = (char)('1' + f);
        loading_tick();
        if (!load_file(name, 1)) return;          /* into the play slots: free till now */
        n = *(const unsigned char __far *)SLOT_LONG;
        src = SLOT_LONG + 1 + (unsigned int)n * 6;
        e = (const unsigned char __far *)(SLOT_LONG + 1);
        for (k = 0; k < n; k++, e += 6) {
            /* its header: number, coded length, decoded length, slot */
            len = e[1] | (unsigned int)e[2] << 8;
            decode(src, len, SND_ATTIC + snd_at[e[0]]);
            src += len;
        }
    }
    for (k = 0; k < 4; k++) CH(k, 0) = 0;
    REG(0xD711) = 0x80;
    audio_ok = 1;
#endif
}

