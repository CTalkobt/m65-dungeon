#!/usr/bin/env python3
"""gen_art.py <build dir> — every art file the game loads, and the C side of it

Writes, in the build directory:
Every .bin is packed (pack.py) and unpacks in place where it says:
  dgtiles.bin   DGTILES: bank 4 ($40000): the palette (entries 16-127: the
                full-colour tiles; 128-255: the map's banks), then from
                $40400 the terrain's 16-colour tiles, the full-colour tiles
                (4 characters each: top left, top right, bottom left,
                bottom right) and the frame and bar characters
  dg<name>a.bin DG<NAME>A: bank 5 ($50000): a close-up's palette (128-254
                set), a header (frames, width, height in characters), each
                frame's map (a character number a cell), its first characters
  dg<name>b.bin DG<NAME>B: bank 1 ($12000): the rest of its characters
  files.txt     the files and their names on the disk (for the Makefile)
  assets.h/.c   the tile kinds, their characters, the scenes
  preview/      each close-up as the machine will show it
Each .bin starts with its load address in its bank (2 bytes): the place
in the area it unpacks to from which it can unpack in place.
"""
import os
import sys
import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tiles  # noqa: E402
import backdrop  # noqa: E402
import monsters  # noqa: E402
import scenes  # noqa: E402
from artlib import to8, quantize, remap, blocks, shrink_charset  # noqa: E402
from pack import pack, unpack_in_place  # noqa: E402
from stage import W, H  # noqa: E402

PAL_BYTES = 768
TILES_LIN = 0x40000
SCENE_A = 0x50000
SCENE_A_END = 0x5E000          # the screen is above
SCENE_B = 0x12000
SCENE_B_END = 0x19800          # the sounds' play slots above (src/audio.c)
FRAMES = 2
HDR = 256                      # a close-up's header (after its palette): what the creature is
DEPTHS = 5
SPAWN = 8                      # level_mons: the creatures a level draws from, 8 a level
FIRE0 = 120                    # the tiles' fire: 4 palette entries the program turns round
FIRE_RAMP = [(150, 30, 4), (235, 100, 14), (255, 190, 50), (255, 240, 160)]
PAL_MAP = 128                  # the map's palette banks: 8 + bank (gen_art map_palette)
MAP_GENERAL, MAP_EDGE = 11, 3  # a terrain tile's colour numbers: 1-11 the stone, 12-14 its edges
TCHAR0 = 0x1010                # the terrain's first character ($40400): src/vmap.s45 knows it
MAP_LIN = 0x4A000              # the level's map (hw.h MAP_LIN): the tiles end below
BACK1 = 0x2A000                # the backdrop's characters: in the C64 BASIC's space
BACK2 = 0x2E000                #   and the C64 KERNAL's (8 KB each)
BACK_HALF = 128
SCENE_CHARS = 1641             # a close-up's characters at most (bank 5 and bank 1 hold fewer)
SW, SH = W // 8, H // 8


def pal_bytes(pal):
    """256 x 3 to 256 red, 256 green, 256 blue"""
    return bytes(pal[:, 0]) + bytes(pal[:, 1]) + bytes(pal[:, 2])


def write(path, dest, data, limit):
    """data packed (pack.py) to unpack at dest, the file loading where it
    can be unpacked in place; the stream must end below limit. The offset
    it loads at from dest (the program needs it)."""
    stream, skip = pack(data, dest)
    assert unpack_in_place(stream, skip, len(data)) == bytes(data), path
    at = dest + skip
    assert at + len(stream) <= limit, "%s: %X past %X" % (path, at + len(stream), limit)
    with open(path, "wb") as f:
        f.write(bytes([at & 0xFF, (at >> 8) & 0xFF]) + stream)
    return skip, len(stream)


def ncm_tile(nib):
    """a 16 x 16 tile of colour numbers (0-15) as its 4 16-colour characters
    (top left, top right, bottom left, bottom right), each shown 8 pixels
    wide (the colour RAM trims it): a row is 4 bytes of two pixels, the
    left one in the low nibble, then 4 bytes not shown"""
    out = []
    for y0 in (0, 8):
        for x0 in (0, 8):
            c = bytearray()
            for y in range(8):
                r = nib[y0 + y, x0:x0 + 8]
                c += bytes(int(r[k]) | int(r[k + 1]) << 4 for k in range(0, 8, 2)) + bytes(4)
            out.append(bytes(c))
    return out


