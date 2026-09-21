# -*- coding: utf-8 -*-
"""Generate app.ico from the user-supplied grass-block PNG (282x282)."""
import os

SRC = r"H:/workerspace/.reasonix/attachments/clipboard-20260829-113302.464200-000001.png"
DST = r"H:/workerspace/workapace/main/MinecraftPE-Win/handheld/project/win32_gl/app.ico"

try:
    from PIL import Image
except ImportError as e:
    raise SystemExit("Pillow not installed: %s" % e)

im = Image.open(SRC).convert("RGBA")
print("source:", im.size, im.mode)

# Classic grass block: top grass-green (upper half), sides dirt-brown.
# The user image is a front-facing grass block; square-crop is already fine.

sizes = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]
im.save(DST, format="ICO", sizes=sizes)

# Verify what we wrote.
check = Image.open(DST)
print("wrote:", DST, os.path.getsize(DST), "bytes")
print("ico frames:", getattr(check, "n_frames", 1))
