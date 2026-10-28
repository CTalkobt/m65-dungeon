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
    "scroll": [
        "................",
        "................",
        "................",
        "................",
        "................",
        ".kkk........kkk.",
        "kbbBkkkkkkkkbbBk",
        "kbBBbbbbbbbbbBBk",
        "kbBBbBBbBBbbbBBk",
        "kbBBbbbbbrrbbBBk",
        "kbBBbBBbbrrbbBBk",
        "kbbBkkkkkkkkbbBk",
        ".kkk........kkk.",
        "................",
        "................",
        "................",
    ],
    "key": [
        "................",
        "................",
        "................",
        "................",
        "................",
        "..kkkk..........",
        ".kssSSk.........",
        "ksk..kSkkkkkkkk.",
        "ksk..kSssssssssk",
        "ksk..kSSSSSSkSSk",
        ".kSSSSk....kSkSk",
        "..kkkk.....kk.k.",
        "................",
        "................",
        "................",
        "................",
    ],
    "rags": [
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        ".....kkkkk......",
        "...kkfFfffkk....",
        "..kfffFFfftfk...",
        ".kftfffFftffFk..",
        ".kFfftffFfffFFk.",
        "..kFFfFFFtfFFk..",
        "...kkFFFFFFkk...",
        ".....kkkkkk.....",
        "................",
        "................",
    ],
    "sticks": [
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        ".kkkkkkkkkkkkkk.",
        "knnnnnndnnnnnnNk",
        "kNNNNNNdNNNNNNnk",
        "knnnnnndnnnnnnNk",
        "kNNNNNNdNNNNNNnk",
        ".kkkkkkkkkkkkkk.",
        "................",
        "................",
        "................",
        "................",
    ],
    "fur": [
        "................",
        "................",
        "................",
        "................",
        "................",
        "....kkkkkkk.....",
        "..kkMnMMnMMkk...",
        "..kMnnMnnnMnMk..",
        ".kMnnMnnMnnnMk..",
        ".kMnnnMnnnMnnMk.",
        "..kMnMnnMnnMnk..",
        "..k.kkMnnMkkk...",
        "......kkkk......",
        "................",
        "................",
        "................",
    ],
    "candle": [
        "................",
        "................",
        "................",
        "................",
        ".......kk.......",
        "......kbBk......",
        "......kbBk......",
        "......kbBk......",
        "......kbBk......",
        "......kbBk......",
        ".....kbbbBk.....",
        "....kSSSSSSk....",
        "....kkkkkkkk....",
        "................",
        "................",
        "................",
    ],
    "flint": [
        "................",
        "................",
        "................",
        "................",
        "................",
        "...kkkk.........",
        "..kSSSUk........",
        ".kSsSSUUk..kkkk.",
        ".kSSSUUUk.kssssk",
        "..kSUUUk..ks..sk",
        "...kkkk...kssssk",
        "...........kkkk.",
        "................",
        "................",
        "................",
        "................",
    ],
    "firestick": [
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "..kk............",
        ".krRkkkkkkkkkkk.",
        ".kRRnnnnnnnnnnNk",
        "..kkkkkkkkkkkkk.",
        "..kk............",
        ".krRkkkkkkkkkkk.",
        ".kRRnnnnnnnnnnNk",
        "..kkkkkkkkkkkkk.",
        "................",
    ],
    "arrows": [
        "................",
        "................",
        "................",
        "................",
        "................",
        "bB.........ks...",
        ".bnnnnnnnnnkss..",
        "bB.........ks...",
        "................",
        "bB.........ks...",
        ".bnnnnnnnnnkss..",
        "bB.........ks...",
        "................",
        "................",
        "................",
        "................",
    ],
    "shot": [
        "................",
        "................",
        "................",
        "................",
        "....kk.kk.kk....",
        "...kSskSskSsk...",
        "...kbbkbbkbbk...",
        "...kbBkbBkbBk...",
        "...kbBkbBkbBk...",
        "...kbBkbBkbBk...",
        "...kbbkbbkbbk...",
        "....kk.kk.kk....",
        "................",
        "................",
        "................",
        "................",
    ],
    "aim": [
        "eeee........eeee",
        "e..............e",
        "e..............e",
        "e..............e",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "e..............e",
        "e..............e",
        "e..............e",
        "eeee........eeee",
    ],
    "missile": [
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "......ZZZZ......",
        "......ZZZZ......",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
    ],
    "merchant": [
        "................",
        "......kkkk......",
        ".....kNNNNk.....",
        "....kNnnnnNk....",
        "....kNkmmkNk..k.",
        "...kkNmmmmNk.kZk",
        "..knnkNmmNkk.kZk",
        ".knnnnkNNNk.k.k.",
        ".knnnnkNnnNkk...",
        ".knnnnkNnnnNk...",
        ".knnnnkNnnnNk...",
        "..kkkkkNnnnNk...",
        ".....kNnnnnNk...",
        ".....kNNkNNNk...",
        ".....kkk.kkk....",
        "................",
    ],
    "fountain": [
        "................",
        "................",
        ".......kk.......",
        "......kssk......",
        "......kSSk......",
        ".......kk.......",
        "..kkkkkkkkkkkk..",
        ".ksssssssssssSk.",
        ".kSuuuwuuuuuUSk.",
        ".kSuUuuuuwuuUSk.",
        "..kSUUUUUUUUSk..",
        "...kSSSSSSSSk...",
        "....kSssssSk....",
        "....kSSSSSSk....",
        "...kkkkkkkkkk...",
        "................",
    ],
    "shelf": [
        "..kkkkkkkkkkkk..",
        "..kNNNNNNNNNNk..",
        "..kNrpgYbunrNk..",
        "..kNrpgYbunrNk..",
        "..kNNNNNNNNNNk..",
        "..kNubYrgpbuNk..",
        "..kNubYrgpbuNk..",
        "..kNNNNNNNNNNk..",
        "..kNgrbnYpurNk..",
        "..kNgrbnYpurNk..",
        "..kNNNNNNNNNNk..",
        "..kNNkkkkkkNNk..",
        "..kkk......kkk..",
        "................",
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
ART["food"] = [
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "..........kk....",
    ".....kkkkkwwk...",
    "....kMmmmkwk....",
    "...kMmmmmMk.....",
    "...kmmMmmMk.....",
    "...kMmmmMMk.....",
    "....kMMMMk......",
    ".....kkkk.......",
    "................",
    "................",
]
# burning oil spilt on the floor: low flames over a dark pool
ART["flames"] = [
    "................",
    "................",
    "................",
    "................",
    "................",
    "........Z.......",
    "...Z...ZoZ..Z...",
    "..ZoZ.ZoZo.ZoZ..",
    "..oZoZoZZoZoZo..",
    ".ZoZZoZZZZoZZoZ.",
    ".oZZZZoZZZZoZZo.",
    "..hhZZZZZZZZhh..",
    "..hhhhhhhhhhhh..",
    "...hhhhhhhhhh...",
    "................",
    "................",
]
# a pair of bats on the wing (the map's; their eyes seen in the dark)
ART["bats"] = [
    "................",
    "................",
    "k......k........",
    "kF....Fk........",
    ".kFkhkFk......k.",
    ".kFhehFk.k...Fk.",
    "..kFhFk..kF.kFk.",
    "...kkk....kFhFk.",
    "..........kheFk.",
    "...........kFk..",
    "....k.......k...",
    "...kFk..........",
    "..kFhFk..k......",
    "..kheFkkFk......",
    ".kFkhFFFk.......",
    ".k...kkk........",
]
# the ghoul and the ember spider: the skeleton and the spider recoloured
ART["ghoul"] = [r.replace('b', 't').replace('w', 'T').replace('e', 'z') for r in ART["skeleton"]]
ART["ember"] = [r.replace('F', 'R').replace('f', 'd').replace('z', 'Z').replace('r', 'e') for r in ART["spider"]]
# the notes the player puts on the map (levelmap.c): a mark that shines,
# seen however dark it is: danger, treasure, the way, a question
def _note(sym, col):
    rows = ["." * 16 for _ in range(16)]
    for y, r in enumerate(sym):
        rows[3 + y] = "...." + r.replace("#", col).replace(" ", ".").ljust(8, ".") + "...."
    return rows


ART["note1"] = _note(["   ##", "   ##", "   ##", "   ##", "   ##", "   ##", "", "   ##", "   ##"], "e")
ART["note2"] = _note(["   #", "  ####", " # #", "  ###", "   # #", " ####", "   #"], "Z")
ART["note3"] = _note(["#    #", " #  #", "  ##", "  ##", " #  #", "#    #"], "c")
ART["note4"] = _note([" ####", "#    #", "    #", "   #", "  ##", "", "  ##"], "v")
# the strange flasks: the potion in four colours
for _k, (_a, _b) in enumerate((("g", "G"), ("s", "S"), ("p", "P"), ("Y", "d"))):
    ART["flask%d" % (_k + 1)] = [r.replace('r', _a).replace('R', _b) for r in ART["potion"]]
ART["oil"] = [r.replace('r', 'Y').replace('R', 'd').replace('n', 'b') for r in ART["potion"]]
ART["idol_spent"] = [r.replace('z', 'T') for r in ART["idol"]]
ART["draught"] = [r.replace('r', 'u').replace('R', 'U') for r in ART["potion"]]
# a ring: gold, a pale stone set in it (the iron, jade and silver rings)
ART["ring"] = [
    "................",
    "................",
    "................",
    "................",
    "......kkkk......",
    ".....kcwcck.....",
    "......kcck......",
    ".....kYYYYk.....",
    "....kYdkkdYk....",
    "...kYk....kYk...",
    "...kYk....kYk...",
    "...kYk....kYk...",
    "....kYdkkdYk....",
    ".....kYYYYk.....",
    "......kkkk......",
    "................",
]
# an amulet: a stone in gold on its loop (the bone and amber amulets)
ART["amulet"] = [
    "................",
    "................",
    "................",
    ".....kkkkkk.....",
    "....kYddddYk....",
    "...kYk....kYk...",
    "...kYk....kYk...",
    "....kYk..kYk....",
    ".....kYkkYk.....",
    "......kYYk......",
    ".....kkcckk.....",
    "....kYcwccYk....",
    "....kYccccYk....",
    ".....kYccYk.....",
    "......kkkk......",
    "................",
]
# the antidote: a slim vial, pale green, a paper label round it
ART["antidote"] = ["................"] * 3 + [
    ".......kk.......",
    "......knNk......",
    "......kwwk......",
    "......kwwk......",
    ".....kllllk.....",
    ".....klwllk.....",
    ".....kbbbbk.....",
    ".....kBBBBk.....",
    ".....kllzlk.....",
    ".....kGGGGk.....",
    "......kkkk......",
] + ["................"] * 2
ART["fountain_dry"] = [r.replace('u', 'h').replace('U', 'h').replace('w', 'F') for r in ART["fountain"]]


def _fire(name, rows, cols, letters):
    """the flame's pixels to the fire slots, scattered so that turning the
    slots round makes it flicker"""
    art = [list(r) for r in ART[name]]
    for y in rows:
        for x in cols:
            if art[y][x] in letters:
                art[y][x] = "1234"[(x * 3 + y * 5 + (art[y][x] == 'o') * 2) % 4]
    ART[name] = ["".join(r) for r in art]


# a cold brazier: no flame, grey ash and dead coals
ART["brazier_cold"] = ["." * 16 if 1 <= y <= 4 else
                       (r.replace('Z', 'F').replace('o', 'h') if y == 5 else r)
                       for y, r in enumerate(ART["brazier"])]
# the player with the torch out: its head charred
ART["player_dark"] = [r[:10] + r[10:].replace('y', 'h').replace('o', 'k') if y < 5 else r
                      for y, r in enumerate(ART["player"])]
_fire("brazier", range(1, 6), range(16), "Zo")
_fire("flames", range(5, 12), range(16), "Zo")
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


def corridor(seed, style=0):
    """rough-hewn rock, darker: each depth's own (style 0-4: gravel, wet
    stone with puddles, dust and bone chips, cracked rock and webs, dark
    polished slabs)"""
    g = fbm(_xs * 0.4 + seed * 13, _ys * 0.4, 4, seed + 40)
    base = np.array([0.36, 0.33, 0.3], np.float32)
    if style == 1:
        g = fbm(_xs * 0.25 + seed * 13, _ys * 0.25, 3, seed + 42)
        img = base * (0.72 + g * 0.36)[..., None]
        pool = fbm(_xs * 0.3 + seed * 7, _ys * 0.3, 2, seed + 43) > 0.6
        img[pool] *= 0.55
        rim = pool & ~np.roll(pool, 1, axis=0)
        img[rim] *= 1.9                                  # the puddles' near rims catch the light
        return img
    if style == 2:
        img = base * 1.08 * (0.84 + g * 0.26)[..., None]
        rng = np.random.RandomState(seed + 80)
        for _ in range(3):
            x, y = rng.randint(1, 15), rng.randint(1, 15)
            img[y, x] = base * 2.0
            img[y, min(15, x + 1)] = base * 1.6
            img[min(15, y + 1), x] *= 0.5
        return img
    if style == 3:
        g = fbm(_xs * 0.6 + seed * 13, _ys * 0.6, 4, seed + 44)
        img = base * (0.6 + g * 0.7)[..., None]
        rng = np.random.RandomState(seed + 90)
        x, y = rng.randint(3, 13), 0
        for _ in range(16):
            img[y, x] *= 0.4
            x = min(15, max(0, x + rng.randint(-1, 2)))
            y += 1
            if y > 15:
                break
        if seed == 1:                                    # cobwebs in a corner
            for k in range(7):
                img[k, 0 if k < 1 else k - 1] = base * 1.7
                img[0, k * 2 % 16] = base * 1.5
            for k in range(1, 6):
                img[k, 6 - k] = base * 1.5
        return img
    if style == 4:
        slab = ((_xs // 8) + (_ys // 8) * 2 + seed) % 3
        img = base * 0.8 * (0.85 + slab * 0.08 + g * 0.15)[..., None]
        joint = (_xs % 8 == 0) | (_ys % 8 == 0)
        img[joint] *= 0.45
        sheen = ((_xs - _ys) % 16 == 5) & ~joint
        img[sheen] *= 1.35
        return img
    pebble = fbm(_xs * 1.3 + seed, _ys * 1.3, 2, seed + 41) > 0.72
    img = base * (0.7 + g * 0.5)[..., None]
    img[pebble] *= 1.35
    return img


def rock_top(style=0):
    """the top of the rock a wall is cut in: dark, rough; each depth's own"""
    if style == 1:      # wet: smoother, streaked
        g = fbm(_xs * 0.22, _ys * 0.22, 3, 62) * 0.7 + fbm(_xs * 1.1, _ys * 0.12, 2, 63) * 0.3
        return np.array([0.2, 0.18, 0.17], np.float32) * (0.6 + g * 0.6)[..., None]
    if style == 2:      # chalk: paler, fine-grained
        g = fbm(_xs * 0.5, _ys * 0.5, 3, 64)
        return np.array([0.25, 0.23, 0.21], np.float32) * (0.7 + g * 0.45)[..., None]
    if style == 3:      # raw rock: coarse, broken
        g = fbm(_xs * 0.5, _ys * 0.5, 5, 65)
        return np.array([0.19, 0.17, 0.16], np.float32) * (0.4 + g * 1.0)[..., None]
    if style == 4:      # basalt: the columns' tops, six-sided cells
        g = fbm(_xs * 0.3, _ys * 0.3, 3, 66)
        img = np.array([0.15, 0.135, 0.13], np.float32) * (0.75 + g * 0.5)[..., None]
        row = _ys // 4
        cell = ((_xs + (row % 2) * 2) % 4 == 0) | (_ys % 4 == 0)
        img[cell] *= 0.5
        return img
    g = fbm(_xs * 0.3, _ys * 0.3, 4, 60)
    return np.array([0.2, 0.18, 0.17], np.float32) * (0.6 + g * 0.7)[..., None]


def _face(x, y, y0, style):
    """the colour of a wall's face at (x, y), y0 its top row"""
    stone = np.array([0.62, 0.55, 0.48], np.float32)
    r = y - y0
    shade = 1.1 - r * 0.05
    if style == 1:      # big wet ashlar blocks, damp running down
        course = 0 if r < 4 else 1
        joint = r == 3 or (x + course * 4 + 2) % 8 == 0
        v = 0.5 + 0.12 * float(fbm(np.float32(x * 0.3), np.float32(y * 0.3 + course), 2, 67))
        c = stone * v * shade * (0.45 if joint else 1.0)
        if (x * 7 + 3) % 5 == 0:
            c = c * (1.0 - r * 0.06)                     # a damp streak
        return c
    if style == 2:      # an ossuary: the ends of bones set in the wall
        course = r // 3
        v = 0.5 + 0.15 * float(fbm(np.float32(x * 0.5), np.float32(y * 0.5), 2, 68))
        c = stone * v * shade
        if r % 3 == 2:
            return c * 0.4
        bx = (x + course * 2) % 4
        if bx in (1, 2):
            c = stone * (1.15 if r % 3 == 0 else 0.95)   # a bone's end
            if bx == 2 and r % 3 == 1:
                c = c * 0.45
        return c
    if style == 3:      # raw rock, split here and there
        v = 0.36 + 0.36 * float(fbm(np.float32(x * 0.6), np.float32(y * 0.6), 3, 69))
        c = stone * v * shade
        if (x * 3 + r * 5) % 11 == 0 or (x + r * 2) % 13 == 0:
            c = c * 0.45
        return c
    if style == 4:      # basalt columns
        col = x // 3
        v = 0.36 + 0.06 * ((col * 5) % 4) + 0.06 * float(fbm(np.float32(x), np.float32(y * 0.4), 2, 70))
        c = stone * v * (1.15 - r * 0.07)
        if x % 3 == 0:
            c = c * 0.4
        elif x % 3 == 1 and (col * 3 + r) % 5 == 0:
            c = c * 1.4                                  # a glassy glint
        return c
    course = r // 3
    joint = (x + course * 4 + 1) % 8 == 0 or r % 3 == 2
    v = 0.5 + 0.18 * float(fbm(np.float32(x * 0.5), np.float32(y * 0.5 + course), 2, 61))
    return stone * v * shade * (0.45 if joint else 1.0)


def wall(mask, style=0):
    """mask: floor to the N 1, E 2, S 4, W 8. The rock's top, and where
    floor is to the south the wall's face (each depth's own stone: style
    0-4), lit. Its edges: the lines that catch the light on the sides
    toward the floor"""
    img = rock_top(style)
    edge = np.zeros((T, T), bool)
    face = 7
    if mask & 4:
        y0 = T - face
        for y in range(y0, T):
            for x in range(T):
                img[y, x] = _face(x, y, y0, style)
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


def door(vertical, open_, locked=False, wedged=False):
    """a door in a gap of the wall: planks and iron (and a lock plate, when
    locked), or swung open. Its edges: the iron bands and the lock (the
    door swung back, when open)"""
    img = floor(5)
    edge = np.zeros((T, T), bool)
    mat = np.zeros((T, T), bool)
    if open_:
        if vertical:
            img[:, 0:3] = np.array([0.42, 0.27, 0.14], np.float32)
            img[:, 3] *= 0.4
            edge[:, 0:2] = True
            mat[:, 0:3] = True
        else:
            img[0:3, :] = np.array([0.42, 0.27, 0.14], np.float32)
            img[3, :] *= 0.4
            edge[0:2, :] = True
            mat[0:3, :] = True
        return img, edge, mat
    wood = np.array([0.5, 0.32, 0.17], np.float32)
    g = fbm(_xs * (0.2 if vertical else 1.0), _ys * (1.0 if vertical else 0.2), 3, 70)
    img = wood * (0.75 + g * 0.5)[..., None]
    mat[:] = True
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
    if wedged:
        # sticks jammed against it: two bundles braced across in an X, pale
        # wood two pixels thick over a dark shadow (the shadow an edge: it
        # shows in the memory)
        stick = np.array([0.86, 0.7, 0.42], np.float32)
        shadow = np.array([0.12, 0.08, 0.05], np.float32)
        for k in range(1, T - 1):
            for y, x in ((k, k), (k, T - 1 - k)):
                for dy, dx, c in ((1, 0, shadow), (0, 1, shadow), (0, 0, stick), (-1, 0, stick)):
                    yy, xx = y + dy, x + dx
                    if 0 <= yy < T and 0 <= xx < T:
                        img[yy, xx] = c
                        edge[yy, xx] = c is shadow   # (an edge is the material's one edge colour)
    if locked:
        brass = np.array([0.78, 0.62, 0.25], np.float32)
        img[5:11, 5:11] = brass * 0.6
        img[6:10, 6:10] = brass
        img[7:10, 7:9] = 0.06
        img[7, 7:9] = 0.06
        edge[5:11, 5:11] = True
    img[0, :] *= 0.4
    img[:, 0] *= 0.4
    return img, edge, mat


def trap():
    """a pressure plate found in the floor (its rim an edge: remembered)"""
    img = floor(2)
    edge = np.zeros((T, T), bool)
    img[3:13, 3:13] = np.array([0.3, 0.27, 0.24], np.float32)
    img[4:12, 4:12] = np.array([0.42, 0.36, 0.3], np.float32)
    img[7:9, 7:9] = np.array([0.15, 0.12, 0.1], np.float32)
    edge[3, 3:13] = edge[12, 3:13] = edge[3:13, 3] = edge[3:13, 12] = True
    return img, edge


def rubble(style=0):
    """a corridor choked with fallen stones (their outlines edges)"""
    img = corridor(1, style)
    edge = np.zeros((T, T), bool)
    for cx, cy, r in ((4, 5, 3.6), (11, 4, 3.0), (8, 11, 4.2), (13, 12, 2.4), (3, 12, 2.6)):
        d = np.sqrt((_xs - cx) ** 2 + (_ys - cy) ** 2)
        img[d < r] = np.array([0.46, 0.42, 0.38], np.float32) * (1.1 - d[d < r] / r * 0.5)[..., None]
        ring = (d >= r - 1) & (d < r)
        img[ring] *= 0.45
        edge |= ring
    return img, edge


def crack(mask, style=0):
    """a wall between two ways, cracked through: it can be broken"""
    img, edge = wall(mask, style)
    x = 8
    for y in range(T):
        x = min(12, max(4, x + ((y * 5) % 3) - 1))
        a, b = (y, x) if mask & 1 else (x, y)     # (across the wall, from one way to the other)
        img[a, b] = 0.04
        img[(a, a + 1)[mask & 1 == 0], (b + 1, b)[mask & 1 == 0]] *= 0.4
        edge[a, b] = True
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


STYLES = 5             # each depth its own stone (DEPTHS)
# each depth's stone in the palette (gen_art.py map_palette): a tint, how
# much of the colour is kept (0 grey), and a brightness. Brown sandstone,
# green-grey with moss and damp, pale chalk and bone, rust and umber, violet
# black basalt
LOOKS = [((1.0, 1.0, 1.0), 1.0, 1.0),
         ((0.86, 1.03, 0.93), 0.85, 0.92),
         ((1.04, 1.0, 0.9), 0.3, 1.32),
         ((1.14, 0.86, 0.72), 1.15, 0.86),
         ((0.92, 0.84, 1.08), 0.45, 0.74)]


def look(rgb, style):
    """a stone colour (n x 3, 0..1) as depth style shows it"""
    tint, sat, bright = LOOKS[style]
    lum = rgb.mean(axis=-1, keepdims=True)
    return (lum + (rgb - lum) * sat) * np.array(tint, np.float32) * bright


def terrain_kinds(style=0):
    """[(name, image, edge pixels, material pixels)] in the order of the C
    enum (TK_*): the map's 16-colour tiles, unlit (the palette banks light
    them), in a depth's stone (style). The material (wood, iron, brass) is
    not stone: each depth shows it the same"""
    none = np.zeros((T, T), bool)

    def st(img_edge):
        return img_edge + (none,)
    out = [("ROCK", np.zeros((T, T, 3), np.float32), none, none)]
    for k in range(4):
        out.append(("FLOOR%d" % k, floor(k + 1), none, none))
    for k in range(2):
        out.append(("CORR%d" % k, corridor(k, style), none, none))
    for m in range(16):
        out.append(("WALL%d" % m,) + st(wall(m, style)))
    for name, v, o in (("DOOR_H", 0, 0), ("DOOR_V", 1, 0), ("DOOR_OPEN_H", 0, 1), ("DOOR_OPEN_V", 1, 1)):
        out.append((name,) + door(v, o))
    out.append(("STAIRS",) + st(stairs()))
    for name, v in (("DOOR_LOCKED_H", 0), ("DOOR_LOCKED_V", 1)):
        out.append((name,) + door(v, 0, True))
    # secret doors: exactly the wall they hide in (a door across a corridor
    # running north-south has floor north and south of it: WALL5; else WALL10)
    out.append(("SECRET_H",) + st(wall(5, style)))
    out.append(("SECRET_V",) + st(wall(10, style)))
    out.append(("TRAP",) + st(trap()))
    out.append(("RUBBLE",) + st(rubble(style)))
    out.append(("CRACK_NS",) + st(crack(5, style)))
    out.append(("CRACK_EW",) + st(crack(10, style)))
    for name, v in (("DOOR_WEDGED_H", 0), ("DOOR_WEDGED_V", 1)):   # (shut, sticks against it: explore.c K)
        out.append((name,) + door(v, 0, wedged=True))
    return out


SPRITES = ("chest", "chest_open", "idol", "idol_spent", "potion", "gold", "bones", "brazier",
           "brazier_cold", "scroll", "key", "draught", "rags", "sticks", "fur", "candle", "flint", "firestick",
           "arrows", "shot", "merchant", "fountain", "fountain_dry", "shelf", "oil", "food", "flames",
           "flask1", "flask2", "flask3", "flask4", "antidote", "ring", "amulet", "player", "player_dark", "aim", "missile", "rat", "skeleton", "spider", "slime", "orc", "wraith", "lich", "bats", "ghoul", "ember",
           "note1", "note2", "note3", "note4")


def sprite_kinds(base, base_nib, pal_map):
    """[(name, [variant or None] * NV)] in the order of the C enum (T_*):
    the things, the creatures and the player, full colour, each on base
    (the floor as the map shows it, so they sit on it). A variant is its
    image and, for each pixel of that floor, the palette entry the map
    shows the floor's colour number (base_nib) in at that light: the map's
    bank (pal_map + level * 16), so the floor round a thing is the depth's
    as the map's is (-1: the thing's own pixel)"""
    out = []
    for name in SPRITES:
        img, mask, glow = sprite(name)
        comp = np.where(mask[..., None], img, base)
        # a soft shadow under it, down and to the right
        sh = np.zeros((T, T), bool)
        sh[1:, 1:] = mask[:-1, :-1]
        sh &= ~mask
        comp = np.where(sh[..., None], comp * 0.55, comp)

        def bg(level):
            b = np.where(mask, -1, pal_map + level * 16 + base_nib)
            if level < 3:
                b[sh] = pal_map + (level + 1) * 16 + base_nib[sh]   # (the shadow: the next level down)
            else:
                b[sh] = -1
            return b
        v = [(lit(comp, l, glow), bg(l)) for l in range(4)]
        creature = name in ("rat", "skeleton", "spider", "slime", "orc", "wraith", "lich", "bats", "ghoul", "ember",
                            "aim", "missile", "note1", "note2", "note3", "note4",
                            "merchant")     # (his lantern seen in the dark)
        if creature and glow.any():
            eyes = np.zeros((T, T, 3), np.float32)
            eyes[glow] = img[glow] * 0.8
            v.append((eyes, np.full((T, T), -1)))
        else:
            v.append(None)
        if name == "brazier":
            v = [v[0]] * 4 + [None]                     # it lights itself
        elif name == "flames":
            v = [v[0]] * 4 + [(img * mask[..., None], np.full((T, T), -1))]   # (and is seen from the dark)
        elif name in ("aim", "missile"):
            v = [v[4]] * 5                              # only what glows, over anything
        elif name == "player":
            v = [v[0], None, None, None, None]          # (view.c: always bright with the torch lit)
        elif name == "player_dark":
            v = [None, None, None, v[3], None]          # (and dim with it out)
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
    # the level map (levelmap.c): a place a character
    stone = np.array([0.46, 0.41, 0.36], np.float32)
    wood = np.array([0.5, 0.32, 0.17], np.float32)
    flo = blank() + stone * 0.42
    flo[0, :] *= 0.6
    flo[:, 0] *= 0.6
    out.append(("MAP_FLOOR", flo))
    cor = blank() + stone * 0.28
    cor[2, 5] = cor[5, 2] = stone * 0.5
    out.append(("MAP_CORR", cor))
    wal = blank() + stone * 0.9
    wal[0, :] = stone * 1.3
    wal[7, :] = stone * 0.5
    wal[3, :] *= 0.75
    wal[0:3, 4] *= 0.75
    wal[4:7, 1] *= 0.75
    out.append(("MAP_WALL", wal))
    door = blank() + wood
    door[:, 0] = door[:, 7] = stone * 0.9
    door[3:5, 5] = np.array([0.8, 0.7, 0.3], np.float32)
    out.append(("MAP_DOOR", door))
    opn = flo.copy()
    opn[:, 0:2] = wood
    out.append(("MAP_OPEN", opn))
    lck = door.copy()
    lck[2:6, 2:6] = np.array([0.78, 0.62, 0.25], np.float32)
    lck[3:5, 3:5] = 0.05
    out.append(("MAP_LOCKED", lck))
    st = flo.copy()
    for k in range(4):
        st[1:7, 1 + k * 2] = stone * (1.4 - k * 0.3)
    out.append(("MAP_STAIRS", st))
    trp = flo.copy()
    trp[1, 1:7] = trp[6, 1:7] = trp[1:7, 1] = trp[1:7, 6] = np.array([0.8, 0.2, 0.15], np.float32)
    out.append(("MAP_TRAP", trp))
    rub = cor.copy()
    for (y, x) in ((1, 1), (1, 2), (2, 1), (4, 4), (4, 5), (5, 4), (5, 5), (2, 5), (5, 1)):
        rub[y, x] = stone * 0.95
    out.append(("MAP_RUBBLE", rub))
    me = flo.copy()
    me[1:7, 2:6] = np.array([1.0, 0.95, 0.7], np.float32)
    me[1, 2] = me[1, 5] = me[6, 2] = me[6, 5] = stone * 0.42
    me[2:5, 3:5] = np.array([1.0, 0.6, 0.2], np.float32)
    out.append(("MAP_PLAYER", me))
    cur = blank()
    gold = np.array([1.0, 0.85, 0.3], np.float32)
    cur[0, :] = cur[7, :] = cur[:, 0] = cur[:, 7] = gold
    out.append(("MAP_CURSOR", cur))
    return out
