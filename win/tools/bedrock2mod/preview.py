# -*- coding: utf-8 -*-
"""把转换后的模型投影成 ASCII 轮廓 —— 判断"像不像枪"不用跑游戏、不用看图。

手枪侧视应该是 L 形（长枪管 + 垂直握把）；如果渲染出来是一坨方块，说明坐标系解错了。

用法: python preview.py [枪名]
"""
import json
import os
import sys
import zipfile

sys.stdout.reconfigure(encoding="utf-8")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import geo2js as G

MCADDON = "H:/downloded/[苦力怕论坛]二战枪械.mcaddon"
GUNS = {
    "luger": "WocAddonRe(1)/models/entity/gun/luger.geo.json",
    "mp40": "WocAddonRe(1)/models/entity/gun/mp40.geo (10).json",
    "sten": "WocAddonRe(1)/models/entity/gun/sten.geo.json",
}

W, H = 64, 30


def transformed_pts(d, order, origin_mode):
    """按指定语义解算，返回每个 cube 的 8 个顶点（模型空间，未居中）。"""
    g = G.Geometry(d, order, origin_mode)
    cubes = []
    for _, cube, M in g.cubes():
        o, s = cube["origin"], cube["size"]
        pts = []
        for dx in (0, s[0]):
            for dy in (0, s[1]):
                for dz in (0, s[2]):
                    pts.append(M.apply([o[0] + dx, o[1] + dy, o[2] + dz]))
        cubes.append(pts)
    return cubes


def draw(cubes, ha, va, flip_v=False, title=""):
    pts = [p for c in cubes for p in c]
    hs = [p[ha] for p in pts]
    vs = [p[va] for p in pts]
    h0, h1 = min(hs), max(hs)
    v0, v1 = min(vs), max(vs)
    if h1 - h0 < 1e-6:
        h1 = h0 + 1
    if v1 - v0 < 1e-6:
        v1 = v0 + 1
    grid = [[" "] * W for _ in range(H)]
    for c in cubes:
        us = [int((p[ha] - h0) / (h1 - h0) * (W - 1)) for p in c]
        ws = [int((p[va] - v0) / (v1 - v0) * (H - 1)) for p in c]
        for u in range(min(us), max(us) + 1):
            for w in range(min(ws), max(ws) + 1):
                v = (H - 1 - w) if not flip_v else w
                if 0 <= u < W and 0 <= v < H:
                    grid[v][u] = "#"
    print(title)
    print("   " + "-" * W)
    for r in grid:
        print("   |" + "".join(r) + "|")
    print("   " + "-" * W)


def main():
    name = sys.argv[1] if len(sys.argv) > 1 else "luger"
    z = zipfile.ZipFile(MCADDON)
    d = json.loads(z.read(GUNS[name]))

    for origin_mode in ("abs", "rel"):
        for order in ("XYZ", "ZYX"):
            cubes = transformed_pts(d, order, origin_mode)
            # 侧视图 = 把 Z(前后) 当水平、Y(上下) 当垂直，看它是不是 L 形
            draw(cubes, 2, 1, False,
                 "=== origin_mode=%s  order=%s  侧视(Z-Y) ===" % (origin_mode, order))
            print()


if __name__ == "__main__":
    main()
