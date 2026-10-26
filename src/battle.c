/* battle.c — the close-up fight, a turn at a time
 *
 * Each round the creature shows what it means to do (its intent: a blow,
 * a great blow, venom, silk, a spell...); the player answers: strike,
 * parry, cast, step in or back, or drink. Then the creature acts, if it
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
#include "hw.h"
#include "assets.h"
#include "video.h"
#include "rng.h"
#include "game.h"

#define LOG_COL   23
#define LOG_ROW   36
#define LOG_W     32
#define LOG_H     13
#define MENU_COL  2
#define MENU_ROW  37
#define STAT_COL  58

static struct mon *m;
static const struct montype *mt;
static unsigned char dist, intent, last_intent, mhp_max;
static unsigned char frozen, staggered, roar, parrying;
static unsigned char poison, snared, ward;
static unsigned char sel, casting;
static char buf[64];
__attribute__((noinline)) static const char *mon_name(void);

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
}

static void draw_you(void) {
    unsigned char n = 42;
    ui_stats(STAT_COL, 37);
    video_blank(STAT_COL, 42, 21, 6);
    if (ward) { buf[0] = 0; str_cat(buf, "Warded ("); str_num(buf, ward); str_cat(buf, ")"); video_text(STAT_COL, n++, buf, C_BLUE); }
    if (poison) video_text(STAT_COL, n++, "Poisoned", C_GREEN);
    if (snared) video_text(STAT_COL, n++, "Caught in silk", C_BONE);
    if (parrying) video_text(STAT_COL, n++, "On guard", C_TEXT);
}

/* what a creature means to do, as a phrase after its name (by A_*) */
__attribute__((noinline)) static const char *tele(unsigned char a) {
    return nth("\0\0gathers itself for a crushing blow!\0raises its guard.\0bares fangs glistening with venom.\0"
               "gathers a skein of silk.\0swells, ready to spit.\0draws itself together.\0fills its lungs to roar.\0"
               "reaches for your heart.\0breathes out a freezing mist.\0flickers, half in this world.\0"
               "gathers violet fire in its staff.\0whispers a word of withering.\0crouches to spring.\0closes in.", a);
}

/* the creatures' words (by M_*) */
__attribute__((noinline)) static const char *mon_name(void) {
    return nth("giant rat\0skeleton\0cave spider\0ooze\0orc brute\0wraith\0lich", m->kind);
}

__attribute__((noinline)) static const char *mon_tele(void) {
    return nth("bares its yellow teeth\0swings its notched blade\0rears, fangs dripping\0surges up to engulf you\0"
               "swings its spiked club\0reaches for your heart\0raises its staff to strike", m->kind);
}

__attribute__((noinline)) static const char *mon_hit(void) {
    return nth("bites you\0cuts you\0bites you\0burns you\0clubs you\0chills you\0smites you", m->kind);
}

__attribute__((noinline)) static const char *mon_lore(void) {
    return nth("A rat the size of a dog, slick with sewer filth.\0"
               "Old bones bound by older hate. Its eyes are embers.\0"
               "Eight green eyes catch your torchlight.\0"
               "A quivering mass. Something it ate stares out of it.\0"
               "It grins with too many tusks and hefts its club.\0"
               "Cold pours from the hood. Your torch gutters.\0"
               "The master of the deep. It has waited a long time.", m->kind);
}

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
    c = intent == A_HEAVY || intent == A_BOLT || intent == A_LUNGE ? C_RED : intent == A_GUARD || intent == A_PHASE || intent == A_CLOSE ? C_DIM : C_ORANGE;
    if (frozen || staggered) c = C_CYAN;
    video_text(40 - n / 2, 33, buf, c);
}

/* the actions: what each is called and whether it can be done now */
#define NACT 6
static const char act_key[NACT] = {'s', 'p', 'c', 'a', 'r', 'q'};
static const char spell_key[4] = {'f', 'i', 'h', 'w'};
static const unsigned char spell_cost[4] = {3, 4, 5, 4};
static const unsigned char spell_sfx[4] = {SFX_FIRE, SFX_FROST, SFX_HEAL, SFX_WARD};

