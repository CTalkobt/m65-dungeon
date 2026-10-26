/* load.c — files from the disk: the name as PETSCII for the KERNAL */
#include "load.h"
#include "video.h"

static char petname[17];

unsigned char load_file(const char *name, unsigned char bank) {
    unsigned char n = 0, ok;
    while (name[n] && n < 16) {
        petname[n] = name[n];
        n++;
    }
    petname[n] = 0;
    ld_name = petname;
    ld_len = n;
    ld_bank = bank;
    ok = ld_file();
    video_unlock();
    return ok;
}
