# 把 gui/spritesheet.png 放大并画出像素网格 + 坐标刻度 + 1.6.4 代码里用过的
# 候选切片框，方便人工确认"哪个区域是聊天图标"。仅用于人工核对，不参与构建。
from PIL import Image, ImageDraw

SRC = "data/images/gui/spritesheet.png"
OUT = "tools/spritesheet_zoom.png"
SCALE = 8

im = Image.open(SRC).convert("RGBA")
W, H = im.size
big = im.resize((W * SCALE, H * SCALE), Image.NEAREST)

out = Image.new("RGBA", (W * SCALE, H * SCALE), (30, 30, 30, 255))
d = ImageDraw.Draw(out)
for y in range(0, H * SCALE, 16):
    for x in range(0, W * SCALE, 16):
        if ((x // 16) + (y // 16)) % 2 == 0:
            d.rectangle([x, y, x + 15, y + 15], fill=(55, 55, 55, 255))
out.alpha_composite(big)

for gx in range(0, W + 1, 8):
    d.line([(gx * SCALE, 0), (gx * SCALE, H * SCALE)], fill=(255, 0, 0, 110))
for gy in range(0, H + 1, 8):
    d.line([(0, gy * SCALE), (W * SCALE, gy * SCALE)], fill=(255, 0, 0, 110))
for gx in range(0, W + 1, 16):
    d.text((gx * SCALE + 3, 3), str(gx), fill=(255, 255, 0, 255))
for gy in range(0, H + 1, 16):
    d.text((3, gy * SCALE + 3), str(gy), fill=(255, 255, 0, 255))

# 1.6.4 代码里出现过的切片
cands = [
    (48, 4, 12, 12, "A"),
    (36, 0, 13, 11, "B"),
    (36, 9, 12, 8, "C"),
    (112, 0, 8, 67, "D"),
    (120, 0, 8, 67, "E"),
    (8, 32, 8, 8, "F"),
    (0, 32, 8, 8, "G"),
]
for (x, y, w, h, lab) in cands:
    d.rectangle([x * SCALE, y * SCALE, (x + w) * SCALE - 1, (y + h) * SCALE - 1],
                outline=(0, 255, 0, 255))
    d.text((x * SCALE + 3, (y + h) * SCALE + 3), lab, fill=(0, 255, 0, 255))

out.save(OUT)
print("saved", OUT, out.size, "原图", W, "x", H, "放大", SCALE, "倍")
