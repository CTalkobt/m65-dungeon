/* doors.c — a door beside the player shut, or wedged (K on the map); the
 * overlay "aim", where there is room (creatures.c has the battering) */
#pragma cc45 overlay(aim)
#pragma cc45 noescape_strings
#include "hw.h"
#include "assets.h"
#include "game.h"

/* K: a door beside the player shut (nothing in its way), or, already
 * shut, wedged with a bundle of sticks; 1 if that took the turn */
unsigned char door_shut(void) {
    unsigned char k, x, y, t, w;
    for (w = 0; w < 2; w++)
        for (k = 0; k < 9; k++) {
            x = pl.x + (signed char)(k % 3) - 1;
            y = pl.y + (signed char)(k / 3) - 1;
            t = terrain(x, y);
            if (k == 4 || obj_at(AT(x, y)) || mon_at(x, y)) continue;   /* (not where one stands) */
            if (!w && t == TR_OPEN) {
                vm_set(x, y, vm_kind(x, y) - TK_DOOR_OPEN_H + TK_DOOR_H);
                sfx(SFX_DOOR);
                msg("You push the door shut.", C_DIM);
                return 1;
            }
            if (w && t == TR_DOOR) {
                if (!pl.items[I_STICKS]) {
                    msg("You have no sticks to wedge the door with.", C_DIM);
                    return 0;
                }
                pl.items[I_STICKS]--;
                vm_set(x, y, vm_kind(x, y) - TK_DOOR_H + TK_DOOR_WEDGED_H);
                sfx(SFX_HEAVY);
                msg("You wedge the door shut with a bundle of sticks.", C_TEXT);
                return 1;
            }
        }
    msg("There is no door beside you to shut.", C_DIM);
    return 0;
}
