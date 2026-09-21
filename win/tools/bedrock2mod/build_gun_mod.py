# -*- coding: utf-8 -*-
"""把基岩《二战枪械》的一把枪转成本项目的模组：模型 + 骨架 + 动画 + 开火/换弹。

用法:
    python build_gun_mod.py luger
    python build_gun_mod.py --all
    python build_gun_mod.py mp40 --target 16 --item-id 210

输出: out/<name>.zip（main.js + guns/*.png + sounds/*.ogg）
"""
import argparse
import json
import os
import struct
import sys
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import geo2js as G
import anim2js as A

HERE = os.path.dirname(os.path.abspath(__file__))
MCADDON = "H:/downloded/[苦力怕论坛]二战枪械.mcaddon"

# 名字 -> (geo.json, 模型贴图, 显示名, 音效名, 弹匣容量, 射速(秒), 伤害, 子弹速度)
GUNS = {
    "luger":  ("WocAddonRe(1)/models/entity/gun/luger.geo.json",
               "WocAddonRe(1)/textures/guns/luger.png", "Luger P08", "luger", 8, 0.30, 14, 3.0),
    "c96":    ("WocAddonRe(1)/models/entity/gun/c96.geo.json",
               "WocAddonRe(1)/textures/guns/c96.png", "Mauser C96", "c96", 10, 0.28, 13, 3.0),
    "mp40":   ("WocAddonRe(1)/models/entity/gun/mp40.geo (10).json",
               "WocAddonRe(1)/textures/guns/mp40.png", "MP40", "mp40", 32, 0.10, 9, 3.4),
    "sten":   ("WocAddonRe(1)/models/entity/gun/sten.geo.json",
               "WocAddonRe(1)/textures/guns/sten.png", "Sten Mk II", "sten", 32, 0.11, 9, 3.4),
    "mg08":   ("WocAddonRe(1)/models/entity/gun/mg08.geo.json",
               "WocAddonRe(1)/textures/machine/mg08.png", "MG-08", "mg08", 100, 0.08, 11, 4.0),
    "ppsh41": ("WocAddonRe(1)/models/entity/gun/ppsh41.geo.json",
               "WocAddonRe(1)/textures/guns/ppsh41.png", "PPSh-41", "ppsh", 71, 0.09, 9, 3.4),
    "mosin":  ("WocAddonRe(1)/models/entity/gun/mosin.geo (7).json",
               "WocAddonRe(1)/textures/guns/mosin.png", "Mosin-Nagant", "mosin", 5, 1.10, 25, 5.0),
    "lee":    ("WocAddonRe(1)/models/entity/gun/lee-enfield.geo.json",
               "WocAddonRe(1)/textures/guns/lee_enfield.png", "Lee-Enfield", "leeenfield", 10, 0.80, 24, 5.0),
}

HEAD = u"""// name: 二战枪械 - @@DISP@@
// author: bedrock2mod (源: WocAddon by Klep)
// version: 0.3.0
// description: @@DISP@@ —— 模型 + 骨架 + 动画 + 开火/换弹（由 .geo.json / .animation.json 自动生成）

var GUN_ID = Item.defineItem(@@ITEM_ID@@, {
    name: "@@DISP@@",
    modelTexture: "guns/@@NAME@@.png",
    texSize: [@@TEXW@@, @@TEXH@@],
    bones: {
@@BONES@@
    },
    model: [
@@MODEL@@
    ],
    anim: anim
});

var BULLET_ID = Projectile.defineProjectile(@@BULLET_ID@@, {
    modelTexture: "guns/bullet.png",
    box: [-1.5, -1.5, -6, 3, 3, 12],
    damage: @@DAMAGE@@,
    gravity: 0.0,
    drag: 0.0,
    life: 40,
    size: 0.4
});

"""


def png_size(blob):
    if blob[:8] != b"\x89PNG\r\n\x1a\n":
        return None
    w, h = struct.unpack(">II", blob[16:24])
    return w, h


def normalize_negatives(items):
    """本项目把 x1-x0<1e-4 当成'退化成平面'，负尺寸会被误判 —— 交换 min/max。"""
    n = 0
    for it in items:
        b = it["box"]
        for k in range(3):
            if b[3 + k] < 0:
                b[k] += b[3 + k]
                b[3 + k] = -b[3 + k]
                n += 1
    return n


def fit_scale(items, target):
    pts = []
    for it in items:
        b = it["box"]
        for dx in (0, b[3]):
            for dy in (0, b[4]):
                for dz in (0, b[5]):
                    pts.append([b[0] + dx, b[1] + dy, b[2] + dz])
    mn, mx = G.bbox_of(pts)
    span = max(mx[i] - mn[i] for i in range(3))
    return 1.0 if span <= 1e-6 else target / span


def pick_sound(z, snd, kind):
    """从源包里挑该枪的音效。

    实测路径：
      shot  -> WocAddonBe/sounds/guns/<snd>.ogg
      reload-> WocAddonRe(1)/sounds/reload/<snd>/{magless,open,first,magin,close,finish,kokang}.ogg
    各枪的步骤名不统一，所以按"优先名 -> 该目录第一个"两级回退。
    """
    names = [n for n in z.namelist() if n.endswith((".ogg", ".wav"))]
    if kind == "shot":
        for n in names:
            low = n.lower()
            if "/guns/" in low and low.rsplit("/", 1)[-1] in (snd + ".ogg", snd + ".wav"):
                return n
        return None
    order = {"open": ["magless.ogg", "open.ogg", "first.ogg", "clip.ogg"],
             "close": ["magin.ogg", "close.ogg", "finish.ogg", "kokang.ogg", "kokang1.ogg"]}[kind]
    bucket = [n for n in names if ("/reload/%s/" % snd) in n]
    for want in order:
        for n in bucket:
            if n.endswith("/" + want):
                return n
    return bucket[0] if bucket else None


