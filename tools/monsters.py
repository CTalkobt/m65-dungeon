"""monsters.py — the creatures' close-ups, built from shapes (artlib.Scene).

Each function draws one creature in a pose (0 waiting, 1 attacking) into a
Scene of stage.W x stage.H and returns how to light and set it: a dict of
compose() and light() options. The creature stands on the floor near the
middle; the torch is up on the left.
"""
import math
import numpy as np
from artlib import Scene, Mat, rot
from stage import W, H

CX = W // 2


def _arc(cx, cy, cz, rx, rz, a0, a1, droop, n=14):
    """points round an arc in the x-z plane (a rib, a collar), sagging by
    droop toward its ends"""
    pts = []
    for k in range(n + 1):
        a = math.radians(a0 + (a1 - a0) * k / n)
        pts.append((cx + math.sin(a) * rx, cy + (1 - math.cos(a)) * droop, cz + math.cos(a) * rz))
    return pts


def _limb(sc, a, b, c, r0, r1, r2, mat, joint=None):
    """two bones a-b, b-c with a knob at the joint"""
    sc.tube([a, b], [r0, r1], mat)
    sc.tube([b, c], [r1, r2], mat)
    if joint:
        sc.sphere(b, r1 * 1.35, joint)


# ------------------------------------------------------------------ skeleton

def skeleton(sc, pose):
    bone = Mat((222, 210, 182), tex=0.28, texscale=0.09, spec=0.35, shin=20, bump=0.025,
               tex2=(150, 132, 100))
    dark = Mat((18, 12, 10))
    eye = Mat((255, 90, 40), glow=1.0)
    iron = Mat((150, 150, 155), tex=0.3, texscale=0.05, spec=1.2, shin=50, tex2=(105, 70, 50), bump=0.015)
    wood = Mat((110, 72, 40), tex=0.4, texscale=0.12, tex2=(70, 42, 22))
    cloth = Mat((70, 30, 28), tex=0.5, texscale=0.06, tex2=(30, 14, 12), bump=0.04)
    x = CX
    lean = 6 if pose else 0
    # the skull
    hy = 46 - (4 if pose else 0)
    sc.ellipsoid((x + lean, hy, 4), (21, 24, 21), bone)
    sc.ellipsoid((x + lean, hy + 22, 10), (14, 9, 12), bone, rot(-15 if pose else 0, 0, 0))   # the jaw
    for s in (-1, 1):
        sc.ellipsoid((x + lean + s * 8.5, hy + 2, 21), (6.5, 7.5, 3), dark)                  # the sockets
        sc.sphere((x + lean + s * 8.5, hy + 3, 23), 2.6, eye)                               # their fire
        sc.ellipsoid((x + lean + s * 17, hy + 10, 8), (5, 8, 9), bone)                       # cheekbones
    sc.ellipsoid((x + lean, hy + 13, 23), (3, 4, 2), dark)                                   # the nose
    for k in range(-3, 4):                                                                 # teeth
        sc.box((x + lean + k * 3.2, hy + 18 + (6 if pose else 0) * 0, 21), (1.3, 2.4, 2), bone, bevel=0.6)
    if pose:
        sc.ellipsoid((x + lean, hy + 22, 14), (9, 5, 4), dark)                               # the open mouth
    # the neck and the spine
    sc.tube([(x + lean, hy + 26, 0), (x + lean * 0.5, 84, -2), (x, 150, -4)], [4.5, 5.5, 6.5], bone)
    for k in range(10):
        sc.sphere((x + lean * 0.5 * (1 - k / 10), 86 + k * 6.5, 0), 6.0, bone)
    # the collarbones and the ribs
    for s in (-1, 1):
        sc.tube([(x + 2 * s, 84, 6), (x + 22 * s, 86, 8), (x + 38 * s, 84, 2)], [3, 3.2, 3.5], bone)
    for k in range(6):
        y = 96 + k * 9
        rx = 30 - abs(k - 2) * 2.2 - (k > 3) * 3
        for side in (-1, 1):
            sc.tube(_arc(x, y, -4, rx, 18, side * 165, side * 25, 4 + k * 0.8, 10), [2.8] * 4 + [2.4] * 7, bone)
    sc.ellipsoid((x, 112, 15), (5, 24, 4), bone)                                             # the breastbone
    # the pelvis
    sc.ellipsoid((x, 162, 0), (26, 14, 14), bone)
    sc.ellipsoid((x, 166, 9), (12, 9, 6), dark)
    # the legs
    for s in (-1, 1):
        hip = (x + 16 * s, 168, 2)
        knee = (x + 22 * s, 200, 10)
        foot = (x + 24 * s, 232, 4)
        _limb(sc, hip, knee, foot, 6, 5, 4.5, bone, joint=bone)
        sc.ellipsoid((x + 26 * s, 234, 14), (8, 4, 12), bone)
    # a rag of a tabard round the hips
    for k in range(-5, 6):
        sc.box((x + k * 5.5, 178 + (k % 2) * 4, 13), (3.2, 12 + (k % 3) * 3, 1.5), cloth, rot(0, 0, k * 2))
    # the left arm: a round shield
    sl = (x - 38, 86, 2)
    if pose:
        el, hand = (x - 56, 116, 22), (x - 52, 132, 40)
    else:
        el, hand = (x - 52, 118, 8), (x - 56, 146, 22)
    _limb(sc, sl, el, hand, 5, 4.2, 3.5, bone, joint=bone)
    sc.cylinder((hand[0] - 8, hand[1] - 4, hand[2] + 8), 30, 3, wood, rot(90 - 10, 18, 0))
    sc.cylinder((hand[0] - 8, hand[1] - 4, hand[2] + 12), 9, 3, iron, rot(90 - 10, 18, 0))
    for a in range(0, 360, 45):
        r = math.radians(a)
        sc.sphere((hand[0] - 8 + math.cos(r) * 25, hand[1] - 4 + math.sin(r) * 25, hand[2] + 12 + math.cos(r) * 6), 2, iron)
    # the right arm: a notched sword
    sr = (x + 38, 86, 2)
    if pose:
        el, hand = (x + 62, 64, 4), (x + 74, 36, 10)
        blade = rot(0, 0, -150)
    else:
        el, hand = (x + 54, 118, 6), (x + 62, 146, 16)
        blade = rot(-20, 0, -22)
    _limb(sc, sr, el, hand, 5, 4.2, 3.5, bone, joint=bone)
    for k in range(4):
        sc.sphere((hand[0] - 2 + k * 1.5, hand[1] + k * 1.2, hand[2] + 2), 3.2, bone)
    d = blade @ np.array([0, 1, 0], np.float32)
    sc.box(np.array(hand) - d * 6, (2.2, 9, 2.2), wood, blade)                               # the grip
    sc.box(np.array(hand) + d * 4, (11, 2.2, 3), iron, blade, bevel=1)                       # the guard
    sc.box(np.array(hand) + d * 44, (4.6, 38, 1.2), iron, blade, bevel=1.5)                  # the blade
    return dict(glow=(1.0, 0.35, 0.12), pools=((CX, 236, 70, 9, 0.8),))


