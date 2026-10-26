/* sfx.c — sound effects on the SIDs: SID 1 (left) and SID 3 (right) play
 * the same; three voices, the oldest effect gives way. An effect is a
 * waveform, a pitch sliding each frame, an envelope and a length.
 */
#include "hw.h"
#include "game.h"

#define SID_L 0xD400
#define SID_R 0xD440

#define TRI   0x10
#define SAW   0x20
#define PULSE 0x40
#define NOISE 0x80

struct fx {
    unsigned char wave;
    unsigned int freq;
    signed int slide;
    unsigned char frames, ad, sr;
};

static const struct fx fxs[] = {
    {NOISE, 0x0600, 0, 2, 0x01, 0x00},          /* step */
    {NOISE, 0x2400, -0x200, 6, 0x09, 0x00},     /* hit */
    {SAW, 0x0700, -0x30, 12, 0x0A, 0x00},       /* hurt */
    {PULSE, 0x3A00, -0x40, 10, 0x0A, 0x00},     /* clang */
    {TRI, 0x0900, 0x160, 22, 0x2A, 0x00},       /* spell */
    {PULSE, 0x0240, 0x06, 34, 0x8C, 0x60},      /* creak */
    {SAW, 0x0300, -0x05, 32, 0x6B, 0x50},       /* growl */
    {PULSE, 0x3000, 0x500, 9, 0x09, 0x00},      /* coin */
    {NOISE, 0x0500, -0x10, 10, 0x0B, 0x00},     /* door */
    {TRI, 0x2600, 0, 28, 0x0C, 0x00},           /* chime */
    {TRI, 0x1A00, -0x70, 44, 0x0D, 0x00},       /* death */
    {NOISE, 0x7000, -0x280, 22, 0x2B, 0x00},    /* frost */
};

static unsigned char left[3], next;
static unsigned int freq[3];
static signed int slide[3];
static unsigned char wave[3];

static void put(unsigned char v, unsigned char reg, unsigned char val) {
    REG(SID_L + v * 7 + reg) = val;
    REG(SID_R + v * 7 + reg) = val;
}

void sfx_on(void) {
    unsigned char r;
    for (r = 0; r < 0x19; r++) {
        REG(SID_L + r) = 0;
        REG(SID_R + r) = 0;
    }
    REG(SID_L + 0x18) = 0x0F;
    REG(SID_R + 0x18) = 0x0F;
}

void sfx_off(void) {
    REG(SID_L + 0x18) = 0;
    REG(SID_R + 0x18) = 0;
}

void sfx(unsigned char n) {
    const struct fx *f = &fxs[n];
    unsigned char v = next;
    next = next == 2 ? 0 : next + 1;
    put(v, 4, 0);                                /* gate off: the envelope starts again */
    put(v, 5, f->ad);
    put(v, 6, f->sr);
    put(v, 2, 0x00);
    put(v, 3, 0x04);                             /* pulse width: a quarter */
    freq[v] = f->freq;
    slide[v] = f->slide;
    wave[v] = f->wave;
    left[v] = f->frames;
    put(v, 0, (unsigned char)f->freq);
    put(v, 1, (unsigned char)(f->freq >> 8));
    put(v, 4, f->wave | 1);
}

/* once a frame: the slides, the gates let go */
void sfx_update(void) {
    unsigned char v;
    for (v = 0; v < 3; v++) {
        if (!left[v]) continue;
        if (--left[v] == 0) {
            put(v, 4, wave[v]);
            continue;
        }
        if (slide[v]) {
            freq[v] += slide[v];
            put(v, 0, (unsigned char)freq[v]);
            put(v, 1, (unsigned char)(freq[v] >> 8));
        }
    }
}
