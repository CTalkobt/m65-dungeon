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
#include "hw.h"
#include "assets.h"
#include "video.h"
#include "load.h"
#include "rng.h"
#include "game.h"

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
    seed();
    rom_print("\x93loading...\r");
    if (!load_file("DGTILES", 4)) {
        rom_print("dgtiles missing: run dungeon from its disk\r");
        return 1;
    }
    unpack(TILES_LIN + TILES_SKIP);
    video_on();
    video_pal_load(TILES_LIN, PAL_TILES, PAL_SCENE - PAL_TILES);
    sfx_on();
    while (title()) {
        player_new();
        msg_clear();
        ending(explore());
    }
    video_fade(0);
    sfx_off();
    video_off();
    rom_print("\x93");
    return 0;
}
