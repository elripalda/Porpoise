#!/usr/bin/env python3
# Porpoise - cuts assets/fonts/NotoSansJP-Porpoise.ttf: Noto Sans JP (SIL Open
# Font License, https://github.com/google/fonts/tree/main/ofl/notosansjp) at
# weight 600, subset to kana, CJK punctuation and every character the Japanese
# menus use (i18n/ja.json). Run it again after changing the Japanese text.
#   python3 tools/make-jp-font.py path/to/NotoSansJP[wght].ttf
# Needs fontTools.  SPDX-License-Identifier: GPL-3.0-or-later
import json
import os
import re
import sys

from fontTools import subset
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
source = sys.argv[1]
table = "".join(json.load(open(os.path.join(ROOT, "i18n", "ja.json"), encoding="utf-8")).values())
nunito = TTFont(os.path.join(ROOT, "assets", "fonts", "Nunito-Regular.ttf")).getBestCmap()
chars = set(table)
chars.update("日本語本体の設定")
for a, b in ((0x3041, 0x3097), (0x30A0, 0x3100), (0x3000, 0x3020)):
    chars.update(chr(c) for c in range(a, b))
need = sorted(ord(c) for c in chars if ord(c) > 0x7F and ord(c) not in nunito)
font = instancer.instantiateVariableFont(TTFont(source), {"wght": 600})
options = subset.Options()
options.layout_features = []
options.name_IDs = ["*"]
options.notdef_outline = True
options.hinting = False
subsetter = subset.Subsetter(options)
subsetter.populate(unicodes=need)
subsetter.subset(font)
out = os.path.join(ROOT, "assets", "fonts", "NotoSansJP-Porpoise.ttf")
font.save(out)
print("wrote", out, len(font.getBestCmap()), "characters")
