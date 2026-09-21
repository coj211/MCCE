# -*- coding: utf-8 -*-
"""端到端验证：转换结果 + 引擎的解算逻辑  ==  直接解算骨骼？

新的做法是"转换器不烘焙骨骼旋转，交给引擎每帧算"，所以这里要把引擎的
applyItemAnim（静态姿态，无动画）+ modelVert 逐字模拟一遍，再和基岩语义的
直接解算对比。误差应该是 0。

用法: python verify2.py [枪名]
"""
import json
import sys
import zipfile

sys.stdout.reconfigure(encoding="utf-8")
sys.path.insert(0, ".")
import geo2js as G

MCADDON = "H:/downloded/[苦力怕论坛]二战枪械.mcaddon"
GUNS = {
    "luger": "WocAddonRe(1)/models/entity/gun/luger.geo.json",
    "mp40": "WocAddonRe(1)/models/entity/gun/mp40.geo (10).json",
    "sten": "WocAddonRe(1)/models/entity/gun/sten.geo.json",
    "c96": "WocAddonRe(1)/models/entity/gun/c96.geo.json",
}
INV = 1.0 / 16.0


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


def engine_cubes(items, bones):
    """逐字模拟 ModEngine::applyItemAnim（无动画 -> 用静态 rot） + TileRenderer::modelVert。"""
    order, idx = topo(bones)
    world = {}
    for bi in order:
        b = bones[bi]
        piv = [x * INV for x in b["pivot"]]
        rot = b.get("rot", [0, 0, 0])
        R = G.euler_matrix(rot[0], rot[1], rot[2], "XYZ")
        rp = G.mv(R, piv)
        lR, lt = R, [piv[i] - rp[i] for i in range(3)]
        p = b["parent"]
        if p and p in world:
            pR, pt = world[p]
            lR = G.mul(pR, lR)
            lt = [sum(pR[i][k] * lt[k] for k in range(3)) + pt[i] for i in range(3)]
        world[b["name"]] = (lR, lt)

    out = []
    for it in items:
        rot = it.get("rot", [0, 0, 0])
        R = G.euler_matrix(rot[0], rot[1], rot[2], "XYZ")
        c = [x * INV for x in it["pivot"]]
        b = [x * INV for x in it["box"]]
        mo = [x * INV for x in it["move"]]
        M = world.get(it.get("bone", ""))
        if M:
            mR, mt = M
            R = G.mul(mR, R)
            cm = [c[i] + mo[i] for i in range(3)]
            nm = [sum(mR[i][k] * cm[k] for k in range(3)) + mt[i] for i in range(3)]
            mo = [nm[i] - c[i] for i in range(3)]
        pts = []
        for dx in (0, b[3]):
            for dy in (0, b[4]):
                for dz in (0, b[5]):
                    ll = [b[0] + dx, b[1] + dy, b[2] + dz]
                    rel = [ll[i] - c[i] for i in range(3)]
                    w = G.mv(R, rel)
                    pts.append([c[i] + w[i] + mo[i] for i in range(3)])
        out.append(pts)
    return out


def reference_cubes(d, order, origin_mode):
    """基准：基岩语义直接解算（Geometry.cubes 的 M），并换算到格内。"""
    g = G.Geometry(d, order, origin_mode)
    out = []
    for _, cube, M in g.cubes():
        o, s = cube["origin"], cube["size"]
        pts = []
        for dx in (0, s[0]):
            for dy in (0, s[1]):
                for dz in (0, s[2]):
                    w = M.apply([o[0] + dx, o[1] + dy, o[2] + dz])
                    pts.append([w[i] * INV for i in range(3)])
        out.append(pts)
    return out


def main():
    name = sys.argv[1] if len(sys.argv) > 1 else "luger"
    z = zipfile.ZipFile(MCADDON)
    d = json.loads(z.read(GUNS[name]))

    items, bones, _ = G.convert(d, order="XYZ", origin_mode="abs", center=True)
    mine = engine_cubes(items, bones)
    ref = reference_cubes(d, "XYZ", "abs")

    # 居中会平移整体，比形状前先把两边都归到同一中心
    def center(cubes):
        pts = [p for c in cubes for p in c]
        mn = [min(p[i] for p in pts) for i in range(3)]
        mx = [max(p[i] for p in pts) for i in range(3)]
        cen = [(mn[i] + mx[i]) * 0.5 for i in range(3)]
        return [[[p[i] - cen[i] for i in range(3)] for p in c] for c in cubes]

    mine, ref = center(mine), center(ref)

    worst, wi = 0.0, -1
    for i, (a, b) in enumerate(zip(mine, ref)):
        for p, q in zip(a, b):
            dd = max(abs(p[k] - q[k]) for k in range(3))
            if dd > worst:
                worst, wi = dd, i
    print("[%s] cube 数 mine=%d ref=%d" % (name, len(mine), len(ref)))
    print("     最大顶点误差 = %.3e  (cube #%d)  %s"
          % (worst, wi, "OK" if worst < 1e-6 else "FAIL"))
    if worst >= 1e-6:
        print("     mine[%d][0] = %s" % (wi, mine[wi][0]))
        print("     ref [%d][0] = %s" % (wi, ref[wi][0]))


if __name__ == "__main__":
    main()
