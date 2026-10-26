/* explore.c — walking the levels a turn at a time
 *
 * The player moves (8 ways), opens doors by walking into them (a locked
 * one with an iron key), picks up gold and what can be carried, opens
 * chests and prays at idols (the close-ups), fights what it walks into,
 * uses what it carries (I), goes down the stairs. The torch burns a turn
 * at a time: fed with rags, sticks, fur or tallow (F), lit again with
 * flint, a firestick (L) or a brazier's coals. Then every creature
 * moves: asleep until it notices the player, hunting while it sees the
 * player, wandering when it loses track. One that walks into the player
 * starts the fight close (an ambush).
 */
#pragma cc45 overlay(walk)
#pragma cc45 noescape_strings
#include <audio_tune.h>
#include "hw.h"
#include "assets.h"
#include "music.h"
#include "video.h"
#include "rng.h"
#include "game.h"


/* the levels' tune: on while the map is shown; a fight or a close-up
 * pauses it, and it goes on from where it was */
static unsigned int music_ticks;

static void music_on(void) {
    if (v_music) return;
    tune_play_far(MUSIC_LIN + TUNE_EXPLORE_AT);
    if (music_ticks) tune_seek(music_ticks);
    v_music = 1;
}

static void music_off(void) {
    if (!v_music) return;
    music_ticks = tune_ticks() % TUNE_EXPLORE_TICKS;
    v_music = 0;
    tune_stop();
}

static void screen_in(void) {
    video_pal_load(TILES_LIN, PAL_MAP, 128);  /* the map's palette banks (a close-up had them) */
    v_map = 1;
    v_flicker = 1;
    v_fire = 1;
    music_on();
    view_fov();
    view_screen();
    video_fade(16);
}

static void level_enter(void) {
    char buf[64];
    video_fade(0);
    dungeon_make();
    buf[0] = 0;
    str_cat(buf, "You descend into ");
    str_cat(buf, depth_name(pl.depth));
    str_cat(buf, ".");
    msg(buf, C_GOLD);
    if (pl.depth == 1) msg("Arrows / WASD move, QEZC diagonally. H for help.", C_DIM);
    if (pl.depth == DEPTHS) msg("A cold voice whispers your name.", C_PURPLE);
    screen_in();
}

static void help(void) {
    msg("Move: arrows, WASD, QEZC diagonally. Space: wait.", C_TEXT);
    msg("Walk into doors, chests, idols and foes.", C_TEXT);
    msg("I: inventory. P: potion. F: feed torch. L: light it.", C_TEXT);
    msg("RETURN: stairs down. O: settings. RUN/STOP: give up.", C_TEXT);
}

static const unsigned char drops[8] = {O_GOLD, O_GOLD, O_GOLD, O_POTION, O_SCROLL, O_DRAUGHT, O_RAGS, O_FUR};

/* the fight with creature i; WON / FLED / DIED, the screen back */
static unsigned char fight(unsigned char i, unsigned char ambush) {
    unsigned char r;
    music_off();
    r = battle(i, ambush);
    if (r == DIED) return DIED;
    if (r == WON) {
        if (mon_flags[mons[i].kind] & MF_BOSS) return WON;
        /* what it leaves: gold, now and then a potion, a draught, a scroll */
        r = rng_range(10);
        if (!obj_at(AT(mons[i].x, mons[i].y)) && r < sizeof drops) obj_set(AT(mons[i].x, mons[i].y), drops[r]);
        msg("The way is clear.", C_DIM);
    } else {
        mons[i].stun = 3;
        msg("You escape, heart pounding.", C_DIM);
    }
    screen_in();
    return FLED;
}

/* ---------------------------------------------------------------- creatures */

static signed char sgn(signed int v) {
    return v < 0 ? -1 : v > 0 ? 1 : 0;
}

/* a shut door opened: its tile swung back */
static void door_open(unsigned char x, unsigned char y) {
    vm_set(x, y, vm_kind(x, y) + TK_DOOR_OPEN_H - TK_DOOR_H);
}

