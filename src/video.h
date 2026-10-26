/* video.h — the display: VIC-IV 80 x 50 (640 x 400), 16-bit characters
 *
 * Characters above $FF are full colour (64 bytes, a byte a pixel); below,
 * text from the ROM's lower-case character set in a colour of 0-15.
 */
#pragma once

void video_on(void);
void video_off(void);
void video_unlock(void);                     /* again after a load: the DOS locks the VIC-IV */
void video_clear(void);                      /* every cell blank */
void video_frame(void);                      /* wait for the next frame; animate the palette */
void video_frames(unsigned char n);
void video_poll(void);                       /* the music's frame, if one has passed: from long work */

/* text: ASCII (lower and upper case) at a cell in a text colour (hw.h C_*) */
void video_text(unsigned char col, unsigned char row, const char *s, unsigned char colour);
void video_centre(unsigned char row, const char *s, unsigned char colour);
void video_num(unsigned char col, unsigned char row, unsigned int v, unsigned char colour);
void video_fill(unsigned char col, unsigned char row, unsigned char n, unsigned char ch, unsigned char colour);
void video_blank(unsigned char col, unsigned char row, unsigned char w, unsigned char h);
/* n cells (character numbers) at a cell */
void video_cells(unsigned char col, unsigned char row, const unsigned int *cells, unsigned char n);
/* a block of w x h full-colour characters from first on, row by row */
void video_block(unsigned char col, unsigned char row, unsigned char w, unsigned char h, unsigned int first);

/* the palette: the master copy (plain 8-bit values) and how it is shown */
extern unsigned char pal_r[256], pal_g[256], pal_b[256];
/* entries first .. first + n - 1 of a file's palette (768 bytes) into the master */
void video_pal_load(unsigned long lin, unsigned char first, unsigned char n);
extern unsigned char v_bright;               /* 0 black .. 16 as it is */
extern unsigned char v_flicker;              /* 1: the tiles flicker like torchlight */
extern unsigned char v_fire;                 /* 1: the map's fire (PAL_FIRE) burns */
extern unsigned char v_music;                /* 1: a tune plays (tune_update a frame) */
extern unsigned char v_map;                  /* 1: 128-255 are the map's palette banks, not a scene's */
/* the scene (128-254) washed toward a colour by k / 16 (0 none) */
extern unsigned char v_wash, v_wash_r, v_wash_g, v_wash_b;
void video_pal_show(void);                   /* the master to the registers, now */
void video_fade(unsigned char to);           /* v_bright toward to, a step a frame */
void video_sway(signed char dx);             /* the screen moved dx pixels across */
void video_shake(unsigned char frames);