# ------------------------------------------------------------------ giant rat

def rat(sc, pose):
    fur = Mat((112, 92, 78), tex=0.55, texscale=0.16, bump=0.06, tex2=(58, 46, 40), spec=0.15, shin=8)
    belly = Mat((150, 128, 108), tex=0.4, texscale=0.16, bump=0.05, tex2=(96, 80, 66))
    pink = Mat((214, 140, 140), tex=0.2, spec=0.5, shin=20, sss=0.4)
    tooth = Mat((236, 214, 140), spec=0.7, shin=30)
    dark = Mat((14, 8, 8))
    eye = Mat((255, 40, 30), glow=1.0)
    rear = 1 if pose else 0
    bx, by = CX + 40, 178 - rear * 12
    # the tail, behind, curling on the floor
    sc.tube([(bx + 70, by + 30, -30), (bx + 120, 226, -10), (bx + 170, 220, 10), (bx + 190, 200, 20), (bx + 182, 184, 24)],
            [11, 8, 5, 3, 1.5], pink)
    # the body: haunches, back, chest
    sc.ellipsoid((bx + 30, by + 8, -10), (62, 44, 48), fur, rot(0, 0, -8 - rear * 10))
    sc.ellipsoid((bx - 22, by - 6 - rear * 14, 6), (52, 40, 42), fur, rot(0, 0, 20 + rear * 18))
    sc.ellipsoid((bx - 30, by + 22 - rear * 10, 26), (34, 26, 22), belly)
    # the hind legs and feet
    for s, dz in ((-1, 30), (1, -40)):
        sc.ellipsoid((bx + 52, by + 30, dz), (24, 22, 16), fur)
        sc.ellipsoid((bx + 40, 230, dz + 8), (22, 6, 10), pink)
    # the head: skull, snout, nose
    hx, hy = bx - 78 - rear * 6, by - 18 - rear * 40
    tilt = rot(0, 0, -10 - rear * 25)
    sc.ellipsoid((hx + 10, hy, 20), (34, 28, 30), fur, tilt)
    sc.ellipsoid((hx - 22, hy + 8 + rear * 4, 34), (24, 17, 18), fur, rot(0, 0, -15 - rear * 25))
    sc.sphere((hx - 44, hy + 14 + rear * 2, 42), 7, pink)
    sc.ellipsoid((hx - 43, hy + 15 + rear * 2, 48), (3, 2.5, 2), dark)
    # the eyes
    for dz, dx in ((26, 0), (-2, 8)):
        sc.ellipsoid((hx - 2 + dx, hy - 8, 30 + dz), (6, 5, 5), dark)
        sc.sphere((hx - 3 + dx, hy - 8, 34 + dz), 3.2, eye)
    # the ears
    for dz, dx in ((22, 0), (-14, 18)):
        sc.ellipsoid((hx + 22 + dx, hy - 30, dz), (14, 16, 4), pink, rot(0, -30, 20))
        sc.ellipsoid((hx + 22 + dx, hy - 30, dz - 3), (16, 18, 4), fur, rot(0, -30, 20))
    # the mouth and its teeth
    if pose:
        sc.ellipsoid((hx - 30, hy + 28, 34), (14, 9, 10), dark, rot(0, 0, -30))
        sc.box((hx - 38, hy + 22, 44), (2.5, 7, 2), tooth, rot(0, 0, -20))
        sc.box((hx - 33, hy + 22, 44), (2.5, 7, 2), tooth, rot(0, 0, -20))
        sc.box((hx - 30, hy + 38, 40), (2.2, 5, 2), tooth, rot(0, 0, -40))
    else:
        sc.box((hx - 36, hy + 24, 44), (2.5, 6, 2), tooth, rot(0, 0, -12))
        sc.box((hx - 31, hy + 24, 44), (2.5, 6, 2), tooth, rot(0, 0, -12))
    # whiskers
    for k in range(3):
        for dz in (40, 30):
            a = hx - 40
            sc.tube([(a, hy + 14 + k * 3, dz + 6), (a - 30, hy + 6 + k * 8 - 4, dz + 14), (a - 52, hy + 4 + k * 12, dz + 10)],
                    [0.9, 0.7, 0.4], Mat((200, 190, 170)))
    # the forepaws: on the floor, or clawing the air
    for s, dz in ((0, 34), (24, 6)):
        if pose:
            sc.tube([(bx - 40 + s, by + 10, dz), (bx - 66 + s, by - 6, dz + 14), (bx - 84 + s, by - 2, dz + 20)], [8, 6, 5], fur)
            for c in range(3):
                sc.tube([(bx - 86 + s, by - 2 + c * 3, dz + 20), (bx - 98 + s, by + 6 + c * 4, dz + 22)], [1.6, 0.6], tooth)
        else:
            sc.tube([(bx - 46 + s, by + 20, dz), (bx - 60 + s, by + 44, dz + 8), (bx - 66 + s, 228, dz + 10)], [9, 7, 6], fur)
            sc.ellipsoid((bx - 72 + s, 231, dz + 14), (12, 4, 8), pink)
    return dict(glow=(1.0, 0.25, 0.15), pools=((CX + 40, 234, 120, 10, 0.85),))


