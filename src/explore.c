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
#include <dma.h>
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

/* the depth's own stone (DGLOOK, in attic RAM): its characters over the
 * terrain's from TK_CORR0 on, its palette banks over the map's */
static unsigned char looked;

static void depth_look(void) {
    unsigned long at = LOOK_LIN + (unsigned long)(pl.depth - 1) * LOOK_SIZE;
    unsigned char k;
    looked = pl.depth;
    dma_copy_far((unsigned long)(TCHAR0 + TK_CORR0 * 4) * 64, at, LOOK_SIZE - 384);
    at += LOOK_SIZE - 384;
    for (k = 0; k < 3; k++, at += 128) dma_copy_far(TILES_LIN + PAL_MAP + k * 256, at, 128);
}

static void screen_in(void) {
    if (looked != pl.depth) depth_look();
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
    if (pl.dog_hp && !dog_place()) pl.dog_hp = 0, pl.dog_down = 1;   /* (the dog comes down too) */
    pl.dog_stay = 0;
    buf[0] = 0;
    str_cat(buf, "You descend into ");
    str_cat(buf, depth_name(pl.depth));
    str_cat(buf, ".");
    msg(buf, C_GOLD);
    if (pl.depth == 1) {
        msg("Arrows / WASD move, QEZC diagonally. ? for help.", C_DIM);
        msg(dog_says(buf, ", your dog, pads at your heel. B: stay / come."), C_DIM);
    }
    if (pl.depth == DEPTHS) msg("A cold voice whispers your name.", C_PURPLE);
    game_save();                     /* (each level begun is kept, while the screen is dark) */
    screen_in();
}

static const unsigned char drops[9] = {O_GOLD, O_GOLD, O_GOLD, O_POTION, O_SCROLL, O_DRAUGHT, O_RAGS, O_FUR, O_ARROWS};

/* the fight with creature i; WON / FLED / DIED, the screen back */
unsigned char fight(unsigned char i, unsigned char ambush) {
    unsigned char r, k, n;
    char buf[48];
    music_off();
    arrows_spent = 0;
    r = battle(i, ambush);
    if (r == DIED) return DIED;
    if (r == WON) {
        drop_carry(i);
        /* the arrows shot in the fight: one in two whole, gathered up */
        for (k = n = 0; k < arrows_spent; k++)
            if (rng_range(2)) n++;
        if (n) {
            pl.items[I_ARROWS] += n;
            buf[0] = 0;
            str_cat(buf, "You gather ");
            str_num(buf, n);
            str_cat(buf, n == 1 ? " arrow" : " arrows");
            str_cat(buf, " still whole.");
            msg(buf, C_TEXT);
        }
        if (mon_flags[mons[i].kind] & MF_BOSS) return WON;
        /* what it leaves: gold, now and then a potion, a draught, a scroll */
        r = rng_range(10);
        if (!obj_at(AT(mons[i].x, mons[i].y)) && r < sizeof drops) obj_set(AT(mons[i].x, mons[i].y), drops[r]);
        msg("The way is clear.", C_DIM);
    } else {
        msg("You escape, heart pounding.", C_DIM);   /* (battle.c: how long it is stunned) */
    }
    screen_in();
    return FLED;
}

/* the things on the floor one carries (pack.c's pick_up) */
static const unsigned char carried[20] = {
    0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};

/* ---------------------------------------------------------------- the player */

/* a shut door opened: its tile swung back */
static void door_open(unsigned char x, unsigned char y) {
    vm_set(x, y, vm_kind(x, y) + TK_DOOR_OPEN_H - TK_DOOR_H);
}


