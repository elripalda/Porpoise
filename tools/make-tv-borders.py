#!/usr/bin/env python3
# Porpoise - draws the TV borders (assets/borders/tv-*.png and the theme ones):
# old TV sets and monitors round the 4:3 picture, from different eras, with
# no brand on them; and Porpoise's own, matched to its themes.
#
#   tv-80s          80s beige plastic: a channel dial, a volume knob, a grille
#   tv-woodgrain    a wood-grain cabinet with silver trim and two dials
#   tv-90s          90s gray plastic, rounded, a row of small buttons
#   tv-black        late-90s black flat-front tube, speakers both sides
#   monitor-beige   a beige home-computer monitor with its front panel
#   revolution      for the Revolution theme: white gloss, a light ring round the picture
#   star-cube       for the Star Cube theme: indigo, a cube pattern and a glow
#
# Saved small (make-borders.py's save_png): the zip has little room.
# Needs Pillow and numpy, and imagequant for the small files.  SPDX-License-Identifier: GPL-3.0-or-later
import importlib.util
import math
import os
import random
import sys

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "assets", "borders")
ONLY = sys.argv[2:]
spec = importlib.util.spec_from_file_location("mb", os.path.join(ROOT, "tools", "make-borders.py"))
mb = importlib.util.module_from_spec(spec)
sys.argv = sys.argv[:1]
spec.loader.exec_module(mb)
W, H, PX0, PX1 = mb.W, mb.H, mb.PX0, mb.PX1


def tube_opening(inset=22, radius=70, bow=10, s=4):
    """A tube's opening: just inside the picture, sides bowing out a little."""
    m = Image.new("L", (W * s, H * s), 0)
    d = ImageDraw.Draw(m)
    x0, y0, x1, y1 = PX0 + inset, inset, PX1 - inset, H - inset
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
            pts.append(((cx + math.cos(a) * radius) * s, (cy + math.sin(a) * radius) * s))

    r = radius
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


def plastic(base, light=30, noise=0, seed=1):
    """A plastic body: a soft vertical light, a faint texture."""
    top = tuple(min(255, c + light) for c in base)
    bottom = tuple(max(0, c - light) for c in base)
    img = mb.vgradient(top, bottom)
    if noise <= 0:
        return img  # flat plastic: a fraction of the file size
    rng = np.random.default_rng(seed)
    a = np.array(img).astype(np.int16)
    n = rng.integers(-noise, noise + 1, size=(H, W, 1))
    a[..., :3] = np.clip(a[..., :3] + n, 0, 255)
    return Image.fromarray(a.astype(np.uint8), "RGBA")


def wood(seed=3):
    """Wood grain: dark and light bands that wander, and fine streaks."""
    rng = np.random.default_rng(seed)
    y = np.arange(H)[:, None].astype(np.float32)
    x = np.arange(W)[None, :].astype(np.float32)
    warp = np.sin(y / 90.0 + np.sin(x / 300.0) * 2.0) * 18 + np.sin(y / 23.0) * 4
    bands = np.sin((x + warp) / 11.0) * 0.5 + 0.5
    streak = rng.random((1, W)).astype(np.float32)
    streak = np.repeat(streak, H, axis=0)
    t = 0.65 * bands + 0.35 * streak
    dark = np.array([78, 46, 24], np.float32)
    light = np.array([150, 98, 56], np.float32)
    rgb = dark + (light - dark) * t[..., None]
    a = np.dstack([rgb, np.full((H, W), 255, np.float32)]).astype(np.uint8)
    return Image.fromarray(a, "RGBA")


def knob(img, cx, cy, r, body, ridge, mark=(240, 240, 240)):
    d = ImageDraw.Draw(img)
    d.ellipse([cx - r - 4, cy - r + 6, cx + r + 4, cy + r + 10], fill=(0, 0, 0, 90))  # its shadow
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=body, outline=ridge, width=3)
    for i in range(24):  # the grip round its edge
        a = 2 * math.pi * i / 24
        d.line([(cx + math.cos(a) * (r - 8), cy + math.sin(a) * (r - 8)),
                (cx + math.cos(a) * (r - 2), cy + math.sin(a) * (r - 2))], fill=ridge, width=2)
    d.ellipse([cx - r * 0.55, cy - r * 0.55, cx + r * 0.55, cy + r * 0.55], fill=tuple(min(255, c + 25) for c in body[:3]))
    d.line([(cx, cy - r * 0.5), (cx, cy - r * 0.1)], fill=mark, width=4)


