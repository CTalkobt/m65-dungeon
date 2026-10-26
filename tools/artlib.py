"""artlib.py — the art's building blocks: noise, a small renderer, palettes
and full-colour characters.

The renderer draws shapes made of ellipsoids, spheres (and tubes of them),
boxes and cylinders into a depth buffer, orthographically, looking down -z
(x right, y down, z toward the viewer, all in pixels). Each shape has a
material (colour, texture, shine, glow). Lighting comes afterwards, once,
for every pixel: a warm torch from the upper left, a cool fill, a rim from
behind, the shapes' shadows on each other (marched through the depth
buffer) and darkening in creases (the depth against a blurred depth).
"""
import math
import numpy as np
from PIL import Image

# ---------------------------------------------------------------- noise

_rng = np.random.RandomState(1981)
_PERM = np.concatenate([_rng.permutation(256)] * 2).astype(np.int32)
_VAL = _rng.rand(256).astype(np.float32)


def _hash(ix, iy, seed):
    return _VAL[_PERM[(_PERM[(ix + seed) & 255] + iy) & 255]]


def vnoise(x, y, seed=0):
    """value noise in 0..1 at arrays x, y (smooth, period 256)"""
    x = np.asarray(x, np.float32)
    y = np.asarray(y, np.float32)
    ix = np.floor(x).astype(np.int32)
    iy = np.floor(y).astype(np.int32)
    fx = x - ix
    fy = y - iy
    fx = fx * fx * (3 - 2 * fx)
    fy = fy * fy * (3 - 2 * fy)
    a = _hash(ix, iy, seed)
    b = _hash(ix + 1, iy, seed)
    c = _hash(ix, iy + 1, seed)
    d = _hash(ix + 1, iy + 1, seed)
    return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy


def fbm(x, y, octaves=4, seed=0, lac=2.0, gain=0.5):
    t = np.zeros(np.broadcast(x, y).shape, np.float32)
    amp, tot, f = 1.0, 0.0, 1.0
    for o in range(octaves):
        t += amp * vnoise(x * f, y * f, seed + o * 31)
        tot += amp
        amp *= gain
        f *= lac
    return t / tot


def blur(a, r):
    """a box blur, three times (about a Gaussian), radius r pixels"""
    a = a.astype(np.float32)
    for _ in range(3):
        for axis in (0, 1):
            k = 2 * r + 1
            p = np.pad(a, [(r, r) if i == axis else (0, 0) for i in range(2)], mode="edge")
            c = np.cumsum(p, axis=axis, dtype=np.float64)
            c = np.concatenate([np.zeros_like(c.take([0], axis=axis)), c], axis=axis)
            n = a.shape[axis]
            a = ((c.take(range(k, k + n), axis=axis) - c.take(range(0, n), axis=axis)) / k).astype(np.float32)
    return a


# ---------------------------------------------------------------- materials

class Mat:
    def __init__(self, col, tex=0.0, texscale=0.08, spec=0.0, shin=16, glow=0.0, bump=0.0,
                 tex2=None, rim=1.0, sss=0.0, emit=0.0):
        self.col = np.array(col, np.float32) / 255.0
        self.tex = tex              # how much the noise darkens / lightens it
        self.texscale = texscale    # the noise's scale (1 / pixels)
        self.spec = spec
        self.shin = shin
        self.glow = glow            # 0: lit; > 0: emits its own colour (the pulsing glow)
        self.bump = bump            # the noise tilts the surface
        self.tex2 = None if tex2 is None else np.array(tex2, np.float32) / 255.0   # the noise's second colour
        self.rim = rim
        self.sss = sss              # light through it (slime, wraith)
        self.emit = emit            # > 0: shines by itself, steadily (flames)


def rot(rx=0.0, ry=0.0, rz=0.0):
    """a rotation matrix from angles in degrees (x, then y, then z)"""
    rx, ry, rz = (math.radians(a) for a in (rx, ry, rz))
    cx, sx, cy, sy, cz, sz = math.cos(rx), math.sin(rx), math.cos(ry), math.sin(ry), math.cos(rz), math.sin(rz)
    X = np.array([[1, 0, 0], [0, cx, -sx], [0, sx, cx]], np.float32)
    Y = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]], np.float32)
    Z = np.array([[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]], np.float32)
    return Z @ Y @ X


