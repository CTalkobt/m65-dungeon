#!/usr/bin/env python3
"""gen_art.py <build dir> — every art file the game loads, and the C side of it

Writes, in the build directory:
Every .bin is packed (pack.py) and unpacks in place where it says:
  dgtiles.bin   DGTILES: bank 4 ($40000): the palette (entries 16-127 set),
                then the tiles (4 characters each: top left, top right,
                bottom left, bottom right) and the frame and bar characters
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
FIRE0 = 120                    # the tiles' fire: 4 palette entries the program turns round
FIRE_RAMP = [(150, 30, 4), (235, 100, 14), (255, 190, 50), (255, 240, 160)]
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


def build_tiles(out):
    """the tiles and frame characters in one palette; the C tables"""
    kinds = tiles.kinds()
    ui = tiles.ui_chars()
    imgs = []
    for name, vs in kinds:
        for v in vs:
            if v is not None:
                imgs.append(np.clip(v, 0, 1))
    uimgs = [np.clip(c, 0, 1) for n, c in ui]

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

    mosaic = np.concatenate([unfired(i).reshape(-1, 3) for i in imgs + uimgs])[None, ...]
    pal, _ = quantize(to8(mosaic), FIRE0 - 16, dither=False)
    full = np.zeros((256, 3), np.uint8)
    full[16:16 + len(pal)] = pal
    for k, rgb in enumerate(FIRE_RAMP):
        full[FIRE0 + k] = rgb

    def idx(img):
        f = fire_slots(img)
        i = remap(to8(unfired(img)), pal, dither=False) + 16
        return np.where(f >= 0, FIRE0 + f, i)

    chars = []          # 64-byte characters
    seen = {}           # a tile's 4 characters (bytes) -> its first character's index

    def add_tile(img):
        i = idx(img)
        q = [i[y:y + 8, x:x + 8].astype(np.uint8).tobytes() for y in (0, 8) for x in (0, 8)]
        key = b"".join(q)
        if key not in seen:
            seen[key] = len(chars)
            chars.extend(q)
        return seen[key]

    char0 = (TILES_LIN + PAL_BYTES) // 64
    table = []
    for name, vs in kinds:
        row = []
        for v in vs:
            row.append(0 if v is None else char0 + add_tile(np.clip(v, 0, 1)))
        table.append((name, row))
    ui_nums = []
    for name, c in ui:
        i = idx(np.clip(c, 0, 1)).astype(np.uint8).tobytes()
        ui_nums.append((name, char0 + len(chars)))
        chars.append(i)
    data = pal_bytes(full) + b"".join(chars)
    assert TILES_LIN + len(data) <= 0x50000, "the tiles overflow bank 4 (%d bytes)" % len(data)
    skip, packed = write(os.path.join(out, "dgtiles.bin"), TILES_LIN, data, 0x50000)
    print("DGTILES: %d tiles, %d characters, %d bytes free in bank 4; packed %d -> %d"
          % (len(seen), len(chars), 0x10000 - len(data), len(data), packed))
    return table, ui_nums, skip


def build_scene(out, name, pv):
    """a close-up's two files; its map and characters"""
    frames, pal = scenes.render(name)
    blks = [blocks(idx) for _, idx in frames]
    hdr_end = PAL_BYTES + 64 + FRAMES * SW * SH * 2
    assert hdr_end % 64 == 0
    na = (SCENE_A_END - SCENE_A - hdr_end) // 64
    nb = (SCENE_B_END - SCENE_B) // 64 - 1          # (room for the packed file's last bytes)
    uniq, maps = shrink_charset(blks, pal, min(na + nb, SCENE_CHARS))
    nums = np.where(np.arange(len(uniq)) < na,
                    (SCENE_A + hdr_end) // 64 + np.arange(len(uniq)),
                    SCENE_B // 64 + np.arange(len(uniq)) - na)
    hdr = bytearray(64)
    hdr[0], hdr[1], hdr[2] = FRAMES, SW, SH
    mp = b""
    for m in maps:
        mp += np.asarray(nums[m], "<u2").tobytes()
    a = pal_bytes(pal) + bytes(hdr) + mp + uniq[:na].tobytes()
    b = uniq[na:].tobytes()
    base = "dg%s" % name
    skip_a, pa = write(os.path.join(out, base + "a.bin"), SCENE_A, a, 0x60000)   # (a few bytes into the screen: it is dark then)
    skip_b, pb = write(os.path.join(out, base + "b.bin"), SCENE_B, b if b else bytes(64), SCENE_B_END)
    # the preview: what the screen will show, after the merging
    for f, m in enumerate(maps):
        img = uniq[m].reshape(SH, SW, 8, 8).transpose(0, 2, 1, 3).reshape(H, W)
        Image.fromarray(pal[img]).resize((W * 2, H * 2), Image.NEAREST).save(
            os.path.join(pv, "%s_%d.png" % (name, f)))
    print("%-9s %4d characters (%d unique before merging), %6d bytes, packed %6d"
          % (name, len(uniq), len(np.unique(np.concatenate(blks), axis=0)), len(a) + len(b), pa + pb))
    return base, skip_a, skip_b


def main():
    out = sys.argv[1]
    pv = os.path.join(out, "preview")
    os.makedirs(pv, exist_ok=True)
    table, ui_nums, tiles_skip = build_tiles(out)
    files = [("dgtiles.bin", "DGTILES")]
    names = list(scenes.SCENES)
    skips = []
    for name in names:
        base, sa, sb = build_scene(out, name, pv)
        skips.append((sa, sb))
        files += [(base + "a.bin", base.upper() + "A"), (base + "b.bin", base.upper() + "B")]
    with open(os.path.join(out, "files.txt"), "w") as f:
        for fn, n in files:
            f.write("%s %s\n" % (fn, n))
    # the C side
    nk = len(table)
    with open(os.path.join(out, "assets.h"), "w") as f:
        f.write("/* assets.h — made by tools/gen_art.py: the art's tables */\n#pragma once\n\n")
        f.write("/* tile kinds (tile_char[kind][variant]: the first of 4 characters, 0 none) */\n")
        for i, (name, _) in enumerate(table):
            f.write("#define T_%s %d\n" % (name, i))
        f.write("#define T_COUNT %d\n" % nk)
        f.write("#define PAL_FIRE %d          /* the fire's palette entries: 4 from here */\n" % FIRE0)
        f.write("#define TV_EYES %d\n#define TV_MEM %d\n\n" % (tiles.V_EYES, 4))
        f.write("extern const unsigned int tile_char[%d][%d];\n\n" % (nk, tiles.NV))
        f.write("/* the frame and bar characters */\n")
        for name, n in ui_nums:
            f.write("#define UI_%s 0x%04X\n" % (name, n))
        f.write("\n/* the close-ups: DG<NAME>A (bank 5) and DG<NAME>B (bank 1) */\n")
        for i, name in enumerate(names):
            f.write("#define S_%s %d\n" % (name.upper(), i))
        f.write("#define S_COUNT %d\n" % len(names))
        f.write("#define SCENE_W %d\n#define SCENE_H %d\n" % (SW, SH))
        f.write("extern const char *const scene_file[%d];\n" % len(names))
        f.write("/* where the packed files load: from where they unpack to */\n")
        f.write("#define TILES_SKIP 0x%04X\n" % tiles_skip)
        f.write("extern const unsigned int scene_skip[%d][2];\n" % len(names))
    with open(os.path.join(out, "assets.c"), "w") as f:
        f.write("/* assets.c — made by tools/gen_art.py */\n#pragma encoding ascii\n#include \"assets.h\"\n\n")
        f.write("const unsigned int tile_char[%d][%d] = {\n" % (nk, tiles.NV))
        for name, row in table:
            f.write("    {%s},   /* %s */\n" % (", ".join("0x%04X" % v for v in row), name))
        f.write("};\n\n")
        f.write("const char *const scene_file[%d] = {%s};\n"
                % (len(names), ", ".join('"DG%s"' % n.upper() for n in names)))
        f.write("const unsigned int scene_skip[%d][2] = {%s};\n"
                % (len(names), ", ".join("{0x%04X, 0x%04X}" % k for k in skips)))


if __name__ == "__main__":
    main()
