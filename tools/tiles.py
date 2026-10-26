"""tiles.py — the exploration map's tiles (16 x 16: 2 x 2 characters) and
the frame and bar characters, in RGB (0..1); gen_art.py makes them
characters.

The terrain (terrain_kinds: rock, floors, walls, doors, stairs) is drawn
once into the level's map and never changes with the light: its
characters are 16-colour (NCM), and the colour RAM picks the palette bank
they are shown in (gen_art.py): lit at one of the torch's LEVELS, or
remembered (MEMS: only the pixels marked as edges show, as cool lines: the
shape of the place), or not at all. So each terrain tile is an image and
its edge pixels.

The things on it (sprite_kinds: what lies about, the creatures, the
player) are full-colour characters in variants (C's tile_char[kind][v]):
  0-3  in sight, lit by the torch from bright (0) to dim (3)
  4    in shadow: only what shines by itself (creatures' eyes)
A variant a kind does not have is None (the program shows the terrain).
"""
import numpy as np
from artlib import fbm

T = 16
LEVELS = [1.0, 0.66, 0.42, 0.24]
TINTS = [(1.06, 0.96, 0.80), (1.0, 0.9, 0.78), (0.9, 0.86, 0.84), (0.72, 0.78, 0.95)]
MEMS = [0.62, 0.38, 0.2]
MEM_COL = np.array([0.42, 0.52, 0.8], np.float32)
NV = 5
V_EYES = 4

# ---------------------------------------------------------------- pixel art

LEGEND = {
    'k': (20, 14, 12), 'w': (235, 230, 220), 'b': (214, 204, 176), 'B': (150, 138, 110),
    'r': (200, 40, 30), 'R': (110, 20, 16), 'o': (245, 140, 40), 'y': (255, 225, 100),
    'Y': (220, 170, 60), 'd': (150, 100, 30), 'n': (130, 86, 48), 'N': (76, 48, 26),
    'g': (96, 176, 64), 'G': (44, 96, 34), 'l': (170, 240, 120), 's': (176, 176, 186),
    'S': (92, 92, 104), 'p': (140, 64, 170), 'P': (72, 30, 92), 'u': (66, 76, 124),
    'U': (34, 38, 64), 'm': (224, 174, 134), 'M': (164, 112, 82), 'f': (128, 106, 90),
    'F': (84, 68, 58), 't': (150, 150, 130), 'T': (100, 100, 86), 'h': (54, 38, 32),
    # these shine by themselves: not dimmed, and seen in the shadows
    'e': (255, 70, 40), 'c': (160, 225, 255), 'v': (225, 150, 255), 'z': (190, 255, 120),
    'Z': (255, 200, 60),
    # fire: four palette slots the program turns round (gen_art.py FIRE0..)
    '1': (255, 0, 255), '2': (255, 0, 254), '3': (255, 0, 253), '4': (255, 0, 252),
}
GLOWING = set("ecvzZ1234")
FIRE_RGB = [np.array(LEGEND[c], np.float32) / 255 for c in "1234"]

