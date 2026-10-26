# Dungeon — a torchlit descent for the MEGA65
#
#   make              build build/dungeon.d81 with disk45: the program (DUNGEON),
#                     its code overlays (DUNGEON.OV1) and its art (DGTILES, the monsters DGMON0-n, DGCHEST, DGIDOL)
#   make run          build the disk image and boot it in xemu-xmega65
#   make screenshot   run it headless, save build/screenshot.png
#   make art          only the art (build/art/*.bin, previews in build/art/preview/)
#   make sounds       assets/baseimage.d81 from the sound packs (SFX_DIR)
#   make shot KEYS=.. a build that plays those keys, then build/shot.png
#   make clean        remove build/
#
# Variables: M65 (the M65Compiler tree, default ../m65compiler), XEMU,
# SEED (a fixed dungeon seed, e.g. make SEED=1234), SHOT_DELAY (seconds),
# SFX_DIR (the sound packs' zip files for make sounds, default
# ~/resources/audio/dungeon),
# DEFS (extra -D options).

M65     ?= ../m65compiler
CC45    := $(M65)/bin/cc45
CA45    := $(M65)/bin/ca45
LN45    := $(M65)/bin/ln45
DISK45  := $(M65)/bin/disk45
STDLIB  := $(M65)/lib/build/c45.lib
XEMU    ?= xemu-xmega65
PYTHON  ?= python3

BUILD   ?= build
ART     := build/art
SFX     := build/sfx
MUSIC   := build/music
TUNE45  := $(M65)/bin/tune45
# the sampled sounds: on assets/baseimage.d81 (DGSFX1, 2, ...), which
# make sounds rebuilds from the sound packs' zip files in SFX_DIR; without
# the image the game has its SID effects only
BASEIMAGE := assets/baseimage.d81
SFX_DIR ?= $(HOME)/resources/audio/dungeon
DEFS    ?=
ifdef SEED
DEFS    += -DSEED=$(SEED)
endif

CFLAGS  := -O2 -I$(M65)/lib/include -Isrc -I$(ART) -I$(SFX) -I$(MUSIC) $(DEFS)

