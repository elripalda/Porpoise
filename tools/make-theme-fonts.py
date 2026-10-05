#!/usr/bin/env python3
# Porpoise - cuts the fonts two themes write in, from Google Fonts (SIL Open
# Font License, https://github.com/google/fonts/tree/main/ofl):
#   Terminal:  JetBrains Mono at weights 400, 600, 700 and 800
#              -> assets/fonts/JetBrainsMono-{Regular,SemiBold,Bold,ExtraBold}.ttf
#   Broadcast: VT323 -> assets/fonts/VT323-Regular.ttf
# each subset to the letters the menus use beyond Chinese, Japanese and Korean
# (Latin, Latin Extended-A, Cyrillic and typographic marks).
#   python3 tools/make-theme-fonts.py path/to/google/fonts/ofl
# Needs fontTools.  SPDX-License-Identifier: GPL-3.0-or-later
import os
import sys

from fontTools import subset
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OFL = sys.argv[1]
OUT = os.path.join(ROOT, "assets", "fonts")
CHARS = (list(range(0x20, 0x7F)) + list(range(0xA0, 0x180)) + list(range(0x400, 0x460)) +
         [0x2013, 0x2014, 0x2018, 0x2019, 0x201A, 0x201C, 0x201D, 0x201E, 0x2022, 0x2026, 0x20AC, 0x2116, 0x25B6])


def cut(font, out):
    options = subset.Options()
    options.layout_features = ["kern"]
    options.name_IDs = ["*"]
    options.notdef_outline = True
    options.hinting = False
    cmap = font.getBestCmap()
    s = subset.Subsetter(options)
    s.populate(unicodes=[c for c in CHARS if c in cmap])
    s.subset(font)
    font.save(out)
    print("wrote", out, len(font.getBestCmap()), "characters")


for weight, name in ((400, "Regular"), (600, "SemiBold"), (700, "Bold"), (800, "ExtraBold")):
    font = instancer.instantiateVariableFont(TTFont(os.path.join(OFL, "jetbrainsmono", "JetBrainsMono[wght].ttf")),
                                             {"wght": weight})
    cut(font, os.path.join(OUT, "JetBrainsMono-%s.ttf" % name))
cut(TTFont(os.path.join(OFL, "vt323", "VT323-Regular.ttf")), os.path.join(OUT, "VT323-Regular.ttf"))
