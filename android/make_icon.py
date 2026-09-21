#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""从 Windows 版的 app.ico 生成 Android 各密度启动图标。

用法: python make_icon.py <app.ico> <res 目录>
"""
import os
import sys

from PIL import Image

DENSITIES = [
    ('mdpi', 48),
    ('hdpi', 72),
    ('xhdpi', 96),
    ('xxhdpi', 144),
    ('xxxhdpi', 192),
]


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    ico_path, res_dir = sys.argv[1], sys.argv[2]

    im = Image.open(ico_path)
    # 取 ico 里最大的那一层
    sizes = im.info.get('sizes') or [im.size]
    best = max(sizes, key=lambda s: s[0] * s[1])
    im.size = best
    im = im.convert('RGBA')

    for name, px in DENSITIES:
        out_dir = os.path.join(res_dir, 'mipmap-' + name)
        os.makedirs(out_dir, exist_ok=True)
        out = os.path.join(out_dir, 'ic_launcher.png')
        im.resize((px, px), Image.LANCZOS).save(out)
        print('wrote %s (%dx%d)' % (out, px, px))
    return 0


if __name__ == '__main__':
    sys.exit(main())
