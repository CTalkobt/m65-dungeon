# The Fading Deep

A torchlit descent for the MEGA65, in 640 x 400, built with the
[M65Compiler](../m65compiler) tool suite (cc45 / ca45 / ln45 / disk45).

Five levels down to the lich. You explore by torchlight: what the torch
shows you, you remember only as the shape of the place, and that memory
fades. Meet a creature and the view closes in on it for a fight, a turn at
a time. Chests and idols have their own close-ups.

## Build and run

```sh
make            # build/dungeon.d81: DUNGEON, DUNGEON.OV1, DGTILES, DG<NAME>A/B
make run        # boot the disk in xemu-xmega65
make screenshot # headless, build/screenshot.png
make art        # only the art (build/art/, previews in build/art/preview/)
make sounds     # assets/baseimage.d81 from the sound packs (see Sound)
make SEED=1234  # the same dungeons every time
make clean
```

Needs Python 3 with numpy and PIL for the art, the M65Compiler tree next to
this one (`../m65compiler`, or `make M65=<path>`), and xemu to run it. On a
MEGA65, copy `build/dungeon.d81` to the SD card, mount it and run DUNGEON.

For testing, `make shot KEYS='F__dddd' SHOT_DELAY=16` builds a version that
plays the keys (U D L R cursor keys, F RETURN, X RUN/STOP, _ a pause) and
saves `build/shot.png`. `TEST=-DTEST_FIGHT=<0-6>` starts a fight at once
(0 rat, 1 skeleton, 2 spider, 3 ooze, 4 orc, 5 wraith, 6 lich), and
`TEST=-DTEST_VIEW=1` / `2` opens a chest / an idol; `TEST=-DTEST_TIME` shows,
top right, the frames each turn's sight and drawing take; `TEST=-DTEST_ITEMS`
gives two of everything one can carry; `TEST=-DTEST_VAULT` starts beside a
locked door and says where it is and whether the key lies outside the vault. `TEST=-DTEST_FUEL=<n>` starts with n turns
of fuel on the torch; `TEST=-DTEST_SHOOT=<kind>` puts a sleeping creature of
that kind a few places away. `TEST=-DTEST_SECRET` starts beside a secret door,
`TEST=-DTEST_TRAP` with a hidden trap a step east, `TEST=-DTEST_UPS=<n>` with n
levels' gifts to choose, `TEST=-DTEST_BRAZIER` just west of a cold brazier;
`TEST=-DTEST_WIN` goes straight to the end, won; `TEST=-DTEST_MERCHANT` puts a
merchant a step east, and 100 gold; `TEST=-DTEST_FIXTURE` a fountain east and
a shelf west; `TEST=-DTEST_RUBBLE` starts beside fallen stones or a crack.
Other letters in `KEYS` are typed as they are (`KEYS='F__i'` opens the
inventory).
`TEST=-DTEST_HP=<n>` (with TEST_FIGHT) starts the creature with n life; `TEST=-DTEST_LURE=<kind>` puts a found plate two east and a hunting creature beyond it; `TEST=-DTEST_BATTER=<kind>` walls off the start two places east with a
wedged door and a hunting creature beyond; `TEST=-DTEST_GEAR` starts with
Superior dragonbone and rune plate; `TEST=-DTEST_DOG=<life>` starts the dog
with that much life (0: away, back in 3 turns); `TEST_FIGHT`'s creature
stands beside the dog; `TEST=-DTEST_SNIFF=1` puts a mimic, still a chest,
beside the dog, `=2` a hidden plate two east (for the dog's nose). `TEST=-DTEST_VENOM` starts with venom in the blood and an antidote;
`TEST=-DTEST_CHARMS=<0-4>` with one of each charm, wearing the one that
does that (CH_*).

**The tester's lantern.** Type `lantern` on the title screen (nothing
shows but a chime), or build with `DEFS=-DCHEAT`; then `@` on the map
opens a panel: go to any depth, reveal the whole level, know every flask
and charm, mend (life, mana, torch, venom), 100 gold, a level, change the
weapon, armour, bow and gun, put a creature of any kind beside you (awake),
go to the stairs (on the last level, into the lich's sanctum), and set how many of each thing you carry (left, right:
one less or more; RETURN: ten more). It is an overlay of its own, and
without the word `@` does nothing.
`python3 tools/probe.py build/dungeon.d81 <seconds> r m<addr>` boots the disk
and asks xemu's monitor for the registers and memory.

