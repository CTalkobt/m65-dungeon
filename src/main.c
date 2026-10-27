/* The Fading Deep — a torchlit descent for the MEGA65 (cc45)
 *
 * Five levels down to the lich. Explore by torchlight: what the torch
 * shows you remember only as the shape of the place, and that fades. Meet
 * a creature and the view closes in on it for a fight a turn at a time;
 * chests and idols have their close-ups too.
 *
 * 80 x 50 (640 x 400) in 16-bit characters; the art loads from the disk:
 * DGTILES at the start, each close-up when it is needed.
 *
 * Build with -DSEED=n for the same dungeons every time.
 */
#include <dma.h>
#include "hw.h"
#include "assets.h"
#include "video.h"
#include "load.h"
#include "rng.h"
#include "game.h"
#include "music.h"
#include <rom_ram.h>

/* nothing recurses and SAC keeps the variables static: a small stack */
#pragma cc45 stack_size(1024)

/* the program's text on the ROM's screen (before the display is ours) */
static void rom_print(const char *s) {
    unsigned char c;
    while ((c = (unsigned char)*s++) != 0) {
        if (c >= 0x61 && c <= 0x7A) c -= 0x20;            /* ASCII lower case to PETSCII */
        else if (c >= 0x41 && c <= 0x5A) c += 0x80;
        __jsr(0xFFD2, .A = c);
    }
}

static void seed(void) {
#ifdef SEED
    rng_state = SEED;
#else
    rng_state ^= (unsigned int)REG(0xD7EF) << 8 | REG(0xD7EF);
    rng_state ^= (unsigned int)FRAMECOUNT << 3 | REG(0xD012);
    if (!rng_state) rng_state = 0xACE1;
#endif
}

int main(void) {
    unsigned char r;
    seed();
    REG(0xD020) = 0;                          /* black while it loads */
    REG(0xD021) = 0;
    rom_print("\x93\x05loading...\r");
    if (!load_file("DGTILES", 4)) {
        rom_print("dgtiles missing: run dungeon from its disk\r");
        return 1;
    }
    unpack(TILES_LIN + TILES_SKIP);
    /* each depth's stone into attic RAM (through bank 5, free till now) */
    if (!load_file("DGLOOK", 5)) {
        rom_print("dglook missing: run dungeon from its disk\r");
        return 1;
    }
    unpack(SCENE_LIN + LOOK_SKIP);
    dma_copy_far(LOOK_LIN, SCENE_LIN, (unsigned int)(LOOK_SIZE * DEPTHS));
    /* the close-ups' shared backdrop into the C64 ROM's space */
    if (!load_file("DGBACK", 5)) {
        rom_print("dgback missing: run dungeon from its disk\r");
        return 1;
    }
    /* the C64 BASIC's and KERNAL's space: C65 mode never uses it (only
     * GO64 would, until a reset) */
    if (!romram_enable(ROMRAM_C64)) {
        rom_print("the rom space will not take writes\r");
        return 1;
    }
    unpack(SCENE_LIN);
    unpack(SCENE_LIN + BACK_STREAM2);
    romram_protect();
    /* the music into attic RAM */
    if (load_file("DGMUSIC", 5)) dma_copy_far(MUSIC_LIN, SCENE_LIN, TUNES_SIZE);
    video_on();
    video_pal_load(TILES_LIN, PAL_TILES, PAL_SCENE - PAL_TILES);
    sfx_on();
    title_show();                             /* the title first, then the sounds under it */
    audio_init();
    while ((r = title()) != 0) {
        if (r == 2) {
            if (!game_load()) continue;  /* (Continue: the saved descent) */
        } else {
            player_new();
            calling_take();
        }
        msg_clear();
        r = explore(r == 2);
        if (r != SAVED) ending(r);
    }
    video_fade(0);
    sfx_off();
    audio_off();
    video_off();
    rom_print("\x93");
    return 0;
}
