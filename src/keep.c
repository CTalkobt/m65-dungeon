/* keep.c — the game written to the disk and taken up again (DGSAVE); a new
 * player, and the levels gained (the overlay "gen")
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
#define VERSION    8

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
    part(lit_on, MAX_LIT);
    part(&vault_x0, 1);
    part(&vault_y0, 1);
    part(&vault_x1, 1);
    part(&vault_y1, 1);
    part(ware, WARES);
    part(ware_left, WARES);
    part(&rng_state, sizeof rng_state);
    part(&vol_music, 1);
    part(&vol_sfx, 1);
    part(note_kind, MAX_NOTES);
    part(note_at, sizeof note_at);
    return 1;
}

unsigned char game_save(void) {
    unsigned char x, y;
    unsigned char __far *k = (unsigned char __far *)(KEEP_LIN + KEEP_KINDS);
    audio_off();
    out = 1;
    parts();
    for (y = 0; y < MAP_H; y++) {
        video_poll();
        for (x = 0; x < MAP_W; x++) *k++ = vm_kind(x, y);
    }
    dma_copy_far(KEEP_LIN + KEEP_SEEN, SEEN_LIN, MAP_N);
    return save_file("DGSAVE", 1, (unsigned int)KEEP_LIN, (unsigned int)KEEP_LIN + KEEP_END);
}

/* the file into KEEP_LIN, and whether it is a save */
unsigned char game_peek(void) {
    const unsigned char __far *head = (const unsigned char __far *)(KEEP_LIN + KEEP_STATE);
    audio_off();
    if (!load_file("DGSAVE", 1)) return 0;
    out = 0;
    at = KEEP_STATE + 4;
    return head[0] == 'D' && head[1] == 'G' && head[2] == VERSION && head[3] == sizeof pl;
}

unsigned char game_load(void) {
    unsigned char x, y;
    const unsigned char __far *k = (const unsigned char __far *)(KEEP_LIN + KEEP_KINDS);
    if (!game_peek()) return 0;
    out = 0;
    parts();
    for (y = 0; y < MAP_H; y++) {
        video_poll();
        for (x = 0; x < MAP_W; x++) vm_set(x, y, *k++);
    }
    dma_copy_far(SEEN_LIN, KEEP_LIN + KEEP_SEEN, MAP_N);
    sound_levels();
    return 1;
}

void game_unsave(void) {
    scratch_file("DGSAVE");
}

/* ---------------------------------------------------------------- the player */

void player_new(void) {
    unsigned char k;
    pl.hp = pl.maxhp = 30;
    pl.mp = pl.maxmp = 12;
    pl.level = 1;
    for (k = 0; k < I_COUNT; k++) pl.items[k] = 0;
    pl.items[I_POTION] = 2;
    pl.items[I_STICKS] = 1;
    pl.items[I_FIRESTICK] = 2;
    pl.items[I_ARROWS] = 8;
    pl.bow = 1;
    pl.gun = 0;
    pl.fuel = 400;
    pl.lit = 1;
    pl.gold = 0;
    pl.xp = 0;
    pl.depth = 1;
    pl.fade = 2;
    weapon_take(0);
    armour_take(0);
    pl.ups = pl.keen = pl.steady = pl.aim = pl.wary = 0;
    pl.kills = 0;
    pl.cause = 0;
    /* the flasks' colours: a new order each descent, none known */
    for (k = 0; k < FLASKS; k++) pl.flask[k] = k;
    for (k = FLASKS - 1; k; k--) {
        unsigned char j = rng_range(k + 1), t = pl.flask[k];
        pl.flask[k] = pl.flask[j];
        pl.flask[j] = t;
    }
    pl.known = 0;
    pl.sight = 0;
    turn = 1;
}

/* the experience to reach each level after the first */
static const unsigned int next_level[] = {20, 50, 100, 170, 260, 380, 530, 700, 900, 65535};

unsigned char gain_xp(unsigned int xp) {
    unsigned char up = 0;
    pl.xp += xp;
    while (pl.level < 10 && pl.xp >= next_level[pl.level - 1]) {
        pl.level++;
        pl.maxhp += 4;
        pl.maxmp += 2;
        pl.hp = pl.maxhp;
        pl.mp = pl.maxmp;
        pl.ups++;
        up++;
    }
    return up;
}
