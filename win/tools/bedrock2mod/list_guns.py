# -*- coding: utf-8 -*-
"""清点《二战枪械》里到底有哪些武器，以及每把枪的模型/贴图/动画/音效资源。
顺带离线诊断"模型看不见"：算转换后模型的最终包围盒中心与范围（不需要跑游戏）。

用法: python list_guns.py
"""
import json
import os
import sys
import zipfile

sys.stdout.reconfigure(encoding="utf-8")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import geo2js as G

MCADDON = "H:/downloded/[苦力怕论坛]二战枪械.mcaddon"


def main():
    z = zipfile.ZipFile(MCADDON)
    names = z.namelist()

    # ---------------- 1) 物品定义（行为包 items/） ----------------
    print("=" * 78)
    print("一、武器物品（行为包 WocAddonBe/items/）")
    print("=" * 78)
    groups = {}
    for n in names:
        if n.startswith("WocAddonBe/items/") and n.endswith(".json"):
            rel = n[len("WocAddonBe/items/"):]
            parts = rel.split("/")
            d = json.loads(z.read(n))
            item = (d.get("minecraft:item") or {}).get("description", {})
            ident = item.get("identifier", "?")
            groups.setdefault(parts[0], []).append((parts[-1][:-5], ident))
    for g in sorted(groups):
        ent = groups[g]
        ident = ent[0][1]
        states = [e[0] for e in ent]
        print("  %-14s %-22s 状态: %s" % (g, ident, ", ".join(states)))

    # ---------------- 2) 3D 模型 ----------------
    print()
    print("=" * 78)
    print("二、3D 模型（models/entity/gun/ 与 launcher/）")
    print("=" * 78)
    models = {}
    for n in names:
        if "/models/entity/" in n and ("gun/" in n or "launcher/" in n) and n.endswith(".json"):
            base = n.split("/")[-1].split(".")[0]
            g = json.loads(z.read(n))
            geo = (g.get("minecraft:geometry") or [{}])[0]
            desc = geo.get("description", {})
            nb = len(geo.get("bones", []))
            nc = sum(len(b.get("cubes", [])) for b in geo.get("bones", []))
            models[base] = (n, nb, nc, desc.get("texture_width"), desc.get("texture_height"))
    for b in sorted(models):
        n, nb, nc, tw, th = models[b]
        print("  %-16s 骨骼 %3d, 部件 %4d, 贴图 %sx%s" % (b, nb, nc, tw, th))

    # ---------------- 3) 动画 ----------------
    print()
    print("=" * 78)
    print("三、动画文件")
    print("=" * 78)
    anim_groups = {}
    for n in names:
        if "/animations/" not in n or not (n.endswith(".json") or n.endswith(".animation")):
            continue
        rel = n.split("/animations/")[-1]
        top = rel.split("/")[0]
        anim_groups.setdefault(top, []).append(rel.split("/")[-1])
    for g in sorted(anim_groups):
        items = sorted(set(anim_groups[g]))
        print("  %-16s %s" % (g, ", ".join(items)))

    # ---------------- 4) 音效 ----------------
    print()
    print("=" * 78)
    print("四、音效（按目录）")
    print("=" * 78)
    snd = {}
    for n in names:
        if "/sounds/" in n and (n.endswith(".ogg") or n.endswith(".wav")):
            rel = n.split("/sounds/")[-1]
            parts = rel.split("/")
            key = parts[0] if len(parts) > 1 else "(根)"
            snd.setdefault(key, []).append(parts[-1])
    for k in sorted(snd):
        print("  %-14s %d 个: %s" % (k, len(snd[k]), ", ".join(sorted(set(snd[k]))[:12])))

    # ---------------- 5) 诊断"看不见" ----------------
    print()
    print("=" * 78)
    print("五、诊断：转换后模型的实际占位（离线算，不需要跑游戏）")
    print("=" * 78)
    import build_gun_mod as B
    for gun in ["luger", "mp40", "c96", "sten"]:
        hit = None
        for n in names:
            if "/models/entity/gun/" in n and n.endswith(".json") and gun in n.lower():
                hit = n
                break
        if not hit:
            continue
        d = json.loads(z.read(hit))
        items, bones, desc = G.convert(d, order="XYZ", origin_mode="abs", center=True)
        B.normalize_negatives(items)
        s = B.fit_scale(items, 16.0)
        pts = []
        for it in items:
            R = G.euler_matrix(it["rot"][0], it["rot"][1], it["rot"][2], "XYZ")
            c = [it["pivot"][i] * s for i in range(3)]
            b = [it["box"][i] * s for i in range(6)]
            mo = [it["move"][i] * s for i in range(3)]
            for dx in (0, b[3]):
                for dy in (0, b[4]):
                    for dz in (0, b[5]):
                        l = [b[0] + dx, b[1] + dy, b[2] + dz]
                        rel = [l[i] - c[i] for i in range(3)]
                        w = G.mv(R, rel)
                        pts.append([c[i] + w[i] + mo[i] for i in range(3)])
        mn, mx = G.bbox_of(pts)
        cen = [(mn[i] + mx[i]) * 0.5 / 16.0 for i in range(3)]
        span = [(mx[i] - mn[i]) / 16.0 for i in range(3)]
        print("  %-22s 中心=(%+.3f, %+.3f, %+.3f) 格   跨度=(%.2f, %.2f, %.2f) 格"
              % (hit.split("/")[-1], cen[0], cen[1], cen[2], span[0], span[1], span[2]))


if __name__ == "__main__":
    main()
