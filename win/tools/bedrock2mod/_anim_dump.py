# -*- coding: utf-8 -*-
"""看 luger 的 hold 动画到底对哪些骨骼做了什么（换算后的值）。"""
import sys
import zipfile

sys.stdout.reconfigure(encoding="utf-8")
sys.path.insert(0, ".")
import anim2js as A

z = zipfile.ZipFile("H:/downloded/[苦力怕论坛]二战枪械.mcaddon")
anims = A.collect(z, ["luger"])
scale = 0.4234

for key in sorted(anims):
    a = anims[key]
    if not a["bones"]:
        continue
    print("=" * 70)
    print("[%s] len=%s loop=%d  %d 骨骼" % (key, a["len"], 1 if a["loop"] else 0, len(a["bones"])))
    for bn in sorted(a["bones"]):
        ch = a["bones"][bn]
        bits = []
        if "rotation" in ch:
            v = ch["rotation"][0][1]
            bits.append("rot(度)=%s" % [round(x, 1) for x in v])
        if "position" in ch:
            v = ch["position"][0][1]
            bits.append("pos(像素)=%s" % [round(x, 1) for x in v])
        if "scale" in ch:
            s = ch["scale"][0][1]
            bits.append("scale=%s" % (round(s, 3) if isinstance(s, (int, float)) else s))
        print("   %-16s %s" % (bn, "  ".join(bits)))
