/* audio.c — the sampled sounds: decoded into attic RAM at the start,
 * played through the audio DMA from a slot in bank 1
 *
 * The sounds come in files DGSFX1, 2, ... (tools/gen_sfx.py; a header of 6
 * bytes a sound: its number, its coded length, ...): blocks of 16
 * samples, a byte (predictor << 4 | shift) and 8 bytes of 4-bit residues,
 * each sample its prediction from the two before plus (residue << shift).
 * They are decoded once into attic RAM (SND_ATTIC, sfxdata.c snd_at), at
 * the start, under the title: each file loads into the play slots (24 KB,
 * not yet in use; bank 5 holds the title's picture).
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
