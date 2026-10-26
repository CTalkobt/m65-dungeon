/* load.h — files from the disk (load.s45) */
#pragma once

extern const char *ld_name;
extern unsigned char ld_len, ld_bank;
unsigned char ld_file(void);
extern unsigned int ld_from, ld_to;
unsigned char ld_save(void);
void ld_cmd(void);
/* name (ASCII, upper case) into its own address in bank: 1 if it loaded */
unsigned char load_file(const char *name, unsigned char bank);
/* from .. to - 1 of bank saved as name (the file starts with from, where
 * load_file puts it back): 1 if it was; an old file of the name is
 * scratched first */
unsigned char save_file(const char *name, unsigned char bank, unsigned int from, unsigned int to);
void scratch_file(const char *name);
