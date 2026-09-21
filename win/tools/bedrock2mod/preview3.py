# -*- coding: utf-8 -*-
"""渲染「应用 hold 动画之后」的真实形状 —— 这才是游戏里实际画出来的东西。

之前 preview.py 画的是 Geometry.cubes() 的直接解算（不含动画），和游戏里看到的
不是一回事。这里把引擎的 applyItemAnim 完整模拟一遍（含 hold 的 rot/pos/scale），
并且对比几种坐标系假设，看哪种才像一把枪。

用法: python preview3.py [枪名]
"""
import json
import os
import sys
import zipfile

sys.stdout.reconfigure(encoding="utf-8")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import geo2js as G
import anim2js as A

MCADDON = "H:/downloded/[苦力怕论坛]二战枪械.mcaddon"
GUNS = {
    "luger": "WocAddonRe(1)/models/entity/gun/luger.geo.json",
    "mp40": "WocAddonRe(1)/models/entity/gun/mp40.geo (10).json",
}
INV = 1.0 / 16.0
W, H = 70, 30


def topo(bones):
    idx = {b["name"]: i for i, b in enumerate(bones)}
    order, placed = [], [False] * len(bones)
    prog = True
    while prog and len(order) < len(bones):
        prog = False
        for i, b in enumerate(bones):
            if placed[i]:
                continue
            p = b["parent"]
            ok = True if not p else (p not in idx or placed[idx[p]])
            if ok:
                order.append(i)
                placed[i] = True
                prog = True
    order += [i for i in range(len(bones)) if not placed[i]]
    return order, idx


def engine_cubes(items, bones, poses=None):
    """模拟 applyItemAnim（poses 给了就用它替换静态 rot/scale）+ modelVert。
    poses: {bone: {'r':(rad,rad,rad), 'p':(格内,格内,格内), 's':标量}}"""
    poses = poses or {}
    order, idx = topo(bones)
    world = {}
    for bi in order:
        b = bones[bi]
        piv = [x * INV for x in b["pivot"]]
        rotd = b.get("rot", [0, 0, 0])
        R = G.euler_matrix(rotd[0], rotd[1], rotd[2], "XYZ")
        sc = 1.0
        pos = [0.0, 0.0, 0.0]
        pp = poses.get(b["name"])
        if pp:
            if pp.get("r"):
                rr = pp["r"]
                R = G.euler_matrix(rr[0], rr[1], rr[2], "XYZ")   # 替换静态
            if pp.get("s") is not None:
                sc = pp["s"]
            if pp.get("p"):
                pos = list(pp["p"])
        rp = [sc * v for v in G.mv(R, piv)]
        lR, ls, lt = R, sc, [piv[i] - rp[i] + pos[i] for i in range(3)]
        p = b["parent"]
        if p and p in world:
            pR, ps, pt = world[p]
            lR = G.mul(pR, lR)
            ls = ps * ls
            lt = [ps * sum(pR[i][k] * lt[k] for k in range(3)) + pt[i] for i in range(3)]
        world[b["name"]] = (lR, ls, lt)

    out = []
    for it in items:
        rd = it.get("rot", [0, 0, 0])
        R = G.euler_matrix(rd[0], rd[1], rd[2], "XYZ")
        c = [x * INV for x in it["pivot"]]
        b = [x * INV for x in it["box"]]
        mo = [x * INV for x in it["move"]]
        M = world.get(it.get("bone", ""))
        sc = 1.0
        if M:
            mR, ms, mt = M
            R = G.mul(mR, R)
            sc = ms
            cm = [c[i] + mo[i] for i in range(3)]
            nm = [ms * sum(mR[i][k] * cm[k] for k in range(3)) + mt[i] for i in range(3)]
            mo = [nm[i] - c[i] for i in range(3)]
        pts = []
        for dx in (0, b[3]):
            for dy in (0, b[4]):
                for dz in (0, b[5]):
                    ll = [b[0] + dx, b[1] + dy, b[2] + dz]
                    rel = [ll[i] - c[i] for i in range(3)]
                    w = [sc * v for v in G.mv(R, rel)]
                    pts.append([c[i] + w[i] + mo[i] for i in range(3)])
        out.append(pts)
    return out


def draw(cubes, ha, va, flipv, title):
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
                v = w if flipv else (H - 1 - w)
                if 0 <= u < W and 0 <= v < H:
                    grid[v][u] = "#"
    print(title)
    print("   +" + "-" * W + "+")
    for r in grid:
        print("   |" + "".join(r) + "|")
    print("   +" + "-" * W + "+")


def hold_poses(anims, scale):
    """把 hold 动画的常量姿势换算成引擎口径（弧度 / 格内 / 标量）。"""
    out = {}
    h = anims.get("hold")
    if not h:
        return out
    for bn, ch in h["bones"].items():
        e = {}
        if "rotation" in ch:
            v = ch["rotation"][0][1]
            if isinstance(v, list):
                e["r"] = [v[0] * 3.14159265358979 / 180.0, v[1] * 3.14159265358979 / 180.0,
                          v[2] * 3.14159265358979 / 180.0]
        if "position" in ch:
            v = ch["position"][0][1]
            if isinstance(v, list):
                e["p"] = [v[0] * scale / 16.0, v[1] * scale / 16.0, v[2] * scale / 16.0]
        if "scale" in ch:
            v = ch["scale"][0][1]
            e["s"] = float(v[0]) if isinstance(v, list) else float(v)
        if e:
            out[bn] = e
    return out


def main():
    name = sys.argv[1] if len(sys.argv) > 1 else "luger"
    z = zipfile.ZipFile(MCADDON)
    d = json.loads(z.read(GUNS[name]))
    items, bones, _ = G.convert(d, order="XYZ", origin_mode="abs", center=True)
    # 用 1.0 的缩放先看形状（缩放只影响大小，不影响像不像）
    anims = A.collect(z, [name])
    poses = hold_poses(anims, 1.0)
    print("hold 动画作用了 %d 根骨骼: %s" % (len(poses), ", ".join(sorted(poses))))

    noanim = engine_cubes(items, bones, None)
    withhold = engine_cubes(items, bones, poses)

    for tag, cubes in (("** 不带动画（我上轮画的）", noanim),
                       ("** 带 hold 动画（游戏里实际看到的）", withhold)):
        pts = [p for c in cubes for p in c]
        mn = [min(p[i] for p in pts) for i in range(3)]
        mx = [max(p[i] for p in pts) for i in range(3)]
        print("\n%s  跨度 y=%.2f z=%.2f 格" % (tag, mx[1] - mn[1], mx[2] - mn[2]))
        draw(cubes, 2, 1, False, "   侧视 (z 横 / y 纵)")


if __name__ == "__main__":
    main()