# ------------------------------------------------------------------ cave spider

def spider(sc, pose):
    chitin = Mat((52, 44, 50), tex=0.5, texscale=0.07, tex2=(20, 16, 22), spec=1.0, shin=36, bump=0.02)
    hairy = Mat((60, 50, 52), tex=0.7, texscale=0.3, tex2=(26, 20, 22), bump=0.08, spec=0.3)
    mark = Mat((160, 40, 30), tex=0.6, texscale=0.05, tex2=(70, 16, 12), spec=0.8, shin=30)
    fang = Mat((40, 30, 30), spec=1.2, shin=50)
    venom = Mat((150, 255, 120), glow=0.9)
    eye = Mat((150, 255, 110), glow=1.0)
    x, y = CX, 150
    up = 1 if pose else 0
    # the abdomen, behind, with its red hourglass
    sc.ellipsoid((x + 10, y - 40, -70), (78, 64, 70), chitin, rot(-30, 0, 0))
    sc.ellipsoid((x + 10, y - 56, -24), (20, 30, 12), mark, rot(-30, 0, 0))
    # the legs: four a side, three bones each, the front pair up when it strikes
    for s in (-1, 1):
        for k in range(4):
            a = math.radians(-70 + k * 40)
            base = (x + s * 26, y + 8 + k * 4, 10 - k * 14)
            spread = 70 + k * 18
            lift = (k == 0) * up
            knee = (x + s * (spread * 0.9 + 20), y - 60 - k * 6 - lift * 40, 10 - k * 20 + math.cos(a) * 30)
            ank = (x + s * (spread + 70), y + 20 - lift * 90, 20 - k * 26)
            foot = (x + s * (spread + 82), 232 - lift * 120, 26 - k * 30) if not lift else \
                   (x + s * (spread + 40), y - 110, 70)
            sc.tube([base, knee, ank, foot], [9, 7, 5, 2], hairy)
            sc.sphere(knee, 8, chitin)
            sc.sphere(ank, 6, chitin)
    # the head and thorax
    sc.ellipsoid((x, y, 18), (40, 30, 36), chitin)
    sc.ellipsoid((x, y + 10, 46), (26, 20, 14), chitin)
    # eight eyes: two big, six small
    for dx, dy, r in ((-9, -2, 6.5), (9, -2, 6.5), (-20, -10, 4), (20, -10, 4), (-5, -14, 3.5), (5, -14, 3.5),
                      (-26, 2, 3), (26, 2, 3)):
        sc.sphere((x + dx, y + dy + 4, 54 - abs(dx) * 0.6), r + 1.2, Mat((10, 10, 10), spec=1.5, shin=60))
        sc.sphere((x + dx, y + dy + 4, 55.5 - abs(dx) * 0.6), r * 0.7, eye)
    # the fangs, apart when it strikes
    for s in (-1, 1):
        o = 10 + up * 10
        sc.tube([(x + s * 10, y + 22, 54), (x + s * (12 + o), y + 42, 62), (x + s * (6 + o * 0.5), y + 60 + up * 6, 60)],
                [6, 4, 1.2], fang)
        if pose:
            sc.sphere((x + s * (6 + o * 0.5), y + 64, 60), 2.5, venom)
    # pedipalps
    for s in (-1, 1):
        sc.tube([(x + s * 18, y + 18, 40), (x + s * 30, y + 34, 56), (x + s * 26, y + 50, 64)], [5, 4, 3], hairy)
    return dict(glow=(0.55, 1.0, 0.4), pools=((CX, 232, 190, 12, 0.85),), backdrop=dict(warm=0.75))