# ---------------------------------------------------------------- the renderer

class Scene:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.z = np.full((h, w), -1e9, np.float32)
        self.n = np.zeros((h, w, 3), np.float32)
        self.alb = np.zeros((h, w, 3), np.float32)
        self.spec = np.zeros((h, w), np.float32)
        self.shin = np.ones((h, w), np.float32)
        self.glow = np.zeros((h, w), np.float32)
        self.rim = np.ones((h, w), np.float32)
        self.sss = np.zeros((h, w), np.float32)
        self.emit = np.zeros((h, w), np.float32)
        self.id = np.zeros((h, w), np.int32)
        self.count = 0

    def _box(self, cx, cy, r):
        x0 = max(0, int(cx - r) - 1)
        x1 = min(self.w, int(cx + r) + 2)
        y0 = max(0, int(cy - r) - 1)
        y1 = min(self.h, int(cy + r) + 2)
        return x0, x1, y0, y1

    def _write(self, x0, y0, hit, z, n, local, mat):
        """the hits nearer than what is there: their surface"""
        if not hit.any():
            return
        sl = (slice(y0, y0 + hit.shape[0]), slice(x0, x0 + hit.shape[1]))
        zb = self.z[sl]
        m = hit & (z > zb)
        if not m.any():
            return
        self.count += 1
        zb[m] = z[m]
        col = np.broadcast_to(mat.col, n.shape).copy()
        if mat.tex or mat.bump:
            s = mat.texscale
            u = local[..., 0] * s + local[..., 2] * s * 0.7
            v = local[..., 1] * s
            seed = (self.count * 17) & 255
            t = fbm(u, v, 4, seed)
            if mat.tex:
                if mat.tex2 is not None:
                    k = np.clip((t - 0.5) * mat.tex * 2 + 0.5, 0, 1)[..., None]
                    col = col * (1 - k) + mat.tex2 * k
                else:
                    col = col * (1 + (t[..., None] - 0.5) * 2 * mat.tex)
            if mat.bump:
                e = 0.7
                tx = fbm(u + e * s, v, 3, seed) - fbm(u - e * s, v, 3, seed)
                ty = fbm(u, v + e * s, 3, seed) - fbm(u, v - e * s, 3, seed)
                n = n.copy()
                n[..., 0] -= tx * mat.bump * 10
                n[..., 1] -= ty * mat.bump * 10
                n /= np.linalg.norm(n, axis=-1, keepdims=True) + 1e-9
        self.n[sl][m] = n[m]
        self.alb[sl][m] = np.clip(col[m], 0, 2)
        self.spec[sl][m] = mat.spec
        self.shin[sl][m] = mat.shin
        self.glow[sl][m] = mat.glow
        self.rim[sl][m] = mat.rim
        self.sss[sl][m] = mat.sss
        self.emit[sl][m] = mat.emit
        self.id[sl][m] = self.count

    def ellipsoid(self, c, r, mat, R=None):
        c = np.array(c, np.float32)
        r = np.array(r, np.float32)
        R = np.eye(3, dtype=np.float32) if R is None else R
        x0, x1, y0, y1 = self._box(c[0], c[1], float(r.max()))
        if x0 >= x1 or y0 >= y1:
            return
        ys, xs = np.mgrid[y0:y1, x0:x1].astype(np.float32)
        RT = R.T
        o = np.stack([xs - c[0], ys - c[1], np.full_like(xs, 1e4)], -1) @ RT.T
        d = RT @ np.array([0, 0, -1], np.float32)
        O = o / r
        D = d / r
        a = float(D @ D)
        b = 2 * (O @ D)
        cc = (O * O).sum(-1) - 1
        disc = b * b - 4 * a * cc
        hit = disc >= 0
        t = (-b - np.sqrt(np.maximum(disc, 0))) / (2 * a)
        z = c[2] + 1e4 - t
        q = o + t[..., None] * d          # the local hit point
        nl = q / (r * r)
        n = nl @ R.T
        n /= np.linalg.norm(n, axis=-1, keepdims=True) + 1e-9
        self._write(x0, y0, hit, z, n, q, mat)

    def sphere(self, c, r, mat):
        self.ellipsoid(c, (r, r, r), mat)

    def tube(self, pts, radii, mat, step=0.25):
        """spheres along a polyline, the radius going from point to point"""
        pts = [np.array(p, np.float32) for p in pts]
        if np.isscalar(radii):
            radii = [radii] * len(pts)
        for i in range(len(pts) - 1):
            a, b = pts[i], pts[i + 1]
            ra, rb = radii[i], radii[i + 1]
            L = float(np.linalg.norm(b - a))
            k = max(1, int(L / max(0.5, step * min(ra, rb))))
            for j in range(k + (1 if i == len(pts) - 2 else 0)):
                t = j / k
                self.sphere(a + (b - a) * t, ra + (rb - ra) * t, mat)

    def box(self, c, half, mat, R=None, bevel=0.0):
        c = np.array(c, np.float32)
        hb = np.array(half, np.float32)
        R = np.eye(3, dtype=np.float32) if R is None else R
        x0, x1, y0, y1 = self._box(c[0], c[1], float(np.linalg.norm(hb)))
        if x0 >= x1 or y0 >= y1:
            return
        ys, xs = np.mgrid[y0:y1, x0:x1].astype(np.float32)
        RT = R.T
        o = np.stack([xs - c[0], ys - c[1], np.full_like(xs, 1e4)], -1) @ RT.T
        d = RT @ np.array([0, 0, -1], np.float32)
        with np.errstate(divide="ignore", invalid="ignore"):
            inv = 1.0 / np.where(np.abs(d) < 1e-6, 1e-6, d)
            t1 = (-hb - o) * inv
            t2 = (hb - o) * inv
        tn = np.minimum(t1, t2)
        tf = np.maximum(t1, t2)
        tnear = tn.max(-1)
        tfar = tf.min(-1)
        hit = tnear <= tfar
        axis = tn.argmax(-1)
        nl = np.zeros_like(o)
        for k in range(3):
            nl[..., k] = np.where(axis == k, -np.sign(d[k]), 0)
        q = o + tnear[..., None] * d
        if bevel:
            # round the edges a little: tilt the normal toward the edge near it
            e = np.clip((np.abs(q) - (hb - bevel)) / bevel, 0, 1) * np.sign(q)
            nl = nl + e * 0.8
        n = nl @ R.T
        n /= np.linalg.norm(n, axis=-1, keepdims=True) + 1e-9
        self._write(x0, y0, hit, c[2] + 1e4 - tnear, n, q, mat)

    def cylinder(self, c, r, hh, mat, R=None):
        """a cylinder round local y, radius r, half height hh"""
        c = np.array(c, np.float32)
        R = np.eye(3, dtype=np.float32) if R is None else R
        x0, x1, y0, y1 = self._box(c[0], c[1], math.hypot(r, hh))
        if x0 >= x1 or y0 >= y1:
            return
        ys, xs = np.mgrid[y0:y1, x0:x1].astype(np.float32)
        RT = R.T
        o = np.stack([xs - c[0], ys - c[1], np.full_like(xs, 1e4)], -1) @ RT.T
        d = RT @ np.array([0, 0, -1], np.float32)
        best = np.full(xs.shape, np.inf, np.float32)
        nl = np.zeros_like(o)
        a = d[0] ** 2 + d[2] ** 2
        if a > 1e-8:
            b = 2 * (o[..., 0] * d[0] + o[..., 2] * d[2])
            cc = o[..., 0] ** 2 + o[..., 2] ** 2 - r * r
            disc = b * b - 4 * a * cc
            t = (-b - np.sqrt(np.maximum(disc, 0))) / (2 * a)
            yy = o[..., 1] + t * d[1]
            ok = (disc >= 0) & (np.abs(yy) <= hh)
            best = np.where(ok, t, best)
            q = o + t[..., None] * d
            side = np.stack([q[..., 0], np.zeros_like(t), q[..., 2]], -1) / r
            nl = np.where(ok[..., None], side, nl)
        if abs(d[1]) > 1e-6:
            for s in (-1, 1):
                t = (s * hh - o[..., 1]) / d[1]
                q = o + t[..., None] * d
                ok = (q[..., 0] ** 2 + q[..., 2] ** 2 <= r * r) & (t < best)
                best = np.where(ok, t, best)
                nl = np.where(ok[..., None], np.array([0, s, 0], np.float32), nl)
        hit = np.isfinite(best)
        t = np.where(hit, best, 0)
        q = o + t[..., None] * d
        n = nl @ R.T
        n /= np.linalg.norm(n, axis=-1, keepdims=True) + 1e-9
        self._write(x0, y0, hit, c[2] + 1e4 - t, n, q, mat)

    # ------------------------------------------------------------ lighting

    def light(self, key=(-0.55, -0.6, 0.6), key_col=(1.25, 0.88, 0.55), fill=(0.7, 0.2, 0.5),
              fill_col=(0.16, 0.2, 0.34), rim_col=(0.3, 0.33, 0.5), ambient=0.06, shadows=True,
              ao=True):
        """the lit colour (h x w x 3, about 0..1) and the mask of the shapes"""
        mask = self.z > -1e8
        n = self.n
        L = np.array(key, np.float32)
        L /= np.linalg.norm(L)
        F = np.array(fill, np.float32)
        F /= np.linalg.norm(F)
        diff = np.clip((n * L).sum(-1), 0, 1)
        # light through thin things: the back side lit too, a little
        wrap = np.clip(((n * L).sum(-1) + 0.6) / 1.6, 0, 1)
        diff = diff * (1 - self.sss) + wrap * self.sss
        sh = np.ones_like(diff)
        if shadows:
            sh = self._shadow(L, mask)
        fl = np.clip((n * F).sum(-1), 0, 1)
        H = L + np.array([0, 0, 1], np.float32)
        H /= np.linalg.norm(H)
        sp = np.clip((n * H).sum(-1), 0, 1) ** self.shin * self.spec * sh
        rim = (1 - np.clip(n[..., 2], 0, 1)) ** 3 * self.rim * 0.9
        rim *= np.clip(-n[..., 1] * 0.5 + 0.6, 0, 1)        # the rim mostly on the upper edges
        a = np.ones_like(diff)
        if ao:
            zb = np.where(mask, self.z, self.z[mask].min() if mask.any() else 0)
            bl = blur(zb, 6)
            a = np.clip(1 - np.maximum(bl - zb, 0) * 0.035, 0.35, 1)
        kc = np.array(key_col, np.float32)
        fc = np.array(fill_col, np.float32)
        rc = np.array(rim_col, np.float32)
        lit = self.alb * (ambient + diff[..., None] * sh[..., None] * kc + fl[..., None] * fc) * a[..., None]
        lit += sp[..., None] * kc * 0.9 + rim[..., None] * rc * a[..., None] * 0.6
        em = self.emit > 0
        lit[em] = self.alb[em] * self.emit[em][..., None]
        glow = self.glow > 0
        lit[glow] = self.alb[glow] * self.glow[glow][..., None]
        lit[~mask] = 0
        return lit, mask

    def _shadow(self, L, mask):
        """1 lit, toward 0 where another part of the shape is between the
        point and the torch"""
        h, w = self.z.shape
        zb = np.where(mask & (self.glow == 0) & (self.emit == 0), self.z, -1e9)
        lxy = math.hypot(L[0], L[1])
        dx, dy = L[0] / lxy, L[1] / lxy
        dz = L[2] / lxy
        ys, xs = np.mgrid[0:h, 0:w].astype(np.float32)
        occ = np.zeros((h, w), np.float32)
        for k in range(1, 40):
            s = k * 2.0
            sx = np.clip((xs + dx * s).astype(np.int32), 0, w - 1)
            sy = np.clip((ys + dy * s).astype(np.int32), 0, h - 1)
            above = zb[sy, sx] - (self.z + dz * s) - 2.0
            occ = np.maximum(occ, np.clip(above / 6.0, 0, 1))
        return 1 - occ * 0.85


