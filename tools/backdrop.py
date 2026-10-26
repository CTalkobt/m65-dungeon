"""backdrop.py — the close-ups' shared backdrop: a stone wall and a
flagstone floor in a pool of torchlight, the same behind every creature

It is made of BACK_N characters, kept once (in the C64 ROM's space, DGBACK)
rather than in every close-up: a tileable wall (WALL_W x WALL_H characters)
at WALL_LV light levels and a floor (its rows, FLOOR_W characters across)
at FLOOR_LV levels. Each cell of the 56 x 30 picture takes the level the
torch gives it there (dithered between neighbouring levels, 4 x 4 cells),
or none (black) in the dark round the edges.

image() is the picture in RGB as the characters make it; cells() which
character each cell shows (-1 black). The creatures are drawn over it
(stage.py) and only the cells they change get characters of their own.
"""
import numpy as np
from artlib import Mat  # noqa: F401  (the same colours as the rest)
from artlib import tonemap

CW, CH = 56, 30                      # the picture, in characters
FLOOR_ROW = 26                       # the floor's first row (y 208)
WALL_W, WALL_H = 8, 4                # the wall's pattern: 64 x 32 pixels
FLOOR_W = 4                          # the floor's pattern across: 32 pixels
WALL_LV = [0.06, 0.12, 0.22, 0.38, 0.65, 1.10]
FLOOR_LV = [0.10, 0.25, 0.55, 1.05]
BACK_N = WALL_W * WALL_H * len(WALL_LV) + FLOOR_W * (CH - FLOOR_ROW) * len(FLOOR_LV)
LIGHT = (0.42, 0.38)                 # the torch's pool, as fractions of the picture
SPREAD = 0.30
WARM = np.array([1.0, 0.7, 0.42], np.float32)
AMBIENT = np.array([0.5, 0.6, 1.0], np.float32) * 0.02
BAYER = (np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], np.float32) + 0.5) / 16


def periodic_noise(h, w, seed, power=1.6):
    """smooth noise that tiles (h x w), 0..1: random phases, a 1/f^power spectrum"""
    rng = np.random.RandomState(seed)
    fy = np.fft.fftfreq(h)[:, None]
    fx = np.fft.fftfreq(w)[None, :]
    f = np.sqrt(fx * fx + fy * fy)
    f[0, 0] = 1
    spec = (rng.randn(h, w) + 1j * rng.randn(h, w)) / f ** power
    spec[0, 0] = 0
    n = np.real(np.fft.ifft2(spec))
    return (n - n.min()) / (n.max() - n.min())


