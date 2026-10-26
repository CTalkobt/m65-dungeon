#!/usr/bin/env python3
"""sheet.py out.png a.png b.png ... — previews in a grid, two across"""
import sys
from PIL import Image
ims = [Image.open(p) for p in sys.argv[2:]]
w, h = ims[0].size
cols = 2
rows = (len(ims) + cols - 1) // cols
s = Image.new("RGB", (w * cols + 4, h * rows + 2 * rows), (40, 0, 40))
for i, im in enumerate(ims):
    s.paste(im, ((i % cols) * (w + 4), (i // cols) * (h + 2)))
s.save(sys.argv[1])
