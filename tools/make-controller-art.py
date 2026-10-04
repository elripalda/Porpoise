#!/usr/bin/env python3
# Porpoise - draws assets/ui/dualsense.png, the controller on the button-mapping
# screen: a generic outline of the PS5 controller in the launcher's glass style.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The positions of the controls (canvas pixels, 1400x900) are the ones
# src/ui_app_controls.cpp uses for its labels; change both together.
import math
import sys
from PIL import Image, ImageDraw, ImageFilter, ImageChops

W, H = 1400, 900
S = 2  # supersampling
OUT = sys.argv[1] if len(sys.argv) > 1 else "assets/ui/dualsense.png"

def P(x, y):
    return (x * S, y * S)

def body_mask():
    m = Image.new("L", (W * S, H * S), 0)
    d = ImageDraw.Draw(m)
    d.rounded_rectangle([*P(300, 175), *P(1100, 575)], radius=130 * S, fill=255)
    for sx in (1, -1):
        cx = 700
        top = (cx - sx * 360, 340)
        bot = (cx - sx * 465, 700)
        d.line([P(*top), P(*bot)], fill=255, width=300 * S)
        for (x, y) in (top, bot):
            d.ellipse([*P(x - 150, y - 150), *P(x + 150, y + 150)], fill=255)
    # round the joins
    m = m.filter(ImageFilter.GaussianBlur(28 * S)).point(lambda v: 255 if v > 128 else 0)
    return m

def stroke(mask, width):
    outer = mask.filter(ImageFilter.MaxFilter(width * 2 + 1))
    inner = mask.filter(ImageFilter.MinFilter(width * 2 + 1))
    return ImageChops.subtract(outer, inner)

def main():
    img = Image.new("RGBA", (W * S, H * S), (0, 0, 0, 0))
    m = body_mask()
    # body: icy glass, lighter at the top
    grad = Image.new("RGBA", (W * S, H * S))
    gd = ImageDraw.Draw(grad)
    for y in range(H * S):
        t = y / (H * S)
        a = int(78 - 40 * t)
        gd.line([(0, y), (W * S, y)], fill=(150, 205, 255, a))
    body = Image.new("RGBA", (W * S, H * S), (0, 0, 0, 0))
    body.paste(grad, (0, 0), m)
    img = Image.alpha_composite(img, body)
    # soft inner highlight along the top
    hl = Image.new("L", (W * S, H * S), 0)
    ImageDraw.Draw(hl).rounded_rectangle([*P(330, 190), *P(1070, 330)], radius=90 * S, fill=60)
    hl = ImageChops.multiply(hl.filter(ImageFilter.GaussianBlur(30 * S)), m)
    img = Image.alpha_composite(img, Image.merge("RGBA", (*[Image.new("L", hl.size, 255)] * 3, hl)))
    # rim
    rim = stroke(m, 3 * S).filter(ImageFilter.GaussianBlur(1))
    img = Image.alpha_composite(img, Image.merge("RGBA", (Image.new("L", rim.size, 220), Image.new("L", rim.size, 240),
                                                          Image.new("L", rim.size, 255), rim)))
    d = ImageDraw.Draw(img)
    line = (225, 240, 255, 235)
    soft = (200, 225, 255, 120)
    fill = (120, 180, 255, 55)

    def rrect(x0, y0, x1, y1, r, outline=line, fill_=fill, w=3):
        d.rounded_rectangle([*P(x0, y0), *P(x1, y1)], radius=r * S, outline=outline, fill=fill_, width=w * S)

    def circle(cx, cy, r, outline=line, fill_=fill, w=3):
        d.ellipse([*P(cx - r, cy - r), *P(cx + r, cy + r)], outline=outline, fill=fill_, width=w * S)

    # triggers and bumpers (seen from the top edge)
    for sx in (-1, 1):
        cx = 700 + sx * 290
        rrect(cx - 92, 58, cx + 92, 118, 28)          # L2 / R2
        rrect(cx - 112, 125, cx + 112, 162, 18)       # L1 / R1
    # touch pad
    rrect(520, 188, 880, 370, 34)
    # create (left) and options (right)
    rrect(457, 182, 483, 228, 13)
    rrect(917, 182, 943, 228, 13)
    # PS button and mute: drawn faintly, Porpoise can't use them
    circle(700, 472, 24, outline=soft, fill_=(0, 0, 0, 0), w=2)
    rrect(684, 518, 716, 532, 7, outline=soft, fill_=(0, 0, 0, 0), w=2)
    # D-pad: one plus-shaped key, split into its four arms
    cx, cy = 390, 330
    plus = Image.new("L", (W * S, H * S), 0)
    pd = ImageDraw.Draw(plus)
    pd.rounded_rectangle([*P(cx - 104, cy - 31), *P(cx + 104, cy + 31)], radius=14 * S, fill=255)
    pd.rounded_rectangle([*P(cx - 31, cy - 104), *P(cx + 31, cy + 104)], radius=14 * S, fill=255)
    img.alpha_composite(Image.merge("RGBA", (Image.new("L", plus.size, 120), Image.new("L", plus.size, 180),
                                             Image.new("L", plus.size, 255), plus.point(lambda v: v * 55 // 255))))
    edge = stroke(plus, int(1.5 * S))
    img.alpha_composite(Image.merge("RGBA", (Image.new("L", edge.size, 225), Image.new("L", edge.size, 240),
                                             Image.new("L", edge.size, 255), edge.point(lambda v: v * 235 // 255))))
    d = ImageDraw.Draw(img)
    for (dx, dy) in ((0, -1), (0, 1), (-1, 0), (1, 0)):
        # a small arrow in each arm
        ax, ay = cx + dx * 66, cy + dy * 66
        s = 11
        if dy:
            pts = [P(ax, ay + dy * s), P(ax - s, ay - dy * s * 0.6), P(ax + s, ay - dy * s * 0.6)]
        else:
            pts = [P(ax + dx * s, ay), P(ax - dx * s * 0.6, ay - s), P(ax - dx * s * 0.6, ay + s)]
        d.polygon(pts, fill=(225, 240, 255, 200))
    # face buttons with their symbols
    fx, fy, gap, r = 1010, 330, 76, 36
    sym = (240, 248, 255, 255)
    for (name, dx, dy) in (("tri", 0, -1), ("cir", 1, 0), ("crs", 0, 1), ("sqr", -1, 0)):
        bx, by = fx + dx * gap, fy + dy * gap
        circle(bx, by, r)
        s = 15
        if name == "tri":
            pts = [P(bx, by - s - 2), P(bx + s + 2, by + s - 4), P(bx - s - 2, by + s - 4)]
            d.polygon(pts, outline=sym, width=4 * S)
            d.line(pts + [pts[0]], fill=sym, width=4 * S, joint="curve")
        elif name == "cir":
            d.ellipse([*P(bx - s, by - s), *P(bx + s, by + s)], outline=sym, width=4 * S)
        elif name == "crs":
            d.line([P(bx - s, by - s), P(bx + s, by + s)], fill=sym, width=4 * S)
            d.line([P(bx - s, by + s), P(bx + s, by - s)], fill=sym, width=4 * S)
        else:
            d.rectangle([*P(bx - s + 1, by - s + 1), *P(bx + s - 1, by + s - 1)], outline=sym, width=4 * S)
    # sticks
    for sx in (-1, 1):
        cx = 700 + sx * 155
        circle(cx, 480, 64)
        circle(cx, 480, 44, fill_=(150, 200, 255, 70))
    out = img.resize((W, H), Image.LANCZOS)
    out.save(OUT, optimize=True)
    print("wrote", OUT, out.size)

main()
