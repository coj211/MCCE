# 生成"聊天按钮候选样式"对比图：把 gui/spritesheet.png 里 1.6.4 用过的按钮底切片
# 做 9-patch 拉伸，叠上它用过的图标切片，放大排版成编号网格，供人工指认。
from PIL import Image, ImageDraw

SRC = "data/images/gui/spritesheet.png"
OUT = "tools/chat_button_candidates.png"
SCALE = 4

sheet = Image.open(SRC).convert("RGBA")


def nine_patch(src_rect, cut_x, cut_y, tw, th):
    """把 src_rect 当作 9-patch 源，切成九块后拉伸拼成 tw x th。"""
    x, y, w, h = src_rect
    patch = sheet.crop((x, y, x + w, y + h))
    xs = [0, cut_x, w - cut_x, w]
    ys = [0, cut_y, h - cut_y, h]
    out = Image.new("RGBA", (tw, th), (0, 0, 0, 0))
    # 目标九块的位置
    txs = [0, cut_x, tw - cut_x, tw]
    tys = [0, cut_y, th - cut_y, th]
    for i in range(3):
        for j in range(3):
            sw, sh = xs[i + 1] - xs[i], ys[j + 1] - ys[j]
            dw, dh = txs[i + 1] - txs[i], tys[j + 1] - tys[j]
            if sw <= 0 or sh <= 0 or dw <= 0 or dh <= 0:
                continue
            piece = patch.crop((xs[i], ys[j], xs[i] + sw, ys[j] + sh))
            if (dw, dh) != (sw, sh):
                piece = piece.resize((dw, dh), Image.NEAREST)
            out.alpha_composite(piece, (txs[i], tys[j]))
    return out


def make_bg(desc, tw, th):
    if desc[0] == "np":
        return nine_patch(desc[1], desc[2], desc[3], tw, th)
    return Image.new("RGBA", (desc[1], desc[2]), desc[3])


def make_icon(rect):
    if rect is None:
        return None
    x, y, w, h = rect
    return sheet.crop((x, y, x + w, y + h))


# (说明, 底, 图标, 尺寸)
NP8_32 = ("np", (8, 32, 8, 8), 2, 2)
NP0_32 = ("np", (0, 32, 8, 8), 2, 2)
NP112 = ("np", (112, 0, 8, 67), 2, 2)
NP120 = ("np", (120, 0, 8, 67), 2, 2)
FLAT_BLACK = ("flat", 120, 120, (0, 0, 0, 112))   # 半透明黑(相当于现有的 0x70000000)
FLAT_GRAY = ("flat", 120, 120, (60, 60, 60, 200))

cands = [
    ("1 底(8,32)+图标(48,4) 24x24", NP8_32, (48, 4, 12, 12), 24, 24),
    ("2 底(112,0)+图标(48,4) 24x24", NP112, (48, 4, 12, 12), 24, 24),
    ("3 底(112,0)+图标(48,4) 24x20", NP112, (48, 4, 12, 12), 24, 20),
    ("4 底(8,32) 无图标 24x24", NP8_32, None, 24, 24),
    ("5 底(112,0) 无图标 24x24", NP112, None, 24, 24),
    ("6 半透明黑底+图标(48,4) 24x24", FLAT_BLACK, (48, 4, 12, 12), 24, 24),
    ("7 底(112,0)+图标(36,0) 20x20", NP112, (36, 0, 13, 11), 20, 20),
    ("8 底(8,32)+图标(36,0) 20x20", NP8_32, (36, 0, 13, 11), 20, 20),
    ("9 底(120,0)按下+图标(48,4) 24x24", NP120, (48, 4, 12, 12), 24, 24),
    ("10 底(8,32)+图标(60,0) 24x24", NP8_32, (60, 0, 12, 12), 24, 24),
    ("11 底(8,32)+图标(24,0) 24x24", NP8_32, (24, 0, 12, 12), 24, 24),
    ("12 底(8,32)+图标(12,0) 24x24", NP8_32, (12, 0, 12, 12), 24, 24),
]

LABEL_W = 300
CELL_W = 200
CELL_H = 200
COLS = 3
ROWS = (len(cands) + COLS - 1) // COLS
W = COLS * CELL_W
H = ROWS * CELL_H + 30
img = Image.new("RGBA", (W, H), (35, 35, 40, 255))
d = ImageDraw.Draw(img)
d.text((6, 8), "chat button candidates (4x) - pick a number", fill=(255, 255, 120, 255))

for i, (label, bgdesc, iconrect, tw, th) in enumerate(cands):
    cx = (i % COLS) * CELL_W
    cy = 30 + (i // COLS) * CELL_H
    d.rectangle([cx, cy, cx + CELL_W - 1, cy + CELL_H - 1], outline=(90, 90, 100, 255))
    d.text((cx + 6, cy + 4), label, fill=(200, 230, 255, 255))
    # 棋盘底，便于看半透明
    for yy in range(0, CELL_H - 40, 16):
        for xx in range(0, CELL_W, 16):
            if ((xx // 16) + (yy // 16)) % 2 == 0:
                d.rectangle([cx + xx, cy + 30 + yy, cx + xx + 15, cy + 45 + yy], fill=(70, 70, 75, 255))
    bg = make_bg(bgdesc, tw, th)
    scale = SCALE if tw >= 24 else SCALE
    bg = bg.resize((tw * scale, th * scale), Image.NEAREST)
    icon = make_icon(iconrect)
    if icon is not None:
        icon = icon.resize((icon.width * scale, icon.height * scale), Image.NEAREST)
        ox = (bg.width - icon.width) // 2
        oy = (bg.height - icon.height) // 2
        bg.alpha_composite(icon, (ox, oy))
    img.alpha_composite(bg, (cx + 20, cy + 60))

img.save(OUT)
print("saved", OUT, img.size)
