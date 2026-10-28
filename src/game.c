/* game.c — the creatures, the player, the names of things */
#include "hw.h"
#include "game.h"
#include "assets.h"

unsigned char scratch[SCRATCH_SIZE];
struct player pl;
unsigned int turn;
struct mon mons[MAX_MON];
unsigned char nmons;
#ifdef CHEAT
unsigned char cheat_on = 1;
#else
unsigned char cheat_on;
#endif

/* the things lying about: a short list */
unsigned int thing_at[MAX_OBJ];
unsigned char thing_kind[MAX_OBJ];
unsigned char thing_n[MAX_OBJ];               /* a pile's count (O_PILE) */

void obj_clear(void) {
    unsigned char k;
    for (k = 0; k < MAX_OBJ; k++) thing_kind[k] = O_NONE;
}

unsigned char obj_at(unsigned int at) {
    unsigned char k;
    for (k = 0; k < MAX_OBJ; k++)
        if (thing_kind[k] && thing_at[k] == at) return thing_kind[k];
    return O_NONE;
}

void obj_set(unsigned int at, unsigned char kind) {
    unsigned char k, f = 0xFF;
    for (k = 0; k < MAX_OBJ; k++) {
        if (thing_kind[k] && thing_at[k] == at) break;
        if (!thing_kind[k] && f == 0xFF) f = k;
    }
    if (k == MAX_OBJ) {
        if (!kind || f == 0xFF) return;
        k = f;
    }
    thing_at[k] = at;
    thing_kind[k] = kind;
    thing_n[k] = 1;
}

/* n more of what is at; 0 if there is no room in the list */
unsigned char obj_add(unsigned int at, unsigned char kind, unsigned char n) {
    unsigned char k;
    for (k = 0; k < MAX_OBJ; k++)
        if (thing_kind[k] == kind && thing_at[k] == at) {
            thing_n[k] = thing_n[k] + n < thing_n[k] ? 255 : thing_n[k] + n;
            return 1;
        }
    obj_set(at, kind);
    for (k = 0; k < MAX_OBJ; k++)
        if (thing_kind[k] == kind && thing_at[k] == at) {
            thing_n[k] = n;
            return 1;
        }
    return 0;
}

unsigned char obj_count(unsigned int at) {
    unsigned char k;
    for (k = 0; k < MAX_OBJ; k++)
        if (thing_kind[k] && thing_at[k] == at) return thing_n[k];
    return 0;
}

/* a room's floor again where a plate was (as dungeon.c lays it) */
void floor_set(unsigned char x, unsigned char y) {
    vm_set(x, y, TK_FLOOR0 + (((x * 7) ^ (y * 13)) & 3));
}

/* the terrain from the tile kind: TK_ROCK, FLOOR0-3, CORR0-1, WALL0-15,
 * DOOR_H/V, DOOR_OPEN_H/V, STAIRS, DOOR_LOCKED_H/V, SECRET_H/V, TRAP,
 * RUBBLE, CRACK_NS/EW (gen_art.py's order) */
unsigned char terrain(unsigned char x, unsigned char y) {
    unsigned char k = vm_kind(x, y);
    if (k < TK_FLOOR0) return TR_ROCK;
    if (k < TK_CORR0) return TR_FLOOR;
    if (k < TK_WALL0) return TR_CORR;
    if (k < TK_DOOR_H) return TR_ROCK;
    if (k < TK_DOOR_OPEN_H) return TR_DOOR;
    if (k < TK_STAIRS) return TR_OPEN;
    if (k == TK_STAIRS) return TR_STAIRS;
    if (k <= TK_DOOR_LOCKED_V) return TR_LOCKED;
    if (k == TK_TRAP) return TR_FLOOR;      /* (one found: still armed, walked into to disarm) */
    if (k >= TK_DOOR_WEDGED_H) return TR_WEDGED;
    if (k >= TK_RUBBLE) return TR_RUBBLE;
    return TR_ROCK;                         /* a secret door: a wall, till it is found */
}

unsigned char lit_x0[MAX_LIT], lit_y0[MAX_LIT], lit_x1[MAX_LIT], lit_y1[MAX_LIT];   /* (keep.c saves them) */
unsigned char lit_on[MAX_LIT];
unsigned char lit_n;