def wall_albedo():
    """the wall's pattern (WALL_H*8 x WALL_W*8 RGB), unlit: courses of
    stones 16 pixels high and uneven widths (each course's add to the
    pattern's 64), chipped mortar, damp"""
    h, w = WALL_H * 8, WALL_W * 8
    ys, xs = np.mgrid[0:h, 0:w].astype(np.float32)
    courses = [(0, [40, 24]), (6, [20, 44])]         # (offset, widths) of courses 0 and 1
    u = np.zeros((h, w), np.float32)
    bw = np.ones((h, w), np.float32)
    stone_id = np.zeros((h, w), np.int32)
    for y in range(h):
        c = (y // 16) % 2
        off, widths = courses[c]
        for x in range(w):
            p = (x + off) % w
            start, k = 0, 0
            while p >= start + widths[k]:
                start += widths[k]
                k += 1
            u[y, x] = p - start
            bw[y, x] = widths[k]
            stone_id[y, x] = (y // 16) * 4 + k
    v = ys % 16
    chip = periodic_noise(h, w, 6, 0.8) * 2.2
    mort = np.minimum(np.minimum(u, bw - u), np.minimum(v, 16 - v)) - chip
    mortar = np.clip(1 - mort / 1.6, 0, 1)
    rng = np.random.RandomState(3)
    shade = rng.uniform(0.55, 1.0, 16)[stone_id]
    grain = periodic_noise(h, w, 5, 1.2)
    pits = np.clip((periodic_noise(h, w, 8, 1.0) - 0.62) * 4, 0, 1)
    stone = np.array([0.42, 0.38, 0.34], np.float32)
    wall = stone * (shade * (0.6 + grain * 0.7) * (1 - pits * 0.5))[..., None]
    top = np.clip(1 - np.minimum(u, v) / 3.5, 0, 1) * (1 - mortar)
    bot = np.clip(1 - np.minimum(bw - u, 16 - v) / 3.5, 0, 1) * (1 - mortar)
    wall = wall * (1 + top[..., None] * 0.4 - bot[..., None] * 0.35)
    wall = wall * (1 - mortar[..., None] * 0.85)
    damp = np.clip((periodic_noise(h, w, 9, 1.4) - 0.5) * 1.2, 0, 1)
    return wall * (1 - damp[..., None] * np.array([0.45, 0.25, 0.4], np.float32))


def floor_albedo():
    """the floor's rows ((CH - FLOOR_ROW)*8 x FLOOR_W*8 RGB): flagstones
    growing toward the viewer, joints between"""
    h, w = (CH - FLOOR_ROW) * 8, FLOOR_W * 8
    ys, xs = np.mgrid[0:h, 0:w].astype(np.float32)
    bands = [0, 5, 12, 21, h]                      # each course deeper than the one behind
    band = np.zeros((h, w), np.int32)
    for k in range(len(bands) - 1):
        band[(ys >= bands[k]) & (ys < bands[k + 1])] = k
    off = (band % 2) * 16
    u = (xs + off) % 32
    rng = np.random.RandomState(11)
    shade = rng.uniform(0.6, 1.0, (len(bands), 2))[band, ((xs + off) // 32 % 2).astype(int)]
    grain = periodic_noise(h, w, 13, 1.3)
    stone = np.array([0.46, 0.42, 0.37], np.float32)
    floor = stone * (shade * (0.75 + grain * 0.5))[..., None]
    joint = np.minimum(u, 32 - u) < 1.2
    for b in bands[1:-1]:
        joint |= np.abs(ys - b) < 0.8
    floor[joint] *= 0.25
    return floor


def light_map():
    """each cell's light (0 dark .. about 1.5): the torch's pool, the
    vignette, the far floor dimmer"""
    cy, cx = np.mgrid[0:CH, 0:CW].astype(np.float32)
    px, py = (cx + 0.5) * 8, (cy + 0.5) * 8
    w, h = CW * 8, CH * 8
    d = np.sqrt((px - LIGHT[0] * w) ** 2 + ((py - LIGHT[1] * h) * 1.25) ** 2) / (w * SPREAD)
    torch = np.clip(1.6 / (1 + d * d * 2.2) - 0.08, 0, None)
    vig = np.clip(1 - (((px - w / 2) / (w * 0.62)) ** 2 + ((py - h * 0.45) / (h * 0.75)) ** 2) * 0.85, 0, 1)
    fl = cy >= FLOOR_ROW
    torch = torch * vig
    return np.where(fl, torch * (1.1 + 0.5 * (cy - FLOOR_ROW) / (CH - FLOOR_ROW)), torch)


def _lit(albedo, m):
    return tonemap(albedo * (m * WARM + AMBIENT))


def chars():
    """the BACK_N characters (8 x 8 RGB) and, for building the cells, how
    they are numbered: wall (level, y, x), floor (level, row, x)"""
    out = []
    wa, fa = wall_albedo(), floor_albedo()
    for m in WALL_LV:
        img = _lit(wa, m)
        for y in range(WALL_H):
            for x in range(WALL_W):
                out.append(img[y * 8:y * 8 + 8, x * 8:x * 8 + 8])
    for m in FLOOR_LV:
        img = _lit(fa, m)
        for y in range(CH - FLOOR_ROW):
            for x in range(FLOOR_W):
                out.append(img[y * 8:y * 8 + 8, x * 8:x * 8 + 8])
    assert len(out) == BACK_N
    return out


def _level(light, levels, cx, cy):
    """the level for a cell's light, dithered between the two nearest
    (-1: darker than the lowest)"""
    lv = np.array(levels, np.float32)
    if light < lv[0] * 0.4:
        return -1
    k = int(np.searchsorted(lv, light))
    if k == 0:
        t = min(1.0, max(0.0, (light / lv[0] - 0.4) / 0.6))
        return 0 if t > BAYER[cy % 4, cx % 4] else -1
    if k >= len(lv):
        return len(lv) - 1
    t = (light - lv[k - 1]) / (lv[k] - lv[k - 1])
    t = min(1.0, max(0.0, (t - 0.5) * 3 + 0.5))    # solid levels, dithered only where they meet
    return k if t > BAYER[cy % 4, cx % 4] else k - 1


def cells():
    """the character (0..BACK_N-1) each cell shows, -1 black: CH x CW"""
    light = light_map()
    out = np.full((CH, CW), -1, np.int32)
    nwall = WALL_W * WALL_H
    nfrow = (CH - FLOOR_ROW) * FLOOR_W
    for cy in range(CH):
        for cx in range(CW):
            if cy < FLOOR_ROW:
                k = _level(light[cy, cx], WALL_LV, cx, cy)
                if k >= 0:
                    out[cy, cx] = k * nwall + (cy % WALL_H) * WALL_W + cx % WALL_W
            else:
                k = _level(light[cy, cx], FLOOR_LV, cx, cy)
                if k >= 0:
                    out[cy, cx] = len(WALL_LV) * nwall + k * nfrow + (cy - FLOOR_ROW) * FLOOR_W + cx % FLOOR_W
    return out


def image(ch_rgb=None):
    """the picture as the characters make it (RGB): ch_rgb the characters
    as they will look (quantized), else as made"""
    ch_rgb = chars() if ch_rgb is None else ch_rgb
    c = cells()
    img = np.zeros((CH * 8, CW * 8, 3), np.float32)
    for cy in range(CH):
        for cx in range(CW):
            if c[cy, cx] >= 0:
                img[cy * 8:cy * 8 + 8, cx * 8:cx * 8 + 8] = ch_rgb[c[cy, cx]]
    return img