def map_palette(terrain):
    """the map's 16-colour tiles' colours (MAP_GENERAL for the stone, the
    wood, the iron; MAP_EDGE for the edges), the tiles as colour numbers
    (1-14; 0 black), and the palette banks (8 x 16 entries: 128-255)"""
    gen = np.concatenate([img[~edge] for name, img, edge in terrain if name != "ROCK"])
    edg = np.concatenate([img[edge] for name, img, edge in terrain if name != "ROCK"])
    gpal, _ = quantize(to8(np.clip(gen, 0, 1))[None], MAP_GENERAL, dither=False)
    epal, _ = quantize(to8(np.clip(edg, 0, 1))[None], MAP_EDGE, dither=False)
    nibs = []
    for name, img, edge in terrain:
        if name == "ROCK":
            nibs.append(np.zeros(img.shape[:2], np.int32))
            continue
        i8 = to8(np.clip(img, 0, 1))
        g = remap(i8, gpal, dither=False) + 1
        e = remap(i8, epal, dither=False) + 1 + MAP_GENERAL
        nibs.append(np.where(edge, e, g))
    cols = np.zeros((15, 3), np.float32)               # colour number -> RGB (0..1)
    cols[1:1 + len(gpal)] = gpal / 255.0
    cols[1 + MAP_GENERAL:1 + MAP_GENERAL + len(epal)] = epal / 255.0
    banks = np.zeros((128, 3), np.float32)
    for lv in range(4):                                 # MAP_LIT + lv: the torch's levels
        banks[lv * 16 + 1:lv * 16 + 15] = cols[1:] * tiles.LEVELS[lv] * np.array(tiles.TINTS[lv], np.float32)
    for m in range(3):                                  # MAP_MEM + m: remembered, only the edges
        b = (4 + m) * 16
        banks[b + 1 + MAP_GENERAL:b + 15] = tiles.MEM_COL * tiles.MEMS[m]
    return cols, nibs, to8(np.clip(banks, 0, 1))      # (bank 7, MAP_DARK: all black)


def build_tiles(out):
    """the tiles and frame characters; the map's palette banks; the C tables"""
    terrain = tiles.terrain_kinds()
    cols, nibs, banks = map_palette(terrain)
    floor_seen = cols[nibs[1]]                          # FLOOR0 as the map shows it, under the sprites
    kinds = tiles.sprite_kinds(floor_seen)
    ui = tiles.ui_chars()
    imgs = []
    for name, vs in kinds:
        for v in vs:
            if v is not None:
                imgs.append(np.clip(v, 0, 1))
    uimgs = [np.clip(c, 0, 1) for n, c in ui]
    bimgs = [np.clip(c, 0, 1) for c in backdrop.chars()]

    def fire_slots(img):
        """which pixels are fire, and which slot (-1 none)"""
        f = np.full(img.shape[:2], -1, np.int32)
        for k, rgb in enumerate(tiles.FIRE_RGB):
            f[np.all(img == rgb, axis=-1)] = k
        return f

    def unfired(img):
        img = img.copy()
        img[fire_slots(img) >= 0] = 0
        return img

    mosaic = np.concatenate([unfired(i).reshape(-1, 3) for i in imgs + uimgs + bimgs])[None, ...]
    pal, _ = quantize(to8(mosaic), FIRE0 - 16, dither=False)
    full = np.zeros((256, 3), np.uint8)
    full[16:16 + len(pal)] = pal
    for k, rgb in enumerate(FIRE_RAMP):
        full[FIRE0 + k] = rgb
    full[PAL_MAP:] = banks

    def idx(img):
        f = fire_slots(img)
        i = remap(to8(unfired(img)), pal, dither=False) + 16
        return np.where(f >= 0, FIRE0 + f, i)

    # the terrain's characters first, at TCHAR0: TCHAR0 + kind * 4 + quarter
    chars = []
    for nib in nibs:
        chars.extend(ncm_tile(nib))
    seen = {}           # a tile's 4 characters (bytes) -> its first character's index

    def add_tile(img):
        i = idx(img)
        q = [i[y:y + 8, x:x + 8].astype(np.uint8).tobytes() for y in (0, 8) for x in (0, 8)]
        key = b"".join(q)
        if key not in seen:
            seen[key] = len(chars)
            chars.extend(q)
        return seen[key]

    table = []
    for name, vs in kinds:
        row = []
        for v in vs:
            row.append(0 if v is None else TCHAR0 + add_tile(np.clip(v, 0, 1)))
        table.append((name, row))
    ui_nums = []
    for name, c in ui:
        i = idx(np.clip(c, 0, 1)).astype(np.uint8).tobytes()
        ui_nums.append((name, TCHAR0 + len(chars)))
        chars.append(i)
    head = pal_bytes(full)
    head += bytes(TCHAR0 * 64 - TILES_LIN - len(head))
    data = head + b"".join(chars)
    assert TILES_LIN + len(data) <= MAP_LIN, "the tiles reach the map (%d bytes)" % len(data)
    skip, packed = write(os.path.join(out, "dgtiles.bin"), TILES_LIN, data, 0x50000)
    print("DGTILES: %d terrain tiles, %d tiles, %d characters, %d bytes before the map; packed %d -> %d"
          % (len(terrain), len(seen), len(chars), MAP_LIN - TILES_LIN - len(data), len(data), packed))
    back = [idx(c).astype(np.uint8) for c in bimgs]
    tk = [name for name, _, _ in terrain]
    return table, tk, ui_nums, skip, back, full


