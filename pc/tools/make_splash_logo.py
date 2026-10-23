#!/usr/bin/env python3
"""Turn a rendered logo PNG into the loading screen's logo file.

    python pc/tools/make_splash_logo.py logo.png [width]

Writes portmaster/ac-gc/res/splash_logo.bin: "OCLG", u16 width, u16 height
(little endian), then straight-alpha RGBA8 rows, top row first. The boot
splash (pc_overlay.c) reads it with plain fopen, so the port needs no image
decoder; without the file the splash just has no logo.

The PNG comes from docs/logo.svg, rendered with a transparent background,
for example with Chrome:
    chrome --headless --default-background-color=00000000
           --force-device-scale-factor=2 --window-size=969,320
           --screenshot=logo.png docs/logo.svg
Needs Pillow.
"""
import struct
import sys
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[2]
OUT = REPO / "portmaster" / "ac-gc" / "res" / "splash_logo.bin"


def main():
    src = Path(sys.argv[1])
    width = int(sys.argv[2]) if len(sys.argv) > 2 else 420
    im = Image.open(src).convert("RGBA")
    box = im.getbbox()  # trim the empty margin
    if box:
        im = im.crop(box)
    height = round(im.height * width / im.width)
    im = im.resize((width, height), Image.LANCZOS)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "wb") as f:
        f.write(b"OCLG" + struct.pack("<HH", width, height))
        f.write(im.tobytes())
    print(f"{OUT.relative_to(REPO).as_posix()}: {width}x{height}, {OUT.stat().st_size} bytes")


if __name__ == "__main__":
    main()