static unsigned char free_for(struct mon *c, unsigned char x, unsigned char y) {
    unsigned char t = terrain(x, y);
    if (t == TR_ROCK || t == TR_LOCKED || obj_at(AT(x, y)) == O_BRAZIER || mon_at(x, y)) return 0;
    if (t == TR_DOOR) {
        door_open(x, y);                            /* it opens the door: that is its move */
        if (vis_at(x, y)) {
            msg("A latch clicks somewhere in the dark.", C_DIM);
            sfx_far(SFX_DOOR, (signed char)((signed int)x - pl.x));
        }
        return 0;
    }
    (void)c;
    return 1;
}

/* 0, or the outcome of a fight a creature started */
static unsigned char creatures_move(void) {
    unsigned char k, d, sees, x, y;
    signed char dx, dy;
    struct mon *c;
    for (k = 0; k < nmons; k++) {
        c = &mons[k];
        if (!c->hp) continue;
        if (c->stun) {
            c->stun--;
            continue;
        }
        dx = sgn((signed int)pl.x - c->x);
        dy = sgn((signed int)pl.y - c->y);
        d = (unsigned char)(c->x > pl.x ? c->x - pl.x : pl.x - c->x);
        y = (unsigned char)(c->y > pl.y ? c->y - pl.y : pl.y - c->y);
        if (y > d) d = y;
        sees = vis_at(c->x, c->y) && d <= 10;
        if (!c->awake) {
            if (sees && d <= 6 && rng_range(3) == 0) {
                c->awake = 2;
                if (vis_at(c->x, c->y) != VIS_SHADOW) msg("Something stirs.", C_ORANGE);
                sfx(SFX_STIR);
            }
            continue;
        }
        if ((mon_flags[c->kind] & MF_SLOW) && (turn & 1)) continue;
        if (sees) c->awake = 2;
        else if (c->awake == 2 && rng_range(8) == 0) c->awake = 1;
        if (c->awake == 1) {
            if (rng_range(2)) continue;
            dx = (signed char)rng_range(3) - 1;
            dy = (signed char)rng_range(3) - 1;
        }
        if (!dx && !dy) continue;
        x = c->x + dx;
        y = c->y + dy;
        if (x == pl.x && y == pl.y) {
            if (c->awake != 2) continue;
            return fight(k, 1);
        }
        if (free_for(c, x, y)) { c->x = x; c->y = y; continue; }
        if (dx && free_for(c, c->x + dx, c->y) && !(c->x + dx == pl.x && c->y == pl.y)) { c->x += dx; continue; }
        if (dy && free_for(c, c->x, c->y + dy) && !(c->x == pl.x && c->y + dy == pl.y)) { c->y += dy; continue; }
    }
    return 0;
}

/* ---------------------------------------------------------------- the player */

/* the things on the floor one carries: their item (0xFF: not carried) */
static const unsigned char obj_item[18] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, I_POTION, 0xFF, 0xFF, 0xFF,
    0xFF, I_KEY, I_DRAUGHT, I_RAGS, I_STICKS, I_FUR, I_CANDLE, I_FLINT, I_FIRESTICK,
};

/* what lay at i taken: one more of item */
static void take(unsigned int i, unsigned char item, const char *s, unsigned char c) {
    pl.items[item]++;
    obj_set(i, O_NONE);
    msg(s, c);
    sfx(SFX_CHIME);
}

/* a turn's burning, and the warnings */
static void torch_burn(void) {
    if (!pl.lit) return;
    if (pl.fuel) pl.fuel--;
    if (pl.fuel == TORCH_LOW) msg("Your torch burns lower.", C_ORANGE);
    else if (pl.fuel == TORCH_GUTTER) msg("Your torch gutters. Feed it, or it will die.", C_ORANGE);
    else if (!pl.fuel) {
        pl.lit = 0;
        msg("Your torch sputters out. Darkness closes in.", C_RED);
    }
}

/* moving (or bumping) toward dx, dy: 1 if it took a turn; the outcome of
 * a fight in *out */