ART = {
    "player": [
        "................",
        "......kkkk..yy..",
        ".....ksssSkyoy..",
        "....kssSSSSkoy..",
        "....kSkmmkSk.o..",
        "....kSmmmmSkkn..",
        ".....kmmmmk.kn..",
        "...kkuukkuukmn..",
        "..kuuuuuuuuukn..",
        "..kuuyuuuuyuk...",
        "..kmkuuuuuukk...",
        "...k.kuuuuuk....",
        ".....kUUkUUk....",
        ".....knNkNnk....",
        ".....kNNkNNk....",
        "......kk.kk.....",
    ],
    "rat": [
        "................",
        "................",
        "................",
        "................",
        "................",
        "..kk............",
        ".kffk...kkkkk...",
        "kfffkkkkfffffk..",
        "kfefffffffffffk.",
        "kffffffffffffFk.",
        ".kmkfFfffffFFFkk",
        "..kkkFFFFFFFFk.m",
        "...kmkkmkkmkkm.m",
        "....k..k..k..mm.",
        "................",
        "................",
    ],
    "skeleton": [
        "......kkkk......",
        ".....kbbbbk.....",
        "....kbbbbbbk....",
        "....kbebbebk....",
        ".....kbbbbk...s.",
        "......kwwk....s.",
        "....kkbbbbkk..s.",
        "...kbkbkkbkbk.s.",
        "...kbkbbbbkbkkS.",
        "...kbkbkkbkbkbk.",
        "...kbkbbbbk.kb..",
        "....kkbbbbk.....",
        ".....kbkkbk.....",
        ".....kbk.kbk....",
        ".....kbk.kbk....",
        "....kbbk.kbbk...",
    ],
    "spider": [
        "................",
        "..F..........F..",
        "...F..kkkk..F...",
        "F...FkFFFFkF...F",
        ".F..kFFrrFFk..F.",
        "..FFkFFrrFFkFF..",
        "....kFFFFFFk....",
        "FFF..kFFFFk..FFF",
        "...FFkFFFFkFF...",
        "..F..kfzzfk..F..",
        ".F..kfzfzzfk..F.",
        "F...kfffffffk..F",
        "...F.kffffk.F...",
        "..F...kSSk...F..",
        ".F...........F..",
        "................",
    ],
    "slime": [
        "................",
        "................",
        "................",
        "................",
        "......kkkk......",
        "....kklllgkk....",
        "...klllgggggk...",
        "...klkzkgkzkgk..",
        "..klgkkgggkkggk.",
        "..klggggggggggk.",
        ".klggGggggGgggk.",
        ".kgggGgggggggGgk",
        ".kgGGggggGgggGGk",
        "kgGGGGGGGGGGGGGk",
        ".kkkkkkkkkkkkkk.",
        "................",
    ],
    "orc": [
        "................",
        "....kkkkkk......",
        "...khhhhhhk.....",
        "..kgggggggGk....",
        "..kgZkgggZkk..n.",
        "..kgggggggGk..n.",
        "..kgwkkkkwGk.nn.",
        ".kkkggggggkkknn.",
        "kSSSkgggggkgkn..",
        "kSSgggggggggkn..",
        "kgkgnnnnnngkk...",
        "kgknnnYnnnnkgk..",
        ".k.knnnnnnk.k...",
        "...kNNk.kNNk....",
        "...kNNk.kNNk....",
        "..kkkkk.kkkkk...",
    ],
    "wraith": [
        "......kkkk......",
        ".....kuuuuk.....",
        "....kuUUUUuk....",
        "....kukckcuk....",
        "....kuUkkUuk....",
        "...kuuuUUuuuk...",
        "..kuuuuUuuuuuk..",
        ".ktkuuuUuuuuktk.",
        ".ktkuuuUUuuuktk.",
        "...kuuuuUuuuk...",
        "...kuuuuUuuuk...",
        "..kuuuuuUuuuuk..",
        "..kuUuUuUuUuuk..",
        "..kU.kU.kU.kUk..",
        "..k...k..k...k..",
        "................",
    ],
    "lich": [
        "....Y.Y.Y.Y.....",
        "....kYYYYYk.....",
        "...kbbbbbbbk....",
        "...kbvkbkvbk....",
        "...kbbbkbbbk..v.",
        "....kbwwwbk..vvv",
        "...kPPPPPPPk..n.",
        "..kPpppYpppPk.n.",
        ".kPppppYppppPkn.",
        ".kbkpppYpppkbkn.",
        "..kkpppYpppk..n.",
        "...kpppYpppk..n.",
        "...kpppYpppk..n.",
        "..kppppYppppk.n.",
        "..kYYYYYYYYYk.n.",
        "..kkkkkkkkkkk...",
    ],
    "chest": [
        "................",
        "................",
        "................",
        "................",
        "...kkkkkkkkkk...",
        "..knnnnnnnnnnk..",
        "..knNnnnnnnNnk..",
        "..kYYYYYYYYYYk..",
        "..knNnnYYnnNnk..",
        "..knNnnkknnNnk..",
        "..knNnnnnnnNnk..",
        "..kYYYYYYYYYYk..",
        "..kNNNNNNNNNNk..",
        "...kkkkkkkkkk...",
        "................",
        "................",
    ],
    "chest_open": [
        "................",
        "...kkkkkkkkkk...",
        "..kNNNNNNNNNNk..",
        "..knnnnnnnnnnk..",
        "..kkkkkkkkkkkk..",
        "..kNNNNNNNNNNk..",
        "..kNhhhhhhhhNk..",
        "..kYYYYYYYYYYk..",
        "..knNnnnnnnNnk..",
        "..knNnnnnnnNnk..",
        "..knNnnnnnnNnk..",
        "..kYYYYYYYYYYk..",
        "..kNNNNNNNNNNk..",
        "...kkkkkkkkkk...",
        "................",
        "................",
    ],
    "idol": [
        "................",
        "...k........k...",
        "...tk.kkkk.kt...",
        "....tktttttkt...",
        "....kttzttztk...",
        "....kttttttk....",
        "..kttktttkttk...",
        ".kt.ktttttk.tk..",
        "....kttttttk....",
        "...kttttttttk...",
        "..kttTttttTttk..",
        "..kkkkkkkkkkkk..",
        "..kSSSSSSSSSSk..",
        "..kTTTTTTTTTTk..",
        "..kkkkkkkkkkkk..",
        "................",
    ],
    "potion": [
        "................",
        "................",
        "................",
        "................",
        "......kkk.......",
        "......knk.......",
        "......kwk.......",
        ".....kwwwk......",
        "....krrrrrk.....",
        "...krrwrrrRk....",
        "...krwrrrrRk....",
        "...krrrrrRRk....",
        "....kRRRRRk.....",
        ".....kkkkk......",
        "................",
        "................",
    ],
    "gold": [
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "......kkk.......",
        ".....kyYYk......",
        "....kkkYYkkk....",
        "...kyYYkkyYYk...",
        "..kyYYYdkYYYdk..",
        "..kkdddkkddddk..",
        "..kyYYkyYYYk.k..",
        "...kddkkddk.....",
        "................",
        "................",
    ],
    "bones": [
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "...kk......kk...",
        "..kbbkkkkkkbbk..",
        "...kkbbbbbbkk...",
        "...kk..kkk.kk...",
        "..kbbk.kbbk.....",
        "...kk.kbbbbk....",
        "......kbkkbk....",
        ".......kkkk.....",
        "................",
    ],
    "brazier": [
        "................",
        ".......Z........",
        "......ZoZ..Z....",
        "....Z.oZo.Zo....",
        "....oZoZZoZo....",
        "...ZoZZZZZZoZ...",
        "...kSSSSSSSSk...",
        "...kSssssssSk...",
        "....kSSSSSSk....",
        ".....kSSSSk.....",
        "......kSSk......",
        "......kSSk......",
        ".....kSSSSk.....",
        "....kSSSSSSk....",
        "....kkkkkkkk....",
        "................",
    ],
}
ART["idol_spent"] = [r.replace('z', 'T') for r in ART["idol"]]