/* a turn's burning, and the warnings */
static void torch_burn(void) {
    if (!pl.lit) return;
    if (pl.steady && turn % (5 - pl.steady) == 0) return;   /* (a steady torch: 1 turn in 4, 3, 2 free) */
    if (WEARS(CH_LAMP) && (turn & 7) < 3 + pl.cq[pl.worn - 1]) return;   /* (the charm: 3-7 turns in 8 free, by its make) */
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
    m = mon_at(x, y);
    if (m) {
        t = (mon_habits[mons[m - 1].kind] & MH_MIMIC) && !mons[m - 1].awake;
        if (t) {
            msg("You reach for the chest. Its lid gapes, full of teeth!", C_RED);
            mons[m - 1].awake = 2;
        }
        *out = fight(m - 1, t);       /* (a mimic: it strikes first) */
        return 1;
    }
    if (DOG_AT(x, y)) {
        pl.dog_x = pl.x;                 /* (the dog: trade places) */
        pl.dog_y = pl.y;
        pl.x = x;
        pl.y = y;
        sfx(SFX_STEP);
        return 1;
    }
    t = terrain(x, y);
    if (t == TR_ROCK) return 0;
    if (t == TR_RUBBLE) {
        /* heaved at, a turn at a time: one time in three it gives */
        if (rng_range(3)) {
            msg(vm_kind(x, y) == TK_RUBBLE ? "You heave at the fallen stones." : "You batter at the cracked wall.", C_DIM);
            sfx(SFX_HEAVY);
            return 1;
        }
        vm_set(x, y, TK_CORR0 + ((x ^ y) & 1));
        msg("It gives way. The way is open.", C_TEXT);
        sfx(SFX_CLANG);
        return 1;
    }
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
    if (t == TR_WEDGED) {
        vm_set(x, y, vm_kind(x, y) - TK_DOOR_WEDGED_H + TK_DOOR_H);
        pl.items[I_STICKS]++;
        msg("You kick the wedge free and take up the sticks.", C_DIM);
        t = TR_DOOR;
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
    if (n == O_MERCHANT) return shop();
    if (n == O_FLAMES) {
        msg("Burning oil bars the way.", C_ORANGE);
        return 0;
    }
    if (n >= O_FOUNTAIN && n <= O_SHELF_BARE) return fixture(i, n);
    if (vm_kind(x, y) == TK_TRAP) {
        *out = disarm(x, y);         /* (a found plate: not stepped on) */
        return 1;
    }
    if (n == O_BRAZIER_COLD) {
        /* the torch lights it, spending some of itself: the room lit for good */
        if (!pl.lit) msg("A cold brazier, and your torch is out.", C_DIM);
        else if (pl.fuel < TORCH_GUTTER) msg("Your torch is too low to spare a flame.", C_DIM);
        else {
            pl.fuel -= 30;
            obj_set(i, O_BRAZIER);
            lit_light(x, y);
            msg("You light the brazier. The dark draws back.", C_ORANGE);
            sfx(SFX_FIRE);
            return 1;
        }
        return 0;
    }
    pl.x = x;
    pl.y = y;
    sfx(SFX_STEP);
    search(12 - pl.wary * 3);        /* (now and then, what hides is seen in passing) */
    switch (n) {
    case O_TRAP:
        *out = trap_spring();
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
    case O_GOLD:
    case O_ARROWS:
    case O_SHOT:
    case O_SCROLL:
        pick_up(i, n);
        break;
    default:
        if (n >= O_PILE || (n < sizeof carried && carried[n])) pick_up(i, n);
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

#ifdef TEST_SHOOT
/* (tests: a creature of that kind, asleep, a few places from the start) */
static void test_shoot(void) {
    static const signed char ox[4] = {3, -3, 0, 0}, oy[4] = {0, 0, 2, -2};
    unsigned char k;
    for (k = 0; k < 3; k++)
        if (terrain(pl.x + ox[k], pl.y + oy[k]) == TR_FLOOR) break;
    mons[0].x = pl.x + ox[k];
    mons[0].y = pl.y + oy[k];
    mons[0].kind = TEST_SHOOT;
    mons[0].hp = FRESH;
    mons[0].awake = 0;
}
#endif

#if defined(TEST_SECRET) || defined(TEST_RUBBLE)
/* (tests: the player beside a place of kind a or b, if the level has one) */
static void test_beside(unsigned char a, unsigned char b) {
    static const signed char sx[4] = {-1, 1, 0, 0}, sy[4] = {0, 0, -1, 1};
    unsigned char x, y, k, t;
    for (y = 1; y < MAP_H - 1; y++)
        for (x = 1; x < MAP_W - 1; x++) {
            t = vm_kind(x, y);
            if (t != a && t != b) continue;
            for (k = 0; k < 4; k++) {
                t = terrain(x + sx[k], y + sy[k]);
                if (t == TR_CORR || t == TR_FLOOR) {
                    pl.x = x + sx[k];
                    pl.y = y + sy[k];
                    msg(nth("(it is to the east)\0(it is to the west)\0(it is to the south)\0(it is to the north)", k), C_RED);
                    return;
                }
            }
        }
    msg("(there is none)", C_RED);
}
#endif

/* ---------------------------------------------------------------- travel */

/* a creature in sight (eyes in the dark too) */
static unsigned char foe_seen(void) {
    unsigned char k;
    for (k = 0; k < nmons; k++)
        if (mons[k].hp && vis_at(mons[k].x, mons[k].y) &&
            (mons[k].awake || !(mon_habits[mons[k].kind] & MH_MIMIC))) return 1;   /* (a mimic: a chest, till it moves) */
    return 0;
}

static unsigned char turn_pass(void);
static unsigned char step(signed char dx, signed char dy, unsigned char *out);

/* to the stairs: DIED or WON if a step ended that way, else 0 */
static unsigned char travel(void) {
    unsigned char k, hp, lit, r = 0;
    if (!way_find()) {
        msg("You have not found the stairs down.", C_DIM);
        return 0;
    }
    if (way_next() == 0xFF) {
        msg("You cannot recall a way to the stairs from here.", C_DIM);
        return 0;
    }
    if (foe_seen()) {
        msg("Not with something watching you.", C_RED);
        return 0;
    }
    msg("You make for the stairs. A key stops you.", C_DIM);
    for (;;) {
        k = way_next();
        if (k == 0xFF) break;
        hp = pl.hp;
        lit = pl.lit;
        if (!step((signed char)((k & 3) - 1), (signed char)((k >> 2) - 1), &r)) break;
        if (r == DIED || r == WON) return r;
        r = turn_pass();
        if (r == DIED || r == WON) return r;
        view_draw();
        view_hud();
        if (terrain(pl.x, pl.y) == TR_STAIRS) {
            msg("Stairs wind down into the dark. RETURN to descend.", C_GOLD);
            break;
        }
        if (foe_seen()) {
            msg("You stop. Something is near.", C_RED);
            break;
        }
        if (pl.hp < hp || pl.lit != lit || pl.ups) break;
        video_frames(3);
        if (key_get()) break;
    }
    return 0;
}

static unsigned char explore_run(unsigned char resume);

unsigned char explore(unsigned char resume) {
    unsigned char r;
    music_ticks = 0;
    r = explore_run(resume);
    music_off();
    return r;
}

static unsigned char explore_run(unsigned char resume) {
    char buf[40];
    unsigned char k, took, r;
    signed char dx, dy;
    if (resume) {
        msg("You take up your descent again.", C_GOLD);
        screen_in();
    } else {
#ifdef TEST_DEPTH
        pl.depth = TEST_DEPTH;       /* (tests: begin that deep) */
#endif
        level_enter();
    }
#ifdef TEST_ITEMS
    for (k = 0; k < I_COUNT; k++) pl.items[k] = 2;    /* (tests: two of everything) */
#endif
#ifdef TEST_VAULT
    test_vault();
#endif
#ifdef TEST_SHOOT
    test_shoot();
#endif
#ifdef TEST_SECRET
    test_beside(TK_SECRET_H, TK_SECRET_V);
#endif
#ifdef TEST_RUBBLE
    test_beside(TK_RUBBLE, TK_CRACK_NS);
#endif
#ifdef TEST_FIXTURE
    obj_set(AT(pl.x + 1, pl.y), O_FOUNTAIN);   /* (tests: a fountain east, a shelf west) */
    obj_set(AT(pl.x - 1, pl.y), O_SHELF);
#endif
#ifdef TEST_MERCHANT
    /* (tests: a merchant a step east, stocked, and gold to spend) */
    obj_set(AT(pl.x + 1, pl.y), O_MERCHANT);
    for (k = 0; k < WARES; k++) {
        ware[k] = k + 4;
        ware_left[k] = 2;
    }
    pl.gold = 100;
#endif
#ifdef TEST_WIN
    pl.kills = 9;                    /* (tests: the end, won) */
    return WON;
#endif
#ifdef TEST_BRAZIER
    /* (tests: the player just west of a cold brazier, if there is one) */
    for (k = 0; k < MAX_OBJ; k++)
        if (thing_kind[k] == O_BRAZIER_COLD && terrain(thing_at[k] % MAP_W - 1, thing_at[k] / MAP_W) == TR_FLOOR) {
            pl.x = thing_at[k] % MAP_W - 1;
            pl.y = thing_at[k] / MAP_W;
            msg("(a cold brazier to the east)", C_RED);
            break;
        }
#endif
#ifdef TEST_UPS
    pl.ups = TEST_UPS;               /* (tests: gifts to choose) */
#endif
#ifdef TEST_TRAVEL
    for (r = 0; r < MAP_H; r++)      /* (tests: the whole level remembered) */
        for (k = 0; k < MAP_W; k++) seen_set(AT(k, r), epoch());
#endif
#ifdef TEST_TRAP
    obj_set(AT(pl.x + 1, pl.y), O_TRAP);   /* (tests: a hidden trap one step east) */
#endif
#ifdef TEST_SIGHT
    pl.sight = SIGHT_TURNS;          /* (tests: dark-sight, the torch out) */
    pl.lit = 0;
#endif
#ifdef TEST_FUEL
    pl.fuel = TEST_FUEL;             /* (tests: the torch near its end) */
#endif
#ifdef TEST_BATTER
    for (r = 0; r < MAP_H; r++)          /* (tests: a wall two east, a wedged door in it, a hunter beyond) */
        if (terrain(pl.x + 2, r) == TR_FLOOR) vm_set(pl.x + 2, r, TK_WALL0 + 5);
    vm_set(pl.x + 2, pl.y, TK_DOOR_WEDGED_V);
    mons[0].x = pl.x + 3;
    mons[0].y = pl.y;
    mons[0].kind = TEST_BATTER;
    mons[0].hp = FRESH;
    mons[0].awake = 2;
    mons[0].stun = 0;
#endif
#ifdef TEST_DOG
    pl.dog_hp = TEST_DOG;              /* (tests: the dog nearly done; 0: away, back in 3 turns) */
    if (!pl.dog_hp) pl.dog_down = 3;
#endif
#ifdef TEST_SNIFF
#if TEST_SNIFF == 1
    mons[0].x = pl.dog_x + 1;          /* (tests: the dog's nose: a mimic, still a chest, */
    mons[0].y = pl.dog_y;              /* beside it, */
    mons[0].kind = M_MIMIC;
    mons[0].hp = FRESH;
    mons[0].awake = 0;
#else
    obj_set(AT(pl.x + 2, pl.y), O_TRAP);   /* or a hidden plate) */
#endif
#endif
#ifdef TEST_GEAR
    pl.wq = pl.aq = 3;                 /* (tests: Superior dragonbone and rune plate) */
    weapon_take(WEAPONS - 1);
    armour_take(ARMOURS - 1);
#endif
#ifdef TEST_LURE
    vm_set(pl.x + 2, pl.y, TK_TRAP);  /* (tests: a found plate two east, a hunter beyond) */
    mons[0].x = pl.x + 4;
    mons[0].y = pl.y;
    mons[0].kind = TEST_LURE;
    mons[0].hp = FRESH;
    mons[0].awake = 2;
    mons[0].stun = 0;
#endif
#ifdef TEST_OBJS
    for (k = r = 0; k < MAX_OBJ; k++) r += thing_kind[k] != O_NONE;   /* (tests: the things' list, how full) */
    video_num(60, 0, r, C_RED);
    key_wait();
#endif
#ifdef TEST_CHARMS
    for (k = 0; k < CHARMS; k++) pl.items[I_CHARM + k] = 1;   /* (tests: one of each charm, */
    pl.worn = 1;                                              /* and one worn: CH_* TEST_CHARMS) */
    pl.wfx = TEST_CHARMS + 1;
#endif
#ifdef TEST_VENOM
    pl.venom = 40;                   /* (tests: venom in the blood, and an antidote) */
    pl.items[I_ANTIDOTE] = 1;
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
    mons[0].x = pl.dog_x + 1;          /* (beside the dog: it joins in) */
    mons[0].y = pl.dog_y;
#ifdef TEST_HP
    mons[0].hp = TEST_HP;            /* (tests: the creature already wounded) */
#endif
    r = fight(0, 0);
    if (r == DIED || r == WON) return r;
#endif
    for (;;) {
        if (pl.ups) level_up();      /* (a level gained: its gift chosen, on the map) */
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
        case ' ': case '5': case '.':
            took = 1;                /* a rest: a search of the places round about */
            search(2);
            break;
        case 'h': case '?':
            help_screen();
            view_screen();
            break;
        case 'k':
            took = door_shut();
            break;
        case 'b':
            /* the dog bidden: stay, or come (no turn) */
            if (!pl.dog_hp) {
                msg(dog_says(buf, " is off in the dark."), C_DIM);
                break;
            }
            pl.dog_stay ^= 1;
            msg(dog_says(buf, pl.dog_stay ? " sits, and stays." : " bounds back to your side."), C_TEXT);
            break;
        case '@':
            if (!cheat_on) break;
            k = cheat();
            if (k) {
                pl.depth = k;
                msg_clear();
                level_enter();
                continue;
            }
            view_screen();
            break;
        case 'j':
            history_screen();
            view_screen();
            break;
        case 'p':
            took = drink();
            break;
        case 'i':
            took = inventory();
            break;
        case 'f':
            took = feed_any();
            break;
        case 't':
            took = shoot(0);
            break;
        case 'g':
            took = shoot(1);
            break;
        case 'x':
            took = throw_it(I_COUNT);
            break;
        case 'm':
            level_map();
            view_screen();
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
            r = travel();                /* (not on them: the way to them) */
            if (r == DIED || r == WON) return r;
            continue;
        case K_STOP:
            msg("S: save and stop. Y: give up. Another key: go on.", C_RED);
            k = key_wait();
            if (k == 'y') {
                pl.cause = CAUSE_QUIT;
                return DIED;
            }
            if (k == 's') {
                if (game_save()) return SAVED;
                msg("The disk will not take it.", C_RED);
                break;
            }
            msg("You steel yourself.", C_DIM);
            break;
        }
        if (dx || dy) took = step(dx, dy, &r);
        if (r == DIED || r == WON) return r;
        if (!took) continue;
        r = turn_pass();
        if (r == DIED || r == WON) return r;
    }
}

/* the rest of a turn taken: the player's mending, the torch, the creatures */
static unsigned char turn_pass(void) {
    turn++;
    if (!turn) turn = 1;
    if (pl.venom) {
        /* venom: no mending while it lasts, and now and then a pang (it
         * leaves 1 life at the least) */
        if (!--pl.venom) msg("The venom has run its course.", C_DIM);
        else if (turn % 3 == 0 && pl.hp > 1) pl.hp--;
    } else if (turn % (WEARS(CH_MEND) ? 6 - pl.cq[pl.worn - 1] : 12) == 0 && pl.hp < pl.maxhp) pl.hp++;
    if (turn % 18 == 0 && pl.mp < pl.maxmp) pl.mp++;
    if (turn % 64 == 0) memory_sweep();
    torch_burn();
    if (pl.sight && !--pl.sight) msg("The shadows close in again.", C_DIM);
    view_fov();
    return creatures_move();
}
