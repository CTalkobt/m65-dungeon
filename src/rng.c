/* rng.c — a 16-bit xorshift (7, 9, 8) */
#include "rng.h"

unsigned int rng_state = 0xACE1;

unsigned int rng_next(void) {
    unsigned int x = rng_state;
    x ^= x << 7;
    x ^= x >> 9;
    x ^= x << 8;
    rng_state = x;
    return x;
}

unsigned char rng_byte(void) {
    return (unsigned char)(rng_next() >> 4);
}

unsigned char rng_range(unsigned char n) {
    return (unsigned char)(((unsigned long)(rng_next() & 0x7FFF) * n) >> 15);
}

unsigned char rng_dice(unsigned char n, unsigned char sides) {
    unsigned char t = 0;
    while (n--) t += rng_range(sides) + 1;
    return t;
}
