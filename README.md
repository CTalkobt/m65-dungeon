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
top right, the frames each turn's sight and drawing take.
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

| Key | Joystick | Exploring | Fighting |
|---|---|---|---|
| cursor keys, W A S D | directions | move | up / down: choose |
| Q E Z C (or the number keys) | | move diagonally | |
| SPACE | | wait a turn | |
| RETURN | FIRE | go down the stairs | do the chosen action |
| P | | drink a potion | parry |
| S P C A R Q | | | strike, parry, cast, advance, retreat, potion |
| F I H W | | | (casting) firebolt, frost, heal, ward |
| H | | help | |
| RUN/STOP | | give up | back out of the spells |

**Exploring.** Walk into doors to open them, into creatures to fight them.
Gold and potions are picked up as you walk over them, and stepping onto a
chest or an idol opens its close-up. Rooms with a brazier are lit from
wall to wall. Elsewhere you see as far as your torch reaches. Beyond it,
in the shadows, you see only what shines: eyes. What you have seen stays
behind as faint blue outlines of the walls, doors and stairs, dimming over
some hundreds of turns until it is gone. Some idols sharpen your memory,
others make it slip.

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
- *Potion*.

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

**The map** (`src/view.c`, `src/vmap.s45`). A place is 2 x 2 characters
(16 x 16 pixels), and the view is 40 x 22 places of a 64 x 48 level. The
level is made straight into a map of characters in chip RAM ($48000,
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
foe), the levels it lives on, and its words (name, its lines, its lore, its
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

**Packing** (`tools/pack.py`, `unpack` in `src/scene.c`). Every art file
is packed with a byte-oriented LZ (literal runs, fills, and copies from 1
or 2 bytes back that never overlap what they write). Each file loads near
the end of the area it unpacks to, at an offset the packer works out so
that the writing never overtakes the reading, and unpacks in place, a DMA
job per token. The art is about 500 KB on the disk instead of 700 KB unpacked.

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
sound, the tables) ends near $7F00. The code overlays `walk` (explore +
view), `battle`, `gen` (the dungeon generator) and `scenes` (chest, idol,
title, endings) are kept in attic RAM and copied by DMA into one region at
$9000 when called. The game needs the MEGA65's attic RAM (every
production MEGA65 and xemu have it). The level itself is the map of
characters in bank 4; things lying about are a short list, sight a window
round the player, memory a byte a place in colour RAM.

| Where | What |
|---|---|
| $2001-$7EFF | the program, its data and variables |
| $9000-$BBFF | the code overlay region |
| $BC00-$BFFF | the stack |
| $12000-$197FF | DG<NAME>B: a close-up's last characters |
| $19800-$1F7FF | the sounds' play slots (16 KB, 8 KB) |
| $2A000-$2BFFF | the backdrop's first 128 characters (the C64 BASIC's space) |
| $2E000-$2FFFF | the backdrop's other 128 (the C64 KERNAL's space) |
| $40000-$47FFF | DGTILES: the palette (and the map's banks), the terrain's 16-colour tiles, the full-colour tiles, frame and bar characters |
| $48000-$4DFFF | the level's map: its terrain as characters |
| $50000-$5DFFF | DG<NAME>A: a close-up's palette, maps, characters |
| $5E000-$5FF3F | the screen |
| $8000000- | attic RAM: DUNGEON.OV1, the overlays |
| $8080000- | attic RAM: the sounds, decoded |
| $8100000- | attic RAM: the close-ups seen so far, 128 KB each |
