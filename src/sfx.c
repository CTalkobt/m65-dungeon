/* sfx.c — sound effects: a sampled sound where there is one (audio.c,
 * the sound packs), else SID 4's third voice (the twelfth: the music has
 * the other eleven); one voice, a new effect cuts the last. A SID effect is a
 * waveform, a pitch sliding each frame, an envelope and a length.
 */
#include "hw.h"
#include "game.h"
#include "sfxdata.h"

#define SID_V 0xD46E                  /* SID 4's third voice: the music leaves voice 12 free */

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

/* the sampled sound for each effect */
static const unsigned char sampled[SFX_COUNT] = {
    SND_STEP, SND_HIT, SND_HURT, SND_CLANG, SND_FIRE, SND_DOOR, SND_GROWL, SND_COIN, SND_DOOR,
    SND_COIN, SND_DEFEAT, SND_FROST, SND_SWING, SND_HEAVY, SND_FIRE, SND_HEAL, SND_WARD,
    SND_POISON, SND_CURSE, SND_DRINK, SND_VICTORY, SND_LEVEL, SND_DEFEAT, SND_STIR, SND_BELL,
    SND_TRAP, SND_CHEST,
};
/* without samples, the effects past the SID's own: the nearest SID one */
static const unsigned char sid_like[SFX_COUNT - SFX_SWING] = {
    SFX_HIT, SFX_HURT, SFX_SPELL, SFX_CHIME, SFX_SPELL, SFX_FROST, SFX_SPELL, SFX_CHIME,
    SFX_CHIME, SFX_CHIME, SFX_DEATH, SFX_GROWL, SFX_CHIME, SFX_HIT, SFX_CREAK,
};

static unsigned char left[1];
static unsigned int freq[1];
static signed int slide[1];
static unsigned char wave[1];

static void put(unsigned char v, unsigned char reg, unsigned char val) {
    (void)v;
    REG(SID_V + reg) = val;
}

/* the levels (settings.c): the music, the SIDs' volume (0-15); the
 * effects, the samples' (0-15: audio.c scales them); 0 is off */
unsigned char vol_music = 15, vol_sfx = 15;

void sound_levels(void) {
    unsigned char c;
    for (c = 0; c < 4; c++) REG(0xD418 + c * 0x20) = vol_music;
}

void sfx_on(void) {
    unsigned char c, r;
    for (c = 0; c < 4; c++)                      /* all four SIDs quiet */
        for (r = 0; r < 0x18; r++) REG(0xD400 + c * 0x20 + r) = 0;
    sound_levels();
}

void sfx_off(void) {
    unsigned char c;
    for (c = 0; c < 4; c++) REG(0xD418 + c * 0x20) = 0;
}

void sfx(unsigned char n) {
    const struct fx *f;
    unsigned char v = 0;
    if (!vol_sfx) return;
    if (audio_ok && sampled[n] != SND_NONE) {
        audio_play(sampled[n], 200, 200);
        return;
    }
    if (n >= SFX_SWING) n = sid_like[n - SFX_SWING];
    f = &fxs[n];
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

/* an effect from further off: quieter, toward the side it came from
 * (dx < 0 the left); the SIDs play it as they would close by */
void sfx_far(unsigned char n, signed char dx) {
    if (!vol_sfx) return;
    if (audio_ok && sampled[n] != SND_NONE) {
        audio_play(sampled[n], dx > 0 ? 25 : 80, dx < 0 ? 25 : 80);
        return;
    }
    sfx(n);
}

/* once a frame: the slides, the gates let go */
void sfx_update(void) {
    unsigned char v;
    audio_update();
    for (v = 0; v < 1; v++) {
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