def back_char(i):
    """the character number of backdrop character i: the first half in the
    C64 BASIC's space, the second in the C64 KERNAL's"""
    return BACK1 // 64 + i if i < BACK_HALF else BACK2 // 64 + i - BACK_HALF


def build_backdrop(out, back):
    """DGBACK: the backdrop's characters, two packed streams (unpacked into
    the C64 ROM's space, not in place: the file loads into bank 5); the
    picture as indices, and each cell's character (0x20 black)"""
    data = b"".join(c.tobytes() for c in back)
    s1, _ = pack(data[:BACK_HALF * 64], BACK1)
    s2, _ = pack(data[BACK_HALF * 64:], BACK2)
    with open(os.path.join(out, "dgback.bin"), "wb") as f:
        f.write(b"\x00\x00" + s1 + s2)                  # loads at $50000
    cells = backdrop.cells()
    img = np.zeros((H, W), np.int32)
    nums = np.full((SH, SW), 0x20, np.int64)
    for cy in range(SH):
        for cx in range(SW):
            k = cells[cy, cx]
            if k >= 0:
                img[cy * 8:cy * 8 + 8, cx * 8:cx * 8 + 8] = back[k]
                nums[cy, cx] = back_char(k)
    print("DGBACK: %d characters, packed %d -> %d" % (len(back), len(data), len(s1) + len(s2)))
    return len(s1), img, nums


def header(name):
    """a close-up's header (HDR bytes): 0 frames, 1-2 its size in characters;
    for a creature 3 life, 4-6 its blow (dice, sides, plus), 7 armour, 8
    experience, 9 flags (MF_*), 10-13 its actions (A_*), 14-17 their
    weights; from 32 its words, each ending in a NUL: name, what it does
    before a blow, as the blow lands, its lore, its death"""
    h = bytearray(HDR)
    h[0], h[1], h[2] = FRAMES, SW, SH
    mon = [m for m in monsters.MONSTERS if m[0] == name]
    if mon:
        i = mon[0][2]
        h[3:10] = bytes([i["hp"], i["dice"], i["sides"], i["plus"], i["defense"], i["xp"], flags_of(i)])
        for k, (a, w) in enumerate(i["acts"]):
            h[10 + k] = monsters.ACTIONS.index(a)
            h[14 + k] = w
        words = b"".join(i[k].encode() + b"\0" for k in ("name", "tele", "hit", "lore", "death"))
        assert len(words) <= HDR - 32, name
        h[32:32 + len(words)] = words
    return h


def flags_of(i):
    return sum(1 << monsters.FLAGS.index(f) for f in i["flags"])


def spawns():
    """level_mons: for each level SPAWN creatures to draw from, as their
    weights there say"""
    out = []
    for d in range(1, DEPTHS + 1):
        ws = [(k, m[2]["depths"].get(d, 0)) for k, m in enumerate(monsters.MONSTERS)]
        tot = sum(w for k, w in ws) or 1
        row = []
        for k, w in ws:
            row += [k] * round(w * SPAWN / tot)
        row = (row + row[:1] * SPAWN)[:SPAWN] if row else [0] * SPAWN
        out.append(row)
    return out