## Sound

The sampled sounds are on `assets/baseimage.d81`, which `make` takes
them from: the build needs nothing else. They were made from free sound packs
that are not in this repository; `assets/README.md` says which sound comes
from where. With the packs' zip files in `~/resources/audio/dungeon` (or
`SFX_DIR=<dir>`), `make sounds` makes the image anew. Without the image the
game builds with its SID effects only.

## Playing

**The day's descent** (D on the title). The dungeon from the date on the
MEGA65's clock (xemu gives it the host's): everyone who goes down on the
same day finds the same levels, whichever calling they choose. The ending
names the day, and the Hall of the Deep marks such a descent with a *. If
the clock has not been set, the title says so.

The title shows a hint under its keys, another every eight seconds (twelve
of them: disarming plates, wedging doors, antidotes, luring hunters over
plates, charms, fire, the level map).

**Who goes down.** A new descent begins with a choice: the *fighter*
(life 40, a long sword and studded leather, but no bow), the *hunter* (life
34, a short bow with 24 arrows, a sure aim, a Great short sword, two
strips of meat), or the
*lamplighter* (a full torch, a flint, two flasks of oil, rags and a keen
memory, but only 30 life). The ending names who it was.

| Key | Joystick | Exploring | Fighting |
|---|---|---|---|
| cursor keys, W A S D | directions | move | up / down: choose |
| Q E Z C (or the number keys) | | move diagonally | |
| SPACE, 5, . | | rest a turn, searching round about | |
| RETURN | FIRE | go down the stairs; off them, walk to them (once found) | do the chosen action |
| P | | drink a healing potion | parry |
| I | | what you carry: up / down choose, RETURN (U) use, D put down | |
| F / L | | feed the torch / light it | |
| O (S on the title) | | settings: music and sound levels | |
| T / G | | shoot the bow / fire the flintlock (again: the next target, RETURN: shoot) | |
| X | | throw a flask of oil or meat (O / M when you carry both) | |
| M | | the level as you remember it; a cursor, RETURN puts a note (! $ * ?) | |
| K | | shut a door beside you; if it is shut, wedge it with a bundle of sticks | |
| J | | the last 36 messages, to read again | |
| S P C A R I T | | | strike, parry, cast, advance, retreat, item, shoot |
| F I H W | | | (casting) firebolt, frost, heal, ward |
| H M F A | | | (items) potion, draught, scroll of fire, antidote |
| B G | | | (shooting) the bow, the flintlock |
| H, ? | | every key, on one screen | |
| RUN/STOP | | save and stop (S), or give up (Y) | back out of the spells or items |
| C (on the title) | | continue the saved descent | |
| F (on the title) | | the Hall of the Deep: the best descents | |

**Exploring.** Walk into doors to open them, into creatures to fight them.
Gold, healing potions (red), mana draughts (blue), scrolls and keys are
picked up as you walk over them, and stepping onto a chest or an idol opens
its close-up. Rooms with a brazier are lit from
wall to wall. Elsewhere you see as far as your torch reaches. Beyond it,
in the shadows, you see only what shines: eyes. What you have seen stays
behind as faint blue outlines of the walls, doors and stairs, dimming over
some hundreds of turns until it is gone. Some idols sharpen your memory,
others make it slip.

**Saving.** Each level is saved as you enter it, and RUN/STOP then S saves
where you stand and goes back to the title, where C takes it up again. It
is one save (`DGSAVE`, 29 blocks on the game's disk); dying or winning
ends it. On the disk it is the player, the creatures, what lies about,
each place's terrain tile (a byte; the map's characters are drawn again
from them) and its memory.

**The sanctum.** On the last level the lich waits in the last room, with
two of its skeletons on guard and bones strewn about. When it notices you
it speaks, and every creature on the level wakes and hunts. Its violet
fire burns 2d6-2 more than its blow (1 at the least), and armour does not
turn it. Once in a
descent, brought to half its life, it calls on its crown: it mends half of
the life it has taken from you in that fight, less 3 (1 at the least;
what a ward or a parry kept from it does not count), and its next blow will be terrible (enraged).

