/* battle.c — the close-up fight, a turn at a time
 *
 * Each round the creature shows what it means to do (its intent: a blow,
 * a great blow, venom, silk, a spell...); the player answers: strike,
 * parry, cast, step in or back, shoot (from near or far: the bow or the
 * flintlock), or use something carried (a potion, a draught, a scroll of
 * fire). Then the creature acts, if it
 * still can. Reading the intent is the game: parry a great blow and strike
 * back; step back from a lunge; ward against what armour cannot stop.
 *
 * Range: close (blows land), near, far (fleeing is possible from far).
 *
 * The screen: the creature (rows 1-30, cols 12-67) in a frame; its name
 * and life under it, its intent; below, the actions, the log, the player.
 */
#pragma cc45 overlay(battle)
#pragma cc45 noescape_strings
#include <dma.h>
#include "hw.h"
#include "assets.h"
#include "video.h"
#include "rng.h"
#include "game.h"
#include "load.h"

#define LOG_COL   23
#define LOG_ROW   36
#define LOG_W     32
#define LOG_H     13
#define MENU_COL  2
#define MENU_ROW  37
#define STAT_COL  58

static struct mon *m;
/* what the creature is: its close-up's header (gen_art.py header()) */
static struct {
    unsigned char hp, dice, sides, plus, def, xp, flags;
    unsigned char acts[4], weights[4];
} mt;
/* its name, lines, lore, death: NUL after each (in the scratch, after the log's 12 x 53) */
#define WORDS_SIZE (SCENE_HDR_SIZE - 32)
#define words ((char *)scratch + 636)
static unsigned char dist, intent, last_intent, mhp_max;
static unsigned char frozen, staggered, roar, parrying;
static unsigned char poison, snared, ward, burning, numb, chilled;
static unsigned char dealt;                  /* what the creature has taken of the player's life (the lich's rite) */
static unsigned char sel, menu, casting;     /* menu: MENU_*; casting: a spell lands (no guard stops it) */
#define MENU_ACTS   0
#define MENU_SPELLS 1
#define MENU_ITEMS  2
#define MENU_SHOOT  3
static char buf[64];
static const char *mon_name(void);

/* ---------------------------------------------------------------- text */

static void the(const char *rest) {
    buf[0] = 0;
    str_cat(buf, "The ");
    str_cat(buf, mon_name());
    str_cat(buf, " ");
    str_cat(buf, rest);
}

static void say(const char *s, unsigned char c) {
    log_add(s, c);
}

static void said_num(const char *a, unsigned char n, const char *b, unsigned char c) {
    buf[0] = 0;
    str_cat(buf, a);
    str_num(buf, n);
    str_cat(buf, b);
    say(buf, c);
}

/* ---------------------------------------------------------------- effects */

/* the creature washed toward a colour, fading back over the frames */
static void flash(unsigned char r, unsigned char g, unsigned char b, unsigned char k) {
    v_wash_r = r;
    v_wash_g = g;
    v_wash_b = b;
    for (v_wash = k; v_wash; v_wash--) {
        video_frame();
        sfx_update();
    }
    video_pal_show();
}

static void pause(unsigned char frames) {
    while (frames--) {
        video_frame();
        sfx_update();
    }
}

/* ---------------------------------------------------------------- the screen */

static void draw_range(void) {
    unsigned char k;
    video_text(2, 8, "Range", C_GOLD);
    for (k = 0; k < 3; k++) {
        video_text(1, 10 + k * 2, k == dist ? ">" : " ", C_HILITE);
        video_text(3, 10 + k * 2, nth("close\0near\0far", k), k == dist ? C_HILITE : C_DARK);
    }
}

