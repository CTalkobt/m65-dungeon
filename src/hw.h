/* hw.h — registers and the memory map
 *
 * cc45's plain char/int/long are unsigned: signed values say so.
 *
 * Chip RAM:
 *   bank 0      the program (from $2001), the heap and stack up to $BFFF
 *   $40000      DGTILES: the palette (entries 16-127), the exploration tiles,
 *               the frame and bar characters (assets.h T_*)
 *   $50000      the scene: a monster's close-up, a chest or an idol (DGMON*,
 *               DGCHEST, DGIDOL), loaded when it is needed: its palette
 *               (entries 128-255), a header, its characters
 *   $5E000      the screen: 80 x 50 cells of 2 bytes
 * Colour RAM at $FF80000, 2 bytes a cell.
 */
#pragma once

#define REG(a) (*(volatile unsigned char *)(a))

#define FRAMECOUNT REG(0xD7FA)       /* video frames elapsed (wraps) */

#define COLOR_LIN  0xFF80000UL
#define TILES_LIN  0x0040000UL
#define SCENE_LIN  0x0050000UL
#define SCREEN_LIN 0x005E000UL

/* a file's palette: 256 red, 256 green, 256 blue (plain 8-bit values) */
#define PAL_BYTES  768
#define SCENE_HDR  (SCENE_LIN + PAL_BYTES)          /* 64 bytes: assets.h SH_* */
#define SCENE_CHARS (SCENE_LIN + PAL_BYTES + 64)
#define SCENE_CHAR0 (unsigned int)(SCENE_CHARS / 64)
#define TILE_CHAR0 (unsigned int)((TILES_LIN + PAL_BYTES) / 64)

#define COLS       80
#define ROWS       50

/* the palette: 0-15 the text colours (video.c), 16-127 the tiles',
 * 128-254 the scene's (247-254 its glow, which pulses) */
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
