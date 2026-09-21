#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
生成 Android 版的中文（CJK）字形图集。

背景 —— 为什么要这么生成：
  Windows 版 MCPE 的中文字形是「运行期用 GDI 现渲染成图集」的，见
  handheld/src/client/gui/Font.cpp 的 initCjkFont()：
      CreateFontW(-16, ..., NONANTIALIASED_QUALITY, face)
      DrawTextW(hdc, &wc, 1, &rc, DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOCLIP)
  16px 格子、2048x1024 图集、128 列 64 行；字体优先取
  data/fonts/custom.ttf（family = "Minecraft AE Pixel"），否则回退系统字体。

  Android 没有 GDI（那个分支本来就写在 #ifdef _WIN32 里）。Android 分支改成
  从这里离线生成的 assets/fonts/cjk_atlas.bmp 读入后上传为 GL 纹理。

  本脚本走的是**与 Windows 版完全相同的那套 GDI 调用**，所以离线产物与
  Windows 版运行期生成的图集逐像素一致（字体、字号、质量、DT_* 标志、
  格子尺寸全对齐）。

字形顺序（必须与 C++ 侧一致，C++ 侧据此建 unicode -> 格子 索引）：
  [0 .. N-1]                 cjk_gb2312.h 的 g_cjkChars（6763 个 GB2312 汉字）
  [N .. N+extra-1]           Font.cpp 里 kExtraChars（中文标点 / 常用符号）

输出：
  handheld/data/fonts/cjk_atlas.bmp
    54 字节 BMP 头（BITMAPINFOHEADER，biHeight = -1024 表示 top-down）
    + 2048x1024 的 BGRA 像素 = 8388662 字节。
    R = G = B = A = 字形覆盖率（0 或 255），所以拿图片查看器打开这个 BMP
    也能直接看到字形（黑底白字）。C++ 侧只取每像素第 4 字节当 alpha。

用法：
  python gen_cjk_atlas.py                 # 生成图集
  python gen_cjk_atlas.py --preview 中文   # 顺便把指定字符的字形打成字符画核对
