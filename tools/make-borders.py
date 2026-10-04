#!/usr/bin/env python3
# Porpoise - draws the built-in screen borders (assets/borders/*.png): what
# fills the bars beside a 4:3 picture when widescreen is off.
#
# A border is a 1920x1080 PNG, transparent where the picture shows through.
# Make your own the same way and put it in /data/porpoise/borders/; the 4:3
# picture fills x 240..1680 of the full height.
#
#   glass          Porpoise's sky-blue glass, with a soft rim round the picture
#   midnight       deep blue with a little starlight and a thin light rim
#   porpoise       Porpoise's own room: the logo, the name and the glowing grid
#   frost          pale frosted glass, for bright games
#   carbon         dark carbon-fibre weave with a thin steel rim
#   arcade         a cabinet bezel whose opening curves like an arcade tube
#   arcade-retro   a 70s cabinet: dark wood and sunset stripes, a chrome bezel
#   arcade-synth   a synthwave cabinet: a striped sun over a neon grid
#
# Needs Pillow.  SPDX-License-Identifier: GPL-3.0-or-later
import math
import os
import random
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

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


def bevel_opening(img, hole, lip_px=31, light=(190, 170, 255), shadow=0.9, inner=0.55):
    """A dark lip round a curved opening, a thin highlight, and the bezel's shadow on the tube."""
    lip = hole.filter(ImageFilter.MaxFilter(lip_px))
    shade = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    shade.putalpha(ImageChops.subtract(lip, hole).point(lambda a: int(a * shadow)))
    img.alpha_composite(shade)
    hi = hole.filter(ImageFilter.MaxFilter(lip_px + 4))
    band = Image.new("RGBA", (W, H), light + (0,))
    band.putalpha(ImageChops.subtract(hi, lip).filter(ImageFilter.GaussianBlur(1.5)).point(lambda a: int(a * 0.7)))
    img.alpha_composite(band)
    tube = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    tube.putalpha(ImageChops.subtract(hole, hole.filter(ImageFilter.MinFilter(41)).filter(ImageFilter.GaussianBlur(14)))
                  .point(lambda a: int(a * inner)))
    img = cut_opening(img, hole)
    img.alpha_composite(tube)
    return img


def side_mask():
    m = Image.new("L", (W, H), 0)
    d = ImageDraw.Draw(m)
    d.rectangle([0, 0, PX0, H], fill=255)
    d.rectangle([PX1, 0, W, H], fill=255)
    return m


def grid_floor(color, horizon, lines=14, alpha=170):
    """Perspective grid lines on the lower part of each side panel."""
    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    for i in range(1, lines):
        t = (i / lines) ** 2
        y = horizon + (H - horizon) * t
        d.line([(0, y), (W, y)], fill=color + (int(alpha * (0.3 + 0.7 * t)),), width=2)
    for k in range(-24, 25):
        x0 = W / 2 + k * 40
        d.line([(W / 2 + k * 6, horizon), (x0 + k * 120, H)], fill=color + (alpha,), width=2)
    layer.putalpha(ImageChops.multiply(layer.getchannel("A"), side_mask()))
    return layer


def porpoise():
    # The room behind the menus: deep navy, light from the middle, a grid floor.
    img = vgradient((8, 16, 48), (4, 8, 26))
    glow_mask = Image.new("L", (W, H), 0)
    ImageDraw.Draw(glow_mask).ellipse([PX0 - 420, 120, PX1 + 420, 980], fill=110)
    img.alpha_composite(glow(glow_mask, 160, (40, 120, 255), 1.0))
    floor = grid_floor((90, 200, 255), 700)
    img.alpha_composite(floor.filter(ImageFilter.GaussianBlur(0.6)))
    img.alpha_composite(glow(floor.getchannel("A"), 6, (90, 200, 255), 0.6))
    # The dolphin, large on the left, with its glow.
    logo = Image.open(os.path.join(ROOT, "assets", "brand", "dolphin-color.png")).convert("RGBA")
    size = 196
    logo = logo.resize((size, size), Image.LANCZOS)
    lx, ly = (PX0 - size) // 2, 170
    halo = Image.new("L", (W, H), 0)
    halo.paste(logo.getchannel("A"), (lx, ly))
    img.alpha_composite(glow(halo, 26, (80, 190, 255), 1.4))
    img.alpha_composite(logo, (lx, ly))
    # PORPOISE down the right-hand panel, letter-spaced, glowing.
    font = ImageFont.truetype(os.path.join(ROOT, "assets", "fonts", "Nunito-ExtraBold.ttf"), 64)
    word = Image.new("L", (900, 110), 0)
    wd = ImageDraw.Draw(word)
    x = 10
    for ch in "PORPOISE":
        wd.text((x, 10), ch, font=font, fill=255)
        x += font.getbbox(ch)[2] + 26
    word = word.crop((0, 0, x, 110)).rotate(-90, expand=True)
    wx, wy = PX1 + (W - PX1 - word.width) // 2, (H - word.height) // 2 - 60
    text_mask = Image.new("L", (W, H), 0)
    text_mask.paste(word, (wx, wy))
    img.alpha_composite(glow(text_mask, 18, (90, 200, 255), 1.3))
    white = Image.new("RGBA", (W, H), (236, 248, 255, 0))
    white.putalpha(text_mask)
    img.alpha_composite(white)
    # A cyan rim round the picture.
    inner = rect_mask((PX0, -20, PX1, H + 20), 10)
    rim = ImageChops.subtract(rect_mask((PX0 - 5, -20, PX1 + 5, H + 20), 12), inner)
    img.alpha_composite(glow(rim, 16, (60, 190, 255), 1.6))
    line = Image.new("RGBA", (W, H), (170, 230, 255, 0))
    line.putalpha(rim)
    img.alpha_composite(line)
    return cut_opening(img, inner)