**The end.** Dying shows what did it and where ("Slain by a cave spider,
in The Bone Galleries."); destroying the lich shows it come apart, then
its hoard. Either way the tally follows: depth, level, turns, gold,
creatures slain, the gifts taken, how many of the flasks and the charms
you came to know, and the charm you wore. The five best descents (the won first,
then the deepest, the highest level, the richest) are kept on the disk
(`DGFAME`) as the Hall of the Deep: F on the title shows them.

**Balance** (`tools/balance.py [fights] [lean]`). The close-up fight's
rules, ported to Python and played a few thousand times a creature with a
sensible player (a potion when low, great blows parried, fire where it
bites, shots from afar): each creature fought once from full life, at
each depth and for each calling, and a level's fights in a row with
wounds and potions carried. What a player has at each depth is a guess
(PROGRESS; `lean`: slow to level, no better gear). `descent` plays whole
descents instead: five levels in a row, with the torch's fuel found and
burned, hidden plates stepped on, skeletons' arrows, mending as the turns
pass, and the fights; it says how far each calling gets, what ends them,
and how many turns go by in the dark. Its guesses (turns a level, fuel
found, plates stepped on, creatures fought) are at its top. Keep it in
step with `src/battle.c`.

**Settings** (O on the map, S on the title). The music's and the sounds'
levels, 1-15, or off (RETURN turns the chosen one off and back on). The
music's level is the SIDs' volume; the sounds' scales each sample. (`S`
cannot be the key on the map: it is a step south.)

