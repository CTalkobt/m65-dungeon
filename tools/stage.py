"""stage.py — the close-ups' setting: a dungeon wall and floor in torchlight,
a creature (or a chest, an idol) in front, its shadow, the dark round the
edges. Everything a scene file needs from an RGB picture: its palette, its
glow, its characters."""
import numpy as np
from artlib import Scene, fbm, blur, tonemap, to8, quantize, remap, blocks, bayer

W, H = 448, 240          # 56 x 30 characters
FLOOR_Y = 206            # where the floor meets the wall (the creature stands lower)


def backdrop(w=W, h=H, floor_y=FLOOR_Y, light=(0.42, 0.38), spread=0.30, warm=1.0, seed=0,
             stone=(0.42, 0.38, 0.34), ambient=0.02):
    """rough stone blocks and a flagstone floor going away, lit by the
    player's torch (a pool of light round light, as fractions of the size)
    and dark beyond it"""
    ys, xs = np.mgrid[0:h, 0:w].astype(np.float32)
    # the wall: courses of blocks of uneven size, staggered, chipped
    bh = 24.0
    wob = (fbm(xs * 0.02, ys * 0.02, 2, seed + 1) - 0.5) * 7
    yy = ys + wob
    row = np.floor(yy / bh)
    off = fbm(row * 0.9, row * 0.37, 1, seed) * 60
    bw = 40.0 + fbm(row * 1.3, row * 0.2, 1, seed + 2) * 30
    xx = xs + off + (fbm(xs * 0.03, ys * 0.05, 2, seed + 4) - 0.5) * 6
    col = np.floor(xx / bw)
    u = xx - col * bw
    v = yy - row * bh
    chip = fbm(xs * 0.25, ys * 0.25, 2, seed + 6) * 2.4
    mort = np.minimum(np.minimum(u, bw - u), np.minimum(v, bh - v)) - chip
    mortar = np.clip(1 - mort / 1.8, 0, 1)
    blockshade = fbm(col * 3.1 + row * 7.3, row * 1.7, 1, seed + 3) * 0.5 + 0.5
    grain = fbm(xs * 0.13, ys * 0.13, 5, seed + 5)
    pits = np.clip((fbm(xs * 0.08, ys * 0.08, 3, seed + 8) - 0.62) * 4, 0, 1)
    st = np.array(stone, np.float32)
    wall = st * (blockshade * (0.6 + grain * 0.7) * (1 - pits * 0.5))[..., None]
    # the blocks' upper and left edges catch the light, their lower ones are in shadow
    top = np.clip(1 - np.minimum(u, v) / 3.5, 0, 1) * (1 - mortar)
    bot = np.clip(1 - np.minimum(bw - u, bh - v) / 3.5, 0, 1) * (1 - mortar)
    wall = wall * (1 + top[..., None] * 0.4 - bot[..., None] * 0.35)
    wall = wall * (1 - mortar[..., None] * 0.85)
    # damp and moss low on the wall, streaks running down
    streak = fbm(xs * 0.09, ys * 0.004, 3, seed + 10)
    damp = np.clip((ys - floor_y * 0.45) / (floor_y * 0.55), 0, 1) * fbm(xs * 0.03, ys * 0.05, 3, seed + 9)
    damp = np.clip(damp + (streak - 0.55) * 0.8, 0, 1)
    wall = wall * (1 - damp[..., None] * np.array([0.45, 0.25, 0.4], np.float32))
    # the floor: flagstones in perspective below floor_y
    fy = np.maximum(ys - floor_y, 0.0)
    depth = 60.0 / (fy + 6.0)
    fx = (xs - w / 2) * depth / 8.0
    fz = depth * 12.0
    cellz = np.floor(fz / 2.0)
    sx = fx / 3.0 + cellz * 0.5
    cellx = np.floor(sx)
    fu = sx - cellx
    fv = fz / 2.0 - cellz
    fm = np.minimum(np.minimum(fu, 1 - fu), np.minimum(fv, 1 - fv)) / (depth * 0.05 + 0.02)
    fmortar = np.clip(1 - fm, 0, 1)
    fshade = fbm(cellx * 2.3 + cellz * 5.1, cellz * 1.3, 1, seed + 11) * 0.4 + 0.6
    floor = st * 1.1 * (fshade * (0.75 + fbm(fx * 2, fz * 2, 3, seed + 13) * 0.5))[..., None]
    floor = floor * (1 - fmortar[..., None] * 0.8)
    img = np.where((ys >= floor_y)[..., None], floor, wall)
    # the torch's pool of light; beyond it the dark
    lx, ly = light[0] * w, light[1] * h
    d = np.sqrt((xs - lx) ** 2 + ((ys - ly) * 1.25) ** 2) / (w * spread)
    torch = 1.6 / (1 + d * d * 2.2) - 0.08
    torch = np.clip(torch, 0, None) * np.where(ys >= floor_y, 0.7 + 0.5 * np.clip(fy / (h - floor_y), 0, 1), 1.0)
    warmth = np.array([1.0, 0.7, 0.42], np.float32) * warm + np.array([0.35, 0.45, 0.7], np.float32) * (1 - warm)
    lit = img * (torch[..., None] * warmth + np.array([0.5, 0.6, 1.0], np.float32) * ambient)
    return lit


