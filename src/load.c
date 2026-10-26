/* load.c — files from the disk: the name as PETSCII for the KERNAL */
#include "load.h"
#include "video.h"

static char petname[20];

/* the name, after prefix, for the KERNAL */
static void set_name(const char *prefix, const char *name) {
    unsigned char n = 0;
    while (*prefix) petname[n++] = *prefix++;
    while (*name && n < 19) petname[n++] = *name++;
    petname[n] = 0;
    ld_name = petname;
    ld_len = n;
}

unsigned char load_file(const char *name, unsigned char bank) {
    unsigned char ok;
    set_name("", name);
    ld_bank = bank;
    ok = ld_file();
    video_unlock();
    return ok;
}

void scratch_file(const char *name) {
    set_name("S0:", name);
    ld_cmd();
    video_unlock();
}

unsigned char save_file(const char *name, unsigned char bank, unsigned int from, unsigned int to) {
    unsigned char ok;
    scratch_file(name);
    set_name("", name);
    ld_bank = bank;
    ld_from = from;
    ld_to = to;
    ok = ld_save();
    video_unlock();
    return ok;
}
