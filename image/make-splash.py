#!/usr/bin/env python3
"""Draws image/splash.png, shown by the SD card installer while it sets up the card (1920x1080).
Copyright (C) 2026 f3bandit. PolyForm Noncommercial 1.0.0 (see LICENSE.md).
Needs Pillow, Inter (SIL OFL 1.1) and DejaVu Sans Mono. Run it again only to change the picture."""
import sys
from PIL import Image, ImageDraw, ImageFont

W, H, S = 1920, 1080, 1.5               # the shell's sizes are for 1280x720
BG, TEXT, DIM, ACC = (14, 14, 16), (236, 232, 225), (141, 138, 133), (255, 179, 71)
INTER = "/usr/share/fonts/opentype/inter/"
MONO = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"

def font(path, size): return ImageFont.truetype(path, int(size * S))

img = Image.new("RGB", (W, H), BG)
# the zigzag background texture, as in the shell: accent colour at 13 %
tex = Image.new("RGBA", (W, H), (0, 0, 0, 0))
d = ImageDraw.Draw(tex)
a = (*ACC, 33)
t = 16 * S
y = 0.0
while y < H + t:
    x = 0.0
    while x < W + t:
        d.line([(x, y + 11 * S), (x + 8 * S, y + 5 * S), (x + 16 * S, y + 11 * S)], fill=a, width=2)
        x += t
    y += t
img.paste(tex, (0, 0), tex)

d = ImageDraw.Draw(img)
left = 56 * S
def spaced(x, y, s, f, fill, gap):
    for ch in s:
        d.text((x, y), ch, font=f, fill=fill)
        x += d.textlength(ch, font=f) + gap
spaced(left, 26 * S, "PHOSPHOR", font(MONO, 18), ACC, 3 * S)
d.text((left, 300 * S), "SETTING UP", font=font(MONO, 13), fill=DIM)
d.text((left, 322 * S), "Installing MiSTer and Phosphor", font=font(INTER + "Inter-SemiBold.otf", 32), fill=TEXT)
d.text((left, 372 * S), "This takes a few minutes. Leave the power on: the system restarts by itself when it's done.",
       font=font(INTER + "Inter-Regular.otf", 16), fill=DIM)
img.save(sys.argv[1] if len(sys.argv) > 1 else "splash.png", optimize=True)
