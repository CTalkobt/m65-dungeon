"""scenes.py — every close-up by name: its frames rendered over the shared
backdrop (backdrop.py) and given one palette for what they change"""
import monsters
import props
from artlib import Scene
from stage import W, H, compose_on, palette_for

SCENES = {e[0]: e[1] for e in monsters.MONSTERS + props.PROPS}


def render(name, back):
    """back: the backdrop's RGB as the screen shows it. [(rgb frame, palette
    indices, -1 where the backdrop shows as it is)], the palette (128-254)"""
    fn = SCENES[name]
    shots = []
    glow = (1, 1, 1)
    for pose in (0, 1):
        sc = Scene(W, H)
        o = fn(sc, pose)
        lit, mask = sc.light(**o.get("light", {}))
        img, changed, g = compose_on(sc, lit, mask, back, pools=o.get("pools", ()),
                                     exposure=o.get("exposure", 1.0), cast=o.get("cast", 0.75))
        glow = o.get("glow", glow)
        shots.append((img, changed, g))
    pal, idx = palette_for(shots, glow)
    return [(s[0], i) for s, i in zip(shots, idx)], pal