void lit_add(unsigned char x0, unsigned char y0, unsigned char x1, unsigned char y1, unsigned char on) {
    if (lit_n >= MAX_LIT) return;
    lit_on[lit_n] = on;
    lit_x0[lit_n] = x0;
    lit_y0[lit_n] = y0;
    lit_x1[lit_n] = x1;
    lit_y1[lit_n] = y1;
    lit_n++;
}

/* the vault: a room behind locked doors (none: x1 0) */
unsigned char vault_x0, vault_y0, vault_x1, vault_y1;

void vault_set(unsigned char x0, unsigned char y0, unsigned char x1, unsigned char y1) {
    vault_x0 = x0;
    vault_y0 = y0;
    vault_x1 = x1;
    vault_y1 = y1;
}

unsigned char vault_at(unsigned char x, unsigned char y) {
    return x >= vault_x0 && x <= vault_x1 && y >= vault_y0 && y <= vault_y1;
}

unsigned char lit_at(unsigned char x, unsigned char y) {
    unsigned char k;
    for (k = 0; k < lit_n; k++)
        if (lit_on[k] && x >= lit_x0[k] && x <= lit_x1[k] && y >= lit_y0[k] && y <= lit_y1[k]) return 1;
    return 0;
}

void lit_light(unsigned char x, unsigned char y) {
    unsigned char k;
    for (k = 0; k < lit_n; k++)
        if (x >= lit_x0[k] && x <= lit_x1[k] && y >= lit_y0[k] && y <= lit_y1[k]) lit_on[k] = 1;
}

unsigned char mon_at(unsigned char x, unsigned char y) {
    unsigned char i;
    for (i = 0; i < nmons; i++)
        if (mons[i].hp && mons[i].x == x && mons[i].y == y) return i + 1;
    return 0;
}

/* epochs: 4 turns each, 1-255 round and round */
unsigned char epoch(void) {
    return (unsigned char)((turn >> 2) % 255 + 1);
}

unsigned char epoch_age(unsigned char e) {
    unsigned char now = epoch();
    return now >= e ? now - e : (unsigned char)(255 - e + now);
}

/* every 64 turns: the memories older than the slowest fading keeps (225
 * epochs, 900 turns) are gone for good, before their epochs come round */
void memory_sweep(void) {
    unsigned int i;
    unsigned char e;
    for (i = 0; i < MAP_N; i++)
        if ((e = seen_get(i)) != 0 && epoch_age(e) >= 225) seen_set(i, 0);
}



/* each item's colour in lists and messages */
const unsigned char item_col[I_COUNT] = {
    C_RED, C_BLUE, C_BONE, C_BONE, C_BONE, C_GOLD, C_ORANGE, C_ORANGE, C_ORANGE, C_ORANGE, C_GREY, C_ORANGE,
    C_TEXT, C_GREY, C_ORANGE, C_BLOOD, C_CYAN, C_GREEN, C_GREY, C_PURPLE, C_GOLD,
    C_GREY, C_GREEN, C_TEXT, C_BONE, C_GOLD,
};

/* each thing's tile, lying on the floor */
const unsigned char item_tile[I_COUNT] = {
    T_POTION, T_DRAUGHT, T_SCROLL, T_SCROLL, T_SCROLL, T_KEY, T_RAGS, T_STICKS, T_FUR, T_CANDLE,
    T_FLINT, T_FIRESTICK, T_ARROWS, T_SHOT, T_OIL, T_FOOD, T_ANTIDOTE, T_FLASK1, T_FLASK2, T_FLASK3, T_FLASK4,
    T_RING, T_RING, T_RING, T_AMULET, T_AMULET,
};

unsigned char arrows_spent;
unsigned char ware[WARES], ware_left[WARES];

/* the things one carries, one and many; a flask by its colour until it
 * is known, then by what it does */
/* (the names are kept in attic RAM: NAMES_BANK; far_nth copies one out) */
static __bank(NAMES_BANK) const char item_names[] =
    "healing potion\0healing potions\0mana draught\0mana draughts\0"
    "scroll of fire\0scrolls of fire\0scroll of recall\0scrolls of recall\0"
    "scroll of far sight\0scrolls of far sight\0iron key\0iron keys\0"
    "twist of rags\0twists of rags\0bundle of sticks\0bundles of sticks\0"
    "scrap of fur\0scraps of fur\0tallow candle\0tallow candles\0"
    "flint\0flints\0firestick\0firesticks\0arrow\0arrows\0cartridge\0cartridges\0"
    "flask of oil\0flasks of oil\0strip of dried meat\0strips of dried meat\0"
    "vial of antidote\0vials of antidote";
