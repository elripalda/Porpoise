#!/usr/bin/env python3
# Porpoise - cuts the fonts for the menus in Japanese, Chinese and Korean:
# assets/fonts/NotoSans{JP,SC,TC,KR}-Porpoise.ttf, from Noto Sans JP, SC, TC
# and KR (SIL Open Font License, https://github.com/google/fonts/tree/main/ofl)
# at weight 600, each subset to the characters its language's menus use
# (i18n/<code>.json), the symbols every language uses that Nunito lacks
# (□ △ ○ and the like), and the languages' own names in the Language setting.
# Japanese also keeps all of kana and CJK punctuation. Run it again after
# changing any of these languages' text.
#   python3 tools/make-cjk-fonts.py path/to/google/fonts/ofl
# Needs fontTools.  SPDX-License-Identifier: GPL-3.0-or-later
import glob
import json
import os
import sys

from fontTools import subset
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OFL = sys.argv[1]
# (output, source, translation)
FONTS = [
    ("JP", "notosansjp/NotoSansJP[wght].ttf", "ja"),
    ("SC", "notosanssc/NotoSansSC[wght].ttf", "zh-Hans"),
    ("TC", "notosanstc/NotoSansTC[wght].ttf", "zh-Hant"),
    ("KR", "notosanskr/NotoSansKR[wght].ttf", "ko"),
]
# The Language setting's names and "System" in these languages
# (src/ui_i18n.cpp): shown whatever the menus' language, so every font holds
# the ones it can draw and Porpoise takes the rest from the others.
NAMES = "日本語本体の設定简体中文繁體中文한국어跟随系统跟隨系統시스템 설정"


def own_script(cp):
    return 0x2E80 <= cp <= 0x9FFF or 0xAC00 <= cp <= 0xD7AF or 0xF900 <= cp <= 0xFAFF or 0xFF00 <= cp <= 0xFFEF


nunito = TTFont(os.path.join(ROOT, "assets", "fonts", "Nunito-Regular.ttf")).getBestCmap()
symbols = set()
for path in glob.glob(os.path.join(ROOT, "i18n", "*.json")):
    data = json.load(open(path, encoding="utf-8"))
    texts = [s["en"] for s in data] if isinstance(data, list) else list(data.values())
    for text in texts:
        symbols.update(c for c in text if not own_script(ord(c)))

for name, source, lang in FONTS:
    chars = set("".join(json.load(open(os.path.join(ROOT, "i18n", lang + ".json"), encoding="utf-8")).values()))
    chars |= symbols | set(NAMES)
    if lang == "ja":
        for a, b in ((0x3041, 0x3097), (0x30A0, 0x3100), (0x3000, 0x3020)):
            chars.update(chr(c) for c in range(a, b))
    else:
        chars.update(chr(c) for c in range(0x3000, 0x3020))
    font = TTFont(os.path.join(OFL, source))
    cmap = font.getBestCmap()
    need = sorted(ord(c) for c in chars if ord(c) > 0x7F and ord(c) not in nunito and ord(c) in cmap)
    font = instancer.instantiateVariableFont(font, {"wght": 600})
    options = subset.Options()
    options.layout_features = []
    options.name_IDs = ["*"]
    options.notdef_outline = True
    options.hinting = False
    subsetter = subset.Subsetter(options)
    subsetter.populate(unicodes=need)
    subsetter.subset(font)
    out = os.path.join(ROOT, "assets", "fonts", "NotoSans%s-Porpoise.ttf" % name)
    font.save(out)
    print("wrote", out, len(font.getBestCmap()), "characters")
