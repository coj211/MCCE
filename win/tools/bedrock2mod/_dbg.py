# -*- coding: utf-8 -*-
"""临时诊断：anim2js.collect 为什么漏掉 BP 的动画。"""
import sys
import zipfile

sys.stdout.reconfigure(encoding="utf-8")
sys.path.insert(0, ".")
import anim2js as A

z = zipfile.ZipFile("H:/downloded/[苦力怕论坛]二战枪械.mcaddon")
gun = "luger"
for n in z.namelist():
    if "animations" not in n:
        continue
    base = n.split("/")[-1]
    if not (base.endswith(".json") or base.endswith(".animation")):
        continue
    low = n.lower()
    if gun not in low and ("/" + gun + "/") not in low:
        continue
    print("FILE:", n)
    try:
        got = A.parse(z.read(n))
        print("    动画:", {k: "%d 骨 len=%s" % (len(v["bones"]), v["len"]) for k, v in got.items()})
    except Exception as e:
        print("    PARSE FAIL:", type(e).__name__, e)