static void draw_foe(void) {
    unsigned char n = 0;
    while (mon_name()[n]) n++;
    video_blank(12, 32, 56, 1);
    buf[0] = 0;
    str_cat(buf, mon_name());
    buf[0] -= 0x20;                                    /* a capital */
    video_text(38 - n, 32, buf, C_BONE);
    ui_bar(40, 32, 20, m->hp, mhp_max, UI_BAR_G0);
    /* its state, on the right */
    video_blank(70, 8, 10, 8);
    video_text(71, 8, "Foe", C_GOLD);
    n = 10;
    if (frozen) { video_text(71, n, "frozen", C_CYAN); n += 2; }
    if (staggered) { video_text(71, n, "reeling", C_ORANGE); n += 2; }
    if (roar) { video_text(71, n, "enraged", C_RED); n += 2; }
    if (chilled) video_text(71, n, "chilled", C_CYAN);
}

static void draw_you(void) {
    unsigned char n = 42;
    ui_stats(STAT_COL, 37);
    video_blank(STAT_COL, 42, 21, 6);
    if (ward) { buf[0] = 0; str_cat(buf, "Warded ("); str_num(buf, ward); str_cat(buf, ")"); video_text(STAT_COL, n++, buf, C_BLUE); }
    if (poison) video_text(STAT_COL, n++, "Poisoned", C_GREEN);
    if (burning) video_text(STAT_COL, n++, "Burning", C_ORANGE);
    if (numb) video_text(STAT_COL, n++, "Numbed", C_PURPLE);
    if (snared) video_text(STAT_COL, n++, "Caught in silk", C_BONE);
    if (parrying) video_text(STAT_COL, n++, "On guard", C_TEXT);
}

/* what a creature means to do, as a phrase after its name (by A_*) */
__attribute__((noinline)) static const char *tele(unsigned char a) {
    return nth("\0\0gathers itself for a crushing blow!\0raises its guard.\0bares fangs glistening with venom.\0"
               "gathers a skein of silk.\0swells, ready to spit.\0draws itself together.\0fills its lungs to roar.\0"
               "reaches for your heart.\0breathes out a freezing mist.\0flickers, half in this world.\0"
               "gathers violet fire in its staff.\0whispers a word of withering.\0crouches to spring.\0closes in.\0"
               "rears, fire welling in its maw!\0reaches out with grave-cold claws.", a);
}

/* the creature's words, from its header */
static const char *mon_name(void) { return nth(words, 0); }
static const char *mon_tele(void) { return nth(words, 1); }
static const char *mon_hit(void) { return nth(words, 2); }
static const char *mon_lore(void) { return nth(words, 3); }

static void draw_intent(void) {
    unsigned char n = 0, c;
    if (frozen) the("is frozen in place.");
    else if (staggered) the("reels from your blow.");
    else if (intent == A_MELEE) {
        the(mon_tele());
        str_cat(buf, ".");
    } else the(tele(intent));
    while (buf[n]) n++;
    video_blank(1, 33, 78, 1);
    c = intent == A_HEAVY || intent == A_BOLT || intent == A_LUNGE || intent == A_BURN ? C_RED : intent == A_GUARD || intent == A_PHASE || intent == A_CLOSE ? C_DIM : C_ORANGE;
    if (frozen || staggered) c = C_CYAN;
    video_text(40 - n / 2, 33, buf, c);
}

/* the actions: what each is called and whether it can be done now; Cast
 * and Item open lists of their own (menu) */
#define NACT 7
#define ACT_CAST 2
#define ACT_ITEM 5
#define ACT_SHOOT 6
static const char act_key[NACT] = {'s', 'p', 'c', 'a', 'r', 'i', 't'};
static const char shoot_key[2] = {'b', 'g'};
static const char spell_key[4] = {'f', 'i', 'h', 'w'};
static const unsigned char spell_cost[4] = {3, 4, 5, 4};
static const unsigned char spell_sfx[4] = {SFX_FIRE, SFX_FROST, SFX_HEAL, SFX_WARD};
#define NITEM 4
static const char item_key[NITEM] = {'h', 'm', 'f', 'a'};
static const unsigned char item_of[NITEM] = {I_POTION, I_DRAUGHT, I_FIRE, I_ANTIDOTE};

static unsigned char menu_n(void) {
    return menu == MENU_SPELLS ? 4 : menu == MENU_ITEMS ? NITEM : menu == MENU_SHOOT ? 2 : NACT;
}

