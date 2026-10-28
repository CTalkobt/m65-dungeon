"""balance.py — the close-up fight (src/battle.c) played many times over, to
see how hard each creature is at each depth for each calling.

    python3 tools/balance.py [fights] [lean] [descent]

The fight's rules are battle.c's, ported; the player plays sensibly (see
player_turn). What a player has at each depth is a guess, in PROGRESS: the
level reached, the weapon and armour found, potions in hand. Two tables:
each creature fought once from full life (won %, life lost, potions
drunk), and a level's worth of fights in a row, wounds and potions
carried from one to the next (the chance to come through the level).
Change the rules here when battle.c changes.
"""
import random
import sys

sys.path.insert(0, __file__.rsplit("/", 1)[0])
import monsters  # noqa: E402

A = {name: k for k, name in enumerate(monsters.ACTIONS)}
NEEDS_CLOSE = {A["MELEE"], A["HEAVY"], A["POISON"], A["BURN"], A["NUMB"]}
NO_REPEAT = {A["GUARD"], A["PHASE"], A["ROAR"], A["WEB"], A["REGEN"]}

# the weapons (game.c w_*), the short bow (game.c r_*: index 0)
W_DICE, W_SIDES, W_PLUS = [1, 1, 1, 1, 2], [8, 8, 10, 10, 10], [0, 2, 4, 6, 8]

