/* assets.h — made by tools/gen_art.py: the art's tables */
#pragma once

/* the terrain's tiles (16 colours): characters TCHAR0 + kind * 4 + quarter */
#define TK_ROCK 0
#define TK_FLOOR0 1
#define TK_FLOOR1 2
#define TK_FLOOR2 3
#define TK_FLOOR3 4
#define TK_CORR0 5
#define TK_CORR1 6
#define TK_WALL0 7
#define TK_WALL1 8
#define TK_WALL2 9
#define TK_WALL3 10
#define TK_WALL4 11
#define TK_WALL5 12
#define TK_WALL6 13
#define TK_WALL7 14
#define TK_WALL8 15
#define TK_WALL9 16
#define TK_WALL10 17
#define TK_WALL11 18
#define TK_WALL12 19
#define TK_WALL13 20
#define TK_WALL14 21
#define TK_WALL15 22
#define TK_DOOR_H 23
#define TK_DOOR_V 24
#define TK_DOOR_OPEN_H 25
#define TK_DOOR_OPEN_V 26
#define TK_STAIRS 27
#define TK_DOOR_LOCKED_H 28
#define TK_DOOR_LOCKED_V 29
#define TK_SECRET_H 30
#define TK_SECRET_V 31
#define TK_TRAP 32
#define TK_RUBBLE 33
#define TK_CRACK_NS 34
#define TK_CRACK_EW 35
#define TK_COUNT 36
#define TCHAR0 0x1010
#define PAL_MAP 128           /* the map's palette banks (8): 128-255 of DGTILES' palette */

/* the full-colour tiles (tile_char[kind][variant]: the first of 4 characters, 0 none) */
#define T_CHEST 0
#define T_CHEST_OPEN 1
#define T_IDOL 2
#define T_IDOL_SPENT 3
#define T_POTION 4
#define T_GOLD 5
#define T_BONES 6
#define T_BRAZIER 7
#define T_BRAZIER_COLD 8
#define T_SCROLL 9
#define T_KEY 10
#define T_DRAUGHT 11
#define T_RAGS 12
#define T_STICKS 13
#define T_FUR 14
#define T_CANDLE 15
#define T_FLINT 16
#define T_FIRESTICK 17
#define T_ARROWS 18
#define T_SHOT 19
#define T_MERCHANT 20
#define T_FOUNTAIN 21
#define T_FOUNTAIN_DRY 22
#define T_SHELF 23
#define T_OIL 24
#define T_FOOD 25
#define T_FLAMES 26
#define T_FLASK1 27
#define T_FLASK2 28
#define T_FLASK3 29
#define T_FLASK4 30
#define T_PLAYER 31
#define T_PLAYER_DARK 32
#define T_AIM 33
#define T_MISSILE 34
#define T_RAT 35
#define T_SKELETON 36
#define T_SPIDER 37
#define T_SLIME 38
#define T_ORC 39
#define T_WRAITH 40
#define T_LICH 41
#define T_BATS 42
#define T_GHOUL 43
#define T_EMBER 44
#define T_NOTE1 45
#define T_NOTE2 46
#define T_NOTE3 47
#define T_NOTE4 48
#define T_COUNT 49
#define PAL_FIRE 120          /* the fire's palette entries: 4 from here */
#define TV_EYES 4

/* (tables kept in attic RAM, out of bank 0: cc45 __bank, loaded at the start) */
#define NAMES_BANK 0x834
extern __bank(NAMES_BANK) const unsigned int tile_char[49][5];