static char menu_key(unsigned char k) {
    return menu == MENU_SPELLS ? spell_key[k] : menu == MENU_ITEMS ? item_key[k] :
           menu == MENU_SHOOT ? shoot_key[k] : act_key[k];
}

/* the bow (0) or the flintlock (1): there, and something to shoot */
static unsigned char armed(unsigned char gun) {
    return gun ? pl.gun && pl.items[I_SHOT] : pl.bow && pl.items[I_ARROWS];
}

static unsigned char can(unsigned char a) {
    if (menu == MENU_SPELLS) return pl.mp >= spell_cost[a];
    if (menu == MENU_ITEMS) return pl.items[item_of[a]] > 0;
    if (menu == MENU_SHOOT) return armed(a);
    switch (a) {
    case 0: return dist == 0;
    case 2: return pl.mp >= 3;
    case 3: return dist > 0 && !snared;
    case 4: return !snared;
    case 5: return pl.items[I_POTION] || pl.items[I_DRAUGHT] || pl.items[I_FIRE] || pl.items[I_ANTIDOTE];
    case 6: return dist > 0 && (armed(0) || armed(1));     /* (close, it is on you) */
    }
    return 1;
}

static void draw_menu(void) {
    unsigned char k, n = menu_n(), c, step = n > 6 ? 1 : 2;
    video_blank(MENU_COL, MENU_ROW - 1, 18, 13);
    video_text(MENU_COL, MENU_ROW - 1, nth("Actions\0Spells\0Items\0Shoot", menu), C_GOLD);
    for (k = 0; k < n; k++) {
        c = !can(k) ? C_DARK : k == sel ? C_HILITE : C_TEXT;
        buf[0] = k == sel ? '>' : ' ';
        buf[1] = (char)(menu_key(k) - 0x20);
        buf[2] = ' ';
        buf[3] = 0;
        if (menu == MENU_SPELLS) str_cat(buf, nth("Firebolt  3\0Frost     4\0Heal      5\0Ward      4", k));
        else if (menu == MENU_ITEMS) {
            str_cat(buf, nth("Potion     \0Draught    \0Fire scroll\0Antidote   ", k));
            str_cat(buf, " ");
            str_num(buf, pl.items[item_of[k]]);
        } else if (menu == MENU_SHOOT) {
            str_cat(buf, k ? "Flintlock   " : "Bow         ");
            if ((k ? pl.gun : pl.bow) != 0) str_num(buf, pl.items[k ? I_SHOT : I_ARROWS]);
        } else str_cat(buf, k == 4 && dist == 2 ? "Flee" : nth("Strike\0Parry\0Cast\0Advance\0Retreat\0Item\0Shoot", k));
        video_text(MENU_COL, MENU_ROW + 1 + k * step, buf, c);
    }
    if (menu) video_text(MENU_COL, MENU_ROW + 10, "  Esc: back", C_DARK);
}

static void draw_all(void) {
    unsigned int c;
    unsigned char r;
    video_clear();
    ui_frame(11, 0, 58, 32);
    ui_frame(0, 35, 80, 15);
    c = UI_F_V;
    for (r = 36; r < 49; r++) {
        video_cells(21, r, &c, 1);
        video_cells(56, r, &c, 1);
    }
    c = UI_F_T;
    video_cells(21, 35, &c, 1);
    video_cells(56, 35, &c, 1);
    c = UI_F_B;
    video_cells(21, 49, &c, 1);
    video_cells(56, 49, &c, 1);
    scene_show(0);
    draw_range();
    draw_foe();
    draw_you();
    draw_menu();
}

/* ---------------------------------------------------------------- the creature */

static unsigned char needs_close(unsigned char a) {
    return a == A_MELEE || a == A_HEAVY || a == A_POISON || a == A_BURN || a == A_NUMB;
}