static unsigned char can(unsigned char a) {
    if (casting) return pl.mp >= spell_cost[a];
    switch (a) {
    case 0: return dist == 0;
    case 2: return pl.mp >= 3;
    case 3: return dist > 0 && !snared;
    case 4: return !snared;
    case 5: return pl.potions > 0;
    }
    return 1;
}

static void draw_menu(void) {
    unsigned char k, n = casting ? 4 : NACT, c;
    video_blank(MENU_COL, MENU_ROW - 1, 18, 13);
    video_text(MENU_COL, MENU_ROW - 1, casting ? "Spells" : "Actions", C_GOLD);
    for (k = 0; k < n; k++) {
        c = !can(k) ? C_DARK : k == sel ? C_HILITE : C_TEXT;
        buf[0] = k == sel ? '>' : ' ';
        buf[1] = (char)((casting ? spell_key[k] : act_key[k]) - 0x20);
        buf[2] = ' ';
        buf[3] = 0;
        str_cat(buf, casting ? nth("Firebolt  3\0Frost     4\0Heal      5\0Ward      4", k)
                             : k == 4 && dist == 2 ? "Flee" : nth("Strike\0Parry\0Cast\0Advance\0Retreat\0Potion", k));
        video_text(MENU_COL, MENU_ROW + 1 + k * 2, buf, c);
    }
    if (casting) video_text(MENU_COL, MENU_ROW + 10, "  Esc: back", C_DARK);
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
    return a == A_MELEE || a == A_HEAVY || a == A_POISON;
}

static unsigned char choose(void) {
    unsigned char k, t = 0, r, a = mt->acts[0];
    for (k = 0; k < 4; k++) t += mt->weights[k];
    r = rng_range(t);
    for (k = 0; k < 4; k++) {
        if (r < mt->weights[k]) {
            a = mt->acts[k];
            break;
        }
        r -= mt->weights[k];
    }
    /* not the same trick twice running, nor a pointless one */
    if ((a == A_GUARD || a == A_PHASE || a == A_ROAR || a == A_WEB || a == A_REGEN) && a == last_intent) a = mt->acts[0];
    if (a == A_REGEN && m->hp > mhp_max - 4) a = mt->acts[0];
    if (a == A_WEB && snared) a = mt->acts[0];
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
    pl.hp = pl.hp > dmg ? pl.hp - dmg : 0;
    draw_you();
    return pl.hp > 0;
}

