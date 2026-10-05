#!/usr/bin/env python3
# Porpoise - draws assets/ui/report-qr.png, the QR code in Settings > About that
# opens the RIPALDA Discord on a phone (help, bug reports, news): navy on a
# white rounded tile, Ruben's mark (assets/brand/ripalda.png) in the middle,
# which the highest error correction leaves room for.
# Needs qrcode and Pillow.  SPDX-License-Identifier: GPL-3.0-or-later
import os
import sys

import qrcode
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "assets", "ui", "report-qr.png")
URL = "https://discord.gg/GgDE5Vynyu"

qr = qrcode.QRCode(error_correction=qrcode.constants.ERROR_CORRECT_H, box_size=1, border=0)
qr.add_data(URL)
qr.make(fit=True)
m = qr.get_matrix()
n = len(m)
cell, quiet = 8, 3
size = (n + quiet * 2) * cell
tile = Image.new("RGBA", (size, size), (0, 0, 0, 0))
d = ImageDraw.Draw(tile)
d.rounded_rectangle([0, 0, size - 1, size - 1], radius=cell * 2, fill=(255, 255, 255, 255))
for y, row in enumerate(m):
    for x, on in enumerate(row):
        if on:
            x0, y0 = (x + quiet) * cell, (y + quiet) * cell
            d.rectangle([x0, y0, x0 + cell - 1, y0 + cell - 1], fill=(10, 24, 82, 255))
# The mark in the middle: a navy square, the white mark on it (under a fifth
# of the code's width, well inside what level H can lose).
mark_path = os.path.join(ROOT, "assets", "brand", "ripalda.png")
if os.path.exists(mark_path):
    box = (n // 5) | 1
    side = box * cell
    x0 = (size - side) // 2
    d.rounded_rectangle([x0 - cell // 2, x0 - cell // 2, x0 + side + cell // 2, x0 + side + cell // 2],
                        radius=cell, fill=(255, 255, 255, 255))
    d.rounded_rectangle([x0, x0, x0 + side - 1, x0 + side - 1], radius=cell, fill=(10, 24, 82, 255))
    mark = Image.open(mark_path).convert("RGBA").resize((side - cell * 2, side - cell * 2), Image.LANCZOS)
    tile.alpha_composite(mark, (x0 + cell, x0 + cell))
tile.save(OUT, optimize=True)
print("wrote", OUT, f"({n}x{n} modules)")
