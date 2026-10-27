/* cheat.c — the tester's panel over the map (@ on the map, once "lantern"
 * is typed on the title, or a build with -DCHEAT): another depth, the
 * whole level known, every flask and charm known, mending, gold, a level,
 * the gear, a creature beside the player, the stairs, and so many of each
 * thing carried. (An overlay of its own: it costs the game nothing.)
 *
 * Up and down choose; left and right change what can be changed; RETURN
 * does it; RUN/STOP or @: back.
 */
#pragma cc45 overlay(cheat)
#pragma cc45 noescape_strings
#include "hw.h"
#include "assets.h"
#include "video.h"
#include "game.h"

#define CH_COL  14
#define CH_ROW  1
#define CH_W    52
#define CH_H    44
#define CH_ROWS 38                    /* the rows shown (more scroll) */
#define ROW_ITEMS 12                  /* the first item's row; I_COUNT of them */
#define NROWS   (ROW_ITEMS + I_COUNT)

static unsigned char depth, kind;

/* a place beside the player: nothing there, and walkable (0: none) */
static unsigned int beside(void) {
    unsigned char k, x, y, t;
    for (k = 0; k < 9; k++) {
        x = pl.x + (signed char)(k % 3) - 1;
        y = pl.y + (signed char)(k / 3) - 1;
        t = terrain(x, y);
        if (k != 4 && (t == TR_FLOOR || t == TR_CORR) && !mon_at(x, y) && !obj_at(AT(x, y))) return AT(x, y);
    }
    return 0;
}

/* a creature of kind beside the player, awake and hunting */
static void summon(void) {
    unsigned int at = beside();
    unsigned char k;
    struct mon *c;
    for (k = 0; k < nmons && mons[k].hp; k++) {}
    if (!at || k == MAX_MON) {
        msg("No room for it.", C_DIM);
        return;
    }
    if (k == nmons) nmons++;
    c = &mons[k];
    c->x = (unsigned char)(at % MAP_W);
    c->y = (unsigned char)(at / MAP_W);
    c->kind = kind;
    c->hp = FRESH;
    c->awake = 2;
    c->stun = c->carry = 0;
}

/* the player on the stairs; on the last level, in the lich's sanctum, a
 * few places west of it */
static void to_stairs(void) {
    unsigned char x, y;
    for (y = 0; y < MAP_H; y++)
        for (x = 0; x < MAP_W; x++)
            if (terrain(x, y) == TR_STAIRS) {
                pl.x = x;
                pl.y = y;
                return;
            }
    for (y = 0; y < nmons; y++)
        if (mon_flags[mons[y].kind] & MF_BOSS)
            for (x = 4; x; x--)
                if (terrain(mons[y].x - x, mons[y].y) == TR_FLOOR && !mon_at(mons[y].x - x, mons[y].y)) {
                    pl.x = mons[y].x - x;
                    pl.y = mons[y].y;
                    return;
                }
}

static void draw_row(unsigned char r, unsigned char row, unsigned char on) {
    char buf[48];
    unsigned char v = 0xFF;
    buf[0] = on ? '>' : ' ';
    buf[1] = ' ';
    buf[2] = 0;
    if (r >= ROW_ITEMS) {
        r -= ROW_ITEMS;
        v = pl.items[r];
        str_cat(buf, item_name(r, 2));
    } else {
        str_cat(buf, nth("Depth\0Reveal the level\0Know every flask and charm\0Mend: life, mana, torch, venom\0"
                         "100 gold\0A level\0Weapon\0Armour\0Bow\0Gun\0Creature\0To the stairs (the lich)", r));
        str_cat(buf, "  ");
        switch (r) {
        case 0: v = depth; break;
        case 6: str_cat(buf, weapon_name(pl.weapon)); break;
        case 7: str_cat(buf, armour_name(pl.armour)); break;
        case 8: str_cat(buf, pl.bow ? ranged_name(0, pl.bow) : "none"); break;
        case 9: str_cat(buf, pl.gun ? ranged_name(1, pl.gun) : "none"); break;
        case 10: str_cat(buf, mon_name(kind)); break;
        }
    }
    video_blank(CH_COL + 2, row, CH_W - 4, 1);
    video_text(CH_COL + 2, row, buf, on ? C_HILITE : r >= ROW_ITEMS ? C_TEXT : C_GOLD);
    if (v != 0xFF) {
        buf[0] = 0;
        str_num(buf, v);
        video_text(CH_COL + CH_W - 8, row, buf, on ? C_HILITE : C_CYAN);
    }
}