TARGET  := $(BUILD)/dungeon.prg
# the code overlays (walk, battle, gen, scenes), written by ln45 with the
# program and kept in attic RAM (loaded through bank 5 at the start)
OVL     := $(BUILD)/dungeon.ov1
OVL_STORE := 0x8000000-0x800FFFF
C_MODS  := main video load rng game dungeon view ui explore battle detail front scene sfx audio audio_boot
OBJS    := $(C_MODS:%=$(BUILD)/%.o45) $(BUILD)/ld.o45 $(BUILD)/assets.o45 $(BUILD)/sfxdata.o45 $(BUILD)/seen.o45
HEADERS := $(wildcard src/*.h) $(ART)/assets.h $(SFX)/sfxdata.h $(MUSIC)/music.h

# the art: one file a monster, the tiles, the chest and the idol
ART_STAMP := $(ART)/art.stamp
SFX_STAMP := $(SFX)/sfx.stamp
DISK    := $(BUILD)/dungeon.d81

.PHONY: all run disk art sounds screenshot shot clean

all: $(DISK)

$(BUILD):
	mkdir -p $@

$(ART_STAMP) $(ART)/assets.h &: $(wildcard tools/*.py)
	mkdir -p $(ART)
	$(PYTHON) tools/gen_art.py $(ART)
	touch $(ART_STAMP)

art: $(ART_STAMP)

$(SFX_STAMP) $(SFX)/sfxdata.h &: tools/gen_sfx.py $(wildcard $(BASEIMAGE))
	rm -rf $(SFX)
	mkdir -p $(SFX)/disk
	if [ -f $(BASEIMAGE) ]; then $(DISK45) extract-all $(BASEIMAGE) $(SFX)/disk "DGSFX*"; fi
	$(PYTHON) tools/gen_sfx.py tables $(SFX)/disk $(SFX)
	touch $(SFX_STAMP)

# the sound packs to the base image (only with the packs: see assets/README.md)
sounds:
	rm -rf build/sounds
	$(PYTHON) tools/gen_sfx.py pack $(SFX_DIR) build/sounds
	rm -f $(BASEIMAGE)
	$(DISK45) create $(BASEIMAGE) -n "DUNGEON SOUNDS" -i "DS"
	for f in build/sounds/dgsfx*.bin; do n=$$(basename $$f .bin | tr a-z A-Z); $(DISK45) add $(BASEIMAGE) $$f $$n || exit 1; done

# the music: tools/gen_music.py writes it as tune45's text, tune45 makes
# it the file DGMUSIC (the program keeps it in attic RAM)
$(MUSIC)/music.txt: tools/gen_music.py
	mkdir -p $(MUSIC)
	$(PYTHON) tools/gen_music.py $@

$(MUSIC)/dgmusic.bin $(MUSIC)/music.h &: $(MUSIC)/music.txt
	$(TUNE45) convert $< --bin --load-at 0 -o $(MUSIC)/dgmusic.bin --header $(MUSIC)/music.h

$(BUILD)/sfxdata.o45: $(SFX_STAMP) | $(BUILD)
	$(CC45) $(CFLAGS) -c $(SFX)/sfxdata.c -o $@

$(BUILD)/%.o45: src/%.c $(HEADERS) | $(BUILD)
	$(CC45) $(CFLAGS) -c $< -o $@

$(BUILD)/seen.o45: src/seen.s45 | $(BUILD)
	$(CA45) -c $< -o $@

$(BUILD)/ld.o45: src/load.s45 | $(BUILD)
	$(CA45) -c $< -o $@

$(TARGET) $(OVL) &: $(OBJS) $(STDLIB)
	$(LN45) -basic --overlay-store=$(OVL_STORE) -M $(BUILD)/dungeon.map -o $(TARGET) $(OBJS) $(STDLIB)

$(STDLIB):
	$(MAKE) -C $(M65) lib

disk: $(DISK)

# the program first (-autoload runs the first program), then the art under
# the names the program loads it by (build/files.txt, from gen_art.py)
$(DISK): $(TARGET) $(OVL) $(ART_STAMP) $(SFX_STAMP) $(MUSIC)/dgmusic.bin
	rm -f $@
	$(DISK45) create $@ -n "DUNGEON" -i "DG"
	$(DISK45) add $@ $(TARGET) DUNGEON
	$(DISK45) add $@ $(OVL) DUNGEON.OV1
	$(DISK45) add $@ $(MUSIC)/dgmusic.bin DGMUSIC
	while read f n; do $(DISK45) add $@ $(ART)/$$f $$n || exit 1; done < $(ART)/files.txt
	while read f n; do $(DISK45) add $@ $$f $$n || exit 1; done < $(SFX)/files.txt

run: $(DISK)
	$(XEMU) -8 $(abspath $(DISK)) -autoload

screenshot: $(DISK)
	AUTOLOAD=1 $(PYTHON) tools/screenshot.py $(TARGET) $(BUILD)/screenshot.png "$(SHOT_DELAY)" $(DISK)

clean:
	rm -rf build

$(BUILD)/assets.o45: $(ART_STAMP) | $(BUILD)
	$(CC45) $(CFLAGS) -c $(ART)/assets.c -o $@

# a build that plays the keys KEYS (with TEST: more -D options, e.g.
# TEST=-DTEST_FIGHT=4 a fight with an orc at once, TEST=-DTEST_VIEW=1 a chest) (ui.c: U D L R the cursor keys, F
# RETURN, X RUN/STOP, _ a pause), then a screenshot: build/shot.png
shot:
	rm -f build/shot/*.o45
	$(MAKE) BUILD=build/shot DEFS='$(DEFS) $(TEST) -DSCRIPT=\"$(KEYS)\"' disk
	AUTOLOAD=1 $(PYTHON) tools/screenshot.py build/shot/dungeon.prg build/shot.png "$(SHOT_DELAY)" build/shot/dungeon.d81