def _fire(name, rows, cols, letters):
    """the flame's pixels to the fire slots, scattered so that turning the
    slots round makes it flicker"""
    art = [list(r) for r in ART[name]]
    for y in rows:
        for x in cols:
            if art[y][x] in letters:
                art[y][x] = "1234"[(x * 3 + y * 5 + (art[y][x] == 'o') * 2) % 4]
    ART[name] = ["".join(r) for r in art]


_fire("brazier", range(1, 6), range(16), "Zo")
_fire("player", range(1, 5), range(10, 16), "yo")


def sprite(name):
    """the art as RGB (16 x 16 x 3, 0..1), its mask and its glowing pixels"""
    rows = ART[name]
    assert len(rows) == T and all(len(r) == T for r in rows), name
    img = np.zeros((T, T, 3), np.float32)
    mask = np.zeros((T, T), bool)
    glow = np.zeros((T, T), bool)
    for y, r in enumerate(rows):
        for x, ch in enumerate(r):
            if ch != '.':
                img[y, x] = np.array(LEGEND[ch], np.float32) / 255
                mask[y, x] = True
                glow[y, x] = ch in GLOWING
    return img, mask, glow


# ---------------------------------------------------------------- textures

_ys, _xs = np.mgrid[0:T, 0:T].astype(np.float32)


