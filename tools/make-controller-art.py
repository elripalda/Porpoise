#!/usr/bin/env python3
# Porpoise - draws the controller art and the button icons from Zacksly's
# "PS5 Button Icons and Controls" (CC BY 3.0, third_party/zacksly-ps5-icons):
#
#   assets/ui/buttons.png           every button icon, in an 8x4 atlas of
#                                   96-pixel cells, in the order of ICONS below
#                                   (= porpoise::ui::Icon in src/ui_gfx.hpp)
#   assets/ui/dualsense.png         the controller on the button-mapping screen
#   assets/ui/controller-lines.png  the controller with lines out to its buttons,
#                                   for the in-game Controls tab
#
# The art is modified on the way (see third_party/zacksly-ps5-icons/README.md):
# the PlayStation logo is removed, lines are thickened, and the schematic loses
# its own labels so that Porpoise can draw the GameCube buttons there.
#
# Needs cairosvg and Pillow:  pip install cairosvg pillow
# SPDX-License-Identifier: GPL-3.0-or-later
import io
import os
import re
import sys
import xml.etree.ElementTree as ET

import cairosvg
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "third_party", "zacksly-ps5-icons")
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "assets", "ui")

# The atlas order. Keep in step with enum class Icon in src/ui_gfx.hpp.
ICONS = [
    ("outline", "Cross"), ("outline", "Circle"), ("outline", "Square"), ("outline", "Triangle"),
    ("solid", "Cross"), ("solid", "Circle"), ("solid", "Square"), ("solid", "Triangle"),
    ("outline", "L1"), ("outline", "R1"), ("outline", "L2"), ("outline", "R2"),
    ("outline", "Options"), ("outline", "Create"), ("outline", "Touch Pad Press"), ("outline", "D-Pad"),
    ("outline", "D-Pad Up"), ("outline", "D-Pad Down"), ("outline", "D-Pad Left"), ("outline", "D-Pad Right"),
    ("outline", "Left Stick"), ("outline", "Right Stick"), ("outline", "Left Stick Click"),
    ("outline", "Right Stick Click"),
    ("outline", "Left Stick All"), ("outline", "Right Stick All"),
]
CELL, COLS, ROWS = 96, 8, 4

# The PlayStation logo's three paths, in both controller drawings.
LOGO = ("M2059.13,1055.48", "M2104.14,1105", "M1979.14,1097.86")

# Where dualsense.png comes from in the 4096x2160 drawing, and its scale in
# the app's own units (1400x900, the numbers src/ui_app_controls.cpp uses).
PAD_X0, PAD_Y0, PAD_K = 752.0, 228.0, 0.54
PAD_W, PAD_H = 1400, 900
PAD_PIXELS = 1000  # the PNG's width; the app scales it to its units

# The schematic's crop; src/ui_app_ingame.cpp has the same numbers.
LINES_X0, LINES_Y0, LINES_W, LINES_H = 780.0, 150.0, 2540.0, 1610.0
LINES_PIXELS = 1100


def render(svg_bytes, width):
    png = cairosvg.svg2png(bytestring=svg_bytes, output_width=int(round(width)))
    return Image.open(io.BytesIO(png)).convert("RGBA")


def edit_svg(path, drop=lambda el: False, stroke_scale=1.0):
    ET.register_namespace("", "http://www.w3.org/2000/svg")
    tree = ET.parse(path)
    root = tree.getroot()
    for parent in list(root.iter()):
        for el in list(parent):
            if el.attrib.get("d", "").startswith(LOGO) or drop(el):
                parent.remove(el)
    if stroke_scale != 1.0:
        for style in root.iter("{http://www.w3.org/2000/svg}style"):
            style.text = re.sub(r"stroke-width:([\d.]+)px",
                                lambda m: "stroke-width:%.2fpx" % (float(m.group(1)) * stroke_scale), style.text)
    return ET.tostring(root)


def first_point(el):
    tag = el.tag.split("}")[1]
    a = el.attrib
    if tag == "circle":
        return float(a["cx"]), float(a["cy"])
    if tag == "line":
        return float(a["x1"]), float(a["y1"])
    if tag in ("polygon", "polyline"):
        n = a["points"].split()
        return float(n[0]), float(n[1])
    if tag == "path":
        m = re.match(r"M\s*(-?[\d.]+)[ ,]?(-?[\d.]+)", a.get("d", ""))
        if m:
            return float(m.group(1)), float(m.group(2))
    return None


def atlas():
    sheet = Image.new("RGBA", (CELL * COLS, CELL * ROWS), (0, 0, 0, 0))
    for i, (style, name) in enumerate(ICONS):
        with open(os.path.join(SRC, style, name + ".svg"), "rb") as f:
            icon = render(f.read(), CELL)
        sheet.alpha_composite(icon, ((i % COLS) * CELL, (i // COLS) * CELL))
    sheet.save(os.path.join(OUT, "buttons.png"), optimize=True)


def dualsense():
    px = PAD_K * PAD_PIXELS / PAD_W  # PNG pixels per drawing unit
    full_w = 4096 * px
    box = (round(PAD_X0 * px), round(PAD_Y0 * px), round(PAD_X0 * px) + PAD_PIXELS,
           round(PAD_Y0 * px) + round(PAD_PIXELS * PAD_H / PAD_W))
    lines = render(edit_svg(os.path.join(SRC, "controller", "outline.svg"), stroke_scale=1.35), full_w).crop(box)
    solid = render(edit_svg(os.path.join(SRC, "controller", "solid.svg")), full_w).crop(box)
    # A faint glass body under the lines: the solid drawing's shape, in white.
    alpha = solid.getchannel("A").point(lambda a: a * 0.13)
    body = Image.new("RGBA", solid.size, (255, 255, 255, 0))
    body.putalpha(alpha)
    body.alpha_composite(lines)
    body.save(os.path.join(OUT, "dualsense.png"), optimize=True)


def controller_lines():
    def drop(el):
        tag = el.tag.split("}")[1]
        if tag == "polyline":
            return False  # the lines themselves
        p = first_point(el)
        # Everything beside or above the controller is a label of the pack's own.
        return p is not None and (p[0] < 1150 or p[0] > 2950 or p[1] < 300)

    px = LINES_PIXELS / LINES_W
    img = render(edit_svg(os.path.join(SRC, "controller", "schematic.svg"), drop, stroke_scale=1.3), 4096 * px)
    box = (round(LINES_X0 * px), round(LINES_Y0 * px), round(LINES_X0 * px) + LINES_PIXELS,
           round(LINES_Y0 * px) + round(LINES_H * px))
    img.crop(box).save(os.path.join(OUT, "controller-lines.png"), optimize=True)


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    atlas()
    dualsense()
    controller_lines()
    print("wrote buttons.png, dualsense.png, controller-lines.png to", OUT)