def shadow_of(mask, dx=26, dy=6, r=7):
    """the shape's shadow cast on the wall behind (to the lower right, away
    from the torch): 0 none .. 1 dark"""
    h, w = mask.shape
    m = np.zeros((h, w), np.float32)
    ys0, ys1 = max(0, dy), min(h, h + dy)
    xs0, xs1 = max(0, dx), min(w, w + dx)
    m[ys0:ys1, xs0:xs1] = mask[ys0 - dy:ys1 - dy, xs0 - dx:xs1 - dx]
    return np.clip(blur(m, r) * 1.2, 0, 1)


def contact(w, h, cx, cy, rx, ry):
    """a soft dark pool under something standing at cx, cy"""
    ys, xs = np.mgrid[0:h, 0:w].astype(np.float32)
    d = ((xs - cx) / rx) ** 2 + ((ys - cy) / ry) ** 2
    return np.clip(1.2 - d, 0, 1) ** 1.5


def vignette(w, h, strength=0.85):
    ys, xs = np.mgrid[0:h, 0:w].astype(np.float32)
    d = ((xs - w / 2) / (w * 0.62)) ** 2 + ((ys - h * 0.45) / (h * 0.75)) ** 2
    return np.clip(1 - d * strength, 0.0, 1)


def compose(sc, lit, mask, back, cast=0.75, pools=(), exposure=1.0, haze=0.0):
    """the creature over the backdrop with its shadow and pools of dark;
    the dark round the edges; tone-mapped (0..1 RGB) and the glow
    intensity (0 where nothing glows)"""
    h, w = mask.shape
    sh = shadow_of(mask)
    b = back * (1 - sh[..., None] * cast)
    for (cx, cy, rx, ry, k) in pools:
        b = b * (1 - contact(w, h, cx, cy, rx, ry)[..., None] * k)
    img = np.where(mask[..., None], lit, b)
    if haze:
        # dust in the torchlight: a faint warm veil, thicker near the torch
        ys, xs = np.mgrid[0:h, 0:w].astype(np.float32)
        veil = fbm(xs * 0.02, ys * 0.03, 3, 77) * haze
        img = img + veil[..., None] * np.array([0.10, 0.07, 0.04], np.float32)
    img = img * vignette(w, h)[..., None]
    out = tonemap(img, exposure)
    glow = np.where(mask & (sc.glow > 0), sc.glow, 0.0)
    return out, glow


GLOW_N = 8               # the glow's palette entries (247-254)
SCENE_N = 119            # the rest of the scene's (128-246)


def palette_for(frames, glow_col):
    """one palette (128-254) for all the frames of a scene: 119 colours by
    median cut over the frames, then the glow ramp; each frame as indices"""
    rgb = [to8(f) for f, g in frames]
    big = np.concatenate([r.reshape(-1, 3) for r in rgb])[None, ...]
    pal, _ = quantize(big, SCENE_N, dither=False)
    gc = np.array(glow_col, np.float32)
    ramp = np.array([to8(np.clip(gc * (0.25 + 0.75 * k / (GLOW_N - 1)), 0, 1)) for k in range(GLOW_N)])
    out = []
    for (f, g), r in zip(frames, rgb):
        idx = remap(to8(bayer(f)), pal, dither=False) + 128
        gl = g > 0
        if gl.any():
            gv = np.clip(np.rint(g / max(g.max(), 1e-6) * (GLOW_N - 1)), 0, GLOW_N - 1).astype(np.int32)
            idx = np.where(gl, 128 + SCENE_N + gv, idx)
        out.append(idx)
    full = np.zeros((256, 3), np.uint8)
    full[128:128 + SCENE_N] = pal
    full[128 + SCENE_N:128 + SCENE_N + GLOW_N] = ramp
    return full, out
