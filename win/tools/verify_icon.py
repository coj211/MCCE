# -*- coding: utf-8 -*-
"""Verify the freshly built exe embeds the new grass-block icon.

Strategy: Pillow writes the 256x256 ICO frame as a raw PNG inside the ICO
container; the MSVC resource compiler stores that same PNG data verbatim as
the RT_ICON payload. So the exact 256x256 PNG bytes from app.ico must appear
inside MinecraftWin32_GL.exe.
"""
import struct

ICO = r"H:/workerspace/workapace/main/MinecraftPE-Win/handheld/project/win32_gl/app.ico"
EXE = r"H:/workerspace/workapace/main/MinecraftPE-Win/MinecraftWin32_GL.exe"

with open(ICO, "rb") as f:
    ico = f.read()

count = struct.unpack("<H", ico[4:6])[0]
png_256 = None
for i in range(count):
    off = 6 + i * 16
    w, h = ico[off], ico[off + 1]
    size = struct.unpack("<I", ico[off + 8 : off + 12])[0]
    data_off = struct.unpack("<I", ico[off + 12 : off + 16])[0]
    if w == 0 and h == 0:  # 256x256
        png_256 = ico[data_off : data_off + size]
        break

assert png_256, "no 256x256 frame found in app.ico"
print("256x256 PNG bytes from app.ico:", len(png_256))

with open(EXE, "rb") as f:
    exe = f.read()

idx = exe.find(png_256)
print("embedded at exe offset:", idx)
if idx >= 0:
    print("RESULT: OK — new grass-block icon is embedded in", EXE)
else:
    # Fallback: also check the 48x48 frame in case 256 was re-encoded.
    print("RESULT: FAIL — 256x256 PNG not found verbatim in exe")
