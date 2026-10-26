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
`TEST=-DTEST_VIEW=1` / `2` opens a chest / an idol.
`python3 tools/probe.py build/dungeon.d81 <seconds> r m<addr>` boots the disk
and asks xemu's monitor for the registers and memory.

## Sound

The sampled sounds are on `assets/baseimage.d81`, which `make` takes them
from: the build needs nothing else. They were made from free sound packs
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
128-254 the current close-up (247-254 its glow, which pulses). Torchlight
flickers by scaling the tiles' palette each few frames, and the flames of
the braziers and the player's torch are four entries of their own (120-123)
that turn round a red-to-gold ramp. Fades, hit flashes
and spell colours wash the palette, never the pixels.

**The map** (`src/view.c`). A tile is 2 x 2 characters (16 x 16 pixels),
and the view is 40 x 22 tiles of a 64 x 48 level. Each tile kind has up
to 8 variants (`tools/tiles.py`): four torchlight levels, three fading
memory outlines, and "eyes only" for creatures in shadow. Line of sight is
a Bresenham line to every place within 11. The light is chosen for each
8 x 8 character, not each tile, from its centre's distance to the torch, so
the edge of the light is round. A room with a brazier is lit throughout. Memory is a byte per place: the epoch
(4 turns) it was last seen lit, swept every 64 turns so that old epochs
never come round again.

**The close-ups** (`tools/stage.py`, `monsters.py`, `props.py`). Each is
448 x 240 (56 x 30 characters), drawn in Python from 3D shapes
(ellipsoids, tubes of spheres, boxes, cylinders) into a depth buffer, lit
by a warm torch from the upper left with a cool fill and rim light,
shadowed by marching through the depth buffer, with creases darkened. The
backdrop is a wall of rough stone in a pool of torchlight. Each has two
frames (waiting and attacking, shut and open, dormant and awake). Both get
one 119-colour palette plus an 8-colour glow ramp, with ordered (Bayer)
dithering: smooth, and its patterns repeat, so fewer characters differ. Their 8 x 8 characters
are shared between the frames and, past 1257 (what banks 5 and 1
hold beside the sounds' slots), merged: the pair of characters whose merging costs least (colour
difference times the cells using them) first, so the few losses go to
the dark corners. A close-up is two files, DG<NAME>A in bank 5 (palette,
header, both frames' maps, 778 characters) and DG<NAME>B in bank 1 (the
rest), loaded through the KERNAL the first time it is needed
(`src/scene.c`) and then kept in attic RAM, so the next fight with that
creature is two DMA copies. A frame is shown by DMA-copying its map into
the screen, a row at a time.

**Packing** (`tools/pack.py`, `unpack` in `src/scene.c`). Every art file
is packed with a byte-oriented LZ (literal runs, fills, and copies from 1
or 2 bytes back that never overlap what they write). Each file loads near
the end of the area it unpacks to, at an offset the packer works out so
that the writing never overtakes the reading, and unpacks in place, a DMA
job per token. The art is about 700 KB on the disk instead of 1.03 MB, which
would not fit (the tiles to 31%, the close-ups to 58-82%).

**Sound** (`tools/gen_sfx.py`, `src/audio.c`). 28 sounds, 22 s at 12 kHz:
cut, resampled, normalised, and coded in blocks of 16 samples (a predictor
and a shift a block, 4-bit residues: 4.5 bits a sample, 150 KB on the
disk, from `assets/baseimage.d81`). They are decoded at the start into
attic RAM. The audio DMA reads
chip RAM only, so a sound is copied into one of two play slots in bank 1
when it is wanted: a 16 KB one for the long sounds (channel 0) and an 8 KB
one for the short (channel 1). On the map, now and then, a drip, a chain,
a far footfall, off to one side. The SIDs play where there is no sample.

**Memory** (bank 0 is tight). The resident part (video, keys, the UI,
sound, the tables) ends near $8300. The code overlays `walk` (explore +
view), `battle`, `gen` (the dungeon generator) and `scenes` (chest, idol,
title, endings) are kept in attic RAM and copied by DMA into one region at
$9800 when called. The game needs the MEGA65's attic RAM (every
production MEGA65 and xemu have it). Things lying about are a short list, sight a
window round the player, memory a byte a place.

| Where | What |
|---|---|
| $2001-$82FF | the program, its data and variables |
| $9800-$BBFF | the code overlay region |
| $BC00-$BFFF | the stack |
| $12000-$197FF | DG<NAME>B: a close-up's last characters |
| $19800-$1F7FF | the sounds' play slots (16 KB, 8 KB) |
| $40000-$4FFFF | DGTILES: tile palette, tiles, frame and bar characters |
| $50000-$5DFFF | DG<NAME>A: a close-up's palette, maps, characters |
| $5E000-$5FF3F | the screen |
| $8000000- | attic RAM: DUNGEON.OV1, the overlays |
| $8080000- | attic RAM: the sounds, decoded |
| $8100000- | attic RAM: the close-ups seen so far, 128 KB each |