def floor(seed):
    """flagstones: a few stones a tile, uneven, mortar between"""
    rng = np.random.RandomState(seed)
    img = np.zeros((T, T, 3), np.float32)
    split = rng.randint(6, 11)
    shift = rng.randint(4, 12)
    x2 = (shift + 7) % 16 or 8
    stone = np.where(_ys < split, np.where(_xs < shift, 0, 1), np.where(_xs < x2, 2, 3))
    shade = np.array([rng.uniform(0.75, 1.05) for _ in range(4)], np.float32)[stone]
    grain = fbm(_xs * 0.35 + seed * 9, _ys * 0.35, 3, seed)
    base = np.array([0.46, 0.41, 0.36], np.float32)
    img = base * (shade * (0.78 + grain * 0.4))[..., None]
    # mortar on the stones' edges (and the tile's, so the tiles join)
    edge = np.zeros((T, T), bool)
    edge[:, 0] = edge[0, :] = True
    edge[split, :] = True
    edge[:split, shift] = True
    edge[split:, x2] = True
    img[edge] *= 0.45
    # a crack now and then
    if rng.rand() < 0.5:
        x, y = rng.randint(2, 14), rng.randint(2, 14)
        for _ in range(5):
            img[y, x] *= 0.5
            x = min(15, max(0, x + rng.randint(-1, 2)))
            y = min(15, max(0, y + 1))
    return img


def corridor(seed):
    """rough-hewn rock, darker, gravel"""
    g = fbm(_xs * 0.4 + seed * 13, _ys * 0.4, 4, seed + 40)
    pebble = fbm(_xs * 1.3 + seed, _ys * 1.3, 2, seed + 41) > 0.72
    base = np.array([0.36, 0.33, 0.3], np.float32)
    img = base * (0.7 + g * 0.5)[..., None]
    img[pebble] *= 1.35
    return img


def rock_top():
    """the top of the rock a wall is cut in: dark, rough"""
    g = fbm(_xs * 0.3, _ys * 0.3, 4, 60)
    return np.array([0.2, 0.18, 0.17], np.float32) * (0.6 + g * 0.7)[..., None]


def wall(mask):
    """mask: floor to the N 1, E 2, S 4, W 8. The rock's top, and where
    floor is to the south the wall's face: courses of blocks, lit. Its
    edges: the lines that catch the light on the sides toward the floor"""
    img = rock_top()
    edge = np.zeros((T, T), bool)
    face = 7
    if mask & 4:
        y0 = T - face
        for y in range(y0, T):
            course = (y - y0) // 3
            for x in range(T):
                joint = (x + course * 4 + 1) % 8 == 0 or (y - y0) % 3 == 2
                v = 0.5 + 0.18 * float(fbm(np.float32(x * 0.5), np.float32(y * 0.5 + course), 2, 61))
                c = np.array([0.62, 0.55, 0.48], np.float32) * v * (1.1 - (y - y0) * 0.05)
                img[y, x] = c * (0.45 if joint else 1.0)
        img[y0, :] = img[y0, :] * 1.35          # the top edge catches the light
        img[y0 - 1, :] *= 0.6
        edge[y0:y0 + 2, :] = True
    top = T - (face if mask & 4 else 0)
    if mask & 1:
        img[0, :] = img[0, :] * 2.2
        img[1, :] = img[1, :] * 1.5
        edge[0:2, :] = True
    if mask & 8:
        img[:top, 0] *= 2.0
        img[:top, 1] *= 1.4
        edge[:top, 0:2] = True
    if mask & 2:
        img[:top, T - 1] *= 1.6
        img[:top, T - 2] *= 1.2
        edge[:top, T - 2:T] = True
    return img, edge