static unsigned char choose(void) {
    unsigned char k, t = 0, r, a = mt.acts[0];
    for (k = 0; k < 4; k++) t += mt.weights[k];
    r = rng_range(t);
    for (k = 0; k < 4; k++) {
        if (r < mt.weights[k]) {
            a = mt.acts[k];
            break;
        }
        r -= mt.weights[k];
    }
    /* not the same trick twice running, nor a pointless one */
    if ((a == A_GUARD || a == A_PHASE || a == A_ROAR || a == A_WEB || a == A_REGEN) && a == last_intent) a = mt.acts[0];
    if (a == A_REGEN && m->hp > mhp_max - 4) a = mt.acts[0];
    if (a == A_WEB && snared) a = mt.acts[0];
    /* out of reach: come closer (a lunge reaches from near) */
    if (needs_close(a) && dist > 0) a = A_CLOSE;
    if ((a == A_LUNGE || a == A_DRAIN) && dist == 2) a = A_CLOSE;
    return a;
}

/* a blow lands on the player: armour, the ward; 1 if the player still stands */
static unsigned char hurt(unsigned char dmg, unsigned char armour) {
    if (armour) dmg = dmg > pl.def ? dmg - pl.def : 1;
    if (ward) {
        if (ward >= dmg) {
            ward -= dmg;
            said_num("Your ward absorbs ", dmg, ".", C_BLUE);
            dmg = 0;
        } else {
            dmg -= ward;
            ward = 0;
            say("Your ward shatters!", C_BLUE);
        }
    }
    if (!dmg) {
        sfx(SFX_CLANG);
        flash(120, 160, 255, 8);
        return 1;
    }
    said_num("You take ", dmg, " damage.", C_RED);
    sfx(SFX_HURT);
    REG(0xD020) = C_BLOOD;
    video_shake(10);
    REG(0xD020) = 0;
    dmg = pl.hp > dmg ? dmg : pl.hp;
    pl.hp -= dmg;
    dealt = dealt + dmg > 255 ? 255 : dealt + dmg;
    draw_you();
    return pl.hp > 0;
}

static unsigned char roll(void) {
    unsigned char d = rng_dice(mt.dice, mt.sides) + mt.plus;
    if (roar) {
        d += d / 2;
        roar = 0;
    }
    if (chilled) d -= d / 3;              /* (frost: stiff and slow) */
    return d;
}

/* the creature lunges in its attacking pose, with a sound */
static void lunge(unsigned char s) {
    sfx(s);
    scene_show(1);
    pause(14);
}

