#!/usr/bin/env python3
# Porpoise - draws assets/ui/report-qr.png, the QR code in Settings > About that
# opens Porpoise's GitHub issues on a phone: navy on a white rounded tile.
# Needs qrcode and Pillow.  SPDX-License-Identifier: GPL-3.0-or-later
import os
import sys

import qrcode
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "assets", "ui", "report-qr.png")
URL = "https://github.com/elripalda/Porpoise/issues"

qr = qrcode.QRCode(error_correction=qrcode.constants.ERROR_CORRECT_M, box_size=1, border=0)
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
tile.save(OUT, optimize=True)
print("wrote", OUT, f"({n}x{n} modules)")
