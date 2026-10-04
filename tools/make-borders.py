#!/usr/bin/env python3
# Porpoise - draws the built-in screen borders (assets/borders/*.png): what
# fills the bars beside a 4:3 picture when widescreen is off.
#
# A border is a 1920x1080 PNG, transparent where the picture shows through.
# Make your own the same way and put it in /data/porpoise/borders/; the 4:3
# picture fills x 240..1680 of the full height.
#
#   glass     Porpoise's sky-blue glass, with a soft rim round the picture
#   midnight  deep blue with a little starlight and a thin light rim
#   arcade    a cabinet bezel whose opening curves like an arcade tube
#
# Needs Pillow.  SPDX-License-Identifier: GPL-3.0-or-later
import math
import os
import random
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "assets", "borders")
W, H = 1920, 1080
PX0, PX1 = 240, 1680  # the 4:3 picture


def vgradient(top, bottom, w=W, h=H):
    g = Image.new("RGBA", (1, h))
    for y in range(h):
        t = y / (h - 1)
        g.putpixel((0, y), tuple(int(a + (b - a) * t) for a, b in zip(top, bottom)) + (255,))
    return g.resize((w, h))


def glow(mask, radius, color, strength=1.0):
    blurred = mask.filter(ImageFilter.GaussianBlur(radius))
    layer = Image.new("RGBA", mask.size, color + (0,))
    layer.putalpha(blurred.point(lambda a: min(255, int(a * strength))))
    return layer


def cut_opening(img, opening):
    """Make the picture's area transparent: opening is an L mask, 255 = hole."""
    a = ImageChops.subtract(img.getchannel("A"), opening)
    img.putalpha(a)
    return img


def rect_mask(box, radius=0, s=4):
    m = Image.new("L", (W * s, H * s), 0)
    d = ImageDraw.Draw(m)
    x0, y0, x1, y1 = box
    d.rounded_rectangle([x0 * s, y0 * s, x1 * s, y1 * s], radius=radius * s, fill=255)
    return m.resize((W, H), Image.LANCZOS)


def glass():
    img = vgradient((96, 190, 255), (18, 74, 196))
    marks = Image.new("RGBA", (W, H), (0, 0, 0, 0))  # drawn apart, then laid on
    d = ImageDraw.Draw(marks)
    # Light from the top, falling in soft rays across the side panels.
    rays = Image.new("L", (W, H), 0)
    rd = ImageDraw.Draw(rays)
    for i, x in enumerate((60, 150, 1770, 1860)):
        rd.polygon([(x - 30, 0), (x + 40, 0), (x + 160 - i * 40, H), (x + 60 - i * 40, H)], fill=60)
    img.alpha_composite(glow(rays, 30, (255, 255, 255), 1.0))
    # Bubbles rising up the sides.
    rnd = random.Random(7)
    for _ in range(70):
        side = rnd.choice((0, 1))
        x = rnd.uniform(20, PX0 - 30) if side == 0 else rnd.uniform(PX1 + 30, W - 20)
        y = rnd.uniform(0, H)
        r = rnd.choice((4, 5, 7, 9, 12, 16, 22))
        a = int(rnd.uniform(50, 120))
        d.ellipse([x - r, y - r, x + r, y + r], outline=(235, 248, 255, a), width=2 if r > 6 else 1)
        d.ellipse([x - r * 0.45, y - r * 0.6, x - r * 0.05, y - r * 0.2], fill=(255, 255, 255, a))
    img.alpha_composite(marks)
    # The glass rim round the picture: a bright edge, then a soft shadow inward.
    rim = rect_mask((PX0 - 14, -20, PX1 + 14, H + 20), 26)
    inner = rect_mask((PX0, -20, PX1, H + 20), 16)
    band = ImageChops.subtract(rim, inner)
    img.alpha_composite(glow(band, 10, (160, 225, 255), 1.6))
    edge = Image.new("RGBA", (W, H), (230, 246, 255, 0))
    edge.putalpha(band.point(lambda a: int(a * 0.85)))
    img.alpha_composite(edge)
    # A gloss along the top of each side panel.
    gloss = Image.new("L", (W, H), 0)
    gd = ImageDraw.Draw(gloss)
    gd.rounded_rectangle([18, 18, PX0 - 34, 260], radius=40, fill=70)
    gd.rounded_rectangle([PX1 + 34, 18, W - 18, 260], radius=40, fill=70)
    img.alpha_composite(glow(gloss, 18, (255, 255, 255), 1.0))
    return cut_opening(img, inner)