/* 1 if the player still stands */
static unsigned char creature_acts(void) {
    unsigned char d, h, a = intent;
    if (frozen) {
        frozen = 0;
        the("is frozen and cannot act.");
        say(buf, C_CYAN);
        return 1;
    }
    if (staggered) {
        staggered = 0;
        the("reels and cannot act.");
        say(buf, C_DIM);
        return 1;
    }
    if (a == A_LUNGE) {
        if (dist == 2) {
            the("springs - and falls short.");
            say(buf, C_DIM);
            return 1;
        }
        if (dist == 1) {
            dist = 0;
            draw_range();
        }
        a = mt.flags & MF_VENOM ? A_POISON : mt.flags & MF_FIERY ? A_BURN : A_MELEE;
    }
    switch (a) {
    case A_MELEE:
    case A_POISON:
    case A_BURN:
    case A_NUMB:
    case A_HEAVY:
        if (dist) {
            the("strikes at empty air.");
            say(buf, C_DIM);
            return 1;
        }
        lunge(a == A_HEAVY ? SFX_HEAVY : SFX_SWING);
        if (parrying) {
            sfx(SFX_CLANG);
            flash(255, 255, 255, 10);
            scene_show(0);
            d = rng_dice(pl.wdice, pl.wsides) + pl.wplus;
            if (a == A_HEAVY) {
                staggered = 1;
                say("You turn the great blow and strike back!", C_HILITE);
            } else {
                d = d / 2 + 1;
                say("You parry, and riposte!", C_HILITE);
            }
            said_num("The riposte deals ", d, ".", C_TEXT);
            m->hp = m->hp > d ? m->hp - d : 0;
            draw_foe();
            return 1;
        }
        d = roll();
        if (a == A_HEAVY) {
            d *= 2;
            the("lands a crushing blow!");
        } else {
            the(mon_hit());
            str_cat(buf, ".");
        }
        say(buf, C_ORANGE);
        scene_show(0);
        h = pl.hp;
        if (!hurt(d, 1)) return 0;
        if (pl.hp == h) return 1;            /* (the ward took it all) */
        if (a == A_POISON) {
            if (WEARS(CH_SNAKE)) say("The venom finds no hold in you.", C_DIM);
            else {
                poison = 3;
                sfx(SFX_POISON);
                say("Venom burns in your veins.", C_GREEN);
            }
        } else if (a == A_BURN) {
            burning = 2;                 /* (3 rounds was too much: tools/balance.py) */
            sfx(SFX_FIRE);
            say("Fire catches on your clothes!", C_ORANGE);
        } else if (a == A_NUMB && rng_range(2) == 0) {
            numb = 1;
            sfx(SFX_CURSE);
            say("A deathly numbness creeps up your arm.", C_PURPLE);
        }
        draw_you();
        return 1;
    case A_SPIT:
    case A_CHILL:
    case A_BOLT:
        lunge(SFX_GROWL);
        d = roll();
        if (a == A_BOLT) {
            h = rng_dice(2, 6);                  /* (2d6 - 2, 1 at the least: tools/balance.py) */
            d += h > 3 ? h - 2 : 1;
            sfx(SFX_SPELL);
            flash(200, 120, 255, 14);
            say("Violet fire sears you!", C_PURPLE);
        } else if (a == A_CHILL) {
            sfx(SFX_FROST);
            flash(160, 200, 255, 12);
            say("Cold reaches into your bones.", C_CYAN);
            if (pl.lit && rng_range(3) == 0) {
                pl.lit = 0;              /* (back on the map, it is dark) */
                say("The cold snuffs out your torch!", C_CYAN);
            }
        } else {
            say("Acid spatters you!", C_GREEN);
        }
        if (parrying) {
            d = d / 2 + 1;
            say("You catch some of it on your guard.", C_DIM);
        }
        scene_show(0);
        return hurt(d, a == A_SPIT);
    case A_WEB:
        lunge(SFX_GROWL);
        scene_show(0);
        snared = 2;
        say("Sticky silk binds your legs!", C_BONE);
        draw_you();
        return 1;
    case A_DRAIN:
        if (dist == 2) return 1;
        lunge(SFX_GROWL);
        d = roll();
        if (parrying) d = d / 2 + 1;
        say("It drinks your warmth!", C_PURPLE);
        scene_show(0);
        m->hp = m->hp + d > mhp_max ? mhp_max : m->hp + d;
        draw_foe();
        return hurt(d, 0);
    case A_REGEN:
        d = 6 + rng_range(5);
        m->hp = m->hp + d > mhp_max ? mhp_max : m->hp + d;
        the("knits itself together.");
        say(buf, C_GREEN);
        flash(120, 255, 120, 10);
        draw_foe();
        return 1;
    case A_ROAR:
        lunge(SFX_GROWL);
        roar = 1;
        the("roars! Its next blow will be terrible.");
        say(buf, C_RED);
        video_shake(12);
        scene_show(0);
        draw_foe();
        return 1;
    case A_CURSE:
        sfx(SFX_CURSE);
        flash(80, 0, 120, 14);
        d = 3 + rng_range(4);
        pl.mp = pl.mp > d ? pl.mp - d : 0;
        say("Your mana withers.", C_PURPLE);
        return hurt(1 + rng_range(4), 0);
    case A_CLOSE:
        if (dist) dist--;
        the("closes in.");
        say(buf, C_DIM);
        draw_range();
        return 1;
    }
    return 1;
}

/* ---------------------------------------------------------------- the player */

/* fire's damage, as the creature takes it */
static unsigned char fire(unsigned char d) {
    if (mt.flags & MF_FIREWEAK) {
        d += d / 2;
        say("It burns fiercely!", C_ORANGE);
    }
    if (mt.flags & MF_FIERY) {
        d = d / 2 + 1;
        say("It drinks in the fire.", C_DIM);
    }
    return d;
}

