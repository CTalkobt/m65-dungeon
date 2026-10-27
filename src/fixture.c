/* fixture.c — what stands in the rooms to be used: a fountain to drink
 * from, a bookshelf to search; once each (the overlay "scenes") */
#pragma cc45 overlay(scenes)
#pragma cc45 noescape_strings
#include "hw.h"
#include "assets.h"
#include "video.h"
#include "rng.h"
#include "game.h"

unsigned char fixture(unsigned int i, unsigned char n) {
    unsigned char r, d;
    char buf[64];
    if (n == O_FOUNTAIN_DRY) {
        msg("The fountain is dry.", C_DIM);
        return 0;
    }
    if (n == O_SHELF_BARE) {
        msg("Nothing left on the shelf but dust.", C_DIM);
        return 0;
    }
    r = rng_range(10);
    if (n == O_FOUNTAIN) {
        obj_set(i, O_FOUNTAIN_DRY);
        sfx(SFX_DRINK);
        if (r < 4) {
            pl.hp = pl.maxhp;
            msg("Cold, clean water. You feel whole.", C_GREEN);
        } else if (r < 6) {
            pl.mp = pl.maxmp;
            msg("The water tingles with power.", C_BLUE);
        } else if (r < 8) {
            d = 3 + pl.depth;
            pl.hp = pl.hp > d ? pl.hp - d : 1;
            buf[0] = 0;
            str_cat(buf, "The water is foul! (");
            str_num(buf, d);
            str_cat(buf, ")");
            msg(buf, C_RED);
            sfx(SFX_POISON);
        } else if (r < 9) {
            memory_renew();
            msg("In the water you see the way you came.", C_CYAN);
        } else msg("The water tastes of iron, and no more.", C_DIM);
        msg("The fountain runs dry.", C_DIM);
        return 1;
    }
    /* a shelf */
    obj_set(i, O_SHELF_BARE);
    if (r < 3) {
        r = rng_range(10);
        r = r < 4 ? I_FIRE : r < 7 ? I_RECALL : I_SIGHT;
        pl.items[r]++;
        buf[0] = 0;
        str_cat(buf, "Between the rotted tomes: a ");
        str_cat(buf, item_name(r, 1));
        str_cat(buf, ".");
        msg(buf, C_BONE);
        sfx(SFX_CHIME);
    } else if (r < 5) {
        pl.items[I_RAGS]++;
        msg("Old pages, dry enough to burn: a twist of rags.", C_ORANGE);
    } else if (r < 6 && pl.known != (1 << FLASKS) - 1) {
        /* an apothecary's page: what one of the flasks does */
        do d = rng_range(FLASKS); while (pl.known & (1 << d));
        buf[0] = 0;
        str_cat(buf, "An apothecary's page names the ");
        str_cat(buf, item_name(I_FLASK + d, 1));
        str_cat(buf, ":");
        msg(buf, C_BONE);
        pl.known |= 1 << d;
        buf[0] = 0;
        str_cat(buf, "it is a ");
        str_cat(buf, item_name(I_FLASK + d, 1));
        str_cat(buf, ".");
        msg(buf, C_BONE);
        sfx(SFX_CHIME);
    } else if (r < 7) {
        msg("A journal, its last page legible:", C_DIM);
        msg(nth("'Feed the torch, and the dark keeps its distance.'\0"
                "'The deep makes you forget. Look, and look again.'\0"
                "'Oozes eat what they find. Keep your key close.'\0"
                "'Skeletons shoot from the dark. Close with them.'", rng_range(4)), C_BONE);
    } else {
        /* the bindings, at least, will burn */
        pl.items[I_RAGS]++;
        msg("Rotted bindings, dry enough to burn: a twist of rags.", C_ORANGE);
    }
    return 1;
}