# ------------------------------------------------------------------ ooze

def slime(sc, pose):
    goo = Mat((90, 190, 70), tex=0.35, texscale=0.03, tex2=(40, 120, 50), spec=2.2, shin=60, sss=0.8, rim=1.8)
    goo2 = Mat((120, 220, 90), tex=0.3, texscale=0.05, tex2=(60, 150, 60), spec=2.0, shin=60, sss=0.9, rim=1.5)
    bone = Mat((200, 196, 150), tex=0.3, texscale=0.1, tex2=(120, 130, 80), spec=0.5)
    dark = Mat((10, 30, 8))
    eye = Mat((230, 255, 120), glow=1.0)
    core = Mat((180, 255, 120), glow=0.7)
    x = CX
    rise = 1 if pose else 0
    # the mass: wide at the bottom, a dome above
    sc.ellipsoid((x, 214, 0), (130, 30, 80), goo)
    sc.ellipsoid((x - 10, 170 - rise * 20, 4), (96, 62 + rise * 18, 70), goo)
    sc.ellipsoid((x + 6, 122 - rise * 40, 10), (60, 44 + rise * 12, 52), goo2)
    # drips and lumps
    for k in range(12):
        a = k * 2.4
        sc.sphere((x + math.cos(a) * (60 + k * 5), 200 - (k % 4) * 14 - rise * 10, 30 + math.sin(a) * 30), 16 - k * 0.6, goo2)
    # what it has swallowed: a skull and a femur pushing out of the surface
    sc.ellipsoid((x - 66, 186, 64), (16, 18, 14), bone, rot(0, 30, 20))
    sc.ellipsoid((x - 60, 190, 76), (5, 6, 2), dark)
    sc.tube([(x + 60, 196, 50), (x + 104, 170, 58)], 5, bone)
    sc.sphere((x + 106, 168, 58), 8, bone)
    sc.sphere((x + 58, 198, 50), 8, bone)
    # bubbles on the surface
    for k in range(9):
        a = k * 1.7 + 0.4
        sc.sphere((x + math.cos(a) * 50, 150 + math.sin(a * 1.3) * 40 - rise * 20, 66), 4 + (k % 3) * 2, goo2)
    # a pseudopod reaching out when it strikes
    if pose:
        sc.tube([(x + 40, 140, 30), (x + 90, 90, 60), (x + 120, 60, 80), (x + 116, 42, 90)], [30, 22, 14, 8], goo)
    # the eyes, floating in it
    for s in (-1, 1):
        ey = 120 - rise * 44
        sc.ellipsoid((x + s * 20, ey, 58), (13, 11, 4), dark)
        sc.sphere((x + s * 20 + 2, ey + 1, 61), 5, eye)
    sc.sphere((x - 4, 186, 44), 6, core)
    return dict(glow=(0.75, 1.0, 0.35), pools=((CX, 236, 170, 12, 0.9),), backdrop=dict(warm=0.85))


