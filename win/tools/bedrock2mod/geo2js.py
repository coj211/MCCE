# -*- coding: utf-8 -*-
"""基岩 .geo.json 几何 -> 本项目 ModBlockPart 模型数据 的转换核心库。

两侧的坐标语义（都已用代码/数据核对过）：
  基岩:  cube.origin / cube.size / bone.pivot / cube.pivot 都是【模型空间像素】
         (16 像素 = 1 格)，y 向上，原点在模型原点。cube 先绕自己的 pivot 转，
         再从所属 bone 一路向根，每层绕该层 bone 的 pivot 转。
  本项目: ModBlockPart 的 box/pivot/move 也是【像素】，16 = 一格 —— 单位天然一致。
         画法 (TileRenderer::modelVert):
             wx = R(rx,ry,rz) * (l - c) + c + o
         其中 R 的合成顺序 = Rz*Ry*Rx（绕 x 先、y 次、z 后）。

所以只要把每个 cube 的累积变换 M = (R_M, t_M) 求出来，就能解析地写成
  R = R_M,  o = t_M + R_M*c - c   （c 任取，这里取盒子中心）
反过来用 Rz*Ry*Rx 分解 R_M 得到 rx/ry/rz。

uv: 基岩 { "north": {"uv":[u,v], "uv_size":[w,h]} , ... }
 -> 本项目 { "north": [u,v,w,h], ... }（面的名字两边完全相同；负的 uv_size 表示
    镜像，本项目 partUv 里 u1=(u+w)/texW 天然支持负数）。
"""
import json
import math

# ------------------------------------------------------------------ 3x3 矩阵
IDENT = [[1.0, 0, 0], [0, 1.0, 0], [0, 0, 1.0]]


def mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]


def mv(m, v):
    return [sum(m[i][k] * v[k] for k in range(3)) for i in range(3)]


def _axis(ax, deg):
    c, s = math.cos(math.radians(deg)), math.sin(math.radians(deg))
    if ax == "X":
        return [[1, 0, 0], [0, c, -s], [0, s, c]]
    if ax == "Y":
        return [[c, 0, s], [0, 1, 0], [-s, 0, c]]
    return [[c, -s, 0], [s, c, 0], [0, 0, 1]]


def euler_matrix(rx, ry, rz, order):
    """order 里的字母按先后依次作用（左乘），返回 R 使 v' = R*v。
    本项目渲染用的顺序是 'XYZ'（=> R = Rz*Ry*Rx）。"""
    m = IDENT
    vals = {"X": rx, "Y": ry, "Z": rz}
    for ax in order:
        m = mul(_axis(ax, vals[ax]), m)
    return m


def decompose(rx_order, R):
    """把 R 分解成 (rx, ry, rz)，使 R == euler_matrix(rx,ry,rz,rx_order)。"""
    assert rx_order == "XYZ", "只实现了 R = Rz*Ry*Rx 的分解"
    sy = -R[2][0]
    sy = max(-1.0, min(1.0, sy))
    ry = math.degrees(math.asin(sy))
    cy = math.cos(math.radians(ry))
    if abs(cy) > 1e-6:
        rx = math.degrees(math.atan2(R[2][1], R[2][2]))
        rz = math.degrees(math.atan2(R[1][0], R[0][0]))
    else:                                   # 万向锁：rx 与 rz 退化成一自由度
        rx = math.degrees(math.atan2(-R[1][2], R[1][1]))
        rz = 0.0
    return rx, ry, rz


class Tr:
    """仿射 v -> R*v + t"""
    __slots__ = ("R", "t")

    def __init__(self, R=None, t=None):
        self.R = IDENT if R is None else R
        self.t = [0.0, 0.0, 0.0] if t is None else t

    def apply(self, v):
        w = mv(self.R, v)
        return [w[i] + self.t[i] for i in range(3)]

    def then(self, outer):
        """先 self、再 outer。"""
        return Tr(mul(outer.R, self.R), outer.apply(self.t))