**What hides.** A door or two on a level is secret: it looks exactly like
the wall it is in (but never one the way down or the vault's key needs).
Traps lie hidden in the rooms: a pressure plate fires a dart, snuffs the
torch with a gust, rings a chime that wipes what you remember of the
level, or clangs a bell that wakes everything; one time in four you step
off it in time. Resting searches the eight places round you (each hidden
thing one time in two), and walking past now and then finds one; a
scroll of far sight (or the idol's gift) finds them all. A trap found
shows as a plate in the floor, still armed (one you stepped off stays
armed too). Walk into it to try to disarm it: 4 in 12, 2 more for each
step of wary feet and 3 more for the hunter (it gives 2 experience);
fail, and one time in three your hand slips and it springs. Travelling
to the stairs goes round found plates. A creature hunting you does not
watch its feet: any plate it steps on, hidden or found, fires a dart into
it (4-9, and 2 more each level down) and leaves it stunned a moment, so
a plate between you and a hunter is worth keeping (bats fly over). A
plate sprung or disarmed is floor again.

**The creatures on the map.** Skeletons shoot arrows from two to six
places off along a clear line (the arrows can be picked up where they
fall). Giant rats snatch something carried (fuel first, then an arrow) and
run off with it; killed, they drop it. Oozes dissolve whatever lies where
they crawl, but never a key. In a fight a wraith's chill can snuff your
torch. Bats come in flocks on the upper levels: they flit about
fast and erratic, two moves a turn, weak alone. Deeper down, a chest may
be a mimic: it never moves and does not show itself as a creature (it is
not aimed at, and travel does not stop for it) until you reach for it,
and then it strikes first. It is fought in the chest's close-up, with its
own words (DGMIMICH, its header, loaded over the chest's). Ghouls (depths 2-4) shrink from
the light: they keep to the edge of your torch's reach and close in
only when it gutters out, quick in the dark then. Ember spiders (depths
3-5) spit fire. Both are fought in another's close-up recoloured: their
file (DGGHOULH, DGEMBERH: 5 blocks) holds a palette (the creature's own
colours only, not its shadow on the wall) and the header.

**Shooting** (T: the bow, G: the flintlock). A red mark blinks on the
nearest creature you can see (lit, or eyes in the dark) within reach and
with a clear line; the same key or the cursor keys pick another, RETURN
shoots, RUN/STOP thinks better of it. The shot flies, and hits more often
near than far, less often at eyes in the dark; the creature wakes and
comes. What dies of it gives its experience where it fell. A short bow
(1d6, reach 6) and a longbow (1d8+2, 9) shoot arrows; a flintlock pistol
(2d6+2, 5) and a musket (3d6+4, 8) shoot paper cartridges, hit harder,
misfire one time in six, and wake everything near. An arrow lies where
it ended, to be picked up again, unless it broke (one in two that hit,
one in four that missed); after a fight you win, half the arrows you shot
in it are gathered up whole. The lich is warded:
it must be faced. You start with a short bow and 8 arrows; chests give
the better bows, then the flintlocks, and arrows and cartridges lie about.

**Strange flasks.** Cloudy green, smoky grey, violet and amber: what each
colour does is shuffled for every descent, and unknown until you drink
one (from what you carry) or read of it on an apothecary's page in a
bookshelf. One gives dark-sight (120 turns of seeing what is in shadow,
dimly, torch or no torch), one clears your memory (what you see fades
slowly, for the level), one is vigour (3 more life, for good), and one
is poison. Once known, a flask goes by its name.

**How well made.** Weapons, armour, bows and flintlocks come Minor, plain,
Great, Superior or Rare (grey, white, green, cyan, violet in what you
carry): Minor is 1 less to the blow (or the armour), Great 1 more,
Superior 2, Rare 3, and a Rare weapon lands a telling blow twice as often
(1 in 5). A chest's weapon or armour is the next better one or another
like yours, its make rolled (better deeper down, and in a vault); you take
it only if it is the better (a make counts a third of a step up), else
"yours serves better". Each charm's look has a make too, rolled with the
descent and named once known ("Great charm of mending"): the steady flame
spares the torch 3 to 7 turns in 8, mending heals every 6 to 2 turns, the
ward against the dead is 3 stronger for each make. In the tester's
lantern, RETURN on the weapon, armour, bow or gun gives the next make.

**Charms.** Rings and amulets (iron, jade or silver rings; bone or amber
amulets) lie about now and then below the first level, and turn up in
chests. One is worn at a time: put one on from what you carry (the one
worn before goes back with the rest). As with the flasks, what each look
does is shuffled for every descent and known once worn: the *steady
flame* (the torch burns half as fast), *mending* (wounds mend over twice
as fast), the *cat's eye* (what is in shadow is seen dimly, as with
dark-sight), *against the dead* (a ward at the start of every fight with
the undead) and *against venom* (no venom takes hold).

**Your dog.** A dog goes down with you, whoever you are. It keeps close
(two places off at most; left far behind, it catches up), and runs at the
nearest creature in sight (not the lich: it is warded) and bites it, 1 a
turn, waking it. Bitten, a creature snaps back now and then (1-3, and more
deeper down); at no life left the dog yelps and limps off into the dark,
and comes bounding back to your side 35 turns later. Its life is 10, and 2
more for each level you have. It opens no doors, follows you down the
stairs, trades places with you when you walk into it, and creatures cannot
walk through it. In a close-up fight it joins in if it was beside the
creature when the fight began: half the rounds, a bite of 1. It has a
nose, too: a mimic still a chest it does not run at, but when it is
beside one it growls at it, the once, and leaves it be; and a hidden pressure plate
within two places of you it now and then sniffs out (1 in 6 a turn, while
it has nothing to chase): the plate is found, as your own search finds one.

**Doors.** Creatures open shut doors (it takes them a move), but a shut
door breaks their line of sight: K shuts an open door beside you. K again
wedges it with a bundle of sticks, and then nothing opens it, though a
creature hunting you may batter its way through (1 in 8 each try, and you
hear the wood splinter). Walk into a wedged door to kick the wedge free
and take the sticks back.

**Venom, fire and worse.** A venomous bite (rats, cave spiders) burns 2
a round for three rounds of the fight, and what is left of it goes on in
your blood after: no mending while it lasts, and a pang every third turn
(it leaves you 1 life at the least; "venom in your blood" on the top
line). A *vial of antidote* (pale green: found lying about, in chests,
and sold by the merchant) cures it, in a fight (Item, A) or out of one;
a healing potion does not. An *ember spider*'s bite sets you alight: 3 a
round for two rounds, that armour does not turn (or until you *retreat*
and roll the flames out); fire hurts it half as much. A *ghoul*'s grave-cold claws can numb
you, and you lose your next action. *Frost* leaves what it froze chilled
for three rounds after (its blows a third weaker), and a chilled
creature you flee is slow to follow.

**Throwing** (X, or use it from what you carry; reach 5, aimed as a shot).
A *flask of oil* bursts into fire where it lands: 2d6+2 that no armour
turns, twice that to what fire is the bane of (spiders, oozes); the
flames burn there for 12 turns, barring the way, and light a dark room
they are in for 30. *Dried meat* is fallen on by any creature that
hungers (not the dead that walk): it stays put a while and loses track of
you. The merchant sells both; they lie about and turn up in chests.

**The level map** (M). The whole level, a place a character: what you
remember (as on the view: places seen and not yet forgotten), the stairs,
doors, traps found, and you. Move the cursor and press RETURN to put a
note on a place (! danger, $ treasure, * the way, ? a question; again: the
next kind, then off). Notes stay however your memory fades and are shown
on the view, even in the dark; 16 a level, kept with the save.

**Travel** (RETURN off the stairs). Once you have seen the stairs down,
you walk to them over the places you remember, a step a turn, until
something comes into sight, a key is pressed, or you are hurt.

**The torch** burns a turn at a time (the bar at the top). Below 150
turns its light reaches less far, below 60 it gutters, and at 0 it goes
out: then only the braziers' rooms are lit, creatures are eyes in the
dark, and the way is found by what you remember. Feed it (F, or from the
inventory) with a twist of rags (80 turns), a bundle of sticks (120), a
scrap of fur (160) or a tallow candle (220); it holds 600. Out, it is lit
again (L) with a flint (kept, but it catches one strike in three), a
firestick (used up, sure), or by walking into a brazier. Some dark rooms
have a cold brazier: walk into it with the torch alight and it takes a
flame (30 turns of fuel) and lights the room for good. You start with
400 turns, a bundle of sticks and two firesticks.

**Growing.** Each level gained gives 4 more life and 2 more mana and a
gift, chosen (back on the map) from three of: *Hardy* (6 more life),
*Deep well* (4 more mana), *Keen memory* (what you have seen fades more
slowly), *Steady torch* (it burns one turn in 4, then 3, then 2 less),
*Sure aim* (shots hit more often and harder), *Wary feet* (traps found
sooner and stepped off more often). The last four can be chosen three
times each.

**In the rooms and corridors.** A fountain now and then: drink (walk
into it), once: it may heal you, restore your mana, sicken you, or show
you the way you came; then it is dry. Bookshelves against the walls:
search them, once, for a scroll, old pages to burn (rags), a journal's
last word of advice, or dust. Fallen stones choke a narrow corridor or
two, and a cracked wall or two stands between two ways: walk into them to
heave at them, a turn at a time, until they give (one time in three). Both
block sight and creatures; the generator never lets them shut a level
(they can always be cleared).

**The merchant.** On some levels (not the last) a hooded merchant waits
with his lantern (seen in the dark). Walk into him to buy, with gold, from
six of his thirteen kinds of wares: healing potions (25), mana draughts
(30), rags (6), sticks (10), tallow candles (18), firesticks (12), a flint
(40), five arrows (15), three cartridges (24), a scroll of far sight (45),
a flask of oil (20), two strips of meat (8), a vial of antidote (20); one
to three of each.

**What you carry** (I). Choose with up and down; RETURN (or U) uses it,
D puts all of it down where you stand (or on a free place beside you),
where it lies as one pile until you walk over it again. Healing potions and mana draughts; a *scroll of
fire* (for a fight: fire that no guard stops); a *scroll of recall* (all
you have seen comes back fresh); a *scroll of far sight* (the level's
walls, doors and stairs, seen or not); *iron keys*; the torch's fuel and
flints and firesticks; flasks of oil and dried meat, to throw; vials of
antidote. Most levels have a
vault: a room whose doors are locked, with two chests that hold two finds
each and the better ones. Its key lies somewhere you can reach without it.
Chests give weapons and armour (each better than the last), potions,
draughts, scrolls, fuel, flints and firesticks, or gold; creatures leave
gold, potions, draughts, scrolls, rags and fur behind now and then.

**Fighting.** Each round the creature shows what it means to do, and you
answer:

- *Strike* (only when close): your weapon, less the creature's armour.
- *Parry*: stops an ordinary blow and strikes back. Against a *great blow*
  (in red: "gathers itself for a crushing blow!") it also leaves the
  creature reeling for a round. It halves what comes from afar.
- *Cast*: *firebolt* (oozes and spiders burn), *frost* (the creature is
  frozen for a round), *heal*, *ward* (absorbs damage, even a wraith's
  touch that armour cannot stop).
- *Advance* / *Retreat*: the range is close, near or far. Blows need close.
  A lunge reaches from near. Retreating when already far is an attempt to
  flee. Silk keeps you from moving.
- *Item*: a healing potion, a mana draught, a scroll of fire, or an antidote.
- *Shoot* (from near or far, not close): the bow or the flintlock, as on
  the map; a hit is likelier near (85 in 100) than far (70). A guard
  halves it, and a wraith that has faded takes only half of it (a blade
  passes through).

## How it works

**Display** (`src/video.c`). VIC-IV 80 x 50 characters (640 x 400) with
16-bit character numbers. Characters above $FF are full-colour (a byte a
pixel). Below $FF is text from the ROM's lower-case set, in 15 colours of our
own. The screen gets a palette bank of its own: 0-15 text, 16-127 the tiles,
128-254 the current close-up (247-254 its glow, which pulses) or, on the
map, the map's eight palette banks. Torchlight
flickers by scaling the tiles' palette each few frames, and the flames of
the braziers and the player's torch are four entries of their own (120-123)
that turn round a red-to-gold ramp. Fades, hit flashes
and spell colours wash the palette, never the pixels.

**Each depth its own stone.** The walls, corridors, fallen stones and
cracked walls are drawn in five styles (cellar blocks, wet ashlar, an
ossuary, raw rock, basalt columns), and the stone recoloured: sandstone,
mossy grey, chalk, umber, violet basalt; wood and iron stay as they are.
A terrain colour number means the same at every depth (1-7 the stone,
8-11 the wood and iron, 12-13 the stone's edges, 14 the iron's), so a
depth is its characters and its palette banks (DGLOOK, 8 KB each, kept in
attic RAM at $8310000 and put in place as the map is shown). The room
floors are the same everywhere: the things lying on them have the floor
under them drawn in the map's palette banks, so they sit on it at every
depth.

**The map** (`src/view.c`, `src/vmap.s45`). A place is 2 x 2 characters
(16 x 16 pixels), and the view is 40 x 22 places of a 64 x 48 level. The
level is made straight into a map of characters in attic RAM ($8300000,
128 x 96 characters, laid out like the screen): rock, floors, corridors,
walls, doors and stairs, each drawn once and only changed when a door
opens. There is no other copy of the level: what a place is, is read back
from its character (`vm_kind`, `terrain`).

The terrain's characters are 16-colour (NCM), shown 8 pixels wide (the
colour RAM trims the other 8), so one set of them serves however a place
is lit: the colour RAM picks the palette bank (`tools/gen_art.py`
`map_palette`). Banks 8-11 are the torch's four levels, 12-14 the
memory's three, in which only a wall's, a door's or the stairs' edge
pixels (colours 12-14) have a colour, so a remembered room is its
outline, and 15 is dark. Each turn the rows in view are copied (DMA) from
the map to the screen, the things, the creatures and the player are put
over them as full-colour tiles (four torchlight levels, and "eyes only"
for creatures in shadow), and each character's colour is worked out.

Line of sight is a Bresenham line to every place within 11. The light is
chosen for each 8 x 8 character, not each place, from its centre's
distance to the torch, so the edge of the light is round. A room with a
brazier is lit throughout (a short list of lit rooms). Memory is a byte
per place in colour RAM past the screen: the epoch (4 turns) it was last
seen lit, swept every 64 turns so that old epochs never come round again.

(Scrolling the screen itself over the map would not leave the level's
name and the messages where they are: the VIC-IV takes the screen's
address once a frame, at its top, so it cannot be changed between rows.)

**The creatures** (`tools/monsters.py`). Each creature is one entry: the
function that draws its close-up, and its numbers (life, blow, armour,
experience), what it does in a fight (actions and their weights), flags
(burns easily, shrugs off frost, hard to flee, venomous, slow, the last
foe, undead, fiery), the levels it lives on, and its words (name, its lines, its lore, its
death). The fight's part goes in the header of its close-up's file, which
the battle reads when it starts; the map's part (its tile, its close-up,
its flags, which creatures each level draws from) goes in `assets.h`.
Adding a creature is a new entry and its tile (`tools/tiles.py`).

