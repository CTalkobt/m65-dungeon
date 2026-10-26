/* rng.h — a 16-bit xorshift */
#pragma once

extern unsigned int rng_state;
unsigned int rng_next(void);
unsigned char rng_byte(void);
unsigned char rng_range(unsigned char n);              /* 0 .. n - 1 */
unsigned char rng_dice(unsigned char n, unsigned char sides);   /* n d sides */