def frost():
    img = vgradient((236, 244, 255), (186, 206, 236))
    rnd = random.Random(5)
    marks = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(marks)
    for _ in range(900):
        x = rnd.choice((rnd.uniform(0, PX0), rnd.uniform(PX1, W)))
        y = rnd.uniform(0, H)
        r = rnd.uniform(0.6, 2.2)
        d.ellipse([x - r, y - r, x + r, y + r], fill=(255, 255, 255, int(rnd.uniform(60, 200))))
    img.alpha_composite(marks.filter(ImageFilter.GaussianBlur(0.6)))
    sheen = Image.new("L", (W, H), 0)
    sd = ImageDraw.Draw(sheen)
    for x in (40, 1800):
        sd.polygon([(x, 0), (x + 80, 0), (x - 60, H), (x - 140, H)], fill=60)
    img.alpha_composite(glow(sheen, 40, (255, 255, 255), 1.0))
    inner = rect_mask((PX0, -20, PX1, H + 20), 14)
    rim = ImageChops.subtract(rect_mask((PX0 - 10, -20, PX1 + 10, H + 20), 20), inner)
    shadow = Image.new("RGBA", (W, H), (70, 100, 160, 0))
    shadow.putalpha(rim.filter(ImageFilter.GaussianBlur(8)).point(lambda a: int(a * 0.5)))
    img.alpha_composite(shadow)
    edge = Image.new("RGBA", (W, H), (255, 255, 255, 0))
    edge.putalpha(rim.point(lambda a: int(a * 0.9)))
    img.alpha_composite(edge)
    return cut_opening(img, inner)