/* the frame and bar characters */
#define UI_F_H 0x13A8
#define UI_F_V 0x13A9
#define UI_F_TL 0x13AA
#define UI_F_TR 0x13AB
#define UI_F_BL 0x13AC
#define UI_F_BR 0x13AD
#define UI_F_T 0x13AE
#define UI_F_B 0x13AF
#define UI_BAR_R0 0x13B0
#define UI_BAR_R1 0x13B1
#define UI_BAR_R2 0x13B2
#define UI_BAR_R3 0x13B3
#define UI_BAR_R4 0x13B4
#define UI_BAR_R5 0x13B5
#define UI_BAR_R6 0x13B6
#define UI_BAR_R7 0x13B7
#define UI_BAR_R8 0x13B8
#define UI_BAR_B0 0x13B9
#define UI_BAR_B1 0x13BA
#define UI_BAR_B2 0x13BB
#define UI_BAR_B3 0x13BC
#define UI_BAR_B4 0x13BD
#define UI_BAR_B5 0x13BE
#define UI_BAR_B6 0x13BF
#define UI_BAR_B7 0x13C0
#define UI_BAR_B8 0x13C1
#define UI_BAR_G0 0x13C2
#define UI_BAR_G1 0x13C3
#define UI_BAR_G2 0x13C4
#define UI_BAR_G3 0x13C5
#define UI_BAR_G4 0x13C6
#define UI_BAR_G5 0x13C7
#define UI_BAR_G6 0x13C8
#define UI_BAR_G7 0x13C9
#define UI_BAR_G8 0x13CA
#define UI_MAP_FLOOR 0x13CB
#define UI_MAP_CORR 0x13CC
#define UI_MAP_WALL 0x13CD
#define UI_MAP_DOOR 0x13CE
#define UI_MAP_OPEN 0x13CF
#define UI_MAP_LOCKED 0x13D0
#define UI_MAP_STAIRS 0x13D1
#define UI_MAP_TRAP 0x13D2
#define UI_MAP_RUBBLE 0x13D3
#define UI_MAP_PLAYER 0x13D4
#define UI_MAP_CURSOR 0x13D5

/* the close-ups: DG<NAME>A (bank 5) and DG<NAME>B (bank 1) */
#define S_RAT 0
#define S_SKELETON 1
#define S_SPIDER 2
#define S_SLIME 3
#define S_ORC 4
#define S_WRAITH 5
#define S_LICH 6
#define S_BATS 7
#define S_CHEST 8
#define S_IDOL 9
#define S_COUNT 10
#define SCENE_W 56
#define SCENE_H 30
#define SCENE_HDR_SIZE 256      /* hw.h SCENE_HDR: the header, then the maps */

/* the creatures (tools/monsters.py): their fight is in their close-up's header */
#define M_RAT 0
#define M_SKELETON 1
#define M_SPIDER 2
#define M_SLIME 3
#define M_ORC 4
#define M_WRAITH 5
#define M_LICH 6
#define M_BATS 7
#define M_MIMIC 8
#define M_GHOUL 9
#define M_EMBER 10
#define M_KINDS 11
#define M_BOSS 6          /* waits in the last level's last room */
#define A_NONE 0
#define A_MELEE 1
#define A_HEAVY 2
#define A_GUARD 3
#define A_POISON 4
#define A_WEB 5
#define A_SPIT 6
#define A_REGEN 7
#define A_ROAR 8
#define A_DRAIN 9
#define A_CHILL 10
#define A_PHASE 11
#define A_BOLT 12
#define A_CURSE 13
#define A_LUNGE 14
#define A_CLOSE 15
#define MF_FIREWEAK 0x01
#define MF_COLDPROOF 0x02
#define MF_HARDFLEE 0x04
#define MF_VENOM 0x08
#define MF_SLOW 0x10
#define MF_BOSS 0x20
#define MF_UNDEAD 0x40
#define MH_ARCHER 0x01
#define MH_THIEF 0x02
#define MH_EATER 0x04
#define MH_FLIT 0x08
#define MH_MIMIC 0x10
#define MH_DARK 0x20
#define SPAWN 8
extern const unsigned char mon_tile[M_KINDS], mon_scene[M_KINDS], mon_flags[M_KINDS];
/* for a shot on the map (the rest is in the close-up): life, armour, experience, names (nth) */
extern const unsigned char mon_hp[M_KINDS], mon_def[M_KINDS], mon_xp[M_KINDS], mon_habits[M_KINDS];
extern __bank(NAMES_BANK) const char mon_names[];
extern const unsigned char level_mons[5][SPAWN];
extern const char *const scene_file[10];
/* a creature fought in another's close-up: the file of its header (0: its own) */
extern const char *const mon_hdr_file[M_KINDS];
/* where the packed files load: from where they unpack to */
#define TILES_SKIP 0xA450
/* DGLOOK (loaded into bank 5, unpacked to LOOK_LIN): each depth's stone, LOOK_SIZE
 * bytes: the characters of the kinds from TK_CORR0 on, then the map's palette banks */
#define LOOK_SIZE 8320
/* DGBACK (into bank 5): two packed streams, the second this far in */
#define BACK_STREAM2 0x1410
extern const unsigned int scene_skip[10][2];