static __bank(NAMES_BANK) const char flask_names[] =
    "cloudy green flask\0cloudy green flasks\0smoky grey flask\0smoky grey flasks\0"
    "violet flask\0violet flasks\0amber flask\0amber flasks\0"
    "flask of dark-sight\0flasks of dark-sight\0flask of clear memory\0flasks of clear memory\0"
    "flask of vigour\0flasks of vigour\0flask of poison\0flasks of poison";

static __bank(NAMES_BANK) const char charm_names[] =
    "iron ring\0iron rings\0jade ring\0jade rings\0silver ring\0silver rings\0"
    "bone amulet\0bone amulets\0amber amulet\0amber amulets\0"
    "charm of the steady flame\0charms of the steady flame\0charm of mending\0charms of mending\0"
    "charm of the cat's eye\0charms of the cat's eye\0charm against the dead\0charms against the dead\0"
    "charm against venom\0charms against venom";

const char *item_name(unsigned char i, unsigned char n) {
    if (i >= I_CHARM) {
        i -= I_CHARM;
        if (pl.cknown & (1 << i)) return qual_name(pl.cq[i], far_nth(charm_names, (CHARMS + pl.charm[i]) * 2 + (n != 1)));
        return far_nth(charm_names, i * 2 + (n != 1));
    }
    if (i >= I_FLASK) {
        i -= I_FLASK;
        if (pl.known & (1 << i)) i = FLASKS + pl.flask[i];
        return far_nth(flask_names, i * 2 + (n != 1));
    }
    return far_nth(item_names, i * 2 + (n != 1));
}

/* the short bow, the longbow, the flintlock pistol, the musket: their
 * blow and how far they reach */
const unsigned char r_dice[4] = {1, 1, 2, 3};
const unsigned char r_sides[4] = {6, 8, 6, 6};
const unsigned char r_plus[4] = {0, 2, 2, 4};
const unsigned char r_reach[4] = {6, 9, 5, 8};

static __bank(NAMES_BANK) const char gear_names[] =
    "short bow\0longbow\0flintlock pistol\0flintlock musket\0"
    "short sword\0long sword\0war axe\0rune blade\0dragonbone sword\0"
    "leather jerkin\0studded leather\0mail shirt\0plate\0rune plate";

const char *ranged_name(unsigned char gun, unsigned char w) {
    return far_nth(gear_names, gun * 2 + w - 1);
}

/* the weapons: dice, sides, plus */
static const unsigned char w_dice[WEAPONS] = {1, 1, 1, 1, 2};
static const unsigned char w_sides[WEAPONS] = {8, 8, 10, 10, 10};
static const unsigned char w_plus[WEAPONS] = {0, 2, 4, 6, 8};

const char *weapon_name(unsigned char w) {
    return far_nth(gear_names, 4 + w);
}

const char *armour_name(unsigned char a) {
    return far_nth(gear_names, 4 + WEAPONS + a);
}

/* (with pl.wq, pl.aq as they are: set them first) */
void weapon_take(unsigned char w) {
    unsigned char p = w_plus[w] + pl.wq;
    pl.weapon = w;
    pl.wdice = w_dice[w];
    pl.wsides = w_sides[w];
    pl.wplus = p ? p - 1 : 0;
}

void armour_take(unsigned char a) {
    pl.armour = a;
    pl.def = a + pl.aq;
}

const unsigned char qual_col[QUALITIES] = {C_DIM, C_TEXT, C_GREEN, C_CYAN, C_PURPLE};

const char *qual_name(unsigned char q, const char *n) {
    static char qbuf[40];
    qbuf[0] = 0;
    str_cat(qbuf, nth("Minor \0\0Great \0Superior \0Rare ", q));
    str_cat(qbuf, n);
    return qbuf;
}

unsigned char r_bonus(unsigned char gun) {
    unsigned char p = r_plus[gun ? pl.gun + 1 : pl.bow - 1] + (gun ? pl.gq : pl.bq);
    return p ? p - 1 : 0;
}

/* each level's name (resident: the title's overlay names it too) */
static __bank(NAMES_BANK) const char depth_names[] =
    "\0The Sunken Cellars\0The Weeping Halls\0The Bone Galleries\0The Web-Choked Deep\0The Lich's Sanctum";

const char *depth_name(unsigned char depth) {
    return far_nth(depth_names, depth);
}

const char *mon_name(unsigned char kind) {
    return far_nth(mon_names, kind);
}
