#!/usr/bin/env python3
"""preview.py — render one close-up to PNGs (true colour and quantized) to
look at while working on it: preview.py <name> [outdir]"""
import os
import sys
import time
import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(__file__))
import scenes  # noqa: E402
from artlib import to8  # noqa: E402

name = sys.argv[1]
out = sys.argv[2] if len(sys.argv) > 2 else "build/preview"
os.makedirs(out, exist_ok=True)
t = time.time()
frames, pal = scenes.render(name)
for i, (img, idx) in enumerate(frames):
    Image.fromarray(to8(img)).save(os.path.join(out, "%s_%d_rgb.png" % (name, i)))
    Image.fromarray(pal[idx]).save(os.path.join(out, "%s_%d.png" % (name, i)))
print("%s: %.1f s" % (name, time.time() - t))
