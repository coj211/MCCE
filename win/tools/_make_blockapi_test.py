# -*- coding: utf-8 -*-
"""生成方块 API 测试模组 mods/blockapi_test.zip。

一次性测完这几套新 API：
  1) 自定义方块模型 3D（多部件、静态旋转、自发光）
  2) 自定义方块模型 2D（贴地薄片、贴面图案；按数据值换形状）
  3) 方块动画（部件逐帧旋转/开合）
  4) 自定义放置：placeData（竖半砖按点击面朝向）
  5) 自定义放置：onPlaceAttempt 接管（微方块：格内 1/4 小方块，落点量化）
  6) 微方块：格内几何 + 格内碰撞（都来自方块实体数据）

用法： python tools/_make_blockapi_test.py
输出： mods/blockapi_test.zip（贴图 + main.js），并把 zip 加进 mods/modlist.json 的 enabled。
"""
import io
import json
import os
import struct
import zlib
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
MODS = os.path.join(ROOT, "mods")


# ---------------------------------------------------------------- png 编码
def write_png(path, pixels):
    """pixels: 16 行 x 16 列，每项 (r,g,b,a)。"""
    h = len(pixels)
    w = len(pixels[0])
    raw = bytearray()
    for y in range(h):
        raw.append(0)                      # filter type 0
        for x in range(w):
            r, g, b, a = pixels[y][x]
            raw += bytes((r & 255, g & 255, b & 255, a & 255))

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff))

    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    blob = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr)
            + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(blob)


def blank(rgba=(0, 0, 0, 0)):
    return [[rgba for _ in range(16)] for _ in range(16)]


def fill(px, x0, y0, x1, y1, rgba):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            if 0 <= y < 16 and 0 <= x < 16:
                px[y][x] = rgba


def make_textures(outdir):
    os.makedirs(outdir, exist_ok=True)
    made = {}

    # 半砖：石灰色 + 深色边
    p = blank((150, 150, 156, 255))
    fill(p, 0, 0, 15, 0, (90, 90, 96, 255))
    fill(p, 0, 15, 15, 15, (90, 90, 96, 255))
    fill(p, 0, 0, 0, 15, (110, 110, 116, 255))
    fill(p, 15, 0, 15, 15, (110, 110, 116, 255))
    made["blocks/slab.png"] = p

    # 贴地线：透明底 + 红色十字线
    p = blank()
    fill(p, 1, 7, 14, 8, (200, 30, 30, 255))
    fill(p, 7, 1, 8, 14, (200, 30, 30, 255))
    fill(p, 6, 6, 9, 9, (255, 80, 80, 255))
    made["blocks/wire.png"] = p

    # 贴面图案：黄黑斜纹
    p = blank()
    for y in range(16):
        for x in range(16):
            p[y][x] = (230, 200, 60, 255) if ((x + y) % 8) < 4 else (40, 40, 40, 255)
    made["blocks/panel.png"] = p

    # 魔书台：深紫
    p = blank((70, 40, 110, 255))
    fill(p, 0, 0, 15, 1, (110, 70, 160, 255))
    fill(p, 0, 14, 15, 15, (40, 20, 70, 255))
    made["blocks/altar.png"] = p

    # 发光面
    p = blank((255, 240, 170, 255))
    made["blocks/glow.png"] = p

    # 书：棕封面 + 白页
    p = blank((120, 70, 30, 255))
    fill(p, 1, 1, 14, 14, (235, 235, 225, 255))
    fill(p, 1, 1, 2, 14, (120, 70, 30, 255))
    made["blocks/book.png"] = p

    # 水晶：青色菱形
    p = blank()
    for y in range(16):
        for x in range(16):
            if abs(x - 7.5) + abs(y - 7.5) < 7:
                p[y][x] = (90, 230, 230, 255)
    made["blocks/crystal.png"] = p

    # 箱子侧面 / 顶面
    p = blank((150, 105, 55, 255))
    fill(p, 0, 0, 15, 1, (100, 65, 30, 255))
    fill(p, 0, 14, 15, 15, (100, 65, 30, 255))
    made["blocks/box_side.png"] = p
    p = blank((175, 125, 70, 255))
    fill(p, 0, 0, 15, 15, (175, 125, 70, 255))
    fill(p, 6, 6, 9, 9, (90, 60, 30, 255))
    made["blocks/box_top.png"] = p

    # 微方块托盘：浅木色
    p = blank((205, 175, 125, 255))
    fill(p, 0, 0, 15, 15, (205, 175, 125, 255))
    fill(p, 0, 0, 15, 1, (160, 130, 90, 255))
    made["blocks/micro.png"] = p

    paths = {}
    for rel, px in made.items():
        full = os.path.join(outdir, rel.replace("/", os.sep))
        os.makedirs(os.path.dirname(full), exist_ok=True)
        write_png(full, px)
        paths[rel] = full
    return paths


