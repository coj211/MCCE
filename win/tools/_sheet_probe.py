# 分析 gui/spritesheet.png 各候选切片的 alpha/颜色统计，
# 用来判断哪一块是"半透明按钮底"、哪一块是图标（不依赖看图）。
from PIL import Image

im = Image.open("data/images/gui/spritesheet.png").convert("RGBA")

cands = {
    "A (48,4) 12x12":   (48, 4, 12, 12),
    "B (36,0) 13x11":   (36, 0, 13, 11),
    "C (36,9) 12x8":    (36, 9, 12, 8),
    "D (112,0) 8x67":   (112, 0, 8, 67),
    "E (120,0) 8x67":   (120, 0, 8, 67),
    "F (8,32) 8x8":     (8, 32, 8, 8),
    "G (0,32) 8x8":     (0, 32, 8, 8),
    "H (24,0) 12x12":   (24, 0, 12, 12),
    "I (0,0) 12x12":    (0, 0, 12, 12),
    "J (12,0) 12x12":   (12, 0, 12, 12),
    "K (60,0) 12x12":   (60, 0, 12, 12),
    "L (72,0) 12x12":   (72, 0, 12, 12),
}
for name, (x, y, w, h) in cands.items():
    crop = im.crop((x, y, x + w, y + h))
    px = list(crop.getdata())
    a = [p[3] for p in px]
    opaque = sum(1 for v in a if v >= 250)
    semi = sum(1 for v in a if 10 < v < 250)
    clear = sum(1 for v in a if v <= 10)
    n = len(px)
    # 不透明像素的平均 RGB
    rgb = [p for p in px if p[3] >= 250]
    avg = (sum(p[0] for p in rgb) // len(rgb),
           sum(p[1] for p in rgb) // len(rgb),
           sum(p[2] for p in rgb) // len(rgb)) if rgb else (0, 0, 0)
    print("%-18s 不透明%3d%% 半透明%3d%% 全透%3d%%  平均RGB=%s" % (
        name, opaque * 100 // n, semi * 100 // n, clear * 100 // n, avg))

# 顺带扫一遍：整张图里"明显的半透明矩形块"（可能是按钮底）分布
print("\n=== 每 16x16 块的半透明像素比例（>20% 才列出）===")
W, H = im.size
for by in range(0, H, 16):
    for bx in range(0, W, 16):
        crop = im.crop((bx, by, bx + 16, by + 16))
        a = [p[3] for p in crop.getdata()]
        semi = sum(1 for v in a if 10 < v < 250)
        if semi * 100 // len(a) > 20:
            print("  (%3d,%3d) 半透明 %d%%" % (bx, by, semi * 100 // len(a)))