def door(vertical, open_):
    """a door in a gap of the wall: planks and iron, or swung open. Its
    edges: the iron bands (the door swung back, when open)"""
    img = floor(5)
    edge = np.zeros((T, T), bool)
    if open_:
        if vertical:
            img[:, 0:3] = np.array([0.42, 0.27, 0.14], np.float32)
            img[:, 3] *= 0.4
            edge[:, 0:2] = True
        else:
            img[0:3, :] = np.array([0.42, 0.27, 0.14], np.float32)
            img[3, :] *= 0.4
            edge[0:2, :] = True
        return img, edge
    wood = np.array([0.5, 0.32, 0.17], np.float32)
    g = fbm(_xs * (0.2 if vertical else 1.0), _ys * (1.0 if vertical else 0.2), 3, 70)
    img = wood * (0.75 + g * 0.5)[..., None]
    if vertical:
        for x in (4, 8, 12):
            img[:, x] *= 0.55
        img[3:5, :] = np.array([0.35, 0.35, 0.38], np.float32)
        img[11:13, :] = np.array([0.35, 0.35, 0.38], np.float32)
        img[7:9, 10:12] = np.array([0.8, 0.7, 0.3], np.float32)
        edge[3:5, :] = edge[11:13, :] = True
    else:
        for y in (4, 8, 12):
            img[y, :] *= 0.55
        img[:, 3:5] = np.array([0.35, 0.35, 0.38], np.float32)
        img[:, 11:13] = np.array([0.35, 0.35, 0.38], np.float32)
        img[10:12, 7:9] = np.array([0.8, 0.7, 0.3], np.float32)
        edge[:, 3:5] = edge[:, 11:13] = True
    img[0, :] *= 0.4
    img[:, 0] *= 0.4
    return img, edge


def stairs():
    """steps going down into the dark, left to right; their edges: each
    step's lip and the stairwell's sides"""
    img = floor(9)
    edge = np.zeros((T, T), bool)
    for k in range(5):
        x0 = 2 + k * 3
        v = 0.75 - k * 0.15
        img[2:14, x0:x0 + 3] = np.array([0.5, 0.45, 0.4], np.float32) * v
        img[2:14, x0] *= 1.5 if k < 3 else 1.0
        edge[3:13, x0] = True
    img[2, 2:17] *= 0.5
    img[13, 2:17] *= 0.5
    edge[2, 2:T] = edge[13, 2:T] = True
    return img, edge


# ---------------------------------------------------------------- the kinds

def lit(img, level, glow=None):
    out = img * LEVELS[level] * np.array(TINTS[level], np.float32)
    if glow is not None:
        out[glow] = img[glow]
    return out


def terrain_kinds():
    """[(name, image, edge pixels)] in the order of the C enum (TK_*): the
    map's 16-colour tiles, unlit (the palette banks light them)"""
    none = np.zeros((T, T), bool)
    out = [("ROCK", np.zeros((T, T, 3), np.float32), none)]
    for k in range(4):
        out.append(("FLOOR%d" % k, floor(k + 1), none))
    for k in range(2):
        out.append(("CORR%d" % k, corridor(k), none))
    for m in range(16):
        out.append(("WALL%d" % m,) + wall(m))
    for name, v, o in (("DOOR_H", 0, 0), ("DOOR_V", 1, 0), ("DOOR_OPEN_H", 0, 1), ("DOOR_OPEN_V", 1, 1)):
        out.append((name,) + door(v, o))
    out.append(("STAIRS",) + stairs())
    return out


