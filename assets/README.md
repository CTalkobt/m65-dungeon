# assets

## baseimage.d81

A D81 disk image holding the game's sampled sounds, the files `DGSFX1` to
`DGSFX8`. `make` takes them off this image and puts them on
the game's disk (`build/dungeon.d81`), so the game can be built without
the sound packs they were made from.

They are not the packs' files. `tools/gen_sfx.py` made them (`make sounds`):
each sound was cut to the part the game uses, resampled to 12 kHz mono,
normalised and coded in the game's own format (blocks of 16 samples: a
predictor and a shift a block and a 4-bit residue a sample), which
`src/audio.c` decodes into attic RAM when the game starts. The format of
the files is described in `tools/gen_sfx.py`.

### Where the sounds come from

The sounds are from these packs, all royalty-free for use in games with no
attribution required. Their licences do not allow the packs' files to be
passed on by themselves; get the packs from their authors:

- moneywithjj, *Free Fantasy RPG Sounds*: itch.io, <https://moneywithjjcom.itch.io/free-fantasy-sounds>
- moneywithjj, *Free Roguelike Sounds*: itch.io, <https://moneywithjjcom.itch.io/free-roguelike-sounds>
- moneywithjj, *Free Lost Temple Sounds*: itch.io, <https://moneywithjjcom.itch.io/free-lost-temple-sounds>
- NimbeKit, *Fantasy RPG: Dungeon & Magic SFX 160* (from the *Fantasy RPG Complete Collection*), NimbeKit Studio (licence: NimbeKit Game Asset License v1.0)

The moneywithjj sounds were made with ElevenLabs Sound Effects under the
author's commercial subscription (their README says so).

| Sound | Pack | File in the pack |
|---|---|---|
| STEP | NimbeKit | `fr_movement_bare_stone_step.wav` |
| DOOR | moneywithjj | `hidden-latch-click.wav` |
| COIN | NimbeKit | `fr_loot_single_coin.wav` |
| SWING | NimbeKit | `fr_combat_sword_swing.wav` |
| HIT | moneywithjj | `axe-hit-flesh.wav` |
| HURT | NimbeKit | `fr_combat_cloth_armor_hit.wav` |
| CLANG | NimbeKit | `fr_combat_parry_spark.wav` |
| HEAVY | NimbeKit | `fr_combat_heavy_attack.wav` |
| FIRE | NimbeKit | `fr_magic_ember_cast.wav` |
| FROST | NimbeKit | `fr_magic_frost_shards.wav` |
| HEAL | NimbeKit | `fr_magic_restoration.wav` |
| WARD | NimbeKit | `fr_magic_ward_place.wav` |
| POISON | moneywithjj | `poison-tick.wav` |
| CURSE | moneywithjj | `curse-apply.wav` |
| DRINK | NimbeKit | `fr_loot_potion_drink.wav` |
| VICTORY | NimbeKit | `fr_feedback_combat_victory.wav` |
| LEVEL | NimbeKit | `fr_feedback_level_up.wav` |
| DEFEAT | NimbeKit | `fr_feedback_combat_defeat.wav` |
| GROWL | moneywithjj | `dragon-idle.wav` |
| CHEST | moneywithjj | `chest-open.wav` |
| BELL | NimbeKit | `fr_ambience_ancient_bell.wav` |
| TRAP | moneywithjj | `dart-hits-wood.wav` |
| STIR | moneywithjj | `bat-screech.wav` |
| WEDGE | NimbeKit | `fr_dungeon_wooden_bar.wav` |
| BATTER | NimbeKit | `fr_combat_wood_shield_block.wav` |
| STAB | NimbeKit | `fr_combat_dagger_stab.wav` |
| RITE | NimbeKit | `fr_magic_summon_wisp.wav` |
| NUMB | moneywithjj | `freeze-apply.wav` |
| DRIP1 | NimbeKit | `fr_ambience_water_drop_small.wav` |
| DRIP2 | NimbeKit | `fr_ambience_water_drop_deep.wav` |
| CHAIN | NimbeKit | `fr_ambience_distant_chain.wav` |
| FOOT | NimbeKit | `fr_ambience_far_footfall.wav` |
| EMBER | NimbeKit | `fr_ambience_ember_pop.wav` |

### Licensing

These sounds are not covered by this project's own licence: each remains
under the terms of the pack it came from (above). They are here, made
into the game's own format, so that the game can be built; they are not
offered as a sound library. Please get the packs from their authors to
use the sounds in anything else.

For any licensing concern about these sounds, contact the author of this
game, Craig Taylor (CTalkobt): <https://github.com/CTalkobt>. If a
licensor objects, the sounds will be replaced or removed.

### Remaking the image

You don't need to: `assets/baseimage.d81` is in the repository and `make`
uses it as it is. (Without it, the game builds with its SID sound effects
only.) To make it anew, from the packs themselves, get these zip files
from their authors and put them, as downloaded and not unpacked, in
`~/resources/audio/dungeon` (or any folder, given as `SFX_DIR`):

| Zip file | Pack | Sounds taken |
|---|---|---|
| `NimbeKit_Fantasy_RPG_Collection.zip` | NimbeKit Studio, *Fantasy RPG: Dungeon & Magic Complete Collection* v1.0 (a paid pack; the sounds are read from the `NimbeKit_Fantasy_RPG_SFX_160.zip` inside it) | 24 |
| `free-fantasy-sounds-audio.zip` | moneywithjj, *Free Fantasy RPG Sounds* | 3 |
| `free-roguelike-sounds-audio.zip` | moneywithjj, *Free Roguelike Sounds* | 3 |
| `free-lost-temple-sounds-audio.zip` | moneywithjj, *Free Lost Temple Sounds* | 3 |

```sh
make sounds                                 # assets/baseimage.d81 anew
make sounds SFX_DIR=~/Downloads/dungeon-sfx # the zips elsewhere
make
```

`tools/gen_sfx.py` (`SOUNDS`) says, for each sound, which zip and file
it comes from, where it starts, how long it is and its gain.

To change a sound, edit `SOUNDS` in `tools/gen_sfx.py` (the file, where to
start, how long, the gain and the play slot) and run `make sounds`.