/* 0, or the depth to go to */
unsigned char cheat(void) {
    unsigned char k, r, sel = 0, top = 0, *n;
    depth = pl.depth;
    video_blank(CH_COL, CH_ROW, CH_W, CH_H);
    ui_frame(CH_COL, CH_ROW, CH_W, CH_H);
    video_text(CH_COL + 3, CH_ROW + 1, "The tester's lantern", C_PURPLE);
    video_text(CH_COL + 3, CH_ROW + CH_H - 2, "Left, right: change. RETURN: do. @: back.", C_DIM);
    for (;;) {
        if (sel < top) top = sel;
        if (sel >= top + CH_ROWS) top = sel - CH_ROWS + 1;
        for (r = top; r < NROWS && r < top + CH_ROWS; r++) draw_row(r, CH_ROW + 3 + r - top, r == sel);
        k = key_wait();
        n = sel >= ROW_ITEMS ? &pl.items[sel - ROW_ITEMS] : 0;
        if (k == K_UP) sel = sel ? sel - 1 : NROWS - 1;
        else if (k == K_DOWN) sel = sel + 1 < NROWS ? sel + 1 : 0;
        else if (k == K_STOP || k == '@') return 0;
        else if (k == K_LEFT || k == K_RIGHT) {
            r = k == K_RIGHT;
            if (n) {
                if (r && *n < 99) ++*n;
                else if (!r && *n) --*n;
            } else switch (sel) {
            case 0: depth = r ? (depth < DEPTHS ? depth + 1 : 1) : (depth > 1 ? depth - 1 : DEPTHS); break;
            case 6: weapon_take(r ? (pl.weapon + 1) % WEAPONS : (pl.weapon + WEAPONS - 1) % WEAPONS); break;
            case 7: armour_take(r ? (pl.armour + 1) % ARMOURS : (pl.armour + ARMOURS - 1) % ARMOURS); break;
            case 8: pl.bow = r ? (pl.bow + 1) % (RANGED_BEST + 1) : (pl.bow + RANGED_BEST) % (RANGED_BEST + 1); break;
            case 9: pl.gun = r ? (pl.gun + 1) % (RANGED_BEST + 1) : (pl.gun + RANGED_BEST) % (RANGED_BEST + 1); break;
            case 10: kind = r ? (kind + 1) % M_KINDS : (kind + M_KINDS - 1) % M_KINDS; break;
            }
        } else if (k == K_FIRE) {
            if (n) *n = *n > 89 ? 99 : *n + 10;
            else switch (sel) {
            case 0: return depth;
            case 1:
                level_reveal();
                for (r = 0; r < MAP_H; r++)
                    for (k = 0; k < MAP_W; k++)
                        if (terrain(k, r) != TR_ROCK) seen_set(AT(k, r), epoch());
                msg("The level unfolds.", C_PURPLE);
                return 0;
            case 2:
                pl.known = (1 << FLASKS) - 1;
                pl.cknown = (1 << CHARMS) - 1;
                break;
            case 3:
                pl.hp = pl.maxhp;
                pl.mp = pl.maxmp;
                pl.fuel = TORCH_MAX;
                pl.lit = 1;
                pl.venom = 0;
                break;
            case 4: pl.gold += 100; break;
            case 5:
                for (r = 0; r < 100 && !gain_xp(10); r++) {}
                return 0;
            case 10:
                summon();
                return 0;
            case 11:
                to_stairs();
                return 0;
            }
            sfx(SFX_CHIME);
        }
    }
}
