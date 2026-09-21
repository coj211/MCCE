# -*- coding: utf-8 -*-
"""基岩 .animation.json / .animation -> JS 动画表。

输出给 mod 的运行时的数据形状（已经换算到本项目口径）：
    { "shoot": { loop:0, len:0.2, bones: {
          "luger": { r:[[t,[rx,ry,rz]], ...],   // 弧度
                     p:[[t,[px,py,pz]], ...],   // 格内单位 (1=一整格)
                     s:[[t,scale], ...] } } }   // 标量
关键帧统一成 [[t, 值], ...]；常量就是单帧 [[0, 值]]。

单位换算：
  * rotation 度 -> 弧度
  * position 像素 -> 格内   (px * modelScale / 16)
  * scale 原样
  modelScale 是模型为了塞进物品大小做过的统一缩放，动画必须跟着走，否则骨骼
  一动就和模型对不上。
"""
import json
import math
import re

CHANNELS = ("position", "rotation", "scale")


def _frames(v):
    """统一成 [[t, 值], ...]。"""
    if isinstance(v, (int, float)):
        return [[0.0, v]]
    if isinstance(v, list):
        return [[0.0, v]]
    out = []
    for k in sorted(v.keys(), key=float):
        val = v[k]
        if isinstance(val, dict):          # 基岩的 {pre, post} 缓动写法
            val = val.get("post", val.get("pre", val))
        out.append([float(k), val])
    return out


def parse(raw):
    """一份动画文件 -> {动画名: {len, loop, bones, timeline}}"""
    d = json.loads(raw.decode("utf-8-sig"))
    out = {}
    for name, av in (d.get("animations") or {}).items():
        bones = {}
        for bn, bv in (av.get("bones") or {}).items():
            ch = {}
            for c in CHANNELS:
                if c in bv:
                    ch[c] = _frames(bv[c])
            if ch:
                bones[bn] = ch
        out[name] = {"len": av.get("animation_length"),
                     "loop": bool(av.get("loop", False)),
                     "bones": bones,
                     # 行为包那边往往只有 timeline（纯逻辑，不动骨骼）
                     "timeline": av.get("timeline") or {}}
    return out


def _key(name, gun):
    """animation.luger.first_person.hold -> hold / animation.luger.third_person.hold -> t_hold"""
    n = re.sub(r"^animation\.", "", name)
    n = re.sub(r"^" + re.escape(gun) + r"[._-]*", "", n, flags=re.I)
    if n.startswith("first_person."):
        n = n[len("first_person."):]
    elif n.startswith("third_person."):
        n = "t_" + n[len("third_person."):]
    return n or "base"


def collect(z, gun):
    """把某个枪相关的所有动画抓出来，按动作名归类；BP（只有 timeline）与
    RP（骨骼动画）同名时合并 —— 前者给逻辑，后者给姿态。

    gun 可以是单个关键词，也可以是关键词列表（源包里目录名不统一：
    ppsh41 的动画在 guns/ppsh/、lee 的在 leeenfield/ 和 guns/lee-enfield/）。
    """
    keys = [gun] if isinstance(gun, str) else [k for k in gun if k]
    out = {}
    for n in z.namelist():
        if "animations" not in n:
            continue
        base = n.split("/")[-1]
        if not (base.endswith(".json") or base.endswith(".animation")):
            continue
        low = n.lower()
        if not any(k.lower() in low for k in keys):
            continue
        try:
            got = parse(z.read(n))
        except Exception:
            continue
        for an, av in got.items():
            if not av["bones"] and not av["timeline"]:
                continue
            key = _key(an, keys[0])
            cur = out.get(key)
            if cur is None:
                out[key] = av
                continue
            if av["bones"]:
                cur["bones"] = av["bones"]
            if av["timeline"]:
                cur["timeline"] = av["timeline"]
            if av["len"] is not None:
                cur["len"] = av["len"]
    return out


def to_js(anims, scale=1.0, order=("hold", "shoot", "reload", "draw", "scope", "sprint")):
    """生成 JS 数据文本。scale = 模型统一缩放。"""
    lines = []
    keys = [k for k in order if k in anims] + [k for k in sorted(anims) if k not in order]
    for key in keys:
        a = anims[key]
        if not a["bones"]:
            continue                       # 只有 timeline 的（纯逻辑）不进骨骼表
        ln = a["len"] if a["len"] is not None else 0.0
        lines.append('    %s: { loop:%d, len:%s, bones: {'
                     % (json.dumps(key), 1 if a["loop"] else 0, _num(ln)))
        for bn in sorted(a["bones"]):
            ch = a["bones"][bn]
            parts = []
            if "rotation" in ch:
                fr = _v3_frames(ch["rotation"], lambda v: [math.radians(v[0]), math.radians(v[1]), math.radians(v[2])])
                if fr:
                    parts.append("r:" + _frames_js(fr))
            if "position" in ch:
                k = scale / 16.0
                fr = _v3_frames(ch["position"], lambda v: [v[0] * k, v[1] * k, v[2] * k])
                if fr:
                    parts.append("p:" + _frames_js(fr))
            if "scale" in ch:
                fs = []
                ok = True
                for t, v in ch["scale"]:
                    if isinstance(v, (int, float)):
                        fs.append([t, [v, v, v]])
                    elif _is_v3(v):
                        fs.append([t, [v[0], v[1], v[2]]])
                    else:
                        ok = False
                        break
                if ok and fs:
                    parts.append("s:" + _frames_js(fs))
            if parts:
                lines.append('        %s: { %s },'
                             % (json.dumps(bn), ", ".join(parts)))
        lines.append('    } },')
    return "\n".join(lines)


def _is_v3(v):
    return (isinstance(v, list) and len(v) >= 3
            and all(isinstance(x, (int, float)) for x in v[:3]))


def _v3_frames(src, conv):
    """把 [[t,[x,y,z]],...] 逐帧换算；遇到 molang 字符串之类的非数字就整体放弃。"""
    out = []
    for t, v in src:
        if not _is_v3(v):
            return None
        out.append([t, conv(v)])
    return out


def _num(x):
    s = ("%.4f" % float(x)).rstrip("0").rstrip(".")
    return s if s not in ("", "-0") else "0"


def _v3(v):
    return "[" + ",".join(_num(x) for x in v) + "]"


def _frames_js(fr):
    parts = []
    for t, v in fr:
        val = _v3(v) if isinstance(v, list) else _num(v)
        parts.append("[" + _num(t) + "," + val + "]")
    return "[" + ",".join(parts) + "]"


if __name__ == "__main__":
    import sys
    import zipfile
    sys.stdout.reconfigure(encoding="utf-8")
    z = zipfile.ZipFile("H:/downloded/[苦力怕论坛]二战枪械.mcaddon")
    for gun in ["luger", "mp40", "c96"]:
        got = collect(z, gun)
        print("=" * 72)
        print("[%s] 抓到 %d 个动画:" % (gun, len(got)))
        for k in sorted(got):
            v = got[k]
            chans = set()
            for b in v["bones"].values():
                chans |= set(b.keys())
            print("   %-10s len=%-6s loop=%d  %2d 骨骼  通道=%s"
                  % (k, v["len"], 1 if v["loop"] else 0, len(v["bones"]), sorted(chans)))
        if gun == "luger":
            print("--- to_js 预览（截断） ---")
            print(to_js(got, 0.4234)[:1500])