# ------------------------------------------------------------------ orc brute

def orc(sc, pose):
    skin = Mat((104, 128, 72), tex=0.45, texscale=0.08, tex2=(62, 82, 44), bump=0.04, spec=0.4, shin=14)
    leather = Mat((92, 60, 36), tex=0.5, texscale=0.1, tex2=(50, 32, 20), bump=0.03, spec=0.3)
    iron = Mat((130, 128, 126), tex=0.45, texscale=0.06, tex2=(110, 64, 40), spec=1.3, shin=40, bump=0.02)
    wood = Mat((96, 66, 40), tex=0.6, texscale=0.15, tex2=(56, 36, 22), bump=0.05)
    tusk = Mat((230, 220, 180), spec=0.8, shin=30)
    dark = Mat((16, 8, 6))
    eye = Mat((255, 200, 40), glow=1.0)
    hair = Mat((30, 24, 22), tex=0.6, texscale=0.4, tex2=(10, 8, 8), bump=0.08)
    x = CX - 10
    up = 1 if pose else 0
    # the legs, apart
    for s in (-1, 1):
        sc.tube([(x + s * 30, 170, 0), (x + s * 46, 204, 14), (x + s * 50, 228, 10)], [22, 17, 14], leather)
        sc.ellipsoid((x + s * 54, 232, 22), (20, 8, 22), leather)
    # the belly and chest
    sc.ellipsoid((x, 156, 6), (54, 34, 40), leather)
    sc.ellipsoid((x, 118, 4), (66, 50, 46), skin)
    sc.ellipsoid((x, 134, 34), (46, 28, 20), skin)
    sc.box((x, 160, 44), (50, 6, 6), leather, bevel=2)
    sc.box((x, 160, 50), (9, 8, 3), iron, bevel=1.5)
    # the shoulders: a spiked iron pauldron on the left
    sc.ellipsoid((x - 64, 84, 4), (36, 28, 34), iron, rot(0, 0, 15))
    for k in range(3):
        sc.tube([(x - 66 - k * 14, 66 - k * 2, 14), (x - 74 - k * 16, 46 - k * 6, 18)], [5, 0.8], iron)
    sc.ellipsoid((x + 60, 84, 4), (30, 26, 30), skin)
    # the head: low between the shoulders, a heavy jaw, tusks
    hx, hy = x + up * 6, 58 + up * 6
    sc.ellipsoid((hx, hy, 18), (30, 30, 28), skin)
    sc.ellipsoid((hx, hy + 22, 30), (26, 16, 20), skin)
    sc.ellipsoid((hx, hy - 6, 44), (24, 6, 6), skin)                    # the brow
    sc.ellipsoid((hx, hy - 26, 10), (26, 12, 24), hair)
    for s in (-1, 1):
        sc.ellipsoid((hx + s * 11, hy, 42), (7, 4.5, 3), dark)
        sc.sphere((hx + s * 11, hy + 0.5, 44), 2.4, eye)
        sc.ellipsoid((hx + s * 31, hy - 2, 10), (5, 12, 8), skin, rot(0, 0, s * 20))   # ears
        sc.tube([(hx + s * 14, hy + 30, 46), (hx + s * 18, hy + 16, 54), (hx + s * 15, hy + 8, 54)], [4.5, 3, 1], tusk)
    sc.ellipsoid((hx, hy + 8, 50), (7, 6, 5), skin)                       # the nose
    sc.ellipsoid((hx, hy + 26, 48), (14, 3 + up * 5, 3), dark)            # the mouth: a snarl
    # the left arm hangs, a fist
    sc.tube([(x - 70, 96, 10), (x - 86, 140, 20), (x - 80, 176, 34)], [18, 15, 13], skin)
    sc.sphere((x - 80, 182, 38), 15, skin)
    sc.box((x - 84, 128, 22), (17, 12, 18), leather, rot(0, 0, 20), bevel=3)
    # the right arm and the spiked club: down, or raised to smash
    if pose:
        el, hand = (x + 96, 64, 10), (x + 92, 24, 30)
        club = rot(0, 0, 200)
    else:
        el, hand = (x + 88, 124, 16), (x + 94, 168, 34)
        club = rot(-40, 0, 20)
    sc.tube([(x + 66, 92, 10), el, hand], [18, 15, 13], skin)
    sc.sphere(hand, 15, skin)
    d = club @ np.array([0, -1, 0], np.float32)
    h = np.array(hand, np.float32)
    sc.tube([h - d * 14, h + d * 50, h + d * 92], [7, 12, 18], wood)
    for k in range(10):
        a = k * 2.2
        p = h + d * (60 + (k % 4) * 9) + np.array([math.cos(a) * 15, math.sin(a) * 6, math.sin(a) * 15], np.float32)
        q = p + np.array([math.cos(a) * 10, math.sin(a) * 4, math.sin(a) * 10], np.float32)
        sc.tube([p, q], [3, 0.6], iron)
    return dict(glow=(1.0, 0.75, 0.2), pools=((CX - 10, 236, 120, 10, 0.85),))