static unsigned char step(signed char dx, signed char dy, unsigned char *out) {
    unsigned char x = pl.x + dx, y = pl.y + dy, m, t, n;
    unsigned int i = AT(x, y);
    char buf[64];
    m = mon_at(x, y);
    if (m) {
        *out = fight(m - 1, 0);
        return 1;
    }
    t = terrain(x, y);
    if (t == TR_ROCK) return 0;
    if (t == TR_LOCKED) {
        if (!pl.items[I_KEY]) {
            msg("The door is locked. An iron key would turn it.", C_DIM);
            return 0;
        }
        pl.items[I_KEY]--;
        vm_set(x, y, vm_kind(x, y) - TK_DOOR_LOCKED_H + TK_DOOR_OPEN_H);
        sfx(SFX_DOOR);
        msg("The iron key turns. The door swings open.", C_GOLD);
        return 1;
    }
    if (t == TR_DOOR) {
        door_open(x, y);
        sfx(SFX_DOOR);
        msg("The latch clicks, and the door swings open.", C_DIM);
        return 1;
    }
    n = obj_at(i);
    if (n == O_BRAZIER) {
        if (!pl.lit) return light(I_COUNT);
        msg("The brazier's coals glow too hot to pass.", C_ORANGE);
        return 0;
    }
    pl.x = x;
    pl.y = y;
    sfx(SFX_STEP);
    switch (n) {
    case O_GOLD:
        n = (2 + rng_range(10)) * pl.depth;
        pl.gold += n;
        obj_set(i, O_NONE);
        buf[0] = 0;
        str_cat(buf, "You gather ");
        str_num(buf, n);
        str_cat(buf, " gold coins.");
        msg(buf, C_GOLD);
        sfx(SFX_COIN);
        break;
    case O_SCROLL:
        n = rng_range(10);
        n = n < 4 ? I_FIRE : n < 7 ? I_RECALL : I_SIGHT;
        buf[0] = 0;
        str_cat(buf, "You take up a ");
        str_cat(buf, item_name(n, 1));
        str_cat(buf, ".");
        take(i, n, buf, C_BONE);
        break;
    case O_CHEST:
        view_fov();
        view_draw();
        music_off();
        chest_view(i);
        screen_in();
        break;
    case O_CHEST_OPEN:
        msg("An empty chest.", C_DIM);
        break;
    case O_IDOL:
        view_fov();
        view_draw();
        music_off();
        idol_view(i);
        screen_in();
        break;
    case O_IDOL_SPENT:
        msg("The idol's eyes are dark now.", C_DIM);
        break;
    default:
        if (n >= sizeof obj_item || obj_item[n] == 0xFF) break;
        buf[0] = 0;
        str_cat(buf, "You pick up a ");
        str_cat(buf, item_name(obj_item[n], 1));
        str_cat(buf, ".");
        take(i, obj_item[n], buf, item_col[obj_item[n]]);
        break;
    case O_BONES:
        msg(nth("Old bones crunch underfoot.\0A skull grins up at you from the dust.\0"
                    "Someone came this way before you. They stayed.\0Gnawed bones, and the gnawing is recent.",
                    (x + y) & 3), C_DIM);
        break;
    }
    if (t == TR_STAIRS) msg("Stairs wind down into the dark. RETURN to descend.", C_GOLD);
    return 1;
}

#ifdef TEST_VAULT
/* (tests: the player outside a locked door, if the level has one; the
 * message says which way the door is) */
static void test_vault(void) {
    static const signed char sx[4] = {-1, 1, 0, 0}, sy[4] = {0, 0, -1, 1};
    unsigned char x, y, k, t;
    for (k = 0; k < MAX_OBJ; k++)
        if (thing_kind[k] == O_KEY)
            msg(vault_at(thing_at[k] % MAP_W, thing_at[k] / MAP_W) ? "(the key is in the vault!)" : "(the key lies outside)", C_RED);
    for (y = 1; y < MAP_H - 1; y++)
        for (x = 1; x < MAP_W - 1; x++) {
            if (terrain(x, y) != TR_LOCKED) continue;
            for (k = 0; k < 4; k++) {
                t = terrain(x + sx[k], y + sy[k]);
                if ((t == TR_CORR || t == TR_FLOOR) && !vault_at(x + sx[k], y + sy[k])) {
                    pl.x = x + sx[k];
                    pl.y = y + sy[k];
                    msg(nth("(a locked door to the east)\0(a locked door to the west)\0"
                            "(a locked door to the south)\0(a locked door to the north)", k), C_RED);
                    return;
                }
            }
        }
    msg("(no locked door)", C_RED);
}
#endif

