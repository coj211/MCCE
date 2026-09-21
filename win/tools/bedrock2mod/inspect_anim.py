# -*- coding: utf-8 -*-
"""看基岩 .animation.json 的实际结构，为转换器定格式。

用法: python inspect_anim.py
"""
import json
import sys
import zipfile

sys.stdout.reconfigure(encoding="utf-8")
MCADDON = "H:/downloded/[苦力怕论坛]二战枪械.mcaddon"

TARGETS = [
    "WocAddonRe(1)/animations/luger/luger.shoot.json",
    "WocAddonRe(1)/animations/luger/luger.reload.json",
    "WocAddonRe(1)/animations/guns/ppsh/first_person.hold.json",
    "WocAddonRe(1)/animations/guns/ppsh/first_person.shoot.json",
    "WocAddonRe(1)/animations/guns/mp40/mp40.shoot.json",
    "WocAddonRe(1)/animations/crosshair.animation.json",
]


def dump(bv, indent="        "):
    for ch in ("position", "rotation", "scale"):
        if ch not in bv:
            continue
        v = bv[ch]
        if isinstance(v, (int, float)):
            print("%s%-8s = %s   (单值常量)" % (indent, ch, v))
            continue
        if isinstance(v, list):
            print("%s%-8s = %s   (常量，无关键帧)" % (indent, ch, v))
            continue
        ks = sorted(v.keys(), key=float)
        print("%s%-8s %d 个关键帧:" % (indent, ch, len(ks)))
        for k in ks[:6]:
            print("%s   @%-7s %s" % (indent, k, v[k]))
        if len(ks) > 6:
            print("%s   ... (共 %d)" % (indent, len(ks)))


def main():
    z = zipfile.ZipFile(MCADDON)
    for n in TARGETS:
        print("=" * 74)
        print(n.split("/animations/")[-1])
        try:
            d = json.loads(z.read(n).decode("utf-8-sig"))
        except KeyError:
            print("  (不存在)")
            continue
        for an, av in (d.get("animations") or {}).items():
            print("  动画名: %s" % an)
            print("  length=%s  loop=%s  override_previous=%s  anim_time_update=%s"
                  % (av.get("animation_length"), av.get("loop", "-"),
                     av.get("override_previous_animation", "-"), av.get("anim_time_update", "-")))
            for bn, bv in (av.get("bones") or {}).items():
                print("  骨骼 %s:" % bn)
                dump(bv, "      ")


if __name__ == "__main__":
    main()