SPRITES = ("chest", "chest_open", "idol", "idol_spent", "potion", "gold", "bones", "brazier",
           "player", "rat", "skeleton", "spider", "slime", "orc", "wraith", "lich")


def sprite_kinds(base):
    """[(name, [variant images or None] * NV)] in the order of the C enum
    (T_*): the things, the creatures and the player, full colour, each on
    base (the floor as the map shows it, so they sit on it)"""
    out = []
    for name in SPRITES:
        img, mask, glow = sprite(name)
        comp = np.where(mask[..., None], img, base)
        # a soft shadow under it, down and to the right
        sh = np.zeros((T, T), bool)
        sh[1:, 1:] = mask[:-1, :-1]
        comp = np.where((sh & ~mask)[..., None], comp * 0.55, comp)
        v = [lit(comp, l, glow) for l in range(4)]
        creature = name in ("rat", "skeleton", "spider", "slime", "orc", "wraith", "lich")
        if creature and glow.any():
            eyes = np.zeros((T, T, 3), np.float32)
            eyes[glow] = img[glow] * 0.8
            v.append(eyes)
        else:
            v.append(None)
        if name == "brazier":
            v = [lit(comp, 0, glow)] * 4 + [None]      # it lights itself
        out.append((name.upper(), v))
    return out


# ---------------------------------------------------------------- frame and bars (8 x 8)

def ui_chars():
    """[(name, 8 x 8 RGB)]: the panels' frame, the bars, small icons"""
    out = []
    gold = np.array([0.85, 0.65, 0.28], np.float32)
    dgold = np.array([0.45, 0.3, 0.12], np.float32)
    iron = np.array([0.22, 0.2, 0.22], np.float32)

    def blank():
        return np.zeros((8, 8, 3), np.float32)

    # the frame: an iron band with a gold line, corners with a rivet
    h = blank()
    h[2:6, :] = iron
    h[3, :] = gold
    h[4, :] = dgold
    out.append(("F_H", h))
    v = h.transpose(1, 0, 2).copy()
    out.append(("F_V", v))
    for name, (fy, fx) in (("F_TL", (0, 0)), ("F_TR", (0, 1)), ("F_BL", (1, 0)), ("F_BR", (1, 1))):
        c = blank()
        ys = slice(2, 8) if fy == 0 else slice(0, 6)
        xs = slice(2, 8) if fx == 0 else slice(0, 6)
        c[2:6, xs] = iron
        c[ys, 2:6] = iron
        c[3, xs] = gold
        c[4, xs] = dgold
        c[ys, 3] = gold
        c[ys, 4] = dgold
        c[2:6, 2:6] = iron
        c[3:5, 3:5] = np.array([1.0, 0.85, 0.45], np.float32)
        out.append((name, c))
    # the junctions where a divider meets the frame
    t = h.copy()
    t[4:8, 2:6] = iron
    t[4:8, 3] = gold
    t[4:8, 4] = dgold
    out.append(("F_T", t))
    b = h.copy()
    b[0:4, 2:6] = iron
    b[0:4, 3] = gold
    b[0:4, 4] = dgold
    out.append(("F_B", b))
    # bars: 0..8 pixels full, red (life), blue (mana), grey-red (the foe)
    for name, full, empty in (("BAR_R", (0.85, 0.12, 0.08), (0.18, 0.04, 0.03)),
                              ("BAR_B", (0.25, 0.45, 0.95), (0.05, 0.08, 0.2)),
                              ("BAR_G", (0.85, 0.6, 0.2), (0.15, 0.1, 0.05))):
        for k in range(9):
            c = blank()
            c[1:7, :] = np.array(empty, np.float32)
            if k:
                c[1:7, :k] = np.array(full, np.float32)
                c[2, :k] = np.minimum(np.array(full, np.float32) * 1.5, 1)
            c[1, :] *= 0.5
            c[6, :] *= 0.5
            out.append(("%s%d" % (name, k), c))
    return out
