#!/usr/bin/env python3
"""Sample GameCube .gci saves (invented games) in Dolphin's GCI-folder layout,
with RGB5A3 icons and banners, to exercise Porpoise's memory card reader."""
import struct, sys, time
from pathlib import Path
from PIL import Image

out = Path(sys.argv[1]); covers = Path(sys.argv[2])

def rgb5a3(r, g, b, a):
    if a >= 224:
        return 0x8000 | ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)
    return ((a >> 5) << 12) | ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4)

def tiles(img, w, h):
    px = img.convert('RGBA').resize((w, h), Image.LANCZOS).load()
    data = bytearray()
    for ty in range(0, h, 4):
        for tx in range(0, w, 4):
            for y in range(4):
                for x in range(4):
                    data += struct.pack('>H', rgb5a3(*px[tx + x, ty + y]))
    return bytes(data)

saves = [
    ("A", "PRVW01", "Island Adventure", "Day 12 - Lighthouse", 3),
    ("A", "PRVW02", "Comet Racers", "Grand Prix records", 2),
    ("A", "PRVW03", "Haunted Manor", "Floor 3 - 64 ghosts", 4),
    ("A", "PRVW04", "Sunny Shores", "Sunny Shores Save", 6),
    ("A", "PRVW05", "Sky Sail", "Voyage log", 12),
    ("B", "PRVW06", "Mech Hunter", "Mission 7", 5),
    ("B", "PRVW07", "Kart Party", "Time trials", 2),
    ("B", "PRVW08", "Garden Critters", "Spring garden", 3),
]
now = int(time.time()) - 946684800
for i, (card, gid, title, detail, blocks) in enumerate(saves):
    img = Image.open(covers / f"{gid}.png")
    w, h = img.size
    icon = tiles(img.crop((0, h // 5, w, h // 5 + w)), 32, 32)
    banner = tiles(img.crop((0, int(h * 0.35), w, int(h * 0.35) + w // 3)), 96, 32)
    data = bytearray(blocks * 0x2000)
    data[0:len(banner)] = banner
    data[len(banner):len(banner) + len(icon)] = icon
    comment = len(banner) + len(icon)
    data[comment:comment + 32] = title.encode()[:32].ljust(32, b'\0')
    data[comment + 32:comment + 64] = detail.encode()[:32].ljust(32, b'\0')
    entry = bytearray(64)
    entry[0:4] = gid[:4].encode(); entry[4:6] = gid[4:6].encode(); entry[6] = 0xFF; entry[7] = 2
    entry[8:40] = f"{title.replace(' ', '')}_save".encode()[:32].ljust(32, b'\0')
    struct.pack_into('>IIHHBBHHHI', entry, 0x28, now - i * 86400 * 3, 0, 0x0002, 0x0003, 4, 0, 5 + i * 10,
                     blocks, 0xFFFF, comment)
    d = out / "User" / "GC" / "USA" / f"Card {card}"; d.mkdir(parents=True, exist_ok=True)
    (d / f"01-{gid}-{title.replace(' ', '')}.gci").write_bytes(bytes(entry) + bytes(data))
print("saves in", out)