# ------------------------------------------------------------------ wraith

def wraith(sc, pose):
    robe = Mat((40, 44, 58), tex=0.6, texscale=0.05, tex2=(16, 16, 24), bump=0.05, sss=0.4, rim=2.5)
    robe2 = Mat((54, 60, 78), tex=0.6, texscale=0.07, tex2=(20, 22, 30), bump=0.06, sss=0.5, rim=2.5)
    void = Mat((2, 2, 6))
    eye = Mat((170, 220, 255), glow=1.0)
    hand = Mat((150, 160, 170), tex=0.4, texscale=0.1, tex2=(90, 96, 110), spec=0.6, sss=0.3)
    mist = Mat((90, 120, 160), glow=0.35)
    x = CX
    reach = 1 if pose else 0
    # the robe: a long fall of cloth to tattered ends above the floor
    for k in range(9):
        a = (k - 4) * 0.22
        sc.tube([(x + math.sin(a) * 30, 80, math.cos(a) * 20 - 6), (x + math.sin(a) * 60, 150, math.cos(a) * 36 - 10),
                 (x + math.sin(a) * 74 + (k % 3 - 1) * 8, 200 + (k % 4) * 7, math.cos(a) * 30 - 10)],
                [24, 22, 3], robe if k % 2 else robe2)
    # the hood
    sc.ellipsoid((x, 52 - reach * 4, 2), (36, 40, 34), robe2)
    sc.ellipsoid((x, 64 - reach * 4, 28), (24, 28, 12), void)
    sc.tube([(x, 22 - reach * 4, -6), (x + 4, 4, -20), (x + 14, -6, -36)], [20, 10, 2], robe2)
    for s in (-1, 1):
        sc.sphere((x + s * 9, 60 - reach * 4, 40), 3.6, eye)
    # the sleeves and the long-fingered hands, reaching when it strikes
    for s in (-1, 1):
        sh = (x + s * 34, 92, 6)
        if reach:
            el, wr = (x + s * 70, 104, 40), (x + s * 86, 96, 76)
        else:
            el, wr = (x + s * 56, 134, 16), (x + s * 48, 160, 34)
        sc.tube([sh, el, wr], [16, 15, 13], robe)
        w = np.array(wr, np.float32) + np.array([0, 6, 6], np.float32)
        sc.ellipsoid(w, (7, 9, 6), hand)
        for f in range(4):
            off = (f - 1.5) * 4.2
            tip = w + np.array([s * (6 + reach * 10) + off * 0.5, 22 - reach * 8, 8 + reach * 14], np.float32)
            mid = (w + tip) / 2 + np.array([off, 2, 0], np.float32)
            sc.tube([w + np.array([off, 6, 2], np.float32), mid, tip], [2.2, 1.8, 0.6], hand)
    return dict(glow=(0.6, 0.85, 1.0), pools=((CX, 236, 90, 8, 0.6),),
                backdrop=dict(warm=0.35, spread=0.26), haze=1.0,
                light=dict(key_col=(0.7, 0.75, 1.0), rim_col=(0.5, 0.65, 1.0)))


# ------------------------------------------------------------------ the lich