static unsigned char explore_run(void);

unsigned char explore(void) {
    unsigned char r;
    music_ticks = 0;
    r = explore_run();
    music_off();
    return r;
}

static unsigned char explore_run(void) {
    unsigned char k, took, r;
    signed char dx, dy;
    level_enter();
#ifdef TEST_ITEMS
    for (k = 0; k < I_COUNT; k++) pl.items[k] = 2;    /* (tests: two of everything) */
#endif
#ifdef TEST_VAULT
    test_vault();
#endif
#ifdef TEST_FUEL
    pl.fuel = TEST_FUEL;             /* (tests: the torch near its end) */
#endif
#ifdef TEST_VIEW
    /* (tests: a chest (1) or an idol (2) at once) */
    if (TEST_VIEW == 1) chest_view(AT(pl.x, pl.y));
    else idol_view(AT(pl.x, pl.y));
    screen_in();
#endif
#ifdef TEST_CACHE
    scene_load(S_RAT);               /* (tests: the next close-up from the cache) */
#endif
#ifdef TEST_FIGHT
    /* (tests: a fight with that kind of creature at once) */
    mons[0].kind = TEST_FIGHT;
    mons[0].hp = FRESH;
    r = fight(0, 0);
    if (r == DIED || r == WON) return r;
#endif
    for (;;) {
#ifdef TEST_TIME
        k = FRAMECOUNT;              /* (tests: the frames the sight and the drawing take) */
#endif
        view_fov();
        view_draw();
        view_hud();
#ifdef TEST_TIME
        video_num(60, 0, (unsigned char)(FRAMECOUNT - k), C_RED);
#endif
        k = key_wait();
        took = 0;
        r = 0;
        dx = dy = 0;
        switch (k) {
        case K_UP: case 'w': case '8': dy = -1; break;
        case K_DOWN: case 's': case '2': dy = 1; break;
        case K_LEFT: case 'a': case '4': dx = -1; break;
        case K_RIGHT: case 'd': case '6': dx = 1; break;
        case 'q': case '7': dx = -1; dy = -1; break;
        case 'e': case '9': dx = 1; dy = -1; break;
        case 'z': case '1': dx = -1; dy = 1; break;
        case 'c': case '3': dx = 1; dy = 1; break;
        case ' ': case '5': case '.': took = 1; break;
        case 'h': case '?': help(); break;
        case 'p':
            took = drink();
            break;
        case 'i':
            took = inventory();
            break;
        case 'f':
            took = feed_any();
            break;
        case 'o':
            settings(18, 14);
            view_draw();
            break;
        case 'l':
            took = light_any();
            break;
        case K_FIRE: case '>':
            if (terrain(pl.x, pl.y) == TR_STAIRS) {
                pl.depth++;
                pl.fade = 2;
                msg_clear();
                level_enter();
                continue;
            }
            msg("There are no stairs here.", C_DIM);
            break;
        case K_STOP:
            msg("Give up? Y to end your delve, any other key to go on.", C_RED);
            if (key_wait() == 'y') return DIED;
            msg("You steel yourself.", C_DIM);
            break;
        }
        if (dx || dy) took = step(dx, dy, &r);
        if (r == DIED || r == WON) return r;
        if (!took) continue;
        turn++;
        if (!turn) turn = 1;
        if (turn % 12 == 0 && pl.hp < pl.maxhp) pl.hp++;
        if (turn % 18 == 0 && pl.mp < pl.maxmp) pl.mp++;
        if (turn % 64 == 0) memory_sweep();
        torch_burn();
        view_fov();
        r = creatures_move();
        if (r == DIED || r == WON) return r;
    }
}