def grille(img, x0, y0, w, h, slot=(0, 0, 0, 150), step=12, horizontal=True):
    d = ImageDraw.Draw(img)
    if horizontal:
        for y in range(y0, y0 + h, step):
            d.rounded_rectangle([x0, y, x0 + w, y + step // 2], radius=step // 4, fill=slot)
    else:
        for x in range(x0, x0 + w, step):
            d.rounded_rectangle([x, y0, x + step // 2, y0 + h], radius=step // 4, fill=slot)


def buttons(img, x, y0, count, size, body, gap=18):
    d = ImageDraw.Draw(img)
    for i in range(count):
        y = y0 + i * (size + gap)
        d.rounded_rectangle([x - size // 2, y + 3, x + size // 2, y + size + 3], radius=5, fill=(0, 0, 0, 90))
        d.rounded_rectangle([x - size // 2, y, x + size // 2, y + size], radius=5, fill=body,
                            outline=tuple(max(0, c - 40) for c in body[:3]), width=2)


def led(img, x, y, color):
    m = Image.new("L", (W, H), 0)
    ImageDraw.Draw(m).ellipse([x - 6, y - 6, x + 6, y + 6], fill=255)
    img.alpha_composite(mb.glow(m, 10, color, 1.4))
    ImageDraw.Draw(img).ellipse([x - 5, y - 5, x + 5, y + 5], fill=color + (255,))


def tv_80s():
    img = plastic((196, 178, 146), light=26, seed=11)
    d = ImageDraw.Draw(img)
    # The darker panel on the right, with the dials and a grille under them.
    d.rounded_rectangle([PX1 + 30, 70, W - 34, H - 70], radius=18, fill=(120, 104, 82, 255))
    knob(img, (PX1 + W) // 2, 220, 62, (58, 52, 46, 255), (30, 26, 22, 255))
    knob(img, (PX1 + W) // 2, 400, 44, (58, 52, 46, 255), (30, 26, 22, 255))
    grille(img, PX1 + 70, 520, W - PX1 - 140, 420, slot=(40, 32, 24, 200), step=16)
    # A grille on the left too, and a power button.
    grille(img, 60, 160, PX0 - 120, 720, slot=(120, 104, 82, 220), step=16)
    buttons(img, PX0 // 2, 930, 1, 54, (170, 150, 120, 255))
    led(img, PX0 // 2, 1010, (255, 70, 40))
    return mb.bevel_opening(img, tube_opening(inset=24, radius=80, bow=14), light=(255, 240, 210), shadow=0.85)


def tv_woodgrain():
    img = wood(seed=5)
    d = ImageDraw.Draw(img)
    # Silver trim round the tube's frame.
    trim = tube_opening(inset=2, radius=90, bow=14).filter(ImageFilter.MaxFilter(61))
    silver = mb.vgradient((214, 218, 222), (128, 132, 138))
    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    layer.paste(silver, (0, 0), trim)
    img.alpha_composite(layer)
    # The control panel: brushed metal, two dials, a grille of slats.
    d.rounded_rectangle([PX1 + 40, 90, W - 40, H - 90], radius=10, fill=(170, 172, 168, 255),
                        outline=(90, 92, 90, 255), width=3)
    knob(img, (PX1 + W) // 2, 240, 66, (40, 34, 30, 255), (20, 16, 14, 255))
    knob(img, (PX1 + W) // 2, 430, 48, (40, 34, 30, 255), (20, 16, 14, 255))
    grille(img, PX1 + 74, 560, W - PX1 - 148, 380, slot=(60, 58, 54, 230), step=14)
    grille(img, 70, 200, PX0 - 140, 680, slot=(46, 26, 12, 170), step=14, horizontal=False)
    return mb.bevel_opening(img, tube_opening(inset=26, radius=90, bow=16), light=(240, 244, 248), shadow=0.9)


def tv_90s():
    img = plastic((92, 96, 102), light=34, seed=21)
    d = ImageDraw.Draw(img)
    # A speaker grille each side, round-holed.
    for x0 in (52, PX1 + 52):
        for row in range(16):
            for col in range(6):
                cx, cy = x0 + 16 + col * 24, 190 + row * 34
                d.ellipse([cx - 6, cy - 6, cx + 6, cy + 6], fill=(36, 38, 42, 255))
    buttons(img, (PX1 + W) // 2, 800, 4, 30, (120, 124, 130, 255), gap=14)
    led(img, PX0 // 2, 860, (60, 255, 120))
    return mb.bevel_opening(img, tube_opening(inset=26, radius=110, bow=18), light=(200, 205, 214), shadow=0.9)


def tv_black():
    img = plastic((22, 23, 26), light=18, seed=31)
    # A gloss band, the flat glass's frame, a thin silver line.
    gloss = Image.new("L", (W, H), 0)
    ImageDraw.Draw(gloss).rectangle([0, 0, W, 300], fill=60)
    img.alpha_composite(mb.glow(gloss, 120, (255, 255, 255), 1.0))
    for x0 in (44, PX1 + 44):
        mb_grille = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        grille(mb_grille, x0, 120, PX0 - 88, 840, slot=(8, 8, 10, 255), step=10)
        img.alpha_composite(mb_grille)
    hole = tube_opening(inset=18, radius=26, bow=2)
    line = ImageChops.subtract(hole.filter(ImageFilter.MaxFilter(25)), hole.filter(ImageFilter.MaxFilter(21)))
    silver = Image.new("RGBA", (W, H), (190, 194, 200, 0))
    silver.putalpha(line.point(lambda a: int(a * 0.7)))
    img.alpha_composite(silver)
    led(img, PX1 + 120, H - 70, (80, 170, 255))
    return mb.bevel_opening(img, hole, lip_px=21, light=(120, 124, 132), shadow=0.95, inner=0.45)


def monitor_beige():
    img = plastic((206, 196, 172), light=22, seed=41)
    d = ImageDraw.Draw(img)
    # The darker lower band with its front panel, at the right.
    d.rectangle([PX1, H - 260, W, H], fill=(176, 166, 142, 255))
    d.rectangle([0, H - 260, PX0, H], fill=(176, 166, 142, 255))
    d.line([(0, H - 260), (W, H - 260)], fill=(150, 140, 118, 255), width=3)
    for i in range(4):  # small adjustment knobs behind a flap
        knob(img, PX1 + 60 + i * 44, H - 150, 16, (140, 130, 108, 255), (100, 92, 74, 255), mark=(60, 56, 48))
    buttons(img, PX0 // 2, H - 200, 1, 60, (190, 180, 156, 255))
    led(img, PX0 // 2, H - 100, (60, 230, 90))
    grille(img, 60, 120, PX0 - 120, 520, slot=(150, 140, 118, 255), step=14)
    grille(img, PX1 + 60, 120, W - PX1 - 120, 520, slot=(150, 140, 118, 255), step=14)
    return mb.bevel_opening(img, tube_opening(inset=28, radius=60, bow=10), light=(255, 248, 230), shadow=0.8)


def logo_layer(size, alpha=255):
    path = os.path.join(ROOT, "assets", "brand", "dolphin-color.png")
    logo = Image.open(path).convert("RGBA")
    logo.thumbnail((size, size), Image.LANCZOS)
    if alpha < 255:
        a = logo.getchannel("A").point(lambda v: v * alpha // 255)
        logo.putalpha(a)
    return logo


def porpoise_white():
    img = mb.vgradient((250, 252, 255), (214, 222, 232))
    # Soft stripes, as Porpoise's white theme's background has.
    stripes = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(stripes)
    for y in range(0, H, 6):
        d.line([(0, y), (W, y)], fill=(200, 210, 222, 60), width=1)
    img.alpha_composite(stripes)
    hole = tube_opening(inset=16, radius=48, bow=0)
    ring = ImageChops.subtract(hole.filter(ImageFilter.MaxFilter(31)), hole.filter(ImageFilter.MaxFilter(17)))
    img.alpha_composite(mb.glow(ring, 18, (40, 180, 255), 1.4))
    core = Image.new("RGBA", (W, H), (60, 190, 255, 0))
    core.putalpha(ring)
    img.alpha_composite(core)
    logo = logo_layer(150)
    img.alpha_composite(logo, ((PX0 - logo.width) // 2, H - logo.height - 70))
    img.alpha_composite(logo, (PX1 + (W - PX1 - logo.width) // 2, H - logo.height - 70))
    return mb.cut_opening(img, hole)


def porpoise_indigo():
    img = mb.vgradient((40, 30, 110), (12, 8, 40))
    # A cube pattern: rows of small rhombi, fading toward the picture.
    pattern = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(pattern)
    for row in range(0, H + 60, 52):
        for col in range(0, W + 60, 60):
            x = col + (30 if (row // 52) % 2 else 0)
            y = row
            d.polygon([(x, y - 18), (x + 18, y), (x, y + 18), (x - 18, y)], outline=(120, 100, 255, 70))
    fade = Image.new("L", (W, H), 0)
    fd = ImageDraw.Draw(fade)
    fd.rectangle([0, 0, PX0, H], fill=255)
    fd.rectangle([PX1, 0, W, H], fill=255)
    fade = fade.filter(ImageFilter.GaussianBlur(60))
    pattern.putalpha(ImageChops.multiply(pattern.getchannel("A"), fade))
    img.alpha_composite(pattern)
    hole = tube_opening(inset=18, radius=40, bow=0)
    edge = ImageChops.subtract(hole.filter(ImageFilter.MaxFilter(21)), hole)
    img.alpha_composite(mb.glow(edge, 24, (150, 110, 255), 1.6))
    logo = logo_layer(140, 230)
    img.alpha_composite(logo, ((PX0 - logo.width) // 2, 80))
    img.alpha_composite(logo, (PX1 + (W - PX1 - logo.width) // 2, 80))
    return mb.cut_opening(img, hole)


BORDERS = {
    "tv-80s": tv_80s,
    "tv-woodgrain": tv_woodgrain,
    "tv-90s": tv_90s,
    "tv-black": tv_black,
    "monitor-beige": monitor_beige,
    "revolution": porpoise_white,
    "star-cube": porpoise_indigo,
}


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    for name in ONLY or list(BORDERS):
        size = mb.save_png(BORDERS[name](), os.path.join(OUT, name + ".png"))
        print(f"{name}: {size // 1024} KB")