def build_scene(out, name, pv, back_img, back_nums, tile_pal):
    """a close-up's two files; its map and characters: its own only where
    the creature (its shadow, its glow) changes the shared backdrop"""
    frames, pal = scenes.render(name, tile_pal[back_img].astype(np.float32) / 255)
    comb = pal.copy()                            # (the backdrop's colours too, for the merging)
    comb[16:128] = tile_pal[16:128]
    owned, blks = [], []
    for _, idx in frames:
        own = (idx >= 0).reshape(SH, 8, SW, 8).any(axis=(1, 3)).reshape(-1)
        b = blocks(np.where(idx >= 0, idx, back_img))
        owned.append(own)
        blks.append(b[own])
    hdr_end = PAL_BYTES + HDR + FRAMES * SW * SH * 2
    assert hdr_end % 64 == 0
    na = (SCENE_A_END - SCENE_A - hdr_end) // 64
    nb = (SCENE_B_END - SCENE_B) // 64 - 1          # (room for the packed file's last bytes)
    uniq, maps = shrink_charset(blks, comb, min(na + nb, SCENE_CHARS))
    nums = np.where(np.arange(len(uniq)) < na,
                    (SCENE_A + hdr_end) // 64 + np.arange(len(uniq)),
                    SCENE_B // 64 + np.arange(len(uniq)) - na)
    hdr = header(name)
    mp = b""
    full_maps = []
    for own, m in zip(owned, maps):
        cell = back_nums.reshape(-1).copy()
        cell[own] = nums[m]
        full_maps.append((own, m))
        mp += np.asarray(cell, "<u2").tobytes()
    a = pal_bytes(pal) + bytes(hdr) + mp + uniq[:na].tobytes()
    b = uniq[na:].tobytes()
    base = "dg%s" % name
    skip_a, pa = write(os.path.join(out, base + "a.bin"), SCENE_A, a, 0x60000)   # (a few bytes into the screen: it is dark then)
    skip_b, pb = write(os.path.join(out, base + "b.bin"), SCENE_B, b if b else bytes(64), SCENE_B_END)
    # the preview: what the screen will show, after the merging
    for f, (own, m) in enumerate(full_maps):
        bb = blocks(back_img).copy()
        bb[own] = uniq[m]
        img = bb.reshape(SH, SW, 8, 8).transpose(0, 2, 1, 3).reshape(H, W)
        Image.fromarray(comb[img]).resize((W * 2, H * 2), Image.NEAREST).save(
            os.path.join(pv, "%s_%d.png" % (name, f)))
    print("%-9s %4d cells its own, %4d characters (%d unique before merging), %6d bytes, packed %6d"
          % (name, max(o.sum() for o in owned), len(uniq), len(np.unique(np.concatenate(blks), axis=0)),
             len(a) + len(b), pa + pb))
    return base, skip_a, skip_b


def main():
    out = sys.argv[1]
    pv = os.path.join(out, "preview")
    os.makedirs(pv, exist_ok=True)
    table, terrain, ui_nums, tiles_skip, back, tile_pal = build_tiles(out)
    back_stream2, back_img, back_nums = build_backdrop(out, back)
    files = [("dgtiles.bin", "DGTILES"), ("dgback.bin", "DGBACK")]
    names = list(scenes.SCENES)
    skips = []
    for name in names:
        base, sa, sb = build_scene(out, name, pv, back_img, back_nums, tile_pal)
        skips.append((sa, sb))
        files += [(base + "a.bin", base.upper() + "A"), (base + "b.bin", base.upper() + "B")]
    with open(os.path.join(out, "files.txt"), "w") as f:
        for fn, n in files:
            f.write("%s %s\n" % (fn, n))
    # the C side
    nk = len(table)
    with open(os.path.join(out, "assets.h"), "w") as f:
        f.write("/* assets.h — made by tools/gen_art.py: the art's tables */\n#pragma once\n\n")
        f.write("/* the terrain's tiles (16 colours): characters TCHAR0 + kind * 4 + quarter */\n")
        for i, name in enumerate(terrain):
            f.write("#define TK_%s %d\n" % (name, i))
        f.write("#define TK_COUNT %d\n#define TCHAR0 0x%04X\n" % (len(terrain), TCHAR0))
        f.write("#define PAL_MAP %d           /* the map's palette banks (8): 128-255 of DGTILES' palette */\n" % PAL_MAP)
        f.write("\n/* the full-colour tiles (tile_char[kind][variant]: the first of 4 characters, 0 none) */\n")
        for i, (name, _) in enumerate(table):
            f.write("#define T_%s %d\n" % (name, i))
        f.write("#define T_COUNT %d\n" % nk)
        f.write("#define PAL_FIRE %d          /* the fire's palette entries: 4 from here */\n" % FIRE0)
        f.write("#define TV_EYES %d\n\n" % tiles.V_EYES)
        f.write("extern const unsigned int tile_char[%d][%d];\n\n" % (nk, tiles.NV))
        f.write("/* the frame and bar characters */\n")
        for name, n in ui_nums:
            f.write("#define UI_%s 0x%04X\n" % (name, n))
        f.write("\n/* the close-ups: DG<NAME>A (bank 5) and DG<NAME>B (bank 1) */\n")
        for i, name in enumerate(names):
            f.write("#define S_%s %d\n" % (name.upper(), i))
        f.write("#define S_COUNT %d\n" % len(names))
        f.write("#define SCENE_W %d\n#define SCENE_H %d\n" % (SW, SH))
        f.write("#define SCENE_HDR_SIZE %d      /* hw.h SCENE_HDR: the header, then the maps */\n" % HDR)
        f.write("\n/* the creatures (tools/monsters.py): their fight is in their close-up's header */\n")
        for k, m in enumerate(monsters.MONSTERS):
            f.write("#define M_%s %d\n" % (m[0].upper(), k))
        f.write("#define M_KINDS %d\n" % len(monsters.MONSTERS))
        f.write("#define M_BOSS %d          /* waits in the last level's last room */\n"
                % [k for k, m in enumerate(monsters.MONSTERS) if "BOSS" in m[2]["flags"]][0])
        assert all(m[2]["hp"] < 255 for m in monsters.MONSTERS), "a life of 255 is FRESH"

        for k, a in enumerate(monsters.ACTIONS):
            f.write("#define A_%s %d\n" % (a, k))
        for k, fl in enumerate(monsters.FLAGS):
            f.write("#define MF_%s 0x%02X\n" % (fl, 1 << k))
        for k, h in enumerate(monsters.HABITS):
            f.write("#define MH_%s 0x%02X\n" % (h, 1 << k))
        f.write("#define SPAWN %d\n" % SPAWN)
        f.write("extern const unsigned char mon_tile[M_KINDS], mon_scene[M_KINDS], mon_flags[M_KINDS];\n")
        f.write("/* for a shot on the map (the rest is in the close-up): life, armour, experience, names (nth) */\n")
        f.write("extern const unsigned char mon_hp[M_KINDS], mon_def[M_KINDS], mon_xp[M_KINDS], mon_habits[M_KINDS];\n")
        f.write("extern const char mon_names[];\n")
        f.write("extern const unsigned char level_mons[%d][SPAWN];\n" % DEPTHS)
        f.write("extern const char *const scene_file[%d];\n" % len(names))
        f.write("/* where the packed files load: from where they unpack to */\n")
        f.write("#define TILES_SKIP 0x%04X\n" % tiles_skip)
        f.write("/* DGBACK (into bank 5): two packed streams, the second this far in */\n")
        f.write("#define BACK_STREAM2 0x%04X\n" % back_stream2)
        f.write("extern const unsigned int scene_skip[%d][2];\n" % len(names))
    with open(os.path.join(out, "assets.c"), "w") as f:
        f.write("/* assets.c — made by tools/gen_art.py */\n#pragma encoding ascii\n#include \"assets.h\"\n\n")
        f.write("const unsigned int tile_char[%d][%d] = {\n" % (nk, tiles.NV))
        for name, row in table:
            f.write("    {%s},   /* %s */\n" % (", ".join("0x%04X" % v for v in row), name))
        f.write("};\n\n")
        f.write("const char *const scene_file[%d] = {%s};\n"
                % (len(names), ", ".join('"DG%s"' % n.upper() for n in names)))
        f.write("const unsigned char mon_tile[M_KINDS] = {%s};\n"
                % ", ".join("T_%s" % m[0].upper() for m in monsters.MONSTERS))
        f.write("const unsigned char mon_scene[M_KINDS] = {%s};\n"
                % ", ".join("S_%s" % m[0].upper() for m in monsters.MONSTERS))
        f.write("const unsigned char mon_flags[M_KINDS] = {%s};\n"
                % ", ".join("0x%02X" % flags_of(m[2]) for m in monsters.MONSTERS))
        for t, key in (("mon_hp", "hp"), ("mon_def", "defense"), ("mon_xp", "xp")):
            f.write("const unsigned char %s[M_KINDS] = {%s};\n"
                    % (t, ", ".join(str(m[2][key]) for m in monsters.MONSTERS)))
        f.write("const unsigned char mon_habits[M_KINDS] = {%s};\n"
                % ", ".join("0x%02X" % sum(1 << monsters.HABITS.index(h) for h in m[2]["habits"])
                            for m in monsters.MONSTERS))
        f.write("const char mon_names[] = \"%s\";\n" % "\\0".join(m[2]["name"] for m in monsters.MONSTERS))
        f.write("const unsigned char level_mons[%d][SPAWN] = {%s};\n"
                % (DEPTHS, ", ".join("{%s}" % ", ".join(str(k) for k in row) for row in spawns())))
        f.write("const unsigned int scene_skip[%d][2] = {%s};\n"
                % (len(names), ", ".join("{0x%04X, 0x%04X}" % k for k in skips)))


if __name__ == "__main__":
    main()
