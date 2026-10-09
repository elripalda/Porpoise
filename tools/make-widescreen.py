#!/usr/bin/env python3
# Porpoise - builds assets/widescreen/ from Warped Polygon's GameCube widescreen
# code collection (the "Force 16:9" sets: Force_16x9_Dolphin_WS_Codes, the games
# with codes, and GC_Native_16x9_Forced, the games with their own 16:9 option).
#
#   python3 tools/make-widescreen.py <Force_16x9_Dolphin_WS_Codes> <GC_Native_16x9_Forced> [<Enhancement-Codes>]
#
# With Admentus64's Enhancement Codes (GPL-3.0, github.com/Admentus64/Enhancement-Codes)
# as the third, its 16:9 codes for the Wii's Virtual Console N64 games are added too,
# off: a choice in each game's Cheats and Patches.
#
# Writes:
#   assets/widescreen/codes.ini   each game's codes, as [[<ID>]] then Dolphin's
#                                 [Gecko] / [ActionReplay] / [OnFrame] sections; its
#                                 [<kind>_Enabled] lists name the widescreen codes,
#                                 which Porpoise turns on (the others, such as 60 Hz
#                                 or 60 FPS codes, stay a player's choice)
#   assets/widescreen/native.txt  the games with a 16:9 option of their own
#
# Only the codes are kept: each file's [Video_Settings] and [Core] are Porpoise's
# to decide (src/ui_widescreen.cpp), except that a game whose codes work only
# with Dolphin's widescreen hack on (its file sets wideScreenHack = True) gets
# a [Porpoise] section saying so. A few entries are repaired, each listed in
# FIXES below with why; a code that still has a line Dolphin can't read is left
# out and reported. A code whose name Dolphin's own Sys/GameSettings already uses
# for the game gets " (WP)" added, so the two never mix.
#
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SYS = os.path.join(ROOT, ".deps", "dolphin-src", "Data", "Sys", "GameSettings")
GECKO_LINE = re.compile(r"^[0-9A-Fa-f]{8} [0-9A-Fa-f]{8}$")
AR_ENCRYPTED = re.compile(r"^[0-9A-Z]{4}-[0-9A-Z]{4}-[0-9A-Z]{5}$")
KINDS = ("OnFrame", "ActionReplay", "Gecko")

# (game, what) -> why. Applied before the checks.
FIXES = {
    ("GC6J01", "move"): "a Gecko code (F6 search, D2 and E0 lines) filed under [ActionReplay]; moved to [Gecko], "
                        "and its last line's stray 'B' dropped",
    ("GD6P70", "line"): "0430A434 3F4000004: a stray ninth digit; the pair before it writes BF400000 (-0.75), "
                        "this one 3F400000 (+0.75)",
    ("GHNE71", "move"): "encrypted Action Replay codes filed under [Gecko]; moved to [ActionReplay], which "
                        "Dolphin decrypts",
    ("GT3E52", "move"): "a Gecko code (a C2 assembly insert) filed under [ActionReplay]; moved to [Gecko]",
    ("GPSP8P", "move"): "a Gecko code (20 if, 06 string write, C0 assembly, E2 end-if) filed under "
                        "[ActionReplay]; moved to [Gecko]",
}
GECKO_IN_AR = {"GT3E52", "GPSP8P"}
RENAMED_FILES = {"GP6P01,": "GP6P01"}  # a stray comma in the file name
WIDE = re.compile(r"widescreen|16:9|16x9|ratio|aspect", re.I)
FRAME_RATE = re.compile(r"60 ?(fps|hz)", re.I)