static void wound(unsigned char d) {
    if (intent == A_PHASE && !casting) {
        say("Your blade passes through it!", C_DIM);
        return;
    }
    if (intent == A_GUARD && !casting) {
        d = d / 2 + 1;
        say("It takes the blow on its guard.", C_DIM);
    }
    said_num("You deal ", d, " damage.", C_TEXT);
    m->hp = m->hp > d ? m->hp - d : 0;
    draw_foe();
}

/* the chosen action; FLED if the player got away, else 0 */
static unsigned char player_acts(unsigned char a) {
    unsigned char d;
    parrying = 0;
    if (menu == MENU_SHOOT) {
        menu = MENU_ACTS;
        pl.items[a ? I_SHOT : I_ARROWS]--;
        if (!a) arrows_spent++;
        if (a && rng_range(6) == 0) {
            say("The powder flashes in the pan, and no more.", C_DIM);
            sfx(SFX_CLANG);
            draw_you();
            return 0;
        }
        sfx(a ? SFX_HEAVY : SFX_SWING);
        if (a) video_shake(4);
        if (rng_range(100) >= (dist == 1 ? 85 : 70) + pl.aim * 5) {
            say(a ? "The shot goes wide." : "The arrow goes wide.", C_DIM);
        } else {
            d = a ? pl.gun + 1 : pl.bow - 1;          /* (game.h: r = gun * 2 + w - 1) */
            d = rng_dice(r_dice[d], r_sides[d]) + r_bonus(a) + (pl.level - 1) / 2 + pl.aim;
            d = d > mt.def ? d - mt.def : 1;
            sfx(SFX_HIT);
            say(a ? "The ball strikes home!" : "Your arrow strikes!", C_TEXT);
            flash(255, 255, 255, 8);
            wound(d);
        }
        draw_you();
        return 0;
    }
    if (menu == MENU_ITEMS) {
        menu = MENU_ACTS;
        pl.items[item_of[a]]--;
        switch (a) {
        case 0:
            d = 15 + pl.level * 2;
            pl.hp = pl.hp + d > pl.maxhp ? pl.maxhp : pl.hp + d;
            said_num("You drink. +", d, " life.", C_GREEN);
            sfx(SFX_DRINK);
            break;
        case 1:
            d = 10 + pl.level;
            pl.mp = pl.mp + d > pl.maxmp ? pl.maxmp : pl.mp + d;
            said_num("The draught hums in you. +", d, " mana.", C_BLUE);
            sfx(SFX_DRINK);
            break;
        case 2:
            sfx(SFX_FIRE);
            say("The scroll flares, and fire pours from it!", C_ORANGE);
            flash(255, 120, 20, 16);
            casting = 1;               /* (no guard stops it) */
            wound(fire(rng_dice(3, 6) + pl.depth * 3));
            casting = 0;
            break;
        case 3:
            poison = 0;
            sfx(SFX_DRINK);
            say("Bitter as gall. The venom is gone.", C_CYAN);
            break;
        }
        draw_you();
        return 0;
    }
    if (menu == MENU_SPELLS) {
        menu = MENU_ACTS;
        pl.mp -= spell_cost[a];
        sfx(spell_sfx[a]);
        switch (a) {
        case 0:
            say("You hurl a bolt of fire!", C_ORANGE);
            flash(255, 140, 30, 16);
            casting = 1;               /* (spells are not stopped by a guard) */
            wound(fire(rng_dice(2, 6) + pl.level * 2));
            casting = 0;
            break;
        case 1:
            say("Shards of ice strike it!", C_CYAN);
            flash(150, 210, 255, 16);
            casting = 1;
            wound(rng_dice(1, 6) + pl.level);
            casting = 0;
            if (!(mt.flags & MF_COLDPROOF) || rng_range(3) == 0) {
                frozen = 1;
                chilled = 4;           /* (the round frozen, then 3 more) */
                the("is frozen fast!");
                say(buf, C_CYAN);
            } else {
                the("shrugs off the cold.");
                say(buf, C_DIM);
            }
            draw_foe();
            break;
        case 2:
            d = rng_dice(2, 6) + pl.level * 2;
            pl.hp = pl.hp + d > pl.maxhp ? pl.maxhp : pl.hp + d;
            said_num("Warm light knits you: +", d, " life.", C_GREEN);
            break;
        case 3:
            ward = 8 + pl.level * 2;
            said_num("A shimmering ward surrounds you (", ward, ").", C_BLUE);
            break;
        }
        draw_you();
        return 0;
    }
    switch (a) {
    case 0:
        sfx(SFX_SWING);
        pause(6);
        sfx(SFX_HIT);
        d = rng_dice(pl.wdice, pl.wsides) + pl.wplus + pl.level - 1;
        d = d > mt.def ? d - mt.def : 1;
        if (rng_range(pl.wq == Q_RARE ? 5 : 10) == 0) {
            d *= 2;
            say("A telling blow!", C_HILITE);
        } else {
            say("You strike.", C_TEXT);
        }
        flash(255, 255, 255, 10);
        video_shake(4);
        wound(d);
        break;
    case 1:
        parrying = 1;
        say("You raise your guard.", C_TEXT);
        break;
    case 3:
        dist--;
        sfx(SFX_STEP);
        say(dist ? "You step closer." : "You close with it.", C_TEXT);
        break;
    case 4:
        sfx(SFX_STEP);
        if (burning) {
            burning = 0;
            say("You roll, and beat out the flames.", C_TEXT);
        }
        if (dist == 2) {
            d = mt.flags & MF_HARDFLEE ? 35 : 65;
            if (rng_range(100) < d) {
                say("You flee into the dark!", C_HILITE);
                return FLED;
            }
            say("You cannot get away!", C_RED);
            break;
        }
        dist++;
        say("You fall back.", C_TEXT);
        break;
    }
    draw_range();
    draw_you();
    return 0;
}