def midnight():
    img = vgradient((10, 22, 70), (3, 6, 24))
    marks = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(marks)
    rnd = random.Random(11)
    for _ in range(260):
        x = rnd.uniform(0, W)
        if PX0 - 20 < x < PX1 + 20:
            continue
        y = rnd.uniform(0, H)
        r = rnd.choice((0.8, 1.0, 1.2, 1.6, 2.2))
        a = int(rnd.uniform(60, 220))
        d.ellipse([x - r, y - r, x + r, y + r], fill=(220, 235, 255, a))
    # A few with a cross of light.
    for _ in range(9):
        x = rnd.choice((rnd.uniform(30, PX0 - 40), rnd.uniform(PX1 + 40, W - 30)))
        y = rnd.uniform(40, H - 40)
        s = rnd.uniform(8, 16)
        d.line([(x - s, y), (x + s, y)], fill=(200, 230, 255, 150), width=1)
        d.line([(x, y - s), (x, y + s)], fill=(200, 230, 255, 150), width=1)
    img.alpha_composite(marks)
    inner = rect_mask((PX0, -20, PX1, H + 20), 10)
    rim = rect_mask((PX0 - 4, -20, PX1 + 4, H + 20), 12)
    band = ImageChops.subtract(rim, inner)
    img.alpha_composite(glow(band, 14, (90, 170, 255), 1.4))
    line = Image.new("RGBA", (W, H), (150, 205, 255, 0))
    line.putalpha(band)
    img.alpha_composite(line)
    return cut_opening(img, inner)


def arcade_opening(s=4):
    """The tube's opening: a rectangle whose sides bow outward, round corners."""
    m = Image.new("L", (W * s, H * s), 0)
    d = ImageDraw.Draw(m)
    x0, y0, x1, y1 = PX0 + 18, 34, PX1 - 18, H - 34
    bow, r = 16, 70
    pts = []
    n = 60

    def side(ax, ay, bx, by, nx, ny):
        for i in range(n + 1):
            t = i / n
            k = math.sin(math.pi * t) * bow
            pts.append(((ax + (bx - ax) * t + nx * k) * s, (ay + (by - ay) * t + ny * k) * s))

    def corner(cx, cy, a0):
        for i in range(n // 3 + 1):
            a = math.radians(a0 + 90 * i / (n // 3))
            pts.append(((cx + math.cos(a) * r) * s, (cy + math.sin(a) * r) * s))

    side(x0 + r, y0, x1 - r, y0, 0, -1)
    corner(x1 - r, y0 + r, -90)
    side(x1, y0 + r, x1, y1 - r, 1, 0)
    corner(x1 - r, y1 - r, 0)
    side(x1 - r, y1, x0 + r, y1, 0, 1)
    corner(x0 + r, y1 - r, 90)
    side(x0, y1 - r, x0, y0 + r, -1, 0)
    corner(x0 + r, y0 + r, 180)
    d.polygon(pts, fill=255)
    return m.resize((W, H), Image.LANCZOS)


def arcade():
    # The bezel: near-black plastic with a purple sheen toward the middle.
    img = vgradient((24, 14, 40), (8, 6, 16))
    sheen = Image.new("L", (W, H), 0)
    sd = ImageDraw.Draw(sheen)
    sd.ellipse([PX0 - 260, -200, PX1 + 260, H + 200], fill=90)
    img.alpha_composite(glow(sheen, 120, (110, 60, 190), 1.0))
    # Neon stripes down the side panels, in Porpoise's blues and a pink.
    stripes = [((40, 220, 255), 70), ((90, 120, 255), 108), ((255, 70, 180), 146)]
    for color, x in stripes:
        for xs in (x, W - x):
            m = Image.new("L", (W, H), 0)
            ImageDraw.Draw(m).line([(xs, 60), (xs, H - 60)], fill=255, width=8)
            img.alpha_composite(glow(m, 14, color, 1.6))
            core = Image.new("RGBA", (W, H), tuple(min(255, c + 70) for c in color) + (0,))
            core.putalpha(m.filter(ImageFilter.GaussianBlur(1.2)))
            img.alpha_composite(core)
    # Speaker-grille dots near the bottom corners.
    marks = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(marks)
    for cx in (120, W - 120):
        for row in range(5):
            for col in range(5):
                x = cx - 48 + col * 24
                y = H - 230 + row * 24
                d.ellipse([x - 6, y - 6, x + 6, y + 6], fill=(0, 0, 0, 170), outline=(70, 50, 100, 200))
    img.alpha_composite(marks)
    # The opening, with a bevel: a dark lip, then a thin highlight.
    hole = arcade_opening()
    lip = hole.filter(ImageFilter.MaxFilter(31))
    lip_band = ImageChops.subtract(lip, hole)
    shade = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    shade.putalpha(lip_band.point(lambda a: int(a * 0.9)))
    img.alpha_composite(shade)
    hi = hole.filter(ImageFilter.MaxFilter(35))
    hi_band = ImageChops.subtract(hi, lip)
    light = Image.new("RGBA", (W, H), (190, 170, 255, 0))
    light.putalpha(hi_band.filter(ImageFilter.GaussianBlur(1.5)).point(lambda a: int(a * 0.6)))
    img.alpha_composite(light)
    # Shadow falling onto the tube from the bezel.
    inset = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    inset_mask = ImageChops.subtract(hole, hole.filter(ImageFilter.MinFilter(41)).filter(ImageFilter.GaussianBlur(14)))
    inset.putalpha(inset_mask.point(lambda a: int(a * 0.55)))
    img = cut_opening(img, hole)
    img.alpha_composite(inset)
    return img


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    for name, fn in (("glass", glass), ("midnight", midnight), ("arcade", arcade)):
        fn().save(os.path.join(OUT, name + ".png"), optimize=True)
        print("wrote", name)
