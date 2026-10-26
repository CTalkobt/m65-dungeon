/* keep.c — the game written to the disk and taken up again (DGSAVE) (the
 * overlay "gen")
 *
 * The file is built in bank 1 at KEEP_LIN (the long sounds' play slot: the
 * sounds are stopped meanwhile) and loads back there: the player and the
 * level's state from bank 0 (parts), then each place's terrain tile (a
 * byte: the map's characters are drawn again from them) and its memory. */
#pragma cc45 overlay(gen)
#pragma cc45 noescape_strings
#include <dma.h>
#include "hw.h"
#include "video.h"
#include "load.h"
#include "rng.h"
#include "game.h"

#define KEEP_LIN   0x1A000UL
#define KEEP_STATE 0                  /* the parts, 1 KB at most */
#define KEEP_KINDS 1024
#define KEEP_SEEN  (KEEP_KINDS + MAP_N)
#define KEEP_END   (KEEP_SEEN + MAP_N)
#define VERSION    1

extern unsigned char lit_x0[MAX_LIT], lit_y0[MAX_LIT], lit_x1[MAX_LIT], lit_y1[MAX_LIT];
extern unsigned char vault_x0, vault_y0, vault_x1, vault_y1;

static unsigned int at;
static unsigned char out;                     /* 1 to the file, 0 from it */

static void part(void *p, unsigned int n) {
    if (out) dma_copy_far(KEEP_LIN + at, (unsigned long)(unsigned int)p, n);
    else dma_copy_far((unsigned long)(unsigned int)p, KEEP_LIN + at, n);
    at += n;
}

/* every part in its turn; 0 if what came from the file is not a save */
static unsigned char parts(void) {
    unsigned char head[4];
    head[0] = 'D';
    head[1] = 'G';
    head[2] = VERSION;
    head[3] = sizeof pl;
    at = KEEP_STATE;
    if (out) part(head, 4);
    else {
        part(head, 4);
        if (head[0] != 'D' || head[1] != 'G' || head[2] != VERSION || head[3] != sizeof pl) return 0;
    }
    part(&pl, sizeof pl);
    part(&turn, sizeof turn);
    part(&nmons, 1);
    part(mons, sizeof mons);
    part(thing_at, sizeof thing_at);
    part(thing_kind, sizeof thing_kind);
    part(thing_n, sizeof thing_n);
    part(&lit_n, 1);
    part(lit_x0, MAX_LIT);
    part(lit_y0, MAX_LIT);
    part(lit_x1, MAX_LIT);
    part(lit_y1, MAX_LIT);
    part(&vault_x0, 1);
    part(&vault_y0, 1);
    part(&vault_x1, 1);
    part(&vault_y1, 1);
    part(&rng_state, sizeof rng_state);
    part(&vol_music, 1);
    part(&vol_sfx, 1);
    return 1;
}

unsigned char game_save(void) {
    unsigned char x, y, row[MAP_W];
    audio_off();
    out = 1;
    parts();
    for (y = 0; y < MAP_H; y++) {
        video_poll();
        for (x = 0; x < MAP_W; x++) row[x] = vm_kind(x, y);
        dma_copy_far(KEEP_LIN + KEEP_KINDS + (unsigned int)y * MAP_W, (unsigned long)(unsigned int)row, MAP_W);
    }
    dma_copy_far(KEEP_LIN + KEEP_SEEN, SEEN_LIN, MAP_N);
    return save_file("DGSAVE", 1, (unsigned int)KEEP_LIN, (unsigned int)KEEP_LIN + KEEP_END);
}

/* the file into KEEP_LIN, and whether it is a save */
unsigned char game_peek(void) {
    audio_off();
    if (!load_file("DGSAVE", 1)) return 0;
    out = 0;
    at = KEEP_STATE;
    {
        unsigned char head[4];
        part(head, 4);
        return head[0] == 'D' && head[1] == 'G' && head[2] == VERSION && head[3] == sizeof pl;
    }
}

unsigned char game_load(void) {
    unsigned char x, y, row[MAP_W];
    if (!game_peek()) return 0;
    out = 0;
    parts();
    for (y = 0; y < MAP_H; y++) {
        video_poll();
        dma_copy_far((unsigned long)(unsigned int)row, KEEP_LIN + KEEP_KINDS + (unsigned int)y * MAP_W, MAP_W);
        for (x = 0; x < MAP_W; x++) vm_set(x, y, row[x]);
    }
    dma_copy_far(SEEN_LIN, KEEP_LIN + KEEP_SEEN, MAP_N);
    sound_levels();
    return 1;
}

void game_unsave(void) {
    scratch_file("DGSAVE");
}