/* the player picks an action (a key, or the joystick through the menu) */
static unsigned char pick(void) {
    unsigned char k, i, n;
    for (;;) {
        n = menu_n();
        draw_menu();
        k = key_wait();
        if (k == K_UP) { sel = sel ? sel - 1 : n - 1; continue; }
        if (k == K_DOWN) { sel = sel + 1 < n ? sel + 1 : 0; continue; }
        if (k == K_STOP || (menu && k == K_LEFT)) {
            if (menu) {
                sel = menu == MENU_SPELLS ? ACT_CAST : menu == MENU_ITEMS ? ACT_ITEM : ACT_SHOOT;
                menu = MENU_ACTS;
            }
            continue;
        }
        i = 0xFF;
        if (k == K_FIRE || (k == K_RIGHT && !menu && (sel == ACT_CAST || sel >= ACT_ITEM))) i = sel;
        for (n = 0; n < menu_n(); n++)
            if (k == menu_key(n)) i = n;
        if (i == 0xFF) continue;
        if (!can(i)) {
            sfx(SFX_DOOR);
            continue;
        }
        if (!menu && (i == ACT_CAST || i >= ACT_ITEM)) {
            menu = i == ACT_CAST ? MENU_SPELLS : i == ACT_ITEM ? MENU_ITEMS : MENU_SHOOT;
            sel = 0;
            continue;
        }
        return i;
    }
}