class Geometry:
    """一棵 bone 树的解算器。"""

    def __init__(self, geo_dict, order="XYZ", origin_mode="abs"):
        self.g = geo_dict["minecraft:geometry"][0]
        self.desc = self.g["description"]
        self.order = order
        self.origin_mode = origin_mode
        self.by_name = {b["name"]: b for b in self.g["bones"]}
        self._chain = {}

    # ---- bone 自身那一层: f(v) = pivot + R*(v - pivot)
    def _local(self, bone):
        pivot = [float(x) for x in bone.get("pivot", [0, 0, 0])]
        rot = bone.get("rotation")
        if not rot:
            return Tr(), pivot
        R = euler_matrix(rot[0], rot[1], rot[2], self.order)
        t = [pivot[i] - mv(R, pivot)[i] for i in range(3)]
        return Tr(R, t), pivot

    def _bone_chain(self, bone):
        """bone 自己的旋转 + 所有祖先的旋转，合成为一个 Tr。"""
        key = bone["name"]
        if key in self._chain:
            return self._chain[key]
        me, _ = self._local(bone)
        p = bone.get("parent")
        tr = me if not p else me.then(self._bone_chain(self.by_name[p]))
        self._chain[key] = tr
        return tr

    def cubes(self):
        """产出 (bone_name, cube_dict, M)，M 把"未旋转的盒子角"映射到模型空间。"""
        out = []
        for bone in self.g["bones"]:
            if not bone.get("cubes"):
                continue
            chain = self._bone_chain(bone)
            bpivot = [float(x) for x in bone.get("pivot", [0, 0, 0])]
            for c in bone["cubes"]:
                origin = [float(x) for x in c["origin"]]
                size = [float(x) for x in c["size"]]
                base = list(origin) if self.origin_mode == "abs" \
                    else [bpivot[i] + origin[i] for i in range(3)]
                cp = c.get("pivot")
                cp = [float(x) for x in cp] if cp \
                    else [base[i] + size[i] * 0.5 for i in range(3)]
                rot = c.get("rotation")
                if rot:
                    R = euler_matrix(rot[0], rot[1], rot[2], self.order)
                    t = [cp[i] - mv(R, cp)[i] for i in range(3)]
                    M = Tr(R, t).then(chain)
                else:
                    M = chain
                # 保留 cube 的完整信息（uv / rotation / pivot），只把 origin 换成
                # 处理过的 base —— 下游转换器要用 cube 自己的 rotation。
                cinfo = dict(c)
                cinfo["origin"] = base
                out.append((bone["name"], cinfo, M))
        return out


def bbox_of(pts):
    mn = [min(p[i] for p in pts) for i in range(3)]
    mx = [max(p[i] for p in pts) for i in range(3)]
    return mn, mx