# what a player has, arriving at each depth: level, weapon, armour (tiers),
# potions in hand; the callings' differences are in player_new
PROGRESS = {1: (1, 0, 0, 2), 2: (2, 1, 1, 3), 3: (4, 2, 1, 3), 4: (5, 2, 2, 4), 5: (6, 3, 3, 4)}
# (lean: slow to level, no better weapon or armour found, few potions)
LEAN = {1: (1, 0, 0, 2), 2: (1, 0, 0, 2), 3: (2, 0, 0, 2), 4: (3, 1, 0, 2), 5: (4, 1, 1, 2)}
CALLINGS = ("fighter", "hunter", "lamplighter")
# fights on a level: about half of its creatures (6 + 2 per depth below the first)
FIGHTS = {d: (6 + 2 * (d - 1)) // 2 for d in range(1, 6)}
BOLT_DICE = 2     # the lich's violet fire: 2d6 - 2 (1 at the least) more than its blow (battle.c A_BOLT)
BOLT_PLUS = -2
ARROW_PHASE = 1   # a shot at a faded wraith: half, not nothing
LIFE = {"fighter": 40, "hunter": 34, "lamplighter": 30}
REST = 5          # life mended between fights (a regain a 12 turns, some walking)


def dice(n, s):
    return sum(random.randint(1, s) for _ in range(n))


class Player:
    def __init__(self, calling, depth):
        level, w, a, pots = PROGRESS[depth]
        self.calling = calling
        self.level = level
        base = LIFE[calling]
        # 4 a level, and one gift in three taken as Hardy (+6)
        self.maxhp = base + 4 * (level - 1) + 6 * ((level - 1) // 3)
        self.maxmp = 12 + 2 * (level - 1)
        self.hp, self.mp = self.maxhp, self.maxmp
        if calling == "fighter":
            w, a = max(w, 1), max(a, 1)
            pots += 1
        self.wdice, self.wsides, self.wplus = W_DICE[w], W_SIDES[w], W_PLUS[w]
        if calling == "hunter" and w == 0:
            self.wplus += 1                   # (a Great short sword to begin with)
        self.defence = a + 1
        self.potions = pots
        self.bow = calling == "hunter"
        self.arrows = 24 if calling == "hunter" else 0
        self.aim = 1 if calling == "hunter" else 0


class Fight:
    def __init__(self, pl, mon, ambush):
        self.pl, self.mon = pl, mon
        self.mhp = self.mhp_max = mon["hp"]
        self.dist = 0 if ambush else 1
        self.frozen = self.staggered = self.roar = self.parrying = 0
        self.poison = self.snared = self.ward = self.burning = self.numb = self.chilled = 0
        self.intent = self.last = A["NONE"]
        self.casting = 0
        self.rite = 0
        self.dealt = 0
        self.potions_drunk = 0

    # ---------------------------------------------------------------- the creature
    def choose(self):
        acts = self.mon["acts"]
        total = sum(w for _, w in acts)
        r = random.randrange(total)
        a = A[acts[0][0]]
        for name, w in acts:
            if r < w:
                a = A[name]
                break
            r -= w
        first = A[acts[0][0]]
        if a in NO_REPEAT and a == self.last:
            a = first
        if a == A["REGEN"] and self.mhp > self.mhp_max - 4:
            a = first
        if a == A["WEB"] and self.snared:
            a = first
        if a in NEEDS_CLOSE and self.dist > 0:
            a = A["CLOSE"]
        if a in (A["LUNGE"], A["DRAIN"]) and self.dist == 2:
            a = A["CLOSE"]
        return a

    def hurt(self, d, armour):
        pl = self.pl
        if armour:
            d = d - pl.defence if d > pl.defence else 1
        if self.ward:
            if self.ward >= d:
                self.ward -= d
                d = 0
            else:
                d -= self.ward
                self.ward = 0
        d = min(d, pl.hp)
        pl.hp -= d
        self.dealt += d
        return d

    def roll(self):
        m = self.mon
        d = dice(m["dice"], m["sides"]) + m["plus"]
        if self.roar:
            d += d // 2
            self.roar = 0
        if self.chilled:
            d -= d // 3
        return d

    def creature_acts(self):
        m, pl, a = self.mon, self.pl, self.intent
        if self.frozen:
            self.frozen = 0
            return
        if self.staggered:
            self.staggered = 0
            return
        if a == A["LUNGE"]:
            if self.dist == 2:
                return
            if self.dist == 1:
                self.dist = 0
            a = A["POISON"] if "VENOM" in m["flags"] else A["BURN"] if "FIERY" in m["flags"] else A["MELEE"]
        if a in (A["MELEE"], A["POISON"], A["HEAVY"], A["BURN"], A["NUMB"]):
            if self.dist:
                return
            if self.parrying:
                d = dice(pl.wdice, pl.wsides) + pl.wplus
                if a == A["HEAVY"]:
                    self.staggered = 1
                else:
                    d = d // 2 + 1
                self.mhp = max(0, self.mhp - d)
                return
            d = self.roll()
            if a == A["HEAVY"]:
                d *= 2
            if not self.hurt(d, 1):
                return
            if a == A["POISON"]:
                self.poison = 3
            elif a == A["BURN"]:
                self.burning = 2
            elif a == A["NUMB"] and random.randrange(2) == 0:
                self.numb = 1
        elif a in (A["SPIT"], A["CHILL"], A["BOLT"]):
            d = self.roll()
            if a == A["BOLT"]:
                d += max(1, dice(BOLT_DICE, 6) + BOLT_PLUS)
            if self.parrying:
                d = d // 2 + 1
            self.hurt(d, a == A["SPIT"])
        elif a == A["WEB"]:
            self.snared = 2
        elif a == A["DRAIN"]:
            if self.dist == 2:
                return
            d = self.roll()
            if self.parrying:
                d = d // 2 + 1
            self.mhp = min(self.mhp_max, self.mhp + d)
            self.hurt(d, 0)
        elif a == A["REGEN"]:
            self.mhp = min(self.mhp_max, self.mhp + 6 + random.randrange(5))
        elif a == A["ROAR"]:
            self.roar = 1
        elif a == A["CURSE"]:
            pl.mp = max(0, pl.mp - (3 + random.randrange(4)))
            self.hurt(1 + random.randrange(4), 0)
        elif a == A["CLOSE"]:
            if self.dist:
                self.dist -= 1

    # ---------------------------------------------------------------- the player
    def wound(self, d):
        if self.intent == A["PHASE"] and not self.casting:
            return
        if self.intent == A["GUARD"] and not self.casting:
            d = d // 2 + 1
        self.mhp = max(0, self.mhp - d)

    def fire(self, d):
        f = self.mon["flags"]
        if "FIREWEAK" in f:
            d += d // 2
        if "FIERY" in f:
            d = d // 2 + 1
        return d

    def firebolt(self):
        pl = self.pl
        pl.mp -= 3
        self.casting = 1
        self.wound(self.fire(dice(2, 6) + pl.level * 2))
        self.casting = 0

    def player_turn(self):
        """a sensible player: a potion when low, the flames rolled out,
        a great blow parried, fire where it bites (or through a guard or a
        fading), shots from afar, else a strike (or closing in)"""
        pl, m, it = self.pl, self.mon, self.intent
        self.parrying = 0
        if pl.hp * 3 < pl.maxhp and pl.potions:
            pl.potions -= 1
            self.potions_drunk += 1
            pl.hp = min(pl.maxhp, pl.hp + 15 + pl.level * 2)
            return
        if self.burning >= 2 and not self.snared:
            self.burning = 0
            self.dist = min(2, self.dist + 1)
            return
        fire_good = "FIREWEAK" in m["flags"] or it in (A["GUARD"], A["PHASE"])
        if pl.mp >= 3 and fire_good and "FIERY" not in m["flags"]:
            self.firebolt()
            return
        if self.dist > 0:
            # (a shot only where it is worth it: 3 or more on average, after armour)
            if pl.bow and pl.arrows and 3.5 + (pl.level - 1) // 2 + pl.aim - m["defense"] >= 3:
                pl.arrows -= 1
                if random.randrange(100) < (85 if self.dist == 1 else 70) + pl.aim * 5:
                    d = dice(1, 6) + (pl.level - 1) // 2 + pl.aim
                    d = d - m["defense"] if d > m["defense"] else 1
                    if self.intent == A["PHASE"] and ARROW_PHASE:
                        self.casting = 1          # (half, into what is fading: battle.c)
                        d = d // 2 + 1
                    self.wound(d)
                    self.casting = 0
                return
            if self.snared:
                if pl.mp >= 3:
                    self.firebolt()
                else:
                    self.parrying = 1
                return
            self.dist -= 1
            return
        if it == A["HEAVY"] or (it == A["PHASE"]):
            self.parrying = 1
            return
        d = dice(pl.wdice, pl.wsides) + pl.wplus + pl.level - 1
        d = d - m["defense"] if d > m["defense"] else 1
        if random.randrange(10) == 0:
            d *= 2
        self.wound(d)

    def run(self):
        """1 if the player won, 0 if they fell (no fleeing)"""
        pl, m = self.pl, self.mon
        for _ in range(200):
            self.intent = self.choose()
            if self.numb:
                self.numb = 0
            else:
                self.player_turn()
            if not self.mhp:
                return 1
            self.creature_acts()
            if not pl.hp:
                return 0
            self.last = self.intent
            if not self.mhp:
                return 1
            if "BOSS" in m["flags"] and not self.rite and self.mhp <= self.mhp_max // 2:
                self.rite = 1
                self.mhp += max(1, self.dealt // 2 - 3)
                self.roar = 1
            if self.poison:
                self.poison -= 1
                self.hurt(2, 0)
                if not pl.hp:
                    return 0
            if self.burning:
                self.burning -= 1
                self.hurt(3, 0)
                if not pl.hp:
                    return 0
            if self.chilled:
                self.chilled -= 1
            if self.snared:
                self.snared -= 1
        return 0


def creatures_at(depth):
    """(name, info, weight) of what lives at a depth"""
    out = []
    for name, _, info in monsters.MONSTERS:
        w = info["depths"].get(depth, 0)
        if w:
            out.append((name, info, w))
    return out


def one_fights(n):
    print("Each creature fought once from full life: won %, life lost (% of max), potions drunk per fight")
    for depth in range(1, 6):
        extra = {1: "orc", 5: "lich"}.get(depth)     # (the orc: out of its depth, as a test fought it)
        mons = creatures_at(depth) + [(m[0] + "*", m[2], 0) for m in monsters.MONSTERS if m[0] == extra]
        print("\nDepth %d (level %d, weapon %d, armour %d, %d potions)" % ((depth,) + PROGRESS[depth]))
        print("  %-14s" % "" + "".join("%-26s" % c for c in CALLINGS))
        for name, info, _ in mons:
            row = "  %-14s" % name
            for calling in CALLINGS:
                won = lost = pots = 0
                for _ in range(n):
                    pl = Player(calling, depth)
                    f = Fight(pl, info, ambush=random.randrange(2))
                    won += f.run()
                    lost += (pl.maxhp - pl.hp) / pl.maxhp
                    pots += f.potions_drunk
                row += "%-26s" % ("%3d%% %3d%% lost %.1f pot" % (100 * won // n, 100 * lost / n, pots / n))
            print(row)


def levels(n):
    print("\nA level's fights in a row (wounds and potions carried, %d life mended between):" % REST)
    print("  the chance to come through, and the potions left")
    print("  %-8s" % "" + "".join("%-26s" % c for c in CALLINGS))
    for depth in range(1, 6):
        mons = creatures_at(depth)
        weights = [w for _, _, w in mons]
        row = "  depth %d " % depth
        for calling in CALLINGS:
            through = left = 0
            for _ in range(n):
                pl = Player(calling, depth)
                alive = 1
                for _ in range(FIGHTS[depth]):
                    info = random.choices(mons, weights)[0][1]
                    if not Fight(pl, info, ambush=random.randrange(2)).run():
                        alive = 0
                        break
                    pl.hp = min(pl.maxhp, pl.hp + REST)
                    pl.mp = min(pl.maxmp, pl.mp + 2)
                through += alive
                left += pl.potions
            row += "%-26s" % ("%3d%%  %.1f potions left" % (100 * through // n, left / n))
        print(row)


# ---------------------------------------------------------------- a whole descent

# what the map does between fights, from the game's numbers (dungeon.c,
# trap.c, aim.c foe_shoots, explore.c); the guesses are marked
TURNS = {1: 450, 2: 500, 3: 550, 4: 600, 5: 600}   # (guess) turns a level takes, explored and left
FUEL_FOUND = 0.7         # (guess) of the level's fuel, what the player finds
FUEL = (80, 120, 160, 220)                         # rags, sticks, fur, tallow (pack.c burns_for)
TORCH_MAX, TORCH_LOW = 600, 150
PLATE_STEP = 0.25        # (guess) each hidden plate: the chance it is stepped on before it is found
FOUGHT = 0.5             # (guess) of the level's creatures, those fought
POTIONS_FOUND = 1.0      # (guess) healing potions found a level, on average
ARCHER_SHOTS = 2         # (guess) a skeleton's shots before it closes, 4 places off on average


def descent(calling):
    """one descent: ('won', 5) or (cause, depth); and the turns in the dark"""
    random.seed()
    depth, dark, level = 1, 0, 0
    pl = Player(calling, 1)
    pl.potions = {"fighter": 3, "hunter": 2, "lamplighter": 2}[calling]
    fuel = TORCH_MAX if calling == "lamplighter" else 400
    lights = {"fighter": 2, "hunter": 2, "lamplighter": 99}[calling]   # firesticks; the flint (lamplighter)
    lit = True
    for depth in range(1, 6):
        # arriving: the level (a level gained heals, as gain_xp), the gear found
        lv, w, a, _ = PROGRESS[depth]
        fresh = Player(calling, depth)
        if lv > level:
            pl.hp = fresh.maxhp
            pl.mp = fresh.maxmp
        level = lv
        pl.level, pl.maxhp, pl.maxmp = fresh.level, fresh.maxhp, fresh.maxmp
        pl.wdice, pl.wsides, pl.wplus, pl.defence = fresh.wdice, fresh.wsides, fresh.wplus, fresh.defence
        pl.hp = min(pl.hp, pl.maxhp)
        pl.potions += 1 if random.random() < POTIONS_FOUND else 0
        turns = TURNS[depth]
        # the events, at their turns: fuel found, plates stepped on, archers, fights
        ev = []
        for _ in range(4 + random.randrange(3)):
            if random.random() < FUEL_FOUND:
                ev.append((random.randrange(turns), "fuel", random.choice(FUEL)))
        for _ in range(1 + depth):
            if random.random() < PLATE_STEP:
                ev.append((random.randrange(turns), "plate", 0))
        mons = creatures_at(depth)
        weights = [x[2] for x in mons]
        for _ in range(6 + 2 * (depth - 1)):
            if random.random() < FOUGHT:
                name, info, _ = random.choices(mons, weights)[0]
                ev.append((random.randrange(turns), "fight", info))
                if "ARCHER" in info["habits"]:
                    ev.append((max(0, ev[-1][0] - 1), "arrows", 0))
        if depth == 5:
            lich = [m[2] for m in monsters.MONSTERS if m[0] == "lich"][0]
            skel = [m[2] for m in monsters.MONSTERS if m[0] == "skeleton"][0]
            ev += [(turns - 3, "fight", skel), (turns - 2, "fight", skel), (turns - 1, "fight", lich)]
        ev.sort(key=lambda e: e[0])
        venom, t = 0, 0
        for when, kind, what in ev:
            # the turns between: the torch burns, life mends (not while venom lasts)
            for _ in range(when - t):
                if lit:
                    fuel -= 1
                    if fuel <= 0:
                        fuel, lit = 0, False
                if not lit:
                    dark += 1
                if venom:
                    venom -= 1
                    if venom % 3 == 0 and pl.hp > 1:
                        pl.hp -= 1
                elif random.randrange(12) == 0:
                    pl.hp = min(pl.maxhp, pl.hp + 1)
            t = when
            if kind == "fuel":
                fuel = min(TORCH_MAX, fuel + what) if fuel < TORCH_MAX - 40 else fuel
                if not lit and fuel and lights:
                    lit, lights = True, lights - (calling != "lamplighter")
            elif kind == "plate":
                if random.randrange(8) < 2:
                    continue                          # stepped off in time
                k = random.randrange(4)
                if k == 0:
                    d = 2 + random.randrange(4) + depth
                    if pl.hp <= d:
                        return ("a dart", depth), dark
                    pl.hp -= d
                elif k == 1 and lit:
                    lit = False
                    if lights:
                        lit, lights = True, lights - (calling != "lamplighter")
                elif k == 3:
                    info = random.choices(mons, weights)[0][1]   # the bell: one more comes
                    ev.append((when, "fight", info))
            elif kind == "arrows":
                for _ in range(ARCHER_SHOTS):
                    if random.randrange(100) < 55:
                        d = dice(1, 6) + depth // 2
                        d = d - pl.defence if d > pl.defence else 1
                        if pl.hp <= d:
                            return ("arrows", depth), dark
                        pl.hp -= d
            else:
                f = Fight(pl, what, ambush=random.randrange(2) if lit else 1)
                if not f.run():
                    return (what["name"], depth), dark
                venom = f.poison * 8
                pl.mp = min(pl.maxmp, pl.mp + 2)
        if depth == 5:
            return ("won", 5), dark
    return ("won", 5), dark


def descents(n):
    print("\nWhole descents (%d each): how far, what ends them, the torch" % n)
    for calling in CALLINGS:
        reached = [0] * 6
        causes = {}
        dark = 0
        for _ in range(n):
            (cause, depth), d = descent(calling)
            dark += d
            for k in range(1, depth + 1):
                reached[k] += 1
            causes[cause] = causes.get(cause, 0) + 1
        won = causes.pop("won", 0)
        top = sorted(causes.items(), key=lambda c: -c[1])[:5]
        print("  %-12s reach depth 2-5: %s   win %d%%   turns dark %.0f a descent" % (
            calling, " ".join("%3d%%" % (100 * reached[k] // n) for k in range(2, 6)), 100 * won // n, dark / n))
        print("  %-12s ended by: %s" % ("", ", ".join("%s %d%%" % (c, 100 * k // n) for c, k in top)))


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if a not in ("lean", "descent")]
    if "lean" in sys.argv:
        PROGRESS = LEAN
    n = int(args[0]) if args else 2000
    random.seed(1)
    if "descent" in sys.argv:
        descents(n)
    else:
        one_fights(n)
        levels(n)