# ---------------------------------------------------------------- main.js
MAIN_JS = u"""// name: 方块 API 测试
// author: Reasonix
// version: 1.0
// description: 一次性测完 自定义方块模型(3D/2D/多部件) · 方块动画 · 自定义放置 · 微方块
//
// 打开创造模式物品栏，找这几个方块：
//   竖半砖 / 贴地线 / 贴画板 / 魔书台 / 旋转水晶 / 开合箱 / 微方块

// ===========================================================================
// 1) 竖半砖 —— 对着哪一面放，半砖就贴在哪一面（placeData 决定数据值）
//    0=下半 1=上半 2=北半 3=南半 4=西半 5=东半
// ===========================================================================
var OPPOSITE = [1, 0, 3, 2, 5, 4];   // 点击面 -> 半砖贴目标格里的哪一侧
var SLAB = Block.defineBlock(200, {
    name: "\u7ad6\u534a\u7816",
    texture: "blocks/slab.png",
    material: "rock",
    renderLayer: "alphatest",
    model: [
        { name: "d0", box: [0, 0, 0, 16, 8, 16],  data: 0 },
        { name: "d1", box: [0, 8, 0, 16, 8, 16],  data: 1 },
        { name: "d2", box: [0, 0, 0, 16, 16, 8],  data: 2 },
        { name: "d3", box: [0, 0, 8, 16, 16, 8],  data: 3 },
        { name: "d4", box: [0, 0, 0, 8, 16, 16],  data: 4 },
        { name: "d5", box: [8, 0, 0, 8, 16, 16],  data: 5 }
    ],
    placeData: function (x, y, z, face, cx, cy, cz) {
        return OPPOSITE[face];
    }
});

// ===========================================================================
// 2) 贴地线 —— 2D 平面（box 的 y 宽度为 0），按数据值换形状
//    部件用 data: [...] 声明"只在这些数据值时画" -> 同一个方块换外形
// ===========================================================================
var WIRE = Block.defineBlock(201, {
    name: "\u8d34\u5730\u7ebf",
    texture: "blocks/wire.png",
    material: "plant",
    renderLayer: "alphatest",
    model: [
        { name: "ew",  box: [0, 0.5, 6, 16, 0, 4], data: [0, 2] },
        { name: "ns",  box: [6, 0.5, 0, 4, 0, 16], data: [1, 2] },
        { name: "hub", box: [6, 0.5, 6, 4, 0, 4],  data: [2] }
    ],
    // 按点击点在格内的位置决定走向：0 = 东西向，1 = 南北向，2 = 十字
    placeData: function (x, y, z, face, cx, cy, cz) {
        var ex = Math.abs(cx - 0.5), ez = Math.abs(cz - 0.5);
        if (ex > 0.3 && ez > 0.3) return 2;
        return ex >= ez ? 0 : 1;
    }
});

// ===========================================================================
// 3) 贴画板 —— 2D 贴在格子中间的一个平面上（z 宽度为 0 -> 正反两面都画）
// ===========================================================================
var PANEL = Block.defineBlock(202, {
    name: "\u8d34\u753b\u677f",
    texture: "blocks/panel.png",
    material: "wood",
    renderLayer: "alphatest",
    model: [
        { name: "art", box: [1, 1, 7, 14, 14, 0] }
    ],
    collision: false
});

// ===========================================================================
// 4) 魔书台 —— 3D 多部件（底座 + 四条腿 + 发光面 + 歪着的书）
// ===========================================================================
var ALTAR = Block.defineBlock(203, {
    name: "\u9b54\u4e66\u53f0",
    texture: "blocks/altar.png",
    material: "rock",
    renderLayer: "alphatest",
    model: [
        { name: "base", box: [0, 0, 0, 16, 3, 16] },
        { name: "leg1", box: [0, 3, 0, 3, 8, 3],   uv: "crop" },
        { name: "leg2", box: [13, 3, 0, 3, 8, 3],  uv: "crop" },
        { name: "leg3", box: [0, 3, 13, 3, 8, 3],  uv: "crop" },
        { name: "leg4", box: [13, 3, 13, 3, 8, 3], uv: "crop" },
        { name: "glow", box: [3, 3, 3, 10, 1, 10], texture: "blocks/glow.png", emissive: true },
        { name: "book", box: [5, 10, 6, 6, 1, 4],  texture: "blocks/book.png",
          pivot: [8, 10, 8], rot: [0, -25, 0] }
    ]
});

// ===========================================================================
// 5) 旋转水晶 / 开合箱 —— 方块动画（anim 每帧给部件姿态）
//    anim(part, age, time) 返回 {x,y,z} 弧度（可选 px,py,pz 格内平移）
// ===========================================================================
var CRYSTAL = Block.defineBlock(204, {
    name: "\u65cb\u8f6c\u6c34\u6676",
    texture: "blocks/crystal.png",
    material: "rock",
    renderLayer: "alphatest",
    light: 10,
    model: [
        { name: "base",    box: [2, 0, 2, 12, 2, 12], texture: "blocks/altar.png" },
        { name: "crystal", box: [6, 4, 6, 4, 8, 4],   emissive: true },
        { name: "ring",    box: [1, 7.5, 1, 14, 0, 14], texture: "blocks/panel.png" }
    ],
    anim: function (part, age, time) {
        if (part === "crystal") return { x: 0, y: time * 1.5, z: 0 };
        if (part === "ring")    return { x: Math.sin(time * 2) * 0.6, y: time * 2.0, z: 0 };
        return null;
    }
});

var BOX = Block.defineBlock(205, {
    name: "\u5f00\u5408\u7bb1",
    texture: "blocks/box_side.png",
    material: "wood",
    renderLayer: "alphatest",
    model: [
        { name: "body", box: [1, 0, 1, 14, 9, 14], texture: "blocks/box_side.png" },
        { name: "lid",  box: [1, 9, 1, 14, 4, 14], texture: "blocks/box_top.png",
          pivot: [8, 9, 1] }
    ],
    anim: function (part, age, time) {
        if (part !== "lid") return null;
        var t = (time % 6) / 3;          // 0..2
        var open = t < 1 ? t : (2 - t);  // 0 -> 1 -> 0
        return { x: open * 1.3, y: 0, z: 0 };
    }
});

// ===========================================================================
// 6) 微方块 —— 一个方块格里摆 4×4×4 个小方块（自定义放置 + 格内几何）
//    这个方块格本身是**隐形**的：不画几何、也不贡献碰撞；外观和碰撞
//    完全来自 geom 里的小方块。第一块靠"点别的方块的面"放进来
//    （目标格是那个空格），之后点在已有小方块上就能继续往同一格里加。
//    micro:true 表示"我的外观来自方块实体数据 geom"，
//    geom 格式： "x y z w h d tex"（像素；tex = -1 用方块自己的贴图，
//    或 Block.injectTexture 拿到的槽号），多个小方块用 '|' 分隔。
// ===========================================================================
var MICRO_N = 4;                    // 每格切成 4×4×4 个小格
// 微方块的两种材质：Block.injectTexture 把 zip 里的 png 塞进 terrain 贴图集，
// 返回槽号 —— geom 里的 tex 字段就是它（同一格里可以混用不同材质的小方块）。
var T_STONE = Block.injectTexture("blocks/slab.png");
var T_WOOD = Block.injectTexture("blocks/micro.png");
var MICRO = Block.defineBlock(206, {
    name: "\u5fae\u65b9\u5757",
    texture: "blocks/micro.png",     // 只用于物品栏里那个图标
    material: "wood",
    renderLayer: "alphatest",
    micro: true                      // 方块格本身不画几何、不留碰撞
});

// 接管放置：点哪就往哪长一小块。
// 算法和原版放方块一致 —— 命中点先落到某个子格上，再沿"你点的那一面"
// 往外推一格；推出本格就挪到相邻格。所以点顶面往上长、点底面往下长、
// 点侧面就往那一侧长（以前只在本格里找位置，所以"下面/左面"点不动）。
function onPlaceAttempt(x, y, z, face, cx, cy, cz, itemId, yRot, pitch) {
    if (itemId !== MICRO) return false;

    var N = MICRO_N;
    // 命中点量化成子格（+1e-4 是为了避开"正好落在面上"的浮点误差）。
    // 边界归属：点在朝外的面（上/南/东）上时，floor 已经跳到外面那一格，
    // 不用再动；只有朝内的面（下/北/西）的边界值还落在本格内，要缩回一格。
    var ix = Math.floor(cx * N + 1e-4);
    var iy = Math.floor(cy * N + 1e-4);
    var iz = Math.floor(cz * N + 1e-4);
    if (face === 0) iy -= 1;
    else if (face === 2) iz -= 1;
    else if (face === 4) ix -= 1;

    var tx = x, ty = y, tz = z;
    while (ix < 0)  { ix += N; tx--; }
    while (ix >= N) { ix -= N; tx++; }
    while (iy < 0)  { iy += N; ty--; }
    while (iy >= N) { iy -= N; ty++; }
    while (iz < 0)  { iz += N; tz--; }
    while (iz >= N) { iz -= N; tz++; }

    var at = level.getBlock(tx, ty, tz);
    if (at !== 0 && at !== MICRO) return false;    // 那一格被别的方块占了
    if (at === 0) level.setBlock(tx, ty, tz, MICRO);

    var unit = 16 / N;
    var tex = ((ix + iy + iz) % 2) ? T_WOOD : T_STONE;
    var part = (ix * unit) + " " + (iy * unit) + " " + (iz * unit) + " "
             + unit + " " + unit + " " + unit + " " + tex;
    var d = level.getBlockEntityData(tx, ty, tz);
    var cur = (d && d.geom) ? d.geom : "";
    if (cur.indexOf(part) >= 0) return true;       // 这个子格已经有了
    level.setBlockEntityData(tx, ty, tz, "geom", cur ? (cur + "|" + part) : part);
    modLog("微方块: " + tx + "," + ty + "," + tz + " 子格 " + ix + "," + iy + "," + iz);
    return true;
}

// 挖掉单个小方块：点中哪一块就删哪一块（返回 true = 引擎不拆整个方块）。
// 注：modLog() 是模组的日志接口（写进模组的日志文件，每秒限流 20 条）；
// 想排查问题打它就行，不用改游戏本体。
var _lastBreakTick = -9999;
function onBreakAttempt(x, y, z, face, cx, cy, cz) {
    if (level.getBlock(x, y, z) !== MICRO) return false;
    var d = level.getBlockEntityData(x, y, z);
    var geom = (d && d.geom) ? d.geom : "";
    if (!geom) return false;

    var t = level.getTicks();
    if (t - _lastBreakTick < 1) return true;   // 同一游戏刻只挖一块

    var hx = cx * 16, hy = cy * 16, hz = cz * 16;
    var list = geom.split("|");
    var hitIdx = -1, nearIdx = -1, nearD = 1e9;
    for (var i = 0; i < list.length; i++) {
        var f = list[i].split(" ");
        if (f.length < 6) continue;
        var px = parseFloat(f[0]), py = parseFloat(f[1]), pz = parseFloat(f[2]);
        var w = parseFloat(f[3]), h = parseFloat(f[4]), dep = parseFloat(f[5]);
        var cxx = px + w / 2, cyy = py + h / 2, czz = pz + dep / 2;
        var dx = cxx - hx, dy = cyy - hy, dz = czz - hz;
        var dd = dx * dx + dy * dy + dz * dz;
        if (dd < nearD) { nearD = dd; nearIdx = i; }
        if (hitIdx < 0 && hx >= px && hx <= px + w && hy >= py && hy <= py + h &&
            hz >= pz && hz <= pz + dep)
            hitIdx = i;
    }
    var target = (hitIdx >= 0) ? hitIdx : nearIdx;   // 没精确命中就挖最近那块
    if (target < 0) return true;

    var kept = [];
    for (var i = 0; i < list.length; i++)
        if (i !== target) kept.push(list[i]);

    _lastBreakTick = t;
    if (kept.length === 0) {
        level.setBlock(x, y, z, 0);                  // 挖空了：连容器一起清掉
    } else {
        level.setBlockEntityData(x, y, z, "geom", kept.join("|"));
    }
    return true;
}

function onJoinWorld() {
    player.sendMessage("\u65b9\u5757 API \u6d4b\u8bd5\u6a21\u7ec4\u5df2\u52a0\u8f7d\uff1a"
                       + "\u7ad6\u534a\u7816(\u5bf9\u7740\u54ea\u9762\u653e\u5c31\u8d34\u54ea\u9762) / "
                       + "\u8d34\u5730\u7ebf(\u52a0\u4e00\u4e2a\u53d8\u4e00\u79cd\u5f62\u72b6) / "
                       + "\u8d34\u753b\u677f / \u9b54\u4e66\u53f0 / \u65cb\u8f6c\u6c34\u6676 / "
                       + "\u5f00\u5408\u7bb1 / \u5fae\u65b9\u5757(\u53cd\u590d\u70b9\u540c\u4e00\u683c\u53e0\u5c0f\u65b9\u5757)");
    player.sendMessage("\u53e6\u5916\uff1a\u539f\u7248\u753b\u73b0\u5728\u5de6\u53f3\u4e24\u9762\u5404\u80fd\u6302\u4e00\u5e45\uff1b"
                       + "\u300c\u53cc\u753b\u6302\u9970\u300d\u70b9\u4e00\u4e0b\u4f1a\u4e00\u6b21\u6302\u4e24\u5e45\u753b\uff1b"
                       + "\u5de6\u952e\u70b9\u5fae\u65b9\u5757\u53ef\u4ee5\u4e00\u5757\u4e00\u5757\u6316\u6389");
}

// ===========================================================================
// 7) 双画挂饰 —— 一次挂两幅画（自定义放置 + 悬挂实体 API）
//    原版画现在也能左右两面各挂一幅了（引擎放宽了“一格只能一幅”的限制）；
//    这个物品演示的是“模组完全自己决定放什么、放哪、放几幅”。
// ===========================================================================
var PAINTING_WAND = Item.defineItem(240, {
    name: "\u53cc\u753b\u6302\u9970",
    icon: "blocks/panel.png",
    onUse: function (x, y, z, face) {
        // dir: 0=\u5357 1=\u897f 2=\u5317 3=\u4e1c（画贴在被点方块的这一面）
        // 先试一对相对的方向（南+北），挂不上再试另一对（西+东）。
        var pairs = [[0, 2], [1, 3]];
        var ok = 0;
        for (var p = 0; p < pairs.length && ok === 0; p++) {
            for (var i = 0; i < 2; i++) {
                if (level.spawnPainting(x, y, z, pairs[p][i], "Kebab") >= 0)
                    ok++;
            }
        }
        player.sendMessage("\u53cc\u753b\u6302\u9970\uff1a\u6302\u4e0a\u4e86 " + ok
                           + " \u5e45\u753b\uff08level.spawnPainting \u4e00\u6b21\u591a\u5e45\uff09");
        return true;
    }
});
"""


def main():
    tmp = os.path.join(HERE, "_blockapi_tmp")
    made = make_textures(tmp)

    zpath = os.path.join(MODS, "blockapi_test.zip")
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
        for rel in sorted(made.keys()):
            z.write(made[rel], rel)
        z.writestr("main.js", MAIN_JS.encode("utf-8"))
    print("wrote", zpath, os.path.getsize(zpath), "bytes")
    print("entries:", sorted(zipfile.ZipFile(zpath).namelist()))

    # 加进 modlist.json 的 enabled（测试用；不想要可以自己取消勾选）
    mpath = os.path.join(MODS, "modlist.json")
    try:
        with io.open(mpath, "r", encoding="utf-8") as f:
            cfg = json.load(f)
    except Exception:
        cfg = {"enabled": []}
    if "blockapi_test.zip" not in cfg.get("enabled", []):
        cfg.setdefault("enabled", []).append("blockapi_test.zip")
        with io.open(mpath, "w", encoding="utf-8") as f:
            f.write(json.dumps(cfg, ensure_ascii=False, indent=2))
        print("enabled blockapi_test.zip in modlist.json")


if __name__ == "__main__":
    main()
