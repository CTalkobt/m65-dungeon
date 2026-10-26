# Dungeon — a torchlit descent for the MEGA65
#
#   make              build build/dungeon.d81 with disk45: the program (DUNGEON),
#                     its code overlays (DUNGEON.OV1) and its art (DGTILES, the monsters DGMON0-n, DGCHEST, DGIDOL)
#   make run          build the disk image and boot it in xemu-xmega65
#   make screenshot   run it headless, save build/screenshot.png
#   make art          only the art (build/art/*.bin, previews in build/art/preview/)
#   make shot KEYS=.. a build that plays those keys, then build/shot.png
#   make clean        remove build/
#
# Variables: M65 (the M65Compiler tree, default ../m65compiler), XEMU,
# SEED (a fixed dungeon seed, e.g. make SEED=1234), SHOT_DELAY (seconds),
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
DEFS    ?=
ifdef SEED
DEFS    += -DSEED=$(SEED)
endif

CFLAGS  := -O2 -I$(M65)/lib/include -Isrc -I$(ART) $(DEFS)

TARGET  := $(BUILD)/dungeon.prg
# the code overlays (walk, battle, gen, scenes), written by ln45 with the
# program and kept at the top of bank 1 (bank 5 holds the close-ups)
OVL     := $(BUILD)/dungeon.ov1
OVL_STORE := 0x16000-0x1F7FF
C_MODS  := main video load rng game dungeon view ui explore battle detail front scene sfx
OBJS    := $(C_MODS:%=$(BUILD)/%.o45) $(BUILD)/ld.o45 $(BUILD)/assets.o45
HEADERS := $(wildcard src/*.h) $(ART)/assets.h

# the art: one file a monster, the tiles, the chest and the idol
ART_STAMP := $(ART)/art.stamp
DISK    := $(BUILD)/dungeon.d81

.PHONY: all run disk art screenshot shot clean

all: $(DISK)

$(BUILD):
	mkdir -p $@

$(ART_STAMP) $(ART)/assets.h &: $(wildcard tools/*.py)
	mkdir -p $(ART)
	$(PYTHON) tools/gen_art.py $(ART)
	touch $(ART_STAMP)

art: $(ART_STAMP)

$(BUILD)/%.o45: src/%.c $(HEADERS) | $(BUILD)
	$(CC45) $(CFLAGS) -c $< -o $@

$(BUILD)/ld.o45: src/load.s45 | $(BUILD)
	$(CA45) -c $< -o $@

$(TARGET) $(OVL) &: $(OBJS) $(STDLIB)
	$(LN45) -basic --overlay-store=$(OVL_STORE) -M $(BUILD)/dungeon.map -o $(TARGET) $(OBJS) $(STDLIB)

$(STDLIB):
	$(MAKE) -C $(M65) lib

disk: $(DISK)

# the program first (-autoload runs the first program), then the art under
# the names the program loads it by (build/files.txt, from gen_art.py)
$(DISK): $(TARGET) $(OVL) $(ART_STAMP)
	rm -f $@
	$(DISK45) create $@ -n "DUNGEON" -i "DG"
	$(DISK45) add $@ $(TARGET) DUNGEON
	$(DISK45) add $@ $(OVL) DUNGEON.OV1
	while read f n; do $(DISK45) add $@ $(ART)/$$f $$n || exit 1; done < $(ART)/files.txt

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