def convert(geo_dict, order="XYZ", origin_mode="abs",
            scale=1.0, center=True, scale_from=None):
    """转成 (items, bones, desc)。

    **不烘焙骨骼的旋转** —— 骨骼姿态交给引擎每帧算（它要处理动画的“替换”
    语义、以及 scale）。cube 只保留自己的 rotation，坐标保持模型空间绝对值：
    引擎会从句骨骼链逐层绕各层 pivot 转，正好与基岩语义一致。

    items: [{'name','bone','box','uv','pivot','rot','move'}]，像素单位。
    bones: [{'name','parent','pivot','rot'}]，pivot 像素、rot 度。
    """
    g = Geometry(geo_dict, order, origin_mode)
    items = []
    for idx, (bone_name, cube, M) in enumerate(g.cubes()):
        o = cube["origin"]
        s = cube["size"]
        box = [o[0], o[1], o[2], s[0], s[1], s[2]]
        cp = cube.get("pivot")
        if not cp:
            cp = [o[i] + s[i] * 0.5 for i in range(3)]
        rot = cube.get("rotation") or [0.0, 0.0, 0.0]
        items.append({"name": "%s_%d" % (bone_name, idx), "bone": bone_name,
                      "box": box, "uv": cube.get("uv"),
                      "pivot": [float(x) for x in cp],
                      "rot": [float(x) for x in rot],
                      "move": [0.0, 0.0, 0.0]})

    # 骨架表：所有骨骼都保留（某根骨骼即便自己没有部件，它的变换也要带动子骨骼）。
    bones = []
    for b in g.g["bones"]:
        bones.append({
            "name": b["name"],
            "parent": b.get("parent", ""),
            "pivot": [float(x) for x in b.get("pivot", [0, 0, 0])],
            "rot": [float(x) for x in (b.get("rotation") or [0, 0, 0])],
        })

    # 整体居中 + 缩放（部件和骨骼一起动；角度不受缩放影响）
    if center:
        pts = []
        for it in items:
            b = it["box"]
            for dx in (0, b[3]):
                for dy in (0, b[4]):
                    for dz in (0, b[5]):
                        pts.append([b[0] + dx, b[1] + dy, b[2] + dz])
        mn, mx = bbox_of(pts)
        off = [(mn[i] + mx[i]) * 0.5 for i in range(3)]
        for it in items:
            for i in range(3):
                it["box"][i] -= off[i]
                it["pivot"][i] -= off[i]
        for b in bones:
            for i in range(3):
                b["pivot"][i] -= off[i]
    if scale != 1.0:
        for it in items:
            for i in range(3):
                it["box"][i] *= scale
                it["pivot"][i] *= scale
            it["box"][3] *= scale
            it["box"][4] *= scale
            it["box"][5] *= scale
        for b in bones:
            for i in range(3):
                b["pivot"][i] *= scale
    return items, bones, g.desc


FACE_ORDER = ["down", "up", "north", "south", "west", "east"]


def js_uv(uv, size):
    """基岩 uv -> 本项目 uv。uv_size 缺失/为 0 时用 cube 的对应尺寸兜底。"""
    out = {}
    if uv:
        for f in FACE_ORDER:
            e = uv.get(f)
            if not e:
                continue
            u, v = e["uv"]
            w, h = e.get("uv_size", [0, 0])
            out[f] = [u, v, w, h]
    return out


def fmt(v, nd=4):
    def one(x):
        x = round(float(x) + 0.0, nd)
        if x == 0:
            x = 0.0
        s = ("%.*f" % (nd, x)).rstrip("0").rstrip(".")
        return s if s not in ("", "-0") else "0"
    return "[" + ",".join(one(x) for x in v) + "]"


def to_js_model(items, indent="    "):
    lines = []
    for it in items:
        parts = ["name:" + json.dumps(it["name"], ensure_ascii=False)]
        if it.get("bone"):
            parts.append("bone:" + json.dumps(it["bone"], ensure_ascii=False))
        parts.append("box:" + fmt(it["box"]))
        u = js_uv(it["uv"], it["box"][3:])
        if u:
            kv = ",".join("%s:%s" % (f, fmt(u[f])) for f in FACE_ORDER if f in u)
            parts.append("uv:{" + kv + "}")
        parts.append("pivot:" + fmt(it["pivot"]))
        if any(abs(x) > 1e-6 for x in it["rot"]):
            parts.append("rot:" + fmt(it["rot"]))
        if any(abs(x) > 1e-6 for x in it["move"]):
            parts.append("move:" + fmt(it["move"]))
        lines.append(indent + "{ " + ", ".join(parts) + " },")
    return "\n".join(lines)


def to_js_bones(bones, indent="        "):
    """骨架表 -> Item.defineItem 的 bones: {...}
    pivot 单位像素（引擎 /16）；rot 是**度**的静态旋转（引擎转弧度）。"""
    lines = []
    for b in bones:
        par = (", parent:" + json.dumps(b["parent"], ensure_ascii=False)) if b["parent"] else ""
        rot = ""
        if any(abs(x) > 1e-6 for x in b.get("rot", [])):
            rot = ", rot:" + fmt(b["rot"])
        lines.append("%s%s: { pivot:%s%s%s }," % (
            indent, json.dumps(b["name"], ensure_ascii=False), fmt(b["pivot"]), rot, par))
    return "\n".join(lines)


def load(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)
