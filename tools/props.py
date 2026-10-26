"""props.py — the close-ups that are not creatures: a chest (shut, then
open: frame 1) and an idol (dormant, then awake: frame 1)"""
import math
import numpy as np
from artlib import Mat, rot
from stage import W

CX = W // 2


class Frame:
    """shapes placed in a local frame (a centre and a rotation)"""

    def __init__(self, sc, c, R):
        self.sc, self.c, self.R = sc, np.array(c, np.float32), R

    def p(self, v):
        return self.c + self.R @ np.array(v, np.float32)

    def box(self, c, half, mat, R=None, bevel=0.0):
        self.sc.box(self.p(c), half, mat, self.R if R is None else self.R @ R, bevel)

    def cyl(self, c, r, hh, mat, R=None):
        self.sc.cylinder(self.p(c), r, hh, mat, self.R if R is None else self.R @ R)

    def sphere(self, c, r, mat):
        self.sc.sphere(self.p(c), r, mat)

    def ell(self, c, r, mat, R=None):
        self.sc.ellipsoid(self.p(c), r, mat, self.R if R is None else self.R @ R)

    def tube(self, pts, radii, mat):
        self.sc.tube([self.p(q) for q in pts], radii, mat)


def chest(sc, pose):
    wood = Mat((120, 74, 40), tex=0.65, texscale=0.09, tex2=(64, 38, 20), bump=0.03, spec=0.2)
    plank = Mat((104, 64, 34), tex=0.7, texscale=0.12, tex2=(52, 32, 18), bump=0.04, spec=0.2)
    iron = Mat((96, 94, 92), tex=0.5, texscale=0.08, tex2=(96, 54, 30), spec=1.3, shin=40, bump=0.02)
    inside = Mat((30, 18, 10), tex=0.5, texscale=0.1, tex2=(14, 8, 4))
    gold = Mat((240, 190, 80), tex=0.35, texscale=0.3, tex2=(170, 110, 40), spec=2.2, shin=60)
    gem = Mat((80, 255, 120), glow=1.0)
    ruby = Mat((255, 60, 70), spec=2.0, shin=80, emit=0.0, tex=0.2, tex2=(140, 20, 30))
    f = Frame(sc, (CX, 186, 0), rot(-24, -24, 0))
    hw, hh, hd = 92, 34, 52                   # the body's half sizes
    # the body: planks and iron bands, feet
    f.box((0, 0, 0), (hw, hh, hd), wood, bevel=3)
    for k in range(-1, 2):
        f.box((0, k * 22, hd + 0.8), (hw - 2, 1.0, 0.8), Mat((40, 24, 12)))
    for xb in (-hw + 16, hw - 16):
        f.box((xb, 0, 0), (7, hh + 1.5, hd + 2), iron, bevel=1.5)
        for k in range(3):
            f.sphere((xb, -hh + 10 + k * 24, hd + 2.5), 2.2, iron)
    f.box((0, hh - 3, 0), (hw + 2, 4, hd + 2), iron, bevel=1.5)
    for sx in (-1, 1):
        for sz in (-1, 1):
            f.box((sx * (hw - 8), hh + 6, sz * (hd - 8)), (8, 6, 8), iron, bevel=2)
    # the lock plate
    f.box((0, -hh + 14, hd + 3), (13, 15, 2), iron, bevel=2)
    f.ell((0, -hh + 16, hd + 5), (3, 4.5, 1.5), Mat((10, 6, 4)))
    # the lid: a half barrel hinged at the back, shut or thrown open
    if pose:
        # what is inside: a heap of coins, a gem
        f.box((0, -hh + 2, 0), (hw - 4, 2, hd - 4), inside)
        rng = np.random.RandomState(7)
        for k in range(140):
            px, pz = rng.uniform(-hw + 12, hw - 12), rng.uniform(-hd + 10, hd - 10)
            heap = 18 * math.exp(-(px / 60) ** 2 - (pz / 40) ** 2)
            f.cyl((px, -hh - heap + rng.uniform(-2, 2), pz), 6.5, 1.2, gold,
                  rot(rng.uniform(-40, 40), 0, rng.uniform(-40, 40)))
        f.ell((8, -hh - 22, 6), (9, 7, 9), gem, rot(30, 20, 0))
        f.ell((-36, -hh - 12, 10), (6, 5, 6), ruby)
        lid = rot(-112, 0, 0)
    else:
        lid = rot(0, 0, 0)
    hinge = np.array((0, -hh, -hd), np.float32)
    # in the lid's own frame: from the hinge, the barrel's middle at (0, 0, hd)
    LR = lid @ rot(0, 0, 90)
    sc.cylinder(f.p(hinge + lid @ np.array((0, 0, hd), np.float32)), hd, hw, plank, f.R @ LR)
    for xb in (-hw + 16, hw - 16):
        sc.cylinder(f.p(hinge + lid @ np.array((xb, 0, hd), np.float32)), hd + 2, 7, iron, f.R @ LR)
    sc.box(f.p(hinge + lid @ np.array((0, 0, 2 * hd), np.float32)), (14, 10, 4), iron, f.R @ lid, bevel=2)
    return dict(glow=(1.0, 0.85, 0.4), pools=((CX, 232, 150, 14, 0.9),),
                backdrop=dict(light=(0.5, 0.45), spread=0.34))