static unsigned char roll(void) {
    unsigned char d = rng_dice(mt->dice, mt->sides) + mt->plus;
    if (roar) {
        d += d / 2;
        roar = 0;
    }
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
    unsigned char d, a = intent;
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
        a = m->kind == M_SPIDER ? A_POISON : A_MELEE;
    }
    switch (a) {
    case A_MELEE:
    case A_POISON:
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
        }
        say(buf, C_ORANGE);
        scene_show(0);
        if (!hurt(d, 1)) return 0;
        if (a == A_POISON) {
            poison = 3;
            sfx(SFX_POISON);
            say("Venom burns in your veins.", C_GREEN);
        }
        return 1;
    case A_SPIT:
    case A_CHILL:
    case A_BOLT:
        lunge(SFX_GROWL);
        d = roll();
        if (a == A_BOLT) {
            d += rng_dice(2, 6);
            sfx(SFX_SPELL);
            flash(200, 120, 255, 14);
            say("Violet fire sears you!", C_PURPLE);
        } else if (a == A_CHILL) {
            sfx(SFX_FROST);
            flash(160, 200, 255, 12);
            say("Cold reaches into your bones.", C_CYAN);
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
    if (casting) {
        casting = 0;
        pl.mp -= spell_cost[a];
        sfx(spell_sfx[a]);
        switch (a) {
        case 0:
            say("You hurl a bolt of fire!", C_ORANGE);
            flash(255, 140, 30, 16);
            d = rng_dice(2, 6) + pl.level * 2;
            if (m->kind == M_SLIME || m->kind == M_SPIDER) {
                d += d / 2;
                say("It burns fiercely!", C_ORANGE);
            }
            casting = 1;               /* (spells are not stopped by a guard) */
            wound(d);
            casting = 0;
            break;
        case 1:
            say("Shards of ice strike it!", C_CYAN);
            flash(150, 210, 255, 16);
            casting = 1;
            wound(rng_dice(1, 6) + pl.level);
            casting = 0;
            if (m->kind != M_LICH || rng_range(3) == 0) {
                frozen = 1;
                the("is frozen fast!");
                say(buf, C_CYAN);
            } else {
                say("The lich shrugs off the cold.", C_DIM);
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
        d = d > mt->def ? d - mt->def : 1;
        if (rng_range(10) == 0) {
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
        if (dist == 2) {
            d = m->kind >= M_WRAITH ? 35 : 65;
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
    case 5:
        pl.potions--;
        d = 15 + pl.level * 2;
        pl.hp = pl.hp + d > pl.maxhp ? pl.maxhp : pl.hp + d;
        said_num("You drink. +", d, " life.", C_GREEN);
        sfx(SFX_DRINK);
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
        n = casting ? 4 : NACT;
        draw_menu();
        k = key_wait();
        if (k == K_UP) { sel = sel ? sel - 1 : n - 1; continue; }
        if (k == K_DOWN) { sel = sel + 1 < n ? sel + 1 : 0; continue; }
        if (k == K_STOP || (casting && k == K_LEFT)) {
            if (casting) { casting = 0; sel = 2; }
            continue;
        }
        i = 0xFF;
        if (k == K_FIRE || (k == K_RIGHT && !casting && sel == 2)) i = sel;
        for (n = 0; n < (casting ? 4 : NACT); n++)
            if (k == (casting ? spell_key[n] : act_key[n])) i = n;
        if (i == 0xFF) continue;
        if (!can(i)) {
            sfx(SFX_DOOR);
            continue;
        }
        if (!casting && i == 2) {
            casting = 1;
            sel = 0;
            continue;
        }
        return i;
    }
}

unsigned char battle(unsigned char mi, unsigned char ambush) {
    unsigned char a, r;
    m = &mons[mi];
    mt = &montypes[m->kind];
    mhp_max = mt->hp;
    dist = ambush ? 0 : 1;
    frozen = staggered = roar = parrying = poison = snared = ward = 0;
    sel = casting = 0;
    intent = last_intent = A_NONE;

    video_fade(0);
    v_flicker = 0;
    v_fire = 0;
    scene_load(mt->scene);
    log_init(LOG_COL, LOG_ROW, LOG_W, LOG_H);
    draw_all();
    say(mon_lore(), C_BONE);
    if (ambush) say("It is upon you!", C_RED);
    video_fade(16);
    sfx(SFX_GROWL);

    for (;;) {
        intent = choose();
        draw_intent();
        if (!can(sel)) sel = dist ? 3 : 0;
        draw_foe();
        a = pick();
        r = player_acts(a);
        if (r == FLED) {
            pause(30);
            video_fade(0);
            return FLED;
        }
        if (!m->hp) break;
        pause(10);
        if (!creature_acts()) {
            say("You fall...", C_BLOOD);
            sfx(SFX_DEFEAT);
            pause(60);
            video_fade(0);
            return DIED;
        }
        last_intent = intent;
        if (!m->hp) break;
        if (poison) {
            poison--;
            sfx(SFX_POISON);
            say("The venom burns.", C_GREEN);
            if (!hurt(2, 0)) {
                video_fade(0);
                return DIED;
            }
        }
        if (snared) snared--;
        parrying = 0;
        draw_you();
    }
    /* the creature falls: it fades into the dark */
    sfx(SFX_VICTORY);
    the(m->kind == M_SLIME ? "dissolves." : m->kind == M_WRAITH ? "comes apart in the wind." : "falls, and moves no more.");
    say(buf, C_HILITE);
    v_wash_r = v_wash_g = v_wash_b = 0;
    for (v_wash = 0; v_wash < 16; v_wash++) pause(3);
    said_num("You gain ", mt->xp, " experience.", C_GOLD);
    if (gain_xp(mt->xp)) {
        say("You have grown stronger!", C_GOLD);
        sfx(SFX_LEVEL);
        draw_you();
    }
    m->hp = 0;
    pause(20);
    say("Press a key.", C_DARK);
    key_wait();
    video_fade(0);
    v_wash = 0;
    return WON;
}