**The close-ups** (`tools/stage.py`, `monsters.py`, `props.py`,
`backdrop.py`). Each is 448 x 240 (56 x 30 characters). The creatures are
drawn in Python from 3D shapes (ellipsoids, tubes of spheres, boxes,
cylinders) into a depth buffer, lit by a warm torch from the upper left
with a cool fill and rim light, shadowed by marching through the depth
buffer, with creases darkened. Each has two frames (waiting and
attacking, shut and open, dormant and awake).

They all stand in front of one **shared backdrop**: a stone wall and floor
in a pool of torchlight, made of 256 characters (a tileable pattern of
stones at 6 light levels, the floor at 4), each cell taking the level the
torch gives it, dithered only where two levels meet. Those 256 characters
are kept once, in the C64 ROM's space (below). A close-up is the creature
drawn over that very backdrop; only the cells it (its shadow, its glow)
changes have characters of their own, and in them the backdrop's pixels
keep the backdrop's colours, so there is no seam. Their palette is 119
colours for what the creature changes plus an 8-colour glow ramp, with
ordered (Bayer) dithering; the backdrop shares the map tiles' palette.
Most close-ups need under 1257 characters and are kept whole; past that,
characters are merged (the pair whose merging costs least first).

A close-up is two files, DG<NAME>A in bank 5 (palette, header, both
frames' maps, 778 characters) and DG<NAME>B in bank 1 (the rest), loaded
the first time it is needed (`src/scene.c`) and then kept in attic RAM, so
the next fight with that creature is two DMA copies. A frame is shown by
DMA-copying its map into the screen, a row at a time.

**The C64 ROM's space.** In C65 mode the C64 BASIC and KERNAL in the ROM
area are never used. At the start the program makes that space writable
(cc45's `<rom_ram.h>`: `romram_enable(ROMRAM_C64)`), unpacks the backdrop
(DGBACK) into `$2A000`-`$2BFFF` and `$2E000`-`$2FFFF`, and protects it
again (`romram_protect`). After the game, `GO64` will not work until the
machine is reset.

**Packing** (`tools/pack.py`; cc45's `unpack()`, `<unpack.h>`). Every art
file is packed in pack45's format 1, a byte-oriented LZ (literal runs,
fills, and copies from 1 or 2 bytes back that never overlap what they
write), which the library's `unpack()` undoes with a DMA job per token.
`pack.py` chooses the tokens by an optimal parse (pack45 is greedy): a few
per cent smaller, the same format. Each file loads near the end of the
area it unpacks to, at an offset worked out so that the writing never
overtakes the reading, and unpacks in place; DGLOOK loads into bank 5 and
unpacks straight into attic RAM. The art is about 500 KB on the disk
instead of 700 KB unpacked. The overlay files are packed by ln45
(`--pack-overlays`): 80 KB of code in 48 KB on the disk.

**Sound** (`tools/gen_sfx.py`, `src/audio.c`). 28 sounds, 22 s at 12 kHz:
cut, resampled, normalised, and coded in blocks of 16 samples (a predictor
and a shift a block, 4-bit residues: 4.5 bits a sample, 150 KB on the
disk, from `assets/baseimage.d81`). They are decoded at the start into
attic RAM. The audio DMA reads
chip RAM only, so a sound is copied into one of two play slots in bank 1
when it is wanted: a 16 KB one for the long sounds (channel 0) and an 8 KB
one for the short (channel 1). On the map, now and then, a drip, a chain,
a far footfall, off to one side. The SIDs play where there is no sample.

**Music** (`tools/gen_music.py`). Two tunes for all four SIDs, written
out as tune45's text by a small composer (phrases, chords, delayed and
octave-shifted copies of voices) and made into DGMUSIC by `tune45 convert
--bin`; the program keeps it in attic RAM and plays it with the library's
tune player (`tune_play_far`). The title theme (D minor, 16 bars) plays on
the left and, a few hundredths of a second behind and an octave apart, on
the right: a fuller sound, spread across the two sides. The levels' tune
(2.5 minutes) is a low drone with far-off bells; the bass begins to walk,
a melody looks about; once round, a march (drums, horns, the melody high)
before it sinks back. It pauses for a fight or a close-up and goes on
from where it was. The SID sound effects keep to the twelfth voice, which
the levels' tune leaves free. `build/music/*.wav` (`tune45 play --wav`)
are the tunes to listen to on the PC.

**Memory** (bank 0 is tight). The resident part (video, keys, the UI,
sound, the tables, the message line) ends near $8200; the overlay
region starts at $8D00. The code overlays `walk` (explore +
view), `pack` (the inventory, items, the torch), `aim` (shooting,
throwing, the creatures' turn and tricks, the way to the stairs), `battle`, `gen` (the dungeon generator, saving, traps
and secrets, a new player, levels gained), `scenes` (loading and showing
the close-ups; the chest and the idol) and `front` (the title, settings,
endings, the start) are kept in
attic RAM ($8000000-$801FFFF: two 64 KB banks, a file each, DUNGEON.OV1
and, once the first is full, DUNGEON.OV2; packed, unpacked at the start)
and copied by DMA into one region at $8B00 when called. A call
from one overlay into another brings the caller back on return, but not
the caller's text in the meantime; so what takes text from any overlay
(the message line `msg`, the log, `video_text`) is resident. The map's
drawing buffers share 1 KB (`scratch`, game.h) with a close-up's log and
its creature's words, which are never in use at the same time. cc45
inlines only the smallest functions (`-fmax-inline-ir-size=4`): the fight
overlay was 1.6 KB larger with the default. The game needs the MEGA65's attic RAM (every
production MEGA65 and xemu have it). The level itself is the map of
characters in attic RAM; things lying about are a short list, sight a window
round the player, memory a byte a place in colour RAM.