def idol(sc, pose):
    stone = Mat((112, 118, 104), tex=0.6, texscale=0.06, tex2=(60, 70, 54), bump=0.05, spec=0.25, shin=10)
    moss = Mat((60, 90, 44), tex=0.6, texscale=0.2, tex2=(30, 50, 24), bump=0.06)
    pedestal = Mat((96, 90, 84), tex=0.5, texscale=0.05, tex2=(56, 52, 48), bump=0.03)
    dark = Mat((6, 4, 8))
    eye = Mat((140, 255, 190), glow=1.0)
    wax = Mat((220, 210, 180), sss=0.6, spec=0.4)
    flame = Mat((255, 130, 30), emit=1.6)
    gold = Mat((220, 170, 70), spec=1.8, shin=50, tex=0.3, tex2=(140, 90, 40))
    x = CX
    # the pedestal: three steps
    sc.box((x, 222, 0), (110, 12, 44), pedestal, rot(-10, 0, 0), bevel=3)
    sc.box((x, 200, 0), (88, 12, 36), pedestal, rot(-10, 0, 0), bevel=3)
    sc.box((x, 180, 0), (70, 10, 30), pedestal, rot(-10, 0, 0), bevel=3)
    # carved runes on the front step: small dark notches
    for k in range(-5, 6):
        sc.box((x + k * 15, 222, 46), (2, 5 + (k % 3), 1), dark, rot(0, 0, k * 17))
    # the figure: seated, squat, many-armed, horned
    sc.ellipsoid((x, 150, 4), (56, 34, 38), stone)                 # crossed legs
    sc.ellipsoid((x, 112, 0), (40, 42, 32), stone)                 # the body
    sc.ellipsoid((x, 122, 26), (26, 22, 10), stone)                # the belly
    sc.ellipsoid((x, 64, 6), (30, 30, 28), stone)                  # the head
    sc.ellipsoid((x, 84, 26), (18, 10, 10), stone)                 # the jaw
    sc.ellipsoid((x, 86, 34), (12, 3 + pose * 2, 3), dark)        # the mouth
    for s in (-1, 1):
        sc.tube([(x + s * 22, 46, 4), (x + s * 46, 30, 0), (x + s * 54, 4, -4), (x + s * 48, -8, -6)], [9, 7, 4, 1], stone)
        # four arms: two raised with open palms, two resting on the knees
        sc.tube([(x + s * 34, 96, 4), (x + s * 66, 82, 10), (x + s * 74, 52, 16)], [10, 8, 7], stone)
        sc.ellipsoid((x + s * 75, 44, 18), (8, 10, 4), stone)
        sc.tube([(x + s * 34, 116, 8), (x + s * 50, 140, 24), (x + s * 40, 152, 38)], [9, 8, 7], stone)
        sc.ellipsoid((x + s * 38, 154, 40), (9, 6, 7), stone)
        # three eyes in all: two here and one on the brow
        sc.ellipsoid((x + s * 11, 64, 30), (7, 5, 3), dark)
        sc.sphere((x + s * 11, 64, 32), 3.5 + pose * 1.2, eye)
    sc.ellipsoid((x, 48, 30), (5, 7, 3), dark)
    sc.sphere((x, 48, 32), 3.0 + pose * 1.4, eye)
    sc.ellipsoid((x, 34, 18), (18, 6, 8), gold)                    # a band on the brow
    # moss on the shoulders and the knees
    for (mx, my, mz, r) in ((-30, 86, 18, 12), (34, 92, 14, 10), (-44, 146, 30, 12), (50, 148, 26, 11)):
        sc.ellipsoid((x + mx, my, mz), (r, r * 0.5, r * 0.7), moss)
    # offerings: candles either side, a bowl
    for s, h in ((-1, 26), (1, 18), (-1, 14)):
        cx = x + s * (96 - h)
        sc.cylinder((cx, 204 - h, 30 + h), 6, h, wax)
        sc.ellipsoid((cx, 204 - 2 * h - 9, 30 + h), (5, 10, 4), flame)
        sc.tube([(cx, 204 - 2 * h, 31 + h), (cx, 204 - 2 * h - 5, 31 + h)], 1, Mat((20, 14, 10)))
    sc.ellipsoid((x, 166, 44), (22, 7, 12), gold)
    if pose:
        # awake: motes of light rising round it
        rng = np.random.RandomState(3)
        for k in range(26):
            a = rng.uniform(0, 6.28)
            r = rng.uniform(70, 120)
            sc.sphere((x + math.cos(a) * r, rng.uniform(10, 180), math.sin(a) * 40 + 30), rng.uniform(1.2, 2.6), eye)
    return dict(glow=(0.5, 1.0, 0.7), pools=((CX, 236, 140, 10, 0.8),),
                backdrop=dict(light=(0.5, 0.35), spread=0.3 + pose * 0.04, warm=0.8), haze=1.0)


PROPS = [
    ("chest", chest),
    ("idol", idol),
]
