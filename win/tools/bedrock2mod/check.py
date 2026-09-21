# -*- coding: utf-8 -*-
"""自检 + 预览：
  1) 矩阵<->欧拉角 round-trip
  2) 渲染等价性：把转换出来的 (box,pivot,rot,move) 按项目的 modelVert 公式重算顶点，
     必须等于「直接解算骨骼」得到的顶点 —— 这是转换正确性的硬证据。
  3) 打印跨度与 JS 预览。

用法: python check.py [模型名]
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
    "sten": "WocAddonRe(1)/models/entity/gun/sten.geo.json",
    "mp40": "WocAddonRe(1)/models/entity/gun/mp40.geo (10).json",
    "mg08": "WocAddonRe(1)/models/entity/gun/mg08.geo.json",
    "c96": "WocAddonRe(1)/models/entity/gun/c96.geo.json",
}


def roundtrip_test():
    import random
    random.seed(7)
    worst = 0.0
    for _ in range(2000):
        rx, ry, rz = [random.uniform(-180, 180) for _ in range(3)]
        R = G.euler_matrix(rx, ry, rz, "XYZ")
        a, b, c = G.decompose("XYZ", R)
        R2 = G.euler_matrix(a, b, c, "XYZ")
        worst = max(worst, max(abs(R[i][j] - R2[i][j]) for i in range(3) for j in range(3)))
    print("[1] 欧拉角 round-trip 最大误差 = %.3e  %s" % (worst, "OK" if worst < 1e-9 else "FAIL"))


def direct_verts(d, order, om):
    """基岩语义下每个 cube 的 8 个顶点（模型空间）。"""
    g = G.Geometry(d, order, om)
    verts = []
    for _, cube, M in g.cubes():
        o, s = cube["origin"], cube["size"]
        for dx in (0, s[0]):
            for dy in (0, s[1]):
                for dz in (0, s[2]):
                    verts.append(M.apply([o[0] + dx, o[1] + dy, o[2] + dz]))
    return verts


def model_verts(items):
    """按项目的 modelVert 公式重算顶点。"""
    verts = []
    for it in items:
        R = G.euler_matrix(it["rot"][0], it["rot"][1], it["rot"][2], "XYZ")
        c, b, mo = it["pivot"], it["box"], it["move"]
        for dx in (0, b[3]):
            for dy in (0, b[4]):
                for dz in (0, b[5]):
                    l = [b[0] + dx, b[1] + dy, b[2] + dz]
                    rel = [l[i] - c[i] for i in range(3)]
                    w = G.mv(R, rel)
                    verts.append([c[i] + w[i] + mo[i] for i in range(3)])
    return verts


def verify(d, order, om):
    """逐 cube 对比：转换后按 modelVert 公式重算的 8 顶点 vs 直接解算的 8 顶点。"""
    g = G.Geometry(d, order, om)
    cubes = g.cubes()
    items, _, _ = G.convert(d, order=order, origin_mode=om, center=False, scale=1.0)
    assert len(cubes) == len(items)
    worst, worst_i, worst_bone = 0.0, -1, ""
    for i, ((bone_name, cube, M), it) in enumerate(zip(cubes, items)):
        o, s = cube["origin"], cube["size"]
        R = G.euler_matrix(it["rot"][0], it["rot"][1], it["rot"][2], "XYZ")
        c, b, mo = it["pivot"], it["box"], it["move"]
        for dx in (0, s[0]):
            for dy in (0, s[1]):
                for dz in (0, s[2]):
                    l = [o[0] + dx, o[1] + dy, o[2] + dz]
                    p = M.apply(l)
                    rel = [l[i] - c[i] for i in range(3)]
                    w = G.mv(R, rel)
                    q = [c[i] + w[i] + mo[i] for i in range(3)]
                    dd = max(abs(p[k] - q[k]) for k in range(3))
                    if dd > worst:
                        worst, worst_i, worst_bone = dd, i, bone_name
    return worst, worst_i, worst_bone, len(cubes)


def main():
    name = sys.argv[1] if len(sys.argv) > 1 else "luger"
    roundtrip_test()
    z = zipfile.ZipFile(MCADDON)
    d = json.loads(z.read(GUNS[name]))
    desc = d["minecraft:geometry"][0]["description"]
    print("\n[%s] tex=%sx%s id=%s" % (name, desc.get("texture_width"),
                                      desc.get("texture_height"), desc.get("identifier")))

    for om in ("abs", "rel"):
        worst, wi, wb, n = verify(d, "XYZ", om)
        flag = "OK" if worst < 1e-6 else "FAIL@cube%d(%s)" % (wi, wb)
        items, _, _ = G.convert(d, order="XYZ", origin_mode=om, center=True)
        pts = []
        for it in items:
            b = it["box"]
            for dx in (0, b[3]):
                for dy in (0, b[4]):
                    for dz in (0, b[5]):
                        pts.append([b[0] + dx, b[1] + dy, b[2] + dz])
        mn, mx = G.bbox_of(pts)
        span = [mx[i] - mn[i] for i in range(3)]
        print("[2] %-3s cubes=%3d 渲染等价误差=%9.2e %-22s | 居中跨度 x=%6.2f y=%6.2f z=%6.2f"
              % (om, n, worst, flag, span[0], span[1], span[2]))

    items, bones, _ = G.convert(d, order="XYZ", origin_mode="abs", center=True)
    print("骨架: %d 根 -> %s" % (len(bones), ", ".join(b["name"] for b in bones[:8])))
    print("\n--- JS 预览（前 3 个部件） ---")
    print(G.to_js_model(items[:3]))
    print("... 共 %d 个部件" % len(items))


if __name__ == "__main__":
    main()
