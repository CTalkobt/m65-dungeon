/* audio.c — the sampled sounds: decoded into attic RAM at the start,
 * played through the audio DMA from a slot in bank 1
 *
 * The sounds come in files DGSFX1, 2, ... (tools/gen_sfx.py): blocks of 16
 * samples, a byte (predictor << 4 | shift) and 8 bytes of 4-bit residues,
 * each sample its prediction from the two before plus (residue << shift).
 * They are decoded once into attic RAM (SND_ATTIC, sfxdata.c snd_at).
 *
 * The audio DMA reads chip RAM only, so a sound is copied into a play slot
 * when it is wanted: the long slot (16 KB, channel 0) or the short one
 * (8 KB, channel 1), whichever gen_sfx chose for it. A new sound in a slot
 * cuts off the one before. On the map (v_fire) a sound of the place now
 * and then - drips, a chain, footfalls - off to one side.
 *
 * Channel ch's registers are at $D720 + ch * 16: 0 control (bit 7 on, 5
 * unsigned, 1-0 10 = 8 bits), 1-3 base, 4-6 rate (Hz * 2^24 / 40.5 MHz),
 * 7-8 the end (low 16 bits: a sound never crosses a bank), 9 volume;
 * $D71C + ch the volume on the other side. $D711 bit 7: audio DMA on.
 */
#include <dma.h>
#include "hw.h"
#include "rng.h"
#include "load.h"
#include "video.h"
#include "game.h"
#include "sfxdata.h"

#define SND_ATTIC 0x8080000UL
#define SLOT_LONG 0x0019800UL
#define SLOT_SHORT 0x001D800UL
#define CH(ch, r) REG(0xD720 + (ch) * 16 + (r))

unsigned char audio_ok;
static unsigned char busy[2];                 /* frames each slot still plays */

static unsigned char buf[16 * 16];            /* decoded samples on their way to attic RAM */
static unsigned char blk[9 * 16];             /* coded blocks on their way from bank 5 */

/* one sound: n bytes of blocks at src (bank 5) to dst (attic RAM) */
static void decode(unsigned long src, unsigned int n, unsigned long dst) {
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
    }
}

void audio_init(void) {
#if SND_COUNT
    char name[8] = "DGSFX1";
    unsigned char f, n, k, hdr[1 + 3 * 32];
    unsigned long src;
    unsigned int len;
    for (f = 0; f < SND_FILES; f++) {
        name[5] = (char)('1' + f);
        if (!load_file(name, 5)) return;
        dma_copy_far((unsigned long)(unsigned int)hdr, SCENE_LIN, sizeof hdr);
        n = hdr[0];
        src = SCENE_LIN + 1 + (unsigned int)n * 3;
        for (k = 0; k < n; k++) {
            len = hdr[2 + k * 3] | (unsigned int)hdr[3 + k * 3] << 8;
            decode(src, len, SND_ATTIC + snd_at[hdr[1 + k * 3]]);
            src += len;
        }
    }
    for (k = 0; k < 4; k++) CH(k, 0) = 0;
    REG(0xD711) = 0x80;
    audio_ok = 1;
#endif
}

void audio_off(void) {
    unsigned char k;
    for (k = 0; k < 4; k++) CH(k, 0) = 0;
    REG(0xD711) = 0;
}

/* the sound id at volumes left and right (0-255) */
void audio_play(unsigned char id, unsigned char left, unsigned char right) {
#if SND_COUNT
    unsigned char ch;
    unsigned long at;
    unsigned int end;
    if (!audio_ok || id >= SND_COUNT) return;
    ch = snd_slot[id];
    at = ch ? SLOT_SHORT : SLOT_LONG;
    CH(ch, 0) = 0;                                 /* stopped while it is set up */
    dma_copy_far(at, SND_ATTIC + snd_at[id], snd_len[id]);
    end = (unsigned int)at + snd_len[id];
    CH(ch, 1) = (unsigned char)at;
    CH(ch, 2) = (unsigned char)(at >> 8);
    CH(ch, 3) = (unsigned char)(at >> 16);
    CH(ch, 0xA) = (unsigned char)at;               /* the current address: from the start */
    CH(ch, 0xB) = (unsigned char)(at >> 8);
    CH(ch, 0xC) = (unsigned char)(at >> 16);
    CH(ch, 4) = (unsigned char)SND_RATE_REG;
    CH(ch, 5) = (unsigned char)(SND_RATE_REG >> 8);
    CH(ch, 6) = (unsigned char)(SND_RATE_REG >> 16);
    CH(ch, 7) = (unsigned char)end;
    CH(ch, 8) = (unsigned char)(end >> 8);
    CH(ch, 9) = left;
    REG(0xD71C + ch) = right;
    CH(ch, 0) = 0xA2;                              /* on, unsigned, 8 bits, once */
    busy[ch] = (unsigned char)(snd_len[id] / 200 + 2);
#else
    (void)id; (void)left; (void)right;
#endif
}

/* once a frame: the slots' time; on the map, the place's sounds */
void audio_update(void) {
    unsigned char v;
    if (busy[0]) busy[0]--;
    if (busy[1]) busy[1]--;
#if SND_COUNT
    if (!audio_ok || !v_fire || busy[0] || busy[1]) return;
    if (rng_range(255) >= 1 || rng_range(2)) return;     /* about every 10 s */
    v = 70 + rng_range(70);
    if (rng_range(2)) audio_play(snd_ambient[rng_range(SND_AMBIENT)], v, v >> 2);
    else audio_play(snd_ambient[rng_range(SND_AMBIENT)], v >> 2, v);
#endif
}
