/* load.h — files from the disk (load.s45) */
#pragma once

extern const char *ld_name;
extern unsigned char ld_len, ld_bank;
unsigned char ld_file(void);
/* name (ASCII, upper case) into its own address in bank: 1 if it loaded */
unsigned char load_file(const char *name, unsigned char bank);
