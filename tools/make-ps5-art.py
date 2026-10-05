#!/usr/bin/env python3
# Porpoise - the PS5 home screen's art for the title, from Ruben's pictures in
# tools/ps5-art: icon0.png (512x512 PNG) from icon.jpg, and pic0.dds / pic1.dds
# (the backdrop behind the selected title: 3840x2160 BC7, as
# tools/validate-assets.sh requires) from backdrop.jpg.
#   python3 tools/make-ps5-art.py        (needs Pillow and etcpak)
# SPDX-License-Identifier: GPL-3.0-or-later
import os
import struct

import etcpak
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, "tools", "ps5-art")
SYS = os.path.join(ROOT, "sce_sys")


def fit(image, w, h):
    """Scaled to cover w x h, centred, cropped."""
    scale = max(w / image.width, h / image.height)
    scaled = image.resize((round(image.width * scale), round(image.height * scale)), Image.LANCZOS)
    x, y = (scaled.width - w) // 2, (scaled.height - h) // 2
    return scaled.crop((x, y, x + w, y + h))


def dds_bc7(rgba, w, h):
    """A DX10 DDS holding one BC7_UNORM 2D texture without mipmaps."""
    flags = 0x1 | 0x2 | 0x4 | 0x1000 | 0x20000 | 0x80000  # caps, height, width, pixelformat, mipmaps, linearsize
    linear = w // 4 * (h // 4) * 16
    header = struct.pack("<4sIIIIIII", b"DDS ", 124, flags, h, w, linear, 1, 1)
    header += b"\0" * 44  # reserved
    header += struct.pack("<II4sIIIII", 32, 0x4, b"DX10", 0, 0, 0, 0, 0)  # pixel format: FOURCC DX10
    header += struct.pack("<IIIII", 0x1000, 0, 0, 0, 0)  # caps: texture
    header += struct.pack("<IIIII", 98, 3, 0, 1, 1)  # BC7_UNORM, 2D, no flags, one, straight alpha
    assert len(header) == 148
    return header + etcpak.compress_bc7(rgba, w, h)


def main():
    icon = fit(Image.open(os.path.join(ART, "icon.jpg")).convert("RGB"), 512, 512)
    icon.save(os.path.join(SYS, "icon0.png"), optimize=True)
    back = fit(Image.open(os.path.join(ART, "backdrop.jpg")).convert("RGBA"), 3840, 2160)
    data = dds_bc7(back.tobytes(), 3840, 2160)
    for name in ("pic0.dds", "pic1.dds"):
        with open(os.path.join(SYS, name), "wb") as f:
            f.write(data)
    print("wrote sce_sys/icon0.png, pic0.dds and pic1.dds")


if __name__ == "__main__":
    main()