def build(name, item_id, target=16.0, origin_mode="abs", order="XYZ"):
    geo_path, tex_path, disp, snd, mag, fire, dmg, bspeed = GUNS[name]
    z = zipfile.ZipFile(MCADDON)
    d = json.loads(z.read(geo_path))
    tex_blob = z.read(tex_path)
    tw, th = png_size(tex_blob)
    desc = d["minecraft:geometry"][0]["description"]

    items, bones, _ = G.convert(d, order=order, origin_mode=origin_mode, center=True)
    neg = normalize_negatives(items)

    # ---- 动画（先收，因为缩放要参考 hold 的基准缩放）----
    anims = A.collect(z, [snd, name])         # 目录名不统一，两个关键词都试

    # 模型缩放：目标是最长边 16px（1 格）。但基岩的 hold 动画本身还会再缩一次
    # （它就是用 scale 把"设计尺寸"缩到"显示尺寸"的），所以这里先把 hold 给
    # 主体骨骼的 scale 除掉，否则会双重缩放（枪小成 1/3）。
    hold_base = 1.0
    hb = anims.get("hold") or {}
    for bn, ch in (hb.get("bones") or {}).items():
        if bn.lower() in (name.lower(), snd.lower()) and "scale" in ch:
            try:
                v = ch["scale"][0][1]          # 可能是单值 float，也可能是 [x,y,z]
                hold_base = float(v[0]) if isinstance(v, list) else float(v)
            except Exception:
                hold_base = 1.0
            break
    fit_target = target / hold_base if hold_base and abs(hold_base) > 1e-6 else target

    s = fit_scale(items, fit_target)
    if abs(s - 1.0) > 1e-9:
        for it in items:
            for i in range(3):
                it["box"][i] *= s
                it["box"][3 + i] *= s
                it["pivot"][i] *= s
        for b in bones:                       # 骨架必须跟着一起缩放，否则与部件错位
            for i in range(3):
                b["pivot"][i] *= s

    anims_js = A.to_js(anims, scale=s)

    # ---- head（物品定义）----
    head = HEAD
    for k, v in [("@@DISP@@", disp), ("@@NAME@@", name), ("@@ITEM_ID@@", str(item_id)),
                 ("@@TEXW@@", str(desc.get("texture_width"))), ("@@TEXH@@", str(desc.get("texture_height"))),
                 ("@@BONES@@", G.to_js_bones(bones)), ("@@MODEL@@", G.to_js_model(items)),
                 ("@@BULLET_ID@@", str(60 + (item_id - 210))), ("@@DAMAGE@@", str(dmg))]:
        head = head.replace(k, v)

    # ---- runtime ----
    with open(os.path.join(HERE, "runtime.js"), "r", encoding="utf-8") as f:
        rt = f.read()
    for k, v in [("@@NAME@@", name), ("@@DISP@@", disp), ("@@MAG@@", str(mag)),
                 ("@@FIRE@@", str(fire)), ("@@BSPEED@@", str(bspeed)),
                 ("@@NPARTS@@", str(len(items))), ("@@NBONES@@", str(len(bones))),
                 ("@@NANIMS@@", str(len([1 for a in anims.values() if a["bones"]]))),
                 ("@@ANIMS@@", anims_js)]:
        rt = rt.replace(k, v)

    main_js = head + rt

    # ---- 打包 ----
    out = os.path.join(HERE, "out", name + ".zip")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    sounds = []
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("main.js", main_js.encode("utf-8"))
        zf.writestr("guns/%s.png" % name, tex_blob)
        if "WocAddonRe(1)/textures/bullet.png" in z.namelist():
            zf.writestr("guns/bullet.png", z.read("WocAddonRe(1)/textures/bullet.png"))
        for kind, fname in (("shot", "shot_%s" % name), ("open", "reload_open_%s" % name),
                            ("close", "reload_close_%s" % name)):
            src = pick_sound(z, snd, kind)
            if src:
                ext = src.rsplit(".", 1)[-1]
                zf.writestr("sounds/%s.%s" % (fname, ext), z.read(src))
                sounds.append(fname)
    print("[%s] %s: %d 部件, %d 骨骼, %d 动画, 负尺寸修正 %d, 缩放 %.4f, 贴图 %sx%s, 音效 %s"
          % (name, disp, len(items), len(bones),
             len([1 for a in anims.values() if a["bones"]]), neg, s, tw, th,
             ",".join(sounds) or "无"))
    print("     -> %s (%d KB)" % (out, os.path.getsize(out) // 1024))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("gun", nargs="*", help="枪名；--all 时忽略")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--target", type=float, default=16.0, help="最长边缩放到多少像素(16=1格)")
    ap.add_argument("--item-id", type=int, default=210)
    ap.add_argument("--origin", default="abs", choices=["abs", "rel"])
    args = ap.parse_args()

    names = list(GUNS) if args.all else (args.gun or ["luger"])
    for i, n in enumerate(names):
        if n not in GUNS:
            print("跳过未知枪名:", n)
            continue
        build(n, args.item_id + i, target=args.target, origin_mode=args.origin)


if __name__ == "__main__":
    main()
