/* hw.h — registers and the memory map
 *
 * cc45's plain char/int/long are unsigned: signed values say so.
 *
 * Chip RAM:
 *   bank 0      the program (from $2001), the heap and stack up to $BFFF
 *   $40000      DGTILES: the palette (entries 16-127 the full-colour tiles,
 *               128-255 the map's palette banks); from $40400 the terrain's
 *               16-colour tiles (assets.h TK_*), the full-colour tiles (T_*),
 *               the frame and bar characters: all of bank 4
 *   $50000      the scene: a monster's close-up, a chest or an idol (DGMON*,
 *               DGCHEST, DGIDOL), loaded when it is needed: its palette
 *               (entries 128-255), a header, its characters
 *   $5E000      the screen: 80 x 50 cells of 2 bytes
 * Colour RAM at $FF80000, 2 bytes a cell; the map's memory (game.h seen_*)
 * at $FF82000.
 * Attic RAM: $8000000 the code overlays, $8070000 the tunes, $8080000 the
 * sounds, $8100000 the close-ups seen (128 KB each), $8300000 the level's
 * map: the terrain as characters, 128 x 96 (game.h vm_*), copied a row at a
 * time to the screen (view.c); $8310000 each depth's stone (DGLOOK: its
 * characters and palette banks, put in place as the level is shown).
 */
#pragma once

#define REG(a) (*(volatile unsigned char *)(a))

#define FRAMECOUNT REG(0xD7FA)       /* video frames elapsed (wraps) */

#define COLOR_LIN  0xFF80000UL
#define TILES_LIN  0x0040000UL
#define MAP_LIN    0x8300000UL       /* attic RAM (src/vmap.s45 knows it) */
#define SCENE_LIN  0x0050000UL
#define SCREEN_LIN 0x005E000UL
#define MUSIC_LIN  0x8070000UL       /* DGMUSIC, the tunes: attic RAM (tune_play_far) */
#define LOOK_LIN   0x8310000UL       /* DGLOOK, each depth's stone: attic RAM (explore.c) */

/* a file's palette: 256 red, 256 green, 256 blue (plain 8-bit values) */
#define PAL_BYTES  768
#define SCENE_HDR  (SCENE_LIN + PAL_BYTES)          /* assets.h SCENE_HDR_SIZE: what the creature is (gen_art.py header) */

#define COLS       80
#define ROWS       50

/* the palette: 0-15 the text colours (video.c), 16-127 the tiles',
 * 128-254 the scene's (247-254 its glow, which pulses) or, while the map
 * is shown, the map's palette banks (128-255: view.c) */
#define PAL_TILES  16
#define PAL_SCENE  128
#define PAL_GLOW   247        /* 247-254 (255 would show the colour RAM colour) */

/* text colours */
#define C_BLACK    0
#define C_TEXT     1          /* parchment */
#define C_DIM      2          /* faded parchment */
#define C_GOLD     3
#define C_RED      4
#define C_GREEN    5
#define C_BLUE     6          /* frost, mana */
#define C_ORANGE   7          /* fire */
#define C_GREY     8
#define C_WHITE    9
#define C_PURPLE   10
#define C_DARK     11         /* barely there */
#define C_HILITE   12         /* a chosen menu entry */
#define C_BLOOD    13
#define C_CYAN     14
#define C_BONE     15