| Where | What |
|---|---|
| $2001-$81FF | the program, its data and variables |
| $8200-$89FF | the heap (unused: room for the program to grow) |
| $8A00-$BBFF | the code overlay region (12,800 bytes: one overlay at a time) |
| $BC00-$BFFF | the stack |
| $12000-$197FF | DG<NAME>B: a close-up's last characters |
| $19800-$1F7FF | the sounds' play slots (16 KB, 8 KB) |
| $2A000-$2BFFF | the backdrop's first 128 characters (the C64 BASIC's space) |
| $2E000-$2FFFF | the backdrop's other 128 (the C64 KERNAL's space) |
| $40000-$4FFFF | DGTILES: the palette (and the map's banks), the terrain's 16-colour tiles, the full-colour tiles, frame and bar characters |
| $50000-$5DFFF | DG<NAME>A: a close-up's palette, maps, characters |
| $5E000-$5FF3F | the screen |
| $FF80000-$FF81F3F | colour RAM: the screen's colours |
| $FF82000-$FF82BFF | colour RAM: the map's memory, a byte a place (`seen_*`) |
| $8000000-$801FFFF | attic RAM: the code overlays, unpacked from DUNGEON.OV1 (bank 800) and DUNGEON.OV2 (bank 801) |
| $8070000- | attic RAM: the tunes (DGMUSIC) |
| $8080000- | attic RAM: the sounds, decoded |
| $8100000- | attic RAM: the close-ups seen so far, 128 KB each |
| $8300000-$8305FFF | attic RAM: the level's map, its terrain as characters (128 x 96) |
| $8310000-$831AC7F | attic RAM: DGLOOK, each depth's stone (5 x 8,832 bytes) |
| $8320000-$83208FF | attic RAM: the messages' history (36 x 64 bytes) |
| $8340000-$8340623 | attic RAM: the names and `tile_char`, `__bank(0x834)` data (DUNGEON.OV3) |