def needs_hack(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        return bool(re.search(r"^\s*widescreenhack\s*=\s*true\s*$", f.read(), re.I | re.M))


def parse(path):
    """[(kind, name, lines)], {kind: [enabled names]} from one file."""
    codes, enabled = [], {k: [] for k in KINDS}
    section = None
    with open(path, encoding="utf-8", errors="replace") as f:
        for raw in f.read().splitlines():
            line = raw.strip()
            if not line:
                continue
            if line.startswith("["):
                section = line[1:line.index("]")] if "]" in line else ""
                continue
            if section in KINDS:
                if line.startswith("$"):
                    codes.append([section, line, []])
                elif codes and codes[-1][0] == section:
                    codes[-1][2].append(line)
            elif section and section.endswith("_Enabled") and section[:-8] in KINDS and line.startswith("$"):
                enabled[section[:-8]].append(line)
    return codes, enabled


def dolphin_names(game):
    names = set()
    for n in (game[:3], game[:4], game):
        p = os.path.join(SYS, n + ".ini")
        if os.path.exists(p):
            codes, _ = parse(p)
            names |= {(k, name.lower()) for k, name, _ in codes}
    return names


VC_16x9 = re.compile(r"16:9|^\$widescreen$", re.I)


def vc_blocks(vc_dir, dropped):
    """The Virtual Console N64 games' 16:9 codes from Enhancement Codes: every
    "(VC)" folder's game files, the codes made for 16:9 (not 21:9 or 48:9), none
    turned on."""
    blocks = []
    for folder in sorted(os.listdir(vc_dir)):
        if not folder.endswith("(VC)"):
            continue
        for fname in sorted(os.listdir(os.path.join(vc_dir, folder))):
            game = fname[:-4].upper()
            if not fname.lower().endswith(".ini") or not re.fullmatch(r"[A-Z0-9]{6}", game):
                continue
            codes, _ = parse(os.path.join(vc_dir, folder, fname))
            kept = []
            for kind, name, lines in codes:
                if not VC_16x9.search(name):
                    continue
                body = [l for l in lines if not l.startswith(("*", "#"))]
                if not body or not all(GECKO_LINE.match(l) for l in body):
                    dropped.append(f"{game} {kind} {name}: a line Dolphin can't read")
                    continue
                kept.append((kind, name, lines))
            if not kept:
                continue
            out = [f"[[{game}]]"]
            for kind in KINDS:
                mine = [k for k in kept if k[0] == kind]
                if mine:
                    out.append(f"[{kind}]")
                    for _, name, lines in mine:
                        out.append(name)
                        out.extend(lines)
            blocks.append("\n".join(out))
    return blocks


def own_blocks():
    """Porpoise's own codes (tools/widescreen-porpoise.ini): blocks as they are,
    comments left out, none turned on."""
    path = os.path.join(ROOT, "tools", "widescreen-porpoise.ini")
    if not os.path.exists(path):
        return []
    with open(path, encoding="utf-8") as f:
        text = "\n".join(l for l in f.read().splitlines() if not l.startswith("#")).strip()
    return [b.strip() for b in re.split(r"\n(?=\[\[)", text) if b.strip()]


def main():
    if len(sys.argv) not in (3, 4):
        print(__doc__ or "usage: make-widescreen.py <codes dir> <native dir> [<Enhancement-Codes dir>]")
        return 2
    codes_dir, native_dir = sys.argv[1], sys.argv[2]
    vc_dir = sys.argv[3] if len(sys.argv) == 4 else None
    out_dir = os.path.join(ROOT, "assets", "widescreen")
    os.makedirs(out_dir, exist_ok=True)
    blocks, dropped, renamed = [], [], []
    for fname in sorted(os.listdir(codes_dir)):
        if not fname.lower().endswith(".ini"):
            continue
        game = RENAMED_FILES.get(fname[:-4], fname[:-4]).upper()
        if not re.fullmatch(r"[A-Z0-9]{6}", game):
            dropped.append(f"{fname}: not a game ID")
            continue
        codes, enabled = parse(os.path.join(codes_dir, fname))
        # The repairs.
        for c in codes:
            if game == "GC6J01" and c[0] == "ActionReplay" and c[1] == "$Widescreen Hack":
                c[0] = "Gecko"
                c[2] = [re.sub(r"^(E0000000 80008000)B$", r"\1", l) for l in c[2]]
                enabled["ActionReplay"] = [n for n in enabled["ActionReplay"] if n != c[1]]
                enabled["Gecko"].append(c[1])
            if game in GECKO_IN_AR and c[0] == "ActionReplay" and c[1] == "$Widescreen Hack":
                c[0] = "Gecko"
                if c[1] in enabled["ActionReplay"]:
                    enabled["ActionReplay"].remove(c[1])
                    enabled["Gecko"].append(c[1])
            if game == "GD6P70":
                c[2] = ["0430A434 3F400000" if l == "0430A434 3F4000004" else l for l in c[2]]
            if game == "GHNE71" and c[0] == "Gecko" and c[2] and all(AR_ENCRYPTED.match(l) for l in c[2]):
                if c[1] in enabled["Gecko"]:
                    enabled["Gecko"].remove(c[1])
                    enabled["ActionReplay"].append(c[1])
                c[0] = "ActionReplay"
        taken = dolphin_names(game)
        kept = []
        for kind, name, lines in codes:
            body = [l for l in lines if not l.startswith(("*", "#"))]
            ok = bool(body) and all(GECKO_LINE.match(l) or (kind == "ActionReplay" and AR_ENCRYPTED.match(l))
                                    for l in body)
            if not ok:
                dropped.append(f"{game} {kind} {name}: a line Dolphin can't read")
                continue
            new = name
            if (kind, name.lower()) in taken:
                new = name + " (WP)"
                renamed.append(f"{game} {name} -> {new}")
            on = name in enabled[kind]
            kept.append([kind, new, lines, on])
        if not kept:
            continue
        # The widescreen codes: by their names (widescreen, 16:9, a ratio fix),
        # never a frame-rate code. The ones the collection turns on; where it
        # names one that isn't there (a name that differs), its first one.
        ws = [k for k in kept if WIDE.search(k[1]) and not FRAME_RATE.search(k[1])]
        chosen = [k for k in ws if k[3]] or ws[:1]
        for k in kept:
            k[3] = k in chosen
        out = [f"[[{game}]]"]
        if needs_hack(os.path.join(codes_dir, fname)):
            out += ["[Porpoise]", "widescreen_hack = True"]
        for kind in KINDS:
            mine = [k for k in kept if k[0] == kind]
            if not mine:
                continue
            out.append(f"[{kind}]")
            for _, name, lines, _ in mine:
                out.append(name)
                out.extend(lines)
            on = [name for _, name, _, o in mine if o]
            if on:
                out.append(f"[{kind}_Enabled]")
                out.extend(on)
        blocks.append("\n".join(out))
    if vc_dir:
        blocks += vc_blocks(vc_dir, dropped)
    blocks += own_blocks()
    natives = sorted({RENAMED_FILES.get(f[:-4], f[:-4]).upper() for f in os.listdir(native_dir)
                      if f.lower().endswith(".ini")})
    header = ("# Porpoise: GameCube widescreen codes, from Warped Polygon's collection (the Force 16:9 set),\n"
              "# made by tools/make-widescreen.py. Each game is [[<ID>]] followed by Dolphin's sections.\n")
    if vc_dir:
        header += ("# The Virtual Console N64 games' 16:9 codes (off, a choice in Cheats and Patches) are from\n"
                   "# Admentus64's Enhancement Codes (GPL-3.0), with their authors' names where they give them.\n")
    with open(os.path.join(out_dir, "codes.ini"), "w", encoding="utf-8", newline="\n") as f:
        f.write(header + "\n" + "\n\n".join(blocks) + "\n")
    with open(os.path.join(out_dir, "native.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write("# GameCube games with a 16:9 option of their own (Warped Polygon's collection).\n")
        f.write("\n".join(natives) + "\n")
    print(f"codes: {len(blocks)} games; native 16:9: {len(natives)} games")
    for why in FIXES.values():
        print("fixed:", why)
    for d in dropped:
        print("left out:", d)
    for r in renamed:
        print("renamed:", r)
    return 0


if __name__ == "__main__":
    sys.exit(main())