def lich(sc, pose):
    bone = Mat((210, 200, 172), tex=0.35, texscale=0.09, tex2=(120, 104, 80), spec=0.4, shin=20, bump=0.02)
    robe = Mat((70, 30, 90), tex=0.55, texscale=0.05, tex2=(30, 12, 40), bump=0.05, spec=0.2)
    trim = Mat((200, 160, 60), tex=0.3, texscale=0.2, tex2=(120, 80, 30), spec=1.4, shin=40)
    gold = Mat((230, 180, 70), tex=0.25, texscale=0.1, tex2=(150, 100, 40), spec=1.6, shin=50)
    wood = Mat((60, 44, 36), tex=0.6, texscale=0.14, tex2=(30, 22, 18), bump=0.05)
    dark = Mat((10, 4, 14))
    eye = Mat((200, 120, 255), glow=1.0)
    orb = Mat((220, 160, 255), glow=1.0)
    gem = Mat((90, 255, 160), glow=0.8)
    x = CX - 16
    cast = 1 if pose else 0
    # the robe: a tall bell of cloth, narrow at the shoulders; a high collar
    for k in range(48):
        t = k / 47
        sc.ellipsoid((x, 92 + t * 128, 0), (40 + t * 34, 14, 30 + t * 16), robe)
    for k in range(-3, 4):
        sc.box((x + k * 17, 206, 38 - abs(k) * 6), (7, 26, 3), robe, rot(0, k * 12, k * 2))
        sc.box((x + k * 17, 232, 40 - abs(k) * 6), (7.5, 3, 3.5), trim, rot(0, k * 12, k * 2))
    sc.ellipsoid((x, 96, 6), (52, 18, 36), robe)
    sc.ellipsoid((x, 58, -14), (34, 44, 26), robe)                      # the cowl behind the head
    sc.tube(_arc(x, 86, 2, 46, 36, -120, 120, 5), 4.5, trim)
    sc.tube([(x, 100, 36), (x, 200, 50)], 3.5, trim)
    sc.ellipsoid((x, 122, 40), (7, 9, 4), gem)
    # the skull and its crown
    hy = 54
    sc.ellipsoid((x, hy, 10), (23, 26, 22), bone)
    sc.ellipsoid((x, hy + 24, 16), (15, 9, 12), bone, rot(-10 * cast, 0, 0))
    for s in (-1, 1):
        sc.ellipsoid((x + s * 9, hy + 2, 29), (7, 8, 3), dark)
        sc.sphere((x + s * 9, hy + 3, 31), 3.0, eye)
        sc.ellipsoid((x + s * 18, hy + 11, 14), (5, 8, 9), bone)
    sc.ellipsoid((x, hy + 13, 31), (2.8, 4, 2), dark)
    for k in range(-3, 4):
        sc.box((x + k * 3.3, hy + 19, 29), (1.3, 2.4, 1.5), bone)
    sc.cylinder((x, hy - 16, 8), 21, 5, gold)
    for k in range(7):
        a = math.radians(-90 + (k - 3) * 30)
        sc.tube([(x + math.cos(a) * 19, hy - 20, 8 + math.sin(-a) * 19 * 0 + (12 - abs(k - 3) * 4)),
                 (x + math.cos(a) * 20, hy - 34 - (k % 2) * 8, 8 + (12 - abs(k - 3) * 4))], [3, 0.8], gold)
    sc.sphere((x, hy - 18, 30), 3, gem)
    # the left hand: a staff with an orb
    sl = (x - 48, 96, 8)
    if cast:
        el, hd = (x - 78, 96, 20), (x - 86, 72, 30)
    else:
        el, hd = (x - 70, 130, 24), (x - 76, 152, 42)
    sc.tube([sl, el, hd], [14, 12, 6], robe)
    sc.sphere(hd, 6, bone)
    top = np.array(hd, np.float32) + np.array([-4, -42 if cast else -60, 0], np.float32)
    bot = np.array(hd, np.float32) + np.array([4, 70 if cast else 80, 0], np.float32)
    sc.tube([bot, top], [3.5, 4.5], wood)
    for a in range(0, 360, 60):
        r = math.radians(a)
        sc.tube([top, top + np.array([math.cos(r) * 10, -14, math.sin(r) * 10], np.float32)], [2, 1], gold)
    sc.sphere(top + np.array([0, -16, 0], np.float32), 10 + cast * 3, orb)
    # the right hand: open, casting, a ring of sparks over it when it does
    sr = (x + 48, 96, 8)
    if cast:
        el, hd = (x + 82, 92, 40), (x + 96, 70, 70)
    else:
        el, hd = (x + 66, 136, 24), (x + 60, 160, 44)
    sc.tube([sr, el, hd], [14, 12, 6], robe)
    w = np.array(hd, np.float32)
    sc.ellipsoid(w, (6, 7, 5), bone)
    for f in range(4):
        off = (f - 1.5) * 4
        tip = w + np.array([off * 1.6, -16 if cast else 16, 6], np.float32)
        sc.tube([w + np.array([off, 0, 2], np.float32), tip], [1.8, 1.0], bone)
    if cast:
        for k in range(10):
            a = k * 0.63
            sc.sphere(w + np.array([math.cos(a) * 26, -30 + math.sin(a) * 10, math.sin(a) * 26], np.float32),
                      2.5 + (k % 3), orb)
    return dict(glow=(0.8, 0.5, 1.0), pools=((CX - 16, 236, 110, 10, 0.85),),
                backdrop=dict(warm=0.6), haze=0.9)


# ------------------------------------------------------------------ what they are
#
# Everything about a creature but its tile (tiles.py) is here: its look
# (the function above), its numbers, what it does in a fight, which levels
# it lives on, its words. gen_art.py puts the fight's part in the header of
# its close-up's file (the game reads it when the fight starts) and the
# rest in the game's tables (assets.h: M_<NAME>, mon_tile, level_mons...).

