#!/usr/bin/env python3
"""Porpoise - the Revolution theme's pointer: Ruben's two hands
(tools/pointer-art: open.png, an open hand over nothing; point.png, a hand
pointing up over something it can choose), cropped and scaled alike for the
app. The player's number goes on in the app. Run from the repository root:
    python3 tools/make-pointer-hand.py
Writes assets/ui/pointer-open.png and assets/ui/pointer-hand.png, and prints
the spot each one points at (the index fingertip) and where the number goes,
as fractions of its size, for kHands in src/ui_app_revolution.cpp.
Copyright (C) 2026 Ruben (Project Porpoise)
SPDX-License-Identifier: GPL-3.0-or-later
"""
import numpy as np
from PIL import Image

SCALE = 0.25  # both hands shrink alike, so they stay the same size on screen
PAD = 8       # source pixels kept around each hand

# Per hand: the source, the output, the index finger's column range in the
# source (its tip is the topmost drawn pixel there), and the palm spot for
# the player's number (source pixels).
HANDS = [
    ('open.png', 'pointer-open.png', (380, 540), (650, 860)),
    ('point.png', 'pointer-hand.png', (400, 560), (630, 880)),
]

for src, dst, (fx0, fx1), (nx, ny) in HANDS:
    im = Image.open(f'tools/pointer-art/{src}').convert('RGBA')
    a = np.array(im)[..., 3]
    ys, xs = np.nonzero(a > 8)
    x0, y0 = max(0, xs.min() - PAD), max(0, ys.min() - PAD)
    x1, y1 = min(im.width, xs.max() + PAD + 1), min(im.height, ys.max() + PAD + 1)
    crop = im.crop((x0, y0, x1, y1))
    w, h = round(crop.width * SCALE), round(crop.height * SCALE)
    crop.resize((w, h), Image.LANCZOS).save(f'assets/ui/{dst}', optimize=True)
    col = a[:, fx0:fx1]
    ty = int(np.nonzero((col > 128).any(axis=1))[0].min())
    tx = fx0 + float(np.nonzero(col[ty + 6] > 128)[0].mean())
    w0, h0 = x1 - x0, y1 - y0
    print(f'assets/ui/{dst} {w}x{h}: tip {(tx - x0) / w0:.3f}, {(ty - y0) / h0:.3f}; '
          f'number {(nx - x0) / w0:.3f}, {(ny - y0) / h0:.3f}')