def carbon():
    img = Image.new("RGBA", (W, H), (18, 19, 22, 255))
    weave = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(weave)
    cell = 14
    for gy in range(0, H, cell):
        for gx in range(0, W, cell):
            if (gx // cell + gy // cell) % 2:
                d.rectangle([gx, gy, gx + cell - 2, gy + cell // 2 - 1], fill=(60, 62, 70, 255))
                d.rectangle([gx, gy + cell // 2, gx + cell - 2, gy + cell - 2], fill=(34, 35, 40, 255))
            else:
                d.rectangle([gx, gy, gx + cell // 2 - 1, gy + cell - 2], fill=(46, 48, 54, 255))
                d.rectangle([gx + cell // 2, gy, gx + cell - 2, gy + cell - 2], fill=(28, 29, 33, 255))
    img.alpha_composite(weave)
    light = Image.new("L", (W, H), 0)
    ImageDraw.Draw(light).ellipse([-300, -400, 900, 500], fill=70)
    ImageDraw.Draw(light).ellipse([1020, 580, 2220, 1480], fill=50)
    img.alpha_composite(glow(light, 140, (200, 210, 230), 1.0))
    inner = rect_mask((PX0, -20, PX1, H + 20), 6)
    rim = ImageChops.subtract(rect_mask((PX0 - 4, -20, PX1 + 4, H + 20), 8), inner)
    steel = Image.new("RGBA", (W, H), (190, 198, 214, 0))
    steel.putalpha(rim)
    img.alpha_composite(steel)
    return cut_opening(img, inner)


def arcade_retro():
    # Dark wood with a soft grain, and bold 70s stripes running down the sides.
    img = vgradient((58, 30, 20), (34, 17, 11))
    rnd = random.Random(3)
    grain = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    gd = ImageDraw.Draw(grain)
    for _ in range(420):
        x = rnd.uniform(0, W)
        w = rnd.uniform(1, 3)
        gd.line([(x, 0), (x + rnd.uniform(-30, 30), H)], fill=(20, 8, 4, int(rnd.uniform(30, 80))), width=int(w))
    img.alpha_composite(grain.filter(ImageFilter.GaussianBlur(1.2)))
    stripes = [(255, 196, 40), (255, 128, 30), (232, 60, 40), (150, 30, 70)]
    for side in (0, 1):
        for i, col in enumerate(stripes):
            band = Image.new("L", (W, H), 0)
            bd = ImageDraw.Draw(band)
            x = 36 + i * 30 if side == 0 else W - 36 - i * 30 - 22
            bd.polygon([(x, H), (x + 22, H), (x + 22 + 140, 0), (x + 140, 0)] if side == 0 else
                       [(x, H), (x + 22, H), (x + 22 - 140, 0), (x - 140, 0)], fill=255)
            band = ImageChops.multiply(band, side_mask())
            layer = Image.new("RGBA", (W, H), col + (0,))
            layer.putalpha(band)
            img.alpha_composite(layer)
    # Coin door details, bottom left and right.
    d = ImageDraw.Draw(img)
    for cx in (120, W - 120):
        d.rounded_rectangle([cx - 44, H - 200, cx + 44, H - 80], radius=10, fill=(24, 24, 26, 255),
                            outline=(170, 170, 176, 255), width=3)
        d.rectangle([cx - 4, H - 176, cx + 4, H - 136], fill=(255, 120, 40, 255))
        d.text((cx - 16, H - 122), "25¢", fill=(230, 230, 230, 255))
    # Chrome bezel round the curved tube.
    hole = arcade_opening()
    chrome = hole.filter(ImageFilter.MaxFilter(51))
    ring = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ring_grad = vgradient((235, 238, 245), (120, 124, 136))
    ring.paste(ring_grad, (0, 0))
    ring.putalpha(ImageChops.subtract(chrome, hole))
    img.alpha_composite(ring)
    return bevel_opening(img, hole, lip_px=31, light=(255, 255, 255), shadow=0.85)


def arcade_synth():
    img = vgradient((24, 6, 52), (70, 10, 80))
    # The striped sun, half behind the horizon, on each panel.
    horizon = 640
    for cx in (PX0 / 2, (PX1 + W) / 2):
        sun = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        sd = ImageDraw.Draw(sun)
        r = 96
        for y in range(int(horizon - r), int(horizon)):
            t = (y - (horizon - r)) / r
            col = (int(255), int(220 - 150 * t), int(80 + 60 * t), 255)
            half = math.sqrt(max(0.0, r * r - (y - horizon) ** 2))
            gap = (y - (horizon - r)) > r * 0.45 and int((y - horizon) / 9) % 2 == 0
            if not gap:
                sd.line([(cx - half, y), (cx + half, y)], fill=col)
        img.alpha_composite(glow(sun.getchannel("A"), 30, (255, 90, 160), 0.9))
        img.alpha_composite(sun)
    # Mountains on the horizon, then the neon grid floor.
    m = Image.new("L", (W, H), 0)
    md = ImageDraw.Draw(m)
    rnd = random.Random(9)
    pts = [(0, horizon)]
    for x in range(0, W + 60, 60):
        pts.append((x, horizon - rnd.uniform(10, 70)))
    pts.append((W, horizon))
    md.polygon(pts, fill=255)
    hills = Image.new("RGBA", (W, H), (40, 10, 70, 0))
    hills.putalpha(ImageChops.multiply(m, side_mask()))
    img.alpha_composite(hills)
    floor_bg = Image.new("RGBA", (W, H), (20, 4, 40, 0))
    fm = Image.new("L", (W, H), 0)
    ImageDraw.Draw(fm).rectangle([0, horizon, W, H], fill=255)
    floor_bg.putalpha(ImageChops.multiply(fm, side_mask()))
    img.alpha_composite(floor_bg)
    grid = grid_floor((255, 60, 220), horizon, lines=12, alpha=200)
    img.alpha_composite(glow(grid.getchannel("A"), 5, (255, 60, 220), 0.8))
    img.alpha_composite(grid)
    hole = arcade_opening()
    neon = ImageChops.subtract(hole.filter(ImageFilter.MaxFilter(45)), hole.filter(ImageFilter.MaxFilter(39)))
    img.alpha_composite(glow(neon, 14, (0, 230, 255), 1.6))
    tube = Image.new("RGBA", (W, H), (150, 245, 255, 0))
    tube.putalpha(neon)
    img.alpha_composite(tube)
    return bevel_opening(img, hole, lip_px=31, light=(120, 240, 255), shadow=0.8)


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    for name, fn in (("glass", glass), ("midnight", midnight), ("porpoise", porpoise), ("frost", frost),
                     ("carbon", carbon), ("arcade", arcade), ("arcade-retro", arcade_retro),
                     ("arcade-synth", arcade_synth)):
        fn().save(os.path.join(OUT, name + ".png"), optimize=True)
        print("wrote", name)
