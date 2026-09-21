# -*- coding: utf-8 -*-
"""标定 .geo.json 坐标系语义。

要回答两件事（都用 description.visible_bounds_* 当基准反推，不靠猜）：
  1) cube.origin 是模型空间绝对坐标，还是相对 bone.pivot 的局部坐标？
  2) bone.rotation / cube.rotation 的欧拉应用顺序是 XYZ 还是 ZYX ...？

基岩语义（假定）：每个 bone 有模型空间绝对 pivot + 静态 rotation；cube 的 origin
也是模型空间绝对坐标。渲染时 cube 先绕自己的 pivot 转，再从所在 bone 一路向根，
每层绕该层 bone 的 pivot 转：
    f_bone(v) = pivot + R(rot) * (v - pivot)
    v_final   = f_root( ... f_self( v_cube ) ... )

用法: python geo_probe.py
"""
import json
import math
import zipfile

MCADDON = "H:/downloded/[苦力怕论坛]二战枪械.mcaddon"


# ------------------------------------------------------------ 3x3 旋转
def mat_ident():
    return [[1.0, 0, 0], [0, 1.0, 0], [0, 0, 1.0]]


def mat_mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]


def mat_vec(m, v):
    return [sum(m[i][k] * v[k] for k in range(3)) for i in range(3)]


def _rx(d):
    c, s = math.cos(math.radians(d)), math.sin(math.radians(d))
    return [[1, 0, 0], [0, c, -s], [0, s, c]]


def _ry(d):
    c, s = math.cos(math.radians(d)), math.sin(math.radians(d))
    return [[c, 0, s], [0, 1, 0], [-s, 0, c]]


def _rz(d):
    c, s = math.cos(math.radians(d)), math.sin(math.radians(d))
    return [[c, -s, 0], [s, c, 0], [0, 0, 1]]


_FN = {"X": _rx, "Y": _ry, "Z": _rz}


def euler(rx, ry, rz, order):
    """order='XYZ' -> 依次绕 X、Y、Z 作用（每一步都在原坐标系里，等价 R = Rz*Ry*Rx）。"""
    m = mat_ident()
    for ax in order:
        m = mat_mul(_FN[ax]({"X": rx, "Y": ry, "Z": rz}[ax]), m)
    return m


# ------------------------------------------------------------ 解算
class Tr:
    """仿射变换 v -> R*v + t"""
    __slots__ = ("R", "t")

    def __init__(self, R=None, t=None):
        self.R = R or mat_ident()
        self.t = t or [0.0, 0.0, 0.0]

    def apply(self, v):
        x = mat_vec(self.R, v)
        return [x[i] + self.t[i] for i in range(3)]

    def then(self, outer):
        """先做 self，再做 outer。"""
        R2 = mat_mul(outer.R, self.R)
        t2 = [outer.apply(self.t)[i] for i in range(3)]
        return Tr(R2, t2)


def bone_local(bone, order):
    """bone 自身那一层的变换 f(v) = pivot + R*(v - pivot)。"""
    pivot = [float(x) for x in bone.get("pivot", [0, 0, 0])]
    rot = bone.get("rotation")
    if not rot:
        return Tr(), pivot
    R = euler(rot[0], rot[1], rot[2], order)
    t = [pivot[i] - mat_vec(R, pivot)[i] for i in range(3)]
    return Tr(R, t), pivot


def resolve(g, origin_mode, order):
    byname = {b["name"]: b for b in g["bones"]}
    chain_cache = {}

    def chain(bone):
        """从 bone 自己到根，依次应用 -> 合成一个 Tr。"""
        if bone["name"] in chain_cache:
            return chain_cache[bone["name"]]
        me, _ = bone_local(bone, order)
        p = bone.get("parent")
        tr = me if not p else me.then(chain(byname[p]))
        chain_cache[bone["name"]] = tr
        return tr

    out = []
    for bone in g["bones"]:
        if not bone.get("cubes"):
            continue
        tr = chain(bone)
        bpivot = [float(x) for x in bone.get("pivot", [0, 0, 0])]
        for c in bone.get("cubes", []):
            origin = [float(x) for x in c["origin"]]
            size = [float(x) for x in c["size"]]
            base = list(origin) if origin_mode == "abs" else [bpivot[i] + origin[i] for i in range(3)]
            cp = c.get("pivot")
            cp = [float(x) for x in cp] if cp else [base[i] + size[i] * 0.5 for i in range(3)]
            rot = c.get("rotation")
            cr = euler(rot[0], rot[1], rot[2], order) if rot else mat_ident()
            pts = []
            for dx in (0.0, size[0]):
                for dy in (0.0, size[1]):
                    for dz in (0.0, size[2]):
                        v = [base[0] + dx, base[1] + dy, base[2] + dz]
                        rel = [v[i] - cp[i] for i in range(3)]
                        rv = mat_vec(cr, rel)
                        v = [cp[i] + rv[i] for i in range(3)]
                        pts.append(tr.apply(v))
            mn = [min(p[i] for p in pts) for i in range(3)]
            mx = [max(p[i] for p in pts) for i in range(3)]
            out.append((mn, mx))
    return out


def bbox(res):
    return ([min(r[0][i] for r in res) for i in range(3)],
            [max(r[1][i] for r in res) for i in range(3)])


def probe(name, raw):
    d = json.loads(raw)
    g = d["minecraft:geometry"][0]
    desc = g["description"]
    vw, vh = desc.get("visible_bounds_width") or 0, desc.get("visible_bounds_height") or 0
    voff = desc.get("visible_bounds_offset", [0, 0, 0])
    tgt = (vw * 16, vh * 16)
    print("=" * 78)
    print("%s  visible=%sx%s -> %sx%s px   off=%s" % (name, vw, vh, tgt[0], tgt[1], voff))
    rows = []
    for om in ("abs", "rel"):
        for order in ("XYZ", "ZYX"):
            mn, mx = bbox(resolve(g, om, order))
            span = [mx[i] - mn[i] for i in range(3)]
            ycen = (mn[1] + mx[1]) / 2 - voff[1] * 16
            # visible_bounds_width 取 x/z 里大的那个
            err = abs(max(span[0], span[2]) - tgt[0]) + abs(span[1] - tgt[1]) + abs(ycen)
            rows.append((err, om, order, span, ycen))
    for err, om, order, span, ycen in sorted(rows):
        print("   %-3s %-3s  span x=%7.2f y=%7.2f z=%7.2f  y中心偏差=%7.2f   err=%7.2f"
              % (om, order, span[0], span[1], span[2], ycen, err))


def main():
    z = zipfile.ZipFile(MCADDON)
    for n in sorted(z.namelist()):
        if "models/entity/gun/" in n and n.endswith(".json"):
            probe(n.split("/")[-1], z.read(n))


if __name__ == "__main__":
    main()