# what a creature can do in a fight (the game's A_* numbers, in order)
ACTIONS = ["NONE", "MELEE", "HEAVY", "GUARD", "POISON", "WEB", "SPIT", "REGEN", "ROAR", "DRAIN",
           "CHILL", "PHASE", "BOLT", "CURSE", "LUNGE", "CLOSE"]
# flags (the game's MF_*): burns easily, shrugs off frost (2 in 3), hard to
# flee from, its lunge is venomous, moves every other turn, the last foe,
# dead and walking (no hunger: meat thrown to it is not eaten)
FLAGS = ["FIREWEAK", "COLDPROOF", "HARDFLEE", "VENOM", "SLOW", "BOSS", "UNDEAD"]
# what it does on the map (the game's MH_*): shoots arrows from afar,
# snatches fuel or arrows and runs, dissolves what lies in its way
HABITS = ["ARCHER", "THIEF", "EATER"]


def info(name, tele, hit, lore, death, hp, dice, sides, plus, defense, xp, acts, depths, flags=(), habits=()):
    """acts: (action, weight) a fight's choices; depths: {level: weight}"""
    return dict(name=name, tele=tele, hit=hit, lore=lore, death=death, hp=hp, dice=dice, sides=sides,
                plus=plus, defense=defense, xp=xp, acts=acts, depths=depths, flags=flags, habits=habits)


MONSTERS = [
    ("rat", rat, info(
        "giant rat", "bares its yellow teeth", "bites you",
        "A rat the size of a dog, slick with sewer filth.", "falls, and moves no more.",
        hp=9, dice=1, sides=4, plus=1, defense=0, xp=5,
        acts=[("MELEE", 5), ("LUNGE", 3), ("POISON", 2)], depths={1: 2, 2: 1}, habits=("THIEF",))),
    ("skeleton", skeleton, info(
        "skeleton", "swings its notched blade", "cuts you",
        "Old bones bound by older hate. Its eyes are embers.", "falls apart in a clatter of bones.",
        hp=18, dice=1, sides=6, plus=2, defense=2, xp=12,
        acts=[("MELEE", 4), ("HEAVY", 3), ("GUARD", 2)], depths={1: 1, 2: 1, 3: 1, 5: 1},
        flags=("UNDEAD",), habits=("ARCHER",))),
    ("spider", spider, info(
        "cave spider", "rears, fangs dripping", "bites you",
        "Eight green eyes catch your torchlight.", "curls up its legs and is still.",
        hp=15, dice=1, sides=6, plus=1, defense=1, xp=14,
        acts=[("POISON", 4), ("WEB", 2), ("LUNGE", 3)], depths={2: 1, 3: 1, 4: 1},
        flags=("FIREWEAK", "VENOM"))),
    ("slime", slime, info(
        "ooze", "surges up to engulf you", "burns you",
        "A quivering mass. Something it ate stares out of it.", "dissolves.",
        hp=26, dice=1, sides=6, plus=1, defense=0, xp=16,
        acts=[("MELEE", 3), ("SPIT", 3), ("REGEN", 2)], depths={1: 1, 2: 1, 3: 1},
        flags=("FIREWEAK", "SLOW"), habits=("EATER",))),
    ("orc", orc, info(
        "orc brute", "swings its spiked club", "clubs you",
        "It grins with too many tusks and hefts its club.", "falls, and moves no more.",
        hp=30, dice=1, sides=8, plus=3, defense=2, xp=24,
        acts=[("MELEE", 3), ("HEAVY", 4), ("ROAR", 2)], depths={3: 1, 4: 2, 5: 1})),
    ("wraith", wraith, info(
        "wraith", "reaches for your heart", "chills you",
        "Cold pours from the hood. Your torch gutters.", "comes apart in the wind.",
        hp=26, dice=1, sides=6, plus=3, defense=3, xp=32,
        acts=[("DRAIN", 4), ("CHILL", 3), ("PHASE", 2)], depths={4: 1, 5: 2},
        flags=("HARDFLEE", "UNDEAD"))),
    ("lich", lich, info(
        "lich", "raises its staff to strike", "smites you",
        "The master of the deep. It has waited a long time.", "crumbles, and its crown rolls away.",
        hp=80, dice=2, sides=6, plus=3, defense=3, xp=120,
        acts=[("BOLT", 4), ("CURSE", 2), ("HEAVY", 2), ("GUARD", 1)], depths={},
        flags=("COLDPROOF", "HARDFLEE", "BOSS", "UNDEAD"))),
]