unsigned char battle(unsigned char mi, unsigned char ambush) {
    unsigned char r;
    m = &mons[mi];
    scene_load(mon_scene[m->kind]);
    if (mon_hdr_file[m->kind]) {
        /* another's close-up: its own words (and colours, when it has
         * them); the next close-up comes from the cache again, unchanged */
        load_file(mon_hdr_file[m->kind], 5);
        video_pal_load(SCENE_LIN, PAL_SCENE, 255 - PAL_SCENE);
        scene_forget();
    }
    dma_copy_far((unsigned long)(unsigned int)&mt, SCENE_HDR + 3, sizeof mt);
    dma_copy_far((unsigned long)(unsigned int)words, SCENE_HDR + 32, WORDS_SIZE);
    mhp_max = mt.hp;
    if (m->hp == FRESH) m->hp = mt.hp;
    dist = ambush ? 0 : 1;
    frozen = staggered = roar = parrying = snared = ward = burning = numb = chilled = dealt = 0;
    poison = pl.venom ? 3 : 0;                /* (venom from before goes on burning) */
    sel = menu = casting = 0;
    intent = last_intent = A_NONE;

    video_fade(0);
    v_flicker = 0;
    v_fire = 0;
    log_init(LOG_COL, LOG_ROW, LOG_W, LOG_H);
    draw_all();
    say(mon_lore(), C_BONE);
    if (ambush) say("It is upon you!", C_RED);
    if (WEARS(CH_BONE) && (mt.flags & MF_UNDEAD)) {
        ward = 7 + pl.level + pl.cq[pl.worn - 1] * 3;
        say("Your charm grows cold: a ward against the dead.", C_BLUE);
        draw_you();
    }
    video_fade(16);
    sfx(SFX_GROWL);

    for (;;) {
        intent = choose();
        draw_intent();
        if (!can(sel)) sel = dist ? 3 : 0;
        draw_foe();
        if (numb) {
            numb = 0;
            say("Your limbs will not answer you!", C_PURPLE);
            pause(30);
            r = 0;
        } else r = player_acts(pick());
        if (r == FLED) {
            m->stun = 3;
            if (chilled) {
                m->stun = 8;
                the("is too stiff with cold to follow.");
                say(buf, C_CYAN);
            }
            pl.venom = poison * 8;
            pause(30);
            video_fade(0);
            return FLED;
        }
        if (!m->hp) break;
        pause(10);
        if (!creature_acts()) {
            pl.cause = CAUSE_SLAIN + m->kind;
            say("You fall...", C_BLOOD);
            sfx(SFX_DEFEAT);
            pause(60);
            video_fade(0);
            return DIED;
        }
        last_intent = intent;
        if (!m->hp) break;
        if ((mt.flags & MF_BOSS) && !pl.rite && m->hp <= mhp_max / 2) {
            /* once in a descent, at half its life: the lich calls on its
             * crown, mends half of what it has dealt the player less 3 (1
             * at the least), and its next blow will be terrible */
            pl.rite = 1;
            sfx(SFX_CURSE);
            say("The lich's crown blazes violet. The dark pours into it!", C_PURPLE);
            flash(200, 60, 255, 24);
            m->hp += dealt > 7 ? dealt / 2 - 3 : 1;
            roar = 1;
            draw_foe();
        }
        if (poison) {
            poison--;
            sfx(SFX_POISON);
            say("The venom burns.", C_GREEN);
            if (!hurt(2, 0)) {
                pl.cause = CAUSE_SLAIN + m->kind;
                video_fade(0);
                return DIED;
            }
        }
        if (burning) {
            burning--;
            sfx(SFX_FIRE);
            say("The flames sear you.", C_ORANGE);
            if (!hurt(3, 0)) {
                pl.cause = CAUSE_SLAIN + m->kind;
                video_fade(0);
                return DIED;
            }
        }
        if (chilled && !--chilled) {
            the("shakes off the cold.");
            say(buf, C_DIM);
        }
        if (snared) snared--;
        parrying = 0;
        draw_you();
    }
    /* the creature falls: it fades into the dark */
    sfx(SFX_VICTORY);
    the(nth(words, 4));
    say(buf, C_HILITE);
    v_wash_r = v_wash_g = v_wash_b = 0;
    for (v_wash = 0; v_wash < 16; v_wash++) pause(3);
    said_num("You gain ", mt.xp, " experience.", C_GOLD);
    pl.kills++;
    if (gain_xp(mt.xp)) {
        say("You have grown stronger!", C_GOLD);
        sfx(SFX_LEVEL);
        draw_you();
    }
    m->hp = 0;
    pl.venom = poison * 8;
    pause(20);
    say("Press a key.", C_DARK);
    key_wait();
    video_fade(0);
    v_wash = 0;
    return WON;
}