"""

import ctypes
import os
import re
import struct
import sys

from ctypes import wintypes

# ── 图集几何（与 Font.cpp 的 initCjkFont 保持一致） ────────────────────────
GLYPH = 16          # 每个字形在格子里的分辨率
COLS = 128
ROWS = 64
ATLAS_W = COLS * GLYPH   # 2048
ATLAS_H = ROWS * GLYPH   # 1024
CELLS = COLS * ROWS      # 8192

# GDI 参数（与 Font.cpp 的 initCjkFont 一致）
FONT_HEIGHT = -GLYPH          # CreateFontW 的 nHeight（负 = 字符高度）
FONT_WEIGHT = 400             # FW_NORMAL
FONT_CHARSET = 1              # DEFAULT_CHARSET
FONT_QUALITY = 3              # NONANTIALIASED_QUALITY（保持像素字的硬边）
DT_CENTER, DT_VCENTER, DT_SINGLELINE, DT_NOCLIP = 0x0001, 0x0004, 0x0020, 0x0100

HERE = os.path.dirname(os.path.abspath(__file__))
CJK_H = os.path.join(HERE, "handheld", "src", "client", "gui", "cjk_gb2312.h")
FONT_CPP = os.path.join(HERE, "handheld", "src", "client", "gui", "Font.cpp")
CUSTOM_TTF = os.path.join(HERE, "handheld", "data", "fonts", "custom.ttf")
OUT_BMP = os.path.join(HERE, "handheld", "data", "fonts", "cjk_atlas.bmp")


def parse_hanzi():
    """从 cjk_gb2312.h 取出 g_cjkChars（保持文件里的顺序）。"""
    src = open(CJK_H, encoding="utf-8").read()
    body = src[src.index("g_cjkChars[]"):]
    body = body[:body.index("};")]
    return [int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{4})", body)]


def parse_extra():
    """从 Font.cpp 的 kExtraChars 取出补充标点（保持顺序），保证两边不漂移。

    Font.cpp 里有两份同名表（Android 分支和 _WIN32 分支各一份，互不共享），
    它们必须一模一样 —— 图集是按这个顺序排格子编号的。这里全部读出来比对。
    """
    src = open(FONT_CPP, encoding="utf-8").read()
    tables = []
    pos = 0
    while True:
        i = src.find("kExtraChars[]", pos)
        if i < 0:
            break
        body = src[i:]
        body = body[:body.index("};")]
        tables.append([int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{4})", body)])
        pos = i + 1
    if not tables:
        print("警告：Font.cpp 里找不到 kExtraChars 表，只生成汉字")
        return []
    for t in tables[1:]:
        if t != tables[0]:
            raise SystemExit("Font.cpp 里有多份 kExtraChars 且内容不一致 —— 先对齐它们再生成图集")
    print("kExtraChars：Font.cpp 里有 %d 份，内容一致" % len(tables))
    return tables[0]


def ttf_family_name(path):
    """读 TTF/OTF 的 name 表，返回 Windows Unicode family name（nameID 1）。"""
    data = open(path, "rb").read()
    num_tables = struct.unpack_from(">H", data, 4)[0]
    name_off = 0
    for i in range(num_tables):
        rec = 12 + i * 16
        if data[rec:rec + 4] == b"name":
            name_off = struct.unpack_from(">I", data, rec + 8)[0]
    if not name_off:
        return ""
    count, str_off = struct.unpack_from(">HH", data, name_off + 2)
    for i in range(count):
        rec = name_off + 6 + i * 12
        # NameRecord: platformID(2) encodingID(2) languageID(2) nameID(2) length(2) offset(2)
        plat, enc, _lang, name_id = struct.unpack_from(">HHHH", data, rec)
        length, off = struct.unpack_from(">HH", data, rec + 8)
        if name_id == 1 and plat == 3 and enc in (1, 10):
            raw = data[name_off + str_off + off: name_off + str_off + off + length]
            return raw.decode("utf-16-be", "ignore")
    return ""


class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [
        ("biSize", wintypes.DWORD), ("biWidth", wintypes.LONG), ("biHeight", wintypes.LONG),
        ("biPlanes", wintypes.WORD), ("biBitCount", wintypes.WORD), ("biCompression", wintypes.DWORD),
        ("biSizeImage", wintypes.DWORD), ("biXPelsPerMeter", wintypes.LONG),
        ("biYPelsPerMeter", wintypes.LONG), ("biClrUsed", wintypes.DWORD),
        ("biClrImportant", wintypes.DWORD),
    ]


class BITMAPINFO(ctypes.Structure):
    _fields_ = [("bmiHeader", BITMAPINFOHEADER), ("bmiColors", wintypes.DWORD * 3)]


class RECT(ctypes.Structure):
    _fields_ = [("left", wintypes.LONG), ("top", wintypes.LONG),
                ("right", wintypes.LONG), ("bottom", wintypes.LONG)]


def render_atlas(codepoints, face):
    """用 GDI 把 codepoints 逐个渲染进 ATLAS_W x ATLAS_H 的 top-down 32bpp DIB。

    返回 bytes：每像素 4 字节，byte0=B byte1=G byte2=R byte3=未用。
    """
    gdi32 = ctypes.WinDLL("gdi32")
    user32 = ctypes.WinDLL("user32")

    # 显式声明句柄/指针类型：句柄是 64 位地址，不声明会被 ctypes 按 32 位 int 传，
    # 令牌大的时候直接 OverflowError。
    gdi32.CreateCompatibleDC.restype = ctypes.c_void_p
    gdi32.CreateCompatibleDC.argtypes = [ctypes.c_void_p]
    gdi32.CreateDIBSection.restype = ctypes.c_void_p
    gdi32.CreateDIBSection.argtypes = [ctypes.c_void_p, ctypes.POINTER(BITMAPINFO),
                                       wintypes.UINT, ctypes.POINTER(ctypes.c_void_p),
                                       ctypes.c_void_p, wintypes.UINT]
    gdi32.SelectObject.restype = ctypes.c_void_p
    gdi32.SelectObject.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    gdi32.DeleteObject.argtypes = [ctypes.c_void_p]
    gdi32.DeleteDC.argtypes = [ctypes.c_void_p]
    gdi32.AddFontResourceExW.argtypes = [ctypes.c_wchar_p, wintypes.UINT, ctypes.c_void_p]
    user32.DrawTextW.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_int,
                                 ctypes.POINTER(RECT), wintypes.UINT]

    # 进程内注册 ttf（FR_PRIVATE），与 Win 版 AddFontResourceExW(&pw[0], FR_PRIVATE, 0) 同款
    ttf_path = CUSTOM_TTF if os.path.exists(CUSTOM_TTF) else None
    if ttf_path:
        n = gdi32.AddFontResourceExW(ttf_path, 0x10, None)
        print("注册字体 %s -> %d 个 face" % (os.path.basename(ttf_path), n))

    hdc = gdi32.CreateCompatibleDC(None)
    bmi = BITMAPINFO()
    bmi.bmiHeader.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bmi.bmiHeader.biWidth = ATLAS_W
    bmi.bmiHeader.biHeight = -ATLAS_H        # 负值 = top-down，与 C++ 侧的读取约定一致
    bmi.bmiHeader.biPlanes = 1
    bmi.bmiHeader.biBitCount = 32
    bmi.bmiHeader.biCompression = 0          # BI_RGB
    bits = ctypes.c_void_p()
    hbmp = gdi32.CreateDIBSection(hdc, ctypes.byref(bmi), 0, ctypes.byref(bits), 0, 0)
    if not hbmp or not bits:
        raise SystemExit("CreateDIBSection 失败")
    old_bmp = gdi32.SelectObject(hdc, hbmp)

    gdi32.CreateFontW.restype = ctypes.c_void_p
    gdi32.CreateFontW.argtypes = ([ctypes.c_int] * 5 + [wintypes.DWORD] * 8
                                  + [wintypes.LPCWSTR])
    hfont = gdi32.CreateFontW(FONT_HEIGHT, 0, 0, 0, FONT_WEIGHT,
                              0, 0, 0,                       # italic / underline / strikeout
                              FONT_CHARSET, 0, 0,            # charset / outprec / clipprec
                              FONT_QUALITY, 0,               # quality / pitch|family
                              ctypes.c_wchar_p(face))
    if not hfont:
        raise SystemExit("CreateFontW 失败（face=%r）" % face)
    old_font = gdi32.SelectObject(hdc, hfont)
    gdi32.SetBkMode(hdc, 1)                  # TRANSPARENT
    gdi32.SetTextColor(hdc, 0x00FFFFFF)      # 白字，黑底由 DIB 初值给出

    flags = DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP
    for i, cp in enumerate(codepoints):
        if i >= CELLS:
            print("警告：字形数超过图集格数，多余的被丢掉")
            break
        col, row = i % COLS, i // COLS
        rc = RECT(col * GLYPH, row * GLYPH, col * GLYPH + GLYPH, row * GLYPH + GLYPH)
        # DrawTextW 画一个 wchar（UTF-16 基本平面够用；GB2312+标点全在 BMP 内）
        user32.DrawTextW(hdc, chr(cp), 1, ctypes.byref(rc), flags)

    raw = ctypes.string_at(bits, ATLAS_W * ATLAS_H * 4)
    gdi32.SelectObject(hdc, old_font)
    gdi32.SelectObject(hdc, old_bmp)
    gdi32.DeleteObject(hfont)
    gdi32.DeleteObject(hbmp)
    gdi32.DeleteDC(hdc)
    return raw


def to_bgra_gray(raw):
    """DIB 的 BGR -> 灰度，并写成 BGRA（R=G=B=A=lum）。

    C++ 侧只取每像素第 4 字节当 alpha；把 RGB 也填成灰度只是为了让这个 BMP
    用图片查看器打开时能直接看到字形。
    """
    gray = bytearray(ATLAS_W * ATLAS_H)
    for p in range(ATLAS_W * ATLAS_H):
        b, g, r = raw[p * 4], raw[p * 4 + 1], raw[p * 4 + 2]
        gray[p] = (r * 77 + g * 150 + b * 29) >> 8      # 与 Font.cpp 同一套亮度权重
    out = bytearray(ATLAS_W * ATLAS_H * 4)
    for p, v in enumerate(gray):
        out[p * 4] = v      # B
        out[p * 4 + 1] = v  # G
        out[p * 4 + 2] = v  # R
        out[p * 4 + 3] = v  # A  <- C++ 读这个
    return bytes(out), gray


def write_bmp(path, pixels):
    """写 32bpp BI_RGB 的 BMP（54 字节头，biHeight 为负 = top-down）。"""
    data_off = 54
    file_size = data_off + len(pixels)
    fh = struct.pack("<2sIHHI", b"BM", file_size, 0, 0, data_off)
    ih = struct.pack("<IiiHHIIiiII", 40, ATLAS_W, -ATLAS_H, 1, 32, 0,
                     len(pixels), 2835, 2835, 0, 0)
    with open(path, "wb") as f:
        f.write(fh + ih + pixels)


def preview(gray, codepoints, want):
    """把指定字符的 16x16 格子打成字符画，方便人工核对字形。"""
    for cp, i in codepoints.items():
        if chr(cp) not in want:
            continue
        col, row = i % COLS, i // COLS
        print("--- %s U+%04X cell=%d ---" % (chr(cp), cp, i))
        for y in range(GLYPH):
            line = ""
            for x in range(GLYPH):
                v = gray[(row * GLYPH + y) * ATLAS_W + col * GLYPH + x]
                line += "#" if v > 127 else ("." if v > 0 else " ")
            print("|" + line + "|")


def main():
    want_preview = []
    if "--preview" in sys.argv:
        idx = sys.argv.index("--preview")
        if idx + 1 < len(sys.argv):
            want_preview = list(sys.argv[idx + 1])

    hanzi = parse_hanzi()
    extra = parse_extra()
    print("汉字 %d 个，补充标点 %d 个，共 %d 格（图集 %d 格）"
          % (len(hanzi), len(extra), len(hanzi) + len(extra), CELLS))

    # 与 Font.cpp 的 initCjkFont 一致：优先 custom.ttf 的 family，否则系统字体
    if os.path.exists(CUSTOM_TTF):
        face = ttf_family_name(CUSTOM_TTF) or "Microsoft YaHei"
        print("字体 family = %r（来自 custom.ttf）" % face)
    else:
        face = "Microsoft YaHei"
        print("未找到 custom.ttf，回退系统字体 %r" % face)

    codepoints = hanzi + extra
    raw = render_atlas(codepoints, face)
    pixels, gray = to_bgra_gray(raw)

    nonempty = sum(1 for i in range(len(codepoints))
                   if any(gray[(i // COLS * GLYPH + y) * ATLAS_W
                               + (i % COLS) * GLYPH + x] > 0
                          for y in range(GLYPH) for x in range(GLYPH)))
    print("渲染完成：%d / %d 个格子有字形" % (nonempty, len(codepoints)))

    write_bmp(OUT_BMP, pixels)
    print("已写出 %s（%d 字节）" % (OUT_BMP, os.path.getsize(OUT_BMP)))

    if want_preview:
        preview(gray, {cp: i for i, cp in enumerate(codepoints)}, want_preview)


if __name__ == "__main__":
    main()