# ---------------------------------------------------------------- colour

def tonemap(rgb, exposure=1.0):
    c = np.maximum(rgb * exposure, 0)
    return 1 - np.exp(-c * 1.5)


_BAYER = (np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], np.float32) + 0.5) / 16 - 0.5


def bayer(rgb, strength=0.045):
    """ordered dithering's offsets added (before the nearest palette colour
    is taken): smooth, and its patterns repeat, so characters repeat too"""
    h, w = rgb.shape[:2]
    t = np.tile(_BAYER, ((h + 3) // 4, (w + 3) // 4))[:h, :w, None]
    return np.clip(rgb + t * strength, 0, 1)


def to8(rgb):
    return np.clip(rgb * 255 + 0.5, 0, 255).astype(np.uint8)


def quantize(img8, ncol, dither=True):
    """a palette of ncol colours for an RGB image (h x w x 3 uint8) and the
    image as indices into it"""
    im = Image.fromarray(img8, "RGB")
    q = im.quantize(colors=ncol, method=Image.Quantize.MEDIANCUT, kmeans=3,
                    dither=Image.Dither.NONE)
    pal = np.array(q.getpalette()[:ncol * 3], np.uint8).reshape(-1, 3)
    if dither:
        q = im.quantize(palette=q, dither=Image.Dither.FLOYDSTEINBERG)
    idx = np.array(q, np.int32)
    return pal, idx


def remap(img8, pal, dither=True):
    """an RGB image as indices into a given palette"""
    pim = Image.new("P", (1, 1))
    flat = list(pal.reshape(-1))
    pim.putpalette(flat + [0] * (768 - len(flat)))
    q = Image.fromarray(img8, "RGB").quantize(
        palette=pim, dither=Image.Dither.FLOYDSTEINBERG if dither else Image.Dither.NONE)
    idx = np.array(q, np.int32)
    # PIL may pick the padding (black) past the palette's end: those pixels
    # to their nearest real colour
    bad = idx >= len(pal)
    if bad.any():
        px = img8[bad].astype(np.int32)
        d = ((px[:, None, :] - pal[None, :, :].astype(np.int32)) ** 2).sum(-1)
        idx[bad] = d.argmin(1)
    return idx


# ---------------------------------------------------------------- characters

def blocks(idx):
    """an index image (h x w, both multiples of 8) as 8 x 8 characters, row
    by row: (h / 8 * w / 8, 64) bytes"""
    h, w = idx.shape
    b = idx.reshape(h // 8, 8, w // 8, 8).transpose(0, 2, 1, 3).reshape(-1, 64)
    return b.astype(np.uint8)


def shrink_charset(frames, palette, budget):
    """frames: lists of characters (n, 64) as palette indices. The unique
    characters, merged (the nearest in colour first) until there are at
    most budget; the maps from each frame's cells to them."""
    allc = np.concatenate(frames)
    uniq, inv = np.unique(allc, axis=0, return_inverse=True)
    inv = inv.reshape(-1)
    n = len(uniq)
    if n > budget:
        pal = palette.astype(np.float32)
        rgb = pal[uniq.astype(np.int32)].reshape(n, -1)            # (n, 192)
        uses = np.bincount(inv, minlength=n).astype(np.float32)
        sq = (rgb * rgb).sum(1)
        d = sq[:, None] + sq[None, :] - 2 * rgb @ rgb.T
        np.fill_diagonal(d, np.inf)
        alive = np.ones(n, bool)
        target = np.arange(n)
        nn = d.argmin(1)
        nd = d[np.arange(n), nn]
        left = n
        while left > budget:
            cost = nd * uses
            cost[~alive] = np.inf
            i = int(cost.argmin())
            j = int(nn[i])
            alive[i] = False
            left -= 1
            target[target == i] = j
            uses[j] += uses[i]
            d[i, :] = np.inf
            d[:, i] = np.inf
            nd[i] = np.inf
            for k in np.nonzero(alive & (nn == i))[0]:
                nn[k] = d[k].argmin()
                nd[k] = d[k, nn[k]]
        keep = np.nonzero(alive)[0]
        newi = -np.ones(n, np.int64)
        newi[keep] = np.arange(len(keep))
        inv = newi[target[inv]]
        uniq = uniq[keep]
    maps = []
    at = 0
    for f in frames:
        maps.append(inv[at:at + len(f)])
        at += len(f)
    return uniq, maps
