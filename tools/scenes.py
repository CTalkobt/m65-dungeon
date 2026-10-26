"""scenes.py — every close-up by name: its frames rendered, set on the stage
and given one palette"""
import monsters
import props
from artlib import Scene
from stage import W, H, backdrop, compose, palette_for

SCENES = {name: fn for name, fn in monsters.MONSTERS + props.PROPS}


def render(name):
    """[(rgb frame, palette indices)], the 256-entry palette (128-254 set)"""
    fn = SCENES[name]
    shots = []
    glow = (1, 1, 1)
    for pose in (0, 1):
        sc = Scene(W, H)
        o = fn(sc, pose)
        lit, mask = sc.light(**o.get("light", {}))
        back = backdrop(**o.get("backdrop", {}))
        img, g = compose(sc, lit, mask, back, pools=o.get("pools", ()), exposure=o.get("exposure", 1.0),
                         haze=o.get("haze", 0.6), cast=o.get("cast", 0.75))
        glow = o.get("glow", glow)
        shots.append((img, g))
    pal, idx = palette_for(shots, glow)
    return [(s[0], i) for s, i in zip(shots, idx)], pal
