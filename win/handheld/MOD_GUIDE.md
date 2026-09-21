# Minecraft PE 0.6.1 Win32 模组开发指南

一个模组 = 一个 **zip 包**，内含 `main.js`（入口脚本）+ 资源文件（PNG 贴图、WAV 音效）。把 zip 放进 `mods/` 目录，在游戏"设置 → 模组"里启用即可。支持同时启用多个模组（事件分发互不干扰）。

---

## 1. 模组结构

```
my_mod.zip
├── main.js              # 必填，模组入口
├── mob/golem.png        # 自定义实体贴图（相对路径引用）
├── particles/spark.png  # 自定义粒子贴图
├── blocks/ore.png       # 自定义方块贴图
└── sounds/boom.wav      # 自定义音效（自动注册，文件名即声音名）
```

`main.js` 顶部用注释写元数据（模组页面会显示）：

```js
// name: 我的模组
// author: 你
// version: 1.0.0
// description: 这是做什么的
```

---

## 2. 事件

在 `main.js` 里定义**全局函数**即注册事件。模组加载后自动注册，多个模组同名事件会全部执行。

| 事件 | 参数 | 触发时机 |
|---|---|---|
| `onTick()` | 无 | 每帧（游戏运行中） |
| `onChat(msg)` | `msg` 字符串 | 玩家发消息/输入指令 |
| `onJoinWorld()` | 无 | 进入世界 |
| `onBreakBlock(x, y, z, id)` | 方块坐标+id | 玩家破坏方块 |
| `onPlaceBlock(x, y, z, id)` | 方块坐标+id | 玩家放置方块 |
| `onMobTick(entityId)` | 实体 id | 每个生物每帧 |
| `onGuiRender()` | 无 | 每帧（可画 HUD） |
| `onJump(entityId)` | 实体 id | 任意生物跳跃 |
| `onMobJump(entityId)` | 实体 id | 任意生物跳跃（同 onJump） |
| `onAttack(victimId)` | 被攻击实体 id | 攻击命中瞬间 |
| `onEatFood(itemId, nutrition)` | 物品 id + 饱食度 | 吃东西 |
| `onUseItem(itemId)` | 物品 id | 使用物品 |
| `onPlayerTick(entityId)` | 玩家实体 id | 每个玩家实体每 tick 一次（服务器/房主能查到所有玩家；客户端只有自己） |

---

## 3. 全局对象

### 3.1 `level`（世界操作）

```js
level.getBlock(x, y, z)          // 获取方块 id
level.getData(x, y, z)           // 获取方块数据值
level.setBlock(x, y, z, id[, data])
level.getTime()                  // 世界时间（tick）
level.setTime(t)
level.getDifficulty()            // 0和平 1简单 2普通 3困难
level.spawnMob(typeId, x, y, z)  // 生成自定义生物（Mob.defineMob 注册的）
level.spawnVanilla(name, x, y, z) // 生成原版生物（zombie/cow/creeper...）
level.spawnPainting(x, y, z, dir[, motive]) // 挂一幅画（见 4.2.3；dir 0=南1=西2=北3=东）
level.spawnParticle(name, x, y, z[, xd, yd, zd])
level.getTicks()                    // 游戏刻计数（每 tick +1）：计时/节流用这个
level.getTime() / setTime(t)        // 存档里的世界时间 —— 基本不变，别拿来计时
modLog(text)                        // 模组的日志：写进模组日志文件，每秒限流 20 条
level.playSound(name, x, y, z[, volume, pitch])  // 播放声音（原版或自定义）
level.setModState(key, value)    // 模组状态持久化（保存到 mods/<存档>.state）
level.getModState(key)
level.saveAll()                  // 保存所有区块
level.resetChunks(dimId)         // 进入某个维度（=另一个世界；配合 Dimension.define）
level.getCurrentDimension()
level.saveDimensionState(dimId)
```

### 3.2 `player`（玩家）

```js
player.getX(); player.getY(); player.getZ()
player.getXRot(); player.getYRot()   // 俯仰角 / 偏航角
player.getHealth(); player.setHealth(h)
player.damage(n)                     // 扣血
player.teleport(x, y, z)
player.sendMessage(text)             // 聊天栏消息
player.isCreative()
player.setSprinting(bool)            // 疾跑状态
player.isForwardHeld()               // 是否按住前进（键盘 W / 触屏）
```

> `player` 永远指**你自己**。要找别的玩家、或给玩家加动作，见 **3.5 `Player`**。

### 3.3 `mob`（实体句柄，配合 onMobTick/onAttack 的实体 id）

```js
mob.getPos(entityId)     // 返回 [x, y, z]
mob.setPos(id, x, y, z)
mob.getHealth(id); mob.getMaxHealth(id)
mob.remove(id)           // 移除实体
mob.setRot(id, yaw, pitch)
mob.getXRot(id); mob.getYRot(id)
```

### 3.4 `ui`（HUD 绘制，只能在 onGuiRender 里用）

```js
ui.drawText(x, y, text, color)      // color = 0xAARRGGBB
ui.drawShadow(x, y, text, color)    // 带阴影
ui.drawImage(x, y, w, h, "path.png") // 画 zip 内图片
ui.fillRect(x, y, w, h, color)
ui.getWidth(); ui.getHeight(); ui.getScale()  // 屏幕尺寸/缩放（UI 自适应用）
ui.showLoading(text); ui.hideLoading()        // 全屏加载遮罩
```

### 3.5 `Player`（按 id 找玩家 / 给玩家加动作）

`player`（小写）指**你自己**；`Player`（大写）是“按 id 找玩家 + 给玩家加动作”，
和 `mob` 那套对称 —— 玩家也有 id（`Player.self().id`）。

```js
Player.self()                 // 我自己（返回对象；纯服务器上没有本地玩家 -> null）
Player.list()                 // 玩家数组。单机/客户端：只有自己；
                              // 服务器/房主：自己 + 每个连进来的玩家
Player.get(id)                // 按 id 找玩家；找不到返回 null

// 玩家对象字段（快照，改它不影响游戏）：
//   id / name / x / y / z / yRot / xRot / health / dim / action
//   isInWater / isSneaking / onGround / isSitting / local
```

动作（给玩家加姿态，**按玩家分别** —— 一个人在游泳不影响别人）：

```js
// 注册动作：fn(part, age, time, firstPerson) -> {x,y,z[,px,py,pz]} | null
//   part: "head" | "body" | "arm0"（右臂）| "arm1"（左臂）| "leg0" | "leg1"
//   age : 这个玩家已存在的 tick 数（做循环动画用）
//   firstPerson : true = 正在画第一人称那只手（可分开写，免得抬手挡住视线）
//   返回 null = 这个部件走原版姿态（走路、抬头、挥手都还在）
//   旋转单位是弧度：1.57 ≈ 90°，3.14 ≈ 180°
//   可选位置：px/py/pz = 部件位置（模型格）。只有"要整体挪部件"的姿态
//     （比如躺平游泳）才用得上 —— 只转角度是挪不动的。
//     模型空间：y 向下、-z 是前方、1 格 = 1/16 个方块
Player.defineAction("swim", function(part, age, time, firstPerson) {
    var s = Math.sin(age * 0.3);
    if (part === "arm0") return { x: -1.6 + s * 0.7, y: 0, z:  0.35 };
    if (part === "arm1") return { x: -1.6 - s * 0.7, y: 0, z: -0.35 };
    if (part === "leg0") return { x:  s * 0.6 };
    if (part === "leg1") return { x: -s * 0.6 };
    return null;                 // 头和身体不动
});

Player.setAction(id, "swim")     // 给这个玩家挂上
Player.setAction(id, null)       // 取消（回原版姿态）
Player.getAction(id)             // 当前动作名（没有 = null）
```

- 联机时**自动同步**：房主/服务器广播，连服务器的客户端上报 —— 同一世界的人看得见你的动作。
- **第三人称身体 + 第一人称手臂都生效**（背包/主菜单的纸娃娃预览不受影响）。

配合事件系统就是“什么状况就是什么动作”：

```js
function onPlayerTick(id) {
    var p = Player.get(id);
    if (!p) return;
    if (p.isInWater && !p.isSitting) Player.setAction(id, "swim");
    else                             Player.setAction(id, null);
}
```

> 注意：`Player.get(id)` 查不到的玩家（客户端上看不到远程玩家的状态），**先 `return` 再动手** ——
> 别无条件对拿到的 id 调 `setAction`。

完整例子：`handheld/src/mod/demos/pose_demo/main.js`（也打成了 `mods/pose_demo.zip`）。

### 3.6 换外形 `Player.setModel`（把玩家变成别的样子）

外形用 `Mob.defineMob` **登记**一下就行 —— **不用真的生成那只生物**（不占地方、不刷怪）：

```js
Mob.defineMob(40, {
    name: "僵尸",
    texture: "mob/zombie.png",     // 原版贴图路径，或自己 zip 里的 png
    sizeW: 0.6, sizeH: 1.8,        // 碰撞箱：变身之后就是这个尺寸
    model: [ { name:"head", tex:[0,0], box:[-4,-8,-4, 8,8,8], pos:[0,0,0] }, ... ]
});

Player.setModel(Player.self(), 40);    // 穿上，变成它
Player.setModel(Player.self(), null);  // 脱掉，变回玩家自己
Player.getModel(Player.self());        // 当前外形编号（没有 = null）
```

- **体型是真的跟着变**：外形里写多大就是多大 —— 变成大个子就真的过不了矮门、
  被打中的高度也跟着变。（所以别在矮洞里变身，会卡住；走出来就好。）
- 联机时自动同步：同一世界的人看到的样子和大小都一致。
- **第一人称照样画这个模型本身**（摄像机就在眼睛那儿，看到什么就是什么）：
  头会自动藏起来（否则摄像机会卡在头里面，屏幕糊满），身体和手臂照常画 ——
  外形的手臂伸在前面，第一人称就看得见。
- 外形是**每帧临时换上、画完还原**，不影响其他玩家、怪物、背包/主菜单的纸娃娃。
- 玩家穿着的外形**自动获得人形自然动作**（走路摆臂/摆腿/抬头）——
  前提是外形里的部件名跟人形一致（`head`/`body`/`arm0`/`arm1`/`leg0`/`leg1`）。
  你在 `anim` 里返回了的部件归你接管（比如僵尸的前伸手臂），返回 `null` 的部件
  继续走人形动作；**玩家动作（3.5 节）也会套在外形上** —— 变了身照样能游泳/挥手。

### 3.7 op 名单 `level.isOp / addOp / removeOp / listOps`

**每个世界一份**，存在世界目录里的 `ops.txt`（一行一个名字），跟着存档走 ——
原版游戏完全不用它，纯粹给模组判断"谁能做什么"（比如只有 op 能用的命令/操作）。

```js
level.isOp()               // 我自己是不是 op
level.isOp("小明")          // 这个名字是不是 op
level.listOps()            // ["小明", "小红"]
level.addOp("小明")         // 加（立刻写盘）
level.removeOp("小明")      // 去掉（立刻写盘）
```

- 名单为空时，**第一个进这个世界的玩家自动成为 op**（否则模组没法判权限）。
  也可以直接编辑世界目录里的 `ops.txt` 手工写名字（`#` 开头的行忽略）。
- 文件位置：`games/com.mojang/minecraftWorlds/<世界名>/ops.txt`
- 联机（连别人的服务器）时名单只在内存里 —— 那种情况权限该由服务器定。

游戏里也能直接改（不用翻文件），三个内置命令：

| 命令 | 作用 |
|---|---|
| `/op <名字>` | 把某个玩家设为本世界的 op |
| `/deop <名字>` | 去掉某人的 op |
| `/ops` | 看当前名单 |

- 只有**已经在名单里的人**能用这三个（不然谁都能给自己上 op）。
- 单机 / 自己当房主时本地执行；**连别人的服务器**时整行发给服务器，由服务器处理。
- ⚠️ 这三个命令只是**接进现有命令链**（`ChatInputScreen::handleCommand` 里多一个分支），
  **没有另做一套命令系统**：模组的 `onChat` 仍然在 `handleCommand` **之前**先拿到整行
  （见 `submit()`），所以别的模组的 `/xxx` 命令不会被这里挡掉。

---

## 4. 注册 API

### 4.1 自定义生物 `Mob.defineMob`

```js
Mob.defineMob(40, {
    name: "石头巨人",
    texture: "mob/golem.png",
    health: 100,
    sizeW: 0.9, sizeH: 2.5,
    scale: 1.5,            // 模型放大
    boxes: [               // 模型盒（贴图坐标）
        { x: -9, y: 0, z: -6, w: 18, h: 32, d: 12, tx: 0, ty: 0, tw: 18, th: 32 }
    ],
    anim: function(part, tick, time) {  // 可选：动画回调
        // 返回这个部件要覆盖的旋转（弧度）；返回 null = 这个部件不改
        return { z: Math.sin(time * 2) * 0.35 };
    }
});
```

- 用 `/summon 40` 或 `level.spawnMob(40, x, y, z)` 生成
- 贴图放在 zip 里，路径相对 zip 根

### 4.2 自定义方块 `Block.defineBlock`

```js
// ★ v7：返回真实物理 id（id 被占时自动顺延到空闲槽），后续操作用返回值
var MY_BLOCK = Block.defineBlock(100, {
    name: "红石水晶",
    texture: "blocks/crystal.png",   // 或 texture: 6（原版石头纹理）
    material: "stone",
    light: 15                        // v7 新增：发光亮度 0~15（0=不发光）
});
```

- 方块 id 1~255；**v7 起 id 自分配**：被占时自动顺延并返回真实 id，两模组写
  相同 id 不再冲突（用返回值操作）
- `light: 0~15` 发光方块（15=萤石/火把级，8=半亮），放置后自带发光照亮周围
- 自动生成物品（背包/创造栏可见）；贴图注入 terrain 图集
- **显示名**：`name` 会注册成这个方块的翻译名（物品栏 / 掉落物 / 聊天里显示的就是它）。
  运行时改名用 `Block.setName(id, "新名字")` —— id 传 `defineBlock` 的返回值或你写的
  逻辑 id 都行：

```js
var MINE = Block.defineBlock(100, { name: "水晶", texture: "blocks/x.png" });
Block.setName(MINE, "发光水晶");     // 之后物品栏里就叫这个
```

### 4.2.1 自定义方块模型（3D 任意几何 / 2D 贴面贴地）

方块不必是正方体了。`Block.defineBlock` 的这三个字段随便挑一个用：

```js
// ① 单盒子：box:[x, y, z, 宽, 高, 深]（像素，16 = 一整格）
box: [0, 0, 0, 16, 8, 16]      // 半砖高度的盒子

// ② 只要高度：height: 0.5 等价于上面的 box
height: 0.5

// ③ 多部件模型（附魔台那种）：一个数组，每一项是一只盒子
model: [
    { name: "base",  box: [0, 0, 0, 16, 3, 16] },                  // 底座
    { name: "leg",   box: [0, 3, 0, 3, 8, 3], uv: "crop" },         // 腿
    { name: "glow",  box: [3, 3, 3, 10, 1, 10], emissive: true },   // 自发光面
    { name: "book",  box: [5, 10, 6, 6, 1, 4], texture: "blocks/book.png",
      pivot: [8, 10, 8], rot: [0, -25, 0] }                         // 转 25 度的书
]
```

一个部件的字段：

| 字段 | 含义 |
|---|---|
| `name` | 部件名（动画回调按名字认部件） |
| `box` | `[x, y, z, 宽, 高, 深]`，像素单位，16 = 一格；坐标是格子内的，从西/下/北边界量起 |
| `texture` | 这一个部件用的贴图：zip 里的 png 路径，或原版 terrain 槽号数字 |
| `faces` | 可选，逐面贴图 `[下,上,北,南,西,东]` |
| `uv` | `"crop"` = 贴图按部件占格比例裁剪（半砖/雪层那种，和邻居拼得上）；不写 = 整张贴满部件表面 |
| `pivot` | 旋转中心（像素；默认部件中心） |
| `rot` | 静态旋转 `[rx, ry, rz]`，单位**度** |
| `move` | 静态平移 `[x, y, z]`，像素 |
| `emissive` | `true` = 画满亮度（自发光，不受环境光影响） |
| `data` | 只在这个方块的数据值是给定时才画这个部件（`data: 3` 或 `data: [1,2]`）|

**2D（贴地红石线 / 铁轨 / 贴墙图案）**：把某一维的宽度写成 0，这只盒子就退化成
一个平面 —— 只画与该维垂直的两个面（正反两面都画）。例如：

```js
{ name: "wire", box: [0, 0.5, 6, 16, 0, 4] }   // 高 0 = 贴着地面的东西向细线
{ name: "art",  box: [1, 1, 7, 14, 14, 0] }    // 深 0 = 贴在 z 方向的一幅画
```

**按数据值换形状**：一个方块可以按自己的数据值（0~15）画不同部件，比如竖半砖的
6 个朝向就是一串各带 `data` 的部件：

```js
model: [
    { name: "d0", box: [0, 0, 0, 16, 8, 16],  data: 0 },   // 下半
    { name: "d1", box: [0, 8, 0, 16, 8, 16],  data: 1 },   // 上半
    ...
]
```

其他要点：
- **碰撞箱自动跟着模型走**（按非平面部件的盒子生成）；想让它能穿过去写
  `collision: false`（贴地的线/薄片用），或者用 `material: "plant"`。
- 带几何但没写 `renderLayer` 时默认走 **alphatest** 层（模型贴图常带透明区，
  放不透明层会被画成黑块）。
- 手里拿着/背包里/掉在地上也按 3D 模型画。

### 4.2.2 方块动画（箱子开盖那种）

给方块一个 `anim` 回调，它**每一帧**会被问一次"这个部件现在是什么姿态"：

```js
anim: function (part, age, time) {
    // part = 部件名（上面的 name）；age = 世界时间 tick；time = 世界时间（秒）
    if (part === "lid")  return { x: Math.sin(time) * 0.8, y: 0, z: 0 };   // 弧度
    if (part === "ring") return { x: 0, y: time * 1.5, z: 0 };
    return null;     // 返回 null/不返回 = 这个部件保持静态姿态
}
```

- 返回值 `{x, y, z}` 是**弧度**的旋转增量（叠加在静态 `rot` 上）；还可以给
  `px, py, pz` 覆盖平移（格内单位，1 = 一整格）。
- 动画方块的几何**每帧单独重画**，不写进区块网格（否则姿态会被烤进顶点缓存，
  只有重建区块时才更新一下）。
- 数量上限 512 个（超了会写日志，多余的不动）；同屏动画方块很多时会拖慢帧率。
- 动画只影响外观，不影响碰撞箱。

### 4.2.3 自定义放置（放在哪、放几块、写什么数据值）

**想让方块按"点了哪一面"决定数据值**（竖半砖、楼梯朝向、连接形状），用 `placeData`：

```js
placeData: function (x, y, z, face, clickX, clickY, clickZ, itemValue, yRot, pitch) {
    // face: 0下 1上 2北 3南 4西 5东；clickX/Y/Z: 点击点在被点方块内的位置(0~1)
    // yRot/pitch: 玩家朝向。返回 0~15 的数据值
    return face;
}
```

**想完全接管这次放置**（自己决定放哪个方块、放到哪一格、一次放几格），
定义顶层函数 `onPlaceAttempt`：

```js
function onPlaceAttempt(x, y, z, face, clickX, clickY, clickZ, itemId, yRot, pitch) {
    if (itemId !== MY_ITEM) return false;      // 不接管 -> 走原版逻辑
    level.setBlock(x, y + 1, z, MY_ITEM);      // 自己放
    return true;                                // true = 这次放置我已经处理了
    // 也可以直接返回落格： {x:;y:;z:;id:;data:} 或 [{...}, {...}]，引擎替你放
}
```

- 返回 `false` / 不返回 → 不改行为；
- 返回 `true` → 模组自己处理完了（引擎不再走原版放置）；
- 返回对象或数组 → 引擎按 `{x, y, z, id, data}` 替你放（一次可以放多格）。

**放"画"这类实体**：右键的放置不只是方块 —— 挂画（原版"画"物品）也会先经过
`onPlaceAttempt`，模组可以拦下它。要自己挂画用：

```js
level.spawnPainting(x, y, z, dir[, motive])
//   x/y/z 是格子坐标；dir: 0=南 1=西 2=北 3=东（画贴在这一格的那一面）
//   motive 可选（"Kebab"/"Aztec"/"Pool"…），不写就按空间随机挑一个放得下的
//   返回实体 id；放不下（空间不够/贴不住）返回 -1
```

所以"一次挂多幅""指定画面内容""挂到任意格子"这些都由模组自己说了算：

```js
// 例：在被点方块的左右两面各挂一幅
var dirs = [0, 2];            // 南 + 北（一对相对的方向）
for (var i = 0; i < dirs.length; i++)
    level.spawnPainting(x, y, z, dirs[i], "Kebab");
```

> 顺带说明：原版的画以前"同一格空间里只能挂一幅"，现在**朝向正好相对的两幅可以共存**
> 了（左右各一个方块、中间空一格 → 两面各挂一幅）。同一面重叠仍然不允许。

### 4.2.4 微方块（一个方块格里摆很多小方块）

想要"小方块铺满一片、没有缝"，关键是**一个格子能装多个小方块**。
把方块声明成 `micro: true`，它的外观和碰撞就来自**方块实体数据**里的
`geom` 字段（而不是静态模型）：

```js
var MICRO = Block.defineBlock(210, {
    name: "微方块托盘",
    texture: "blocks/micro.png",
    micro: true,                 // 外观来自 geom 数据
    box: [0, 0, 0, 16, 1, 16],   // 托盘底板：能瞄准、有碰撞
    material: "wood"
});

// 放一个小方块：写入 / 追加 geom
// 格式（像素单位，16 = 一整格；tex 用 -1 = 这个方块自己的贴图）：
//   "x y z 宽 高 深 tex"       多个用 '|' 分隔
level.setBlockEntityData(x, y, z, "geom",
    "0 0 0 4 4 4 -1|4 0 0 4 4 4 -1");

// 想让同一格里的小方块各用不同贴图：Block.injectTexture 返回 terrain 槽号
var T_STONE = Block.injectTexture("blocks/slab.png");
var T_WOOD  = Block.injectTexture("blocks/micro.png");
level.setBlockEntityData(x, y, z, "geom",
    "0 0 0 4 4 4 " + T_STONE + "|4 0 0 4 4 4 " + T_WOOD);
```

- 引擎按同一份 `geom` 自动生成**渲染几何和碰撞箱**（格内的小方块能站、能挡）；
- **方块格本身是隐形的**：`micro:true` 的方块不画自己的几何、也不贡献碰撞，
  外观和碰撞**全部来自格内的小方块**，所以不需要任何"托盘"底板。
  第一块靠"点别的方块的面"放进来（目标格就是那个空格），之后点在已有小方块上
  就能继续往同一格里加；
- 挖单个小方块要自己写 `onBreakAttempt`（见 4.2.5），否则左键会把整格一起拆掉；
- `geom` 存在方块实体的 NBT 里，存档自动保存；
- 落点由模组自己算 —— 引擎给的是精确点击点（0~1），**要量化到自己的网格**
  （比如把每格切成 4×4×4，`Math.floor(clickX * 4)`），否则起点对不齐就会出现
  "最后一条不足一格宽的缝"；
- **点哪长哪**（正确算法）：把命中点量化成子格 `floor(坐标 × N)`，再处理**边界归属** ——
  **朝外的面（上/南/东）不要动**（命中点正好落在面的外边界上，量化结果已经是外面那一层），
  **朝内的面（下/北/西）要缩回一格**；越出本格就把目标格挪到相邻格。
  两种典型错法：只在本格里找位置（点底面/侧面放不下去）、六个面都往外推一格
  （点上/南/东会空掉一层）。参考实现见 `mods/blockapi_test.zip` 的 `onPlaceAttempt`；
- 挖的时候引擎会给你**精确命中点**（`onBreakAttempt` 的 clickX/Y/Z），
  拿它量化出"点到了哪个小方块"；挖空整格时记得把方块本身也清掉
  （`level.setBlock(x, y, z, 0)`），免得留下一个隐形格子；
- 微方块的几何是逐格解析并缓存的，改 `geom` 后下一帧生效。

（现成的一整套示例：`mods/blockapi_test.zip` / 生成脚本 `tools/_make_blockapi_test.py`）

### 4.2.5 自定义破坏（挖掉"格里的一小块"）

引擎在"方块即将被破坏"时会先问模组（顶层函数 `onBreakAttempt`），而且**带上精确命中点**，
模组才知道玩家点在方块内的哪一处：

```js
function onBreakAttempt(x, y, z, face, clickX, clickY, clickZ, yRot, pitch) {
    // clickX/Y/Z: 命中点在格子内的位置 0~1（不是整数格）
    if (level.getBlock(x, y, z) !== MY_MICRO) return false;   // 不接管 -> 走原版
    // ... 按命中点算出手上挖的是哪一小块，改数据 ...
    return true;      // true = 这次破坏我处理了，整个方块保持原样
}
```

- 返回 `true` 时引擎**不会**破坏这个方块（不掉落、不播粒子），后续完全由模组决定；
- 返回 `false` / 不返回 → 原版破坏流程照常（模组只想改一部分就返回 true）；
- 创造模式按住左键会**每 tick** 触发一次，模组自己要节流（示例见 `mods/blockapi_test.zip`
  的微方块实现：每 5 tick 才真正挖掉一块，节流期间照样返回 true）。

### 4.3 自定义粒子 `Particle.define`

```js
Particle.define("spark", {
    texture: "particles/spark.png",
    size: 2.0,
    lifetime: 20,       // tick
    gravity: 0.0,
    color: [1.0, 0.5, 0.1]  // 可选：染色
});
// 用 level.spawnParticle("spark", x, y, z, vx, vy, vz) 生成
```

### 4.4 自定义维度 `Dimension.define`

编号**也可以省略**：`Dimension.define({ name: "水晶世界", ... })` 会让引擎从 11 起
自动挑一个没被占用的维度编号（主世界占着 `0` 与 `10`，模组维度不碰它们）。

> ⚠️ **维度编号会写进存档目录**（`<世界>/dim<编号>/`）。自动编号是"按当前已注册的维度"
> 算出来的 —— 模组加载顺序变了、或某个模组被禁用，编号就会**挪位**，旧存档里的维度跟着
> **错位**（原来的 dim11 变成另一个模组的世界）。所以：**在意存档的模组请显式写死编号**
> （像天境那样指定 `20`）；自动编号只适合测试、或不在乎已有存档的场景。（群系没这个问题：
> 群系不进存档，按世界种子实时算。）

```js
Dimension.define(20, {
    name: "水晶世界",          // 维度显示名
    clouds: false,             // 是否有云（默认 true）
    blocks: {                  // 地表方块（默认 grass:2 dirt:3 stone:1）
        grass: 2, dirt: 3, stone: 1
    },
    getHeight: function(x, z) { return 64; },   // 地形高度函数（超平坦=常量）
    getBottom: function(x, z) { return 40; },   // 可选：悬浮地形下界（0=实心到底）
    biome: function(x, z) { return 群系编号; },  // 可选：这个维度自己的群系分布（见 §4.4.1）
    onChunkGenerate: function(cx, cz, setBlock) { /* 可选：装饰/结构（区块内局部坐标） */ }
});
// 配合 level.resetChunks(20) 进入、level.getCurrentDimension() 查询
```

**每个维度就是另一个世界**：它有自己独立的存档，放在主世界文件夹下的
`dim<编号>/` 子目录里（自带 `level.dat`、区块、玩家数据），和主世界互不干扰，
也不会出现在世界列表里（不能从列表直接进）。

`level.resetChunks(dimId)` 就是"进入那个世界"：引擎会先保存当前世界，再在**后台**
加载目标维度并显示加载界面（和"进入世界"完全一样），玩家留在原坐标 ——
不会再同步预生成一大片区块，所以切维度不卡。

### 4.4.1 自定义群系 `Biome.define`

```js
var id = Biome.define({           // 编号可省 —— 引擎自动挑一个没被占用的
    name: "Hell Volcano",
    temperature: 2.0, downfall: 0.0,  // 温度/降水（影响雪线、天空色）
    skyColor: 0x6a1a10,               // 可选：这个群系的天空色（不给就按温度算）
    topMaterial: 87,                  // 表层方块（默认 2 草）
    material: 87,                     // 填充方块（默认 3 泥土）
    surfaceDepth: 40,                 // 可选：表层往下铺多少格（不给=原版公式 3~6）
    heightScale: 1.7,                 // 可选：地形起伏倍率（1=原版，>1 更容易出山）
    heightBias: 0.4,                  // 可选：整体高度偏移（0=原版）
    lavaLakes: 12,                    // 可选：每区块 1/N 概率生成岩浆湖（不给=不生成）
    fogColor: 0x3a0f0f,               // 可选：雾色（不给=用维度雾色）
    grassColor: 0x7a6a3a,             // 可选：草方块/草丛颜色（原版硬编码 0x339933）
    foliageColor: 0x6a5a2a,           // 可选：树叶颜色（不给=原版按树叶种类三色）
    snow: true,                       // 可选：雪原（结冰 + 积雪）；false = 强制不雪
    waterFogColor: 0x3a1010,          // 可选：水下雾/清晰色（不给=原版暗蓝）
    tree: "none",                     // 可选：树种 none|oak|birch|pine|spruce
    trees: 0, grass: 0, flowers: 0,   // 可选：树/草丛/花/蘑菇/甘蔗/仙人掌数量
    mushrooms: 0, reeds: 0, cactus: 0,//   （不给 = 模组群系不长这些；原版群系不受影响）
    spawnYMin: 30, spawnYMax: 100,    // 可选：这个世界刷怪的高度带（不给=0~128）
    monsterLightMax: 7,               // 可选：怪物刷出的最大亮度（不给=原版随机判据）
    creatureProbability: 0.02,        // 可选：区块生成时刷动物的概率（原版 0.08）
    spawns: {                         // 不给 = 沿用原版那套默认怪
        monster:  [{ mob: 36, weight: 12, min: 2, max: 4 }],   // 36 = 僵尸猪人
        creature: [],
        water:    []
    },
    getHeight: function(x, z) { return 70; },   // 可选：这一列的地面高度
    decorate:  function(x, z) { /* 可选：自己装饰（放了就不长原版树/花/草） */ }
});

Biome.distribution(0, function(x, z) {          // 挂在哪个维度（0=主世界）
    return (x > 1000) ? id : 0;                 // 返回 0 = 该坐标用原版群系
});
```

- **编号自动分配**：`Biome.define(配置)` 省略编号时，引擎取"已用最大编号 +1"
  并跳过被占用的号。**原版 11 个群系固定占 1~11**（Rainforest=1 … Tundra=11），
  所以模组群系从 12 起。显式写编号也行（`: Biome.define(20, {...})`），但撞号会
  报错 —— 编号是模组认自己群系的凭据（分布回调按它返回），不静默挪号。
- `Biome.define` **返回真实编号**，后续都用它。
- **群系是按种子实时算的，不进存档**：改群系定义不影响旧存档能不能开，
  但**已经生成过的区块不会变**，只有新生成的区域才按新规则。
- 分布回调逐格调用很贵，引擎按 **4 格对齐缓存**，所以只在每个群系单元进一次 JS；
  没挂过分布的维度完全不进 JS（和以前逐格一致）。
- 群系是个**真的 Biome 子类实例**：地表方块、天空色、刷怪表、地形起伏全都自动跟着走；
  `getMobsAt`（世界刷怪）、`Level::getSkyColor`、`BuildSurfaces` 都不用模组自己接管。
- 想自己写地形形状用 `getHeight(x, z)`（返回该列地面高度，类似维度生成的 `getHeight`）；
  想自己放树/建筑用 `decorate(x, z)`（用了它，这个区块的原版树/花/蘑菇/甘蔗/仙人掌就不长，
  矿物与泉水照旧）。
- **颜色**：`grassColor`（草方块与草丛）、`foliageColor`（树叶）、`fogColor`（雾，按相机所在
  群系混合，昼夜曲线照旧）。原版群系这些值 = 改动前的硬编码，所以原版外观一格不变。
- **植被**：`tree` 选树种，`trees/grass/flowers/mushrooms/reeds/cactus` 给数量。
  模组群系**默认不长**原版植被（给数量才长）；原版群系的这些字段是 -1 = 原版那套，不受影响。
  顺便说一句：引擎里草丛那段代码本来就是注释掉的死代码（原版世界从来不生成草丛），
  现在只有群系给了 `grass` 才会长。
- **刷怪规则**：`spawns` 给表格（哪些生物、权重、数量）之外，还能用 `spawnYMin/spawnYMax`
  限定刷怪高度带、`monsterLightMax` 限定怪物刷出的最大亮度、`creatureProbability` 改
  区块生成时刷动物的概率。原版群系一律走原版规则。
- **雪/冰**：`snow: true` 让这个群系结冰 + 铺顶雪（雪线不再看噪声温度）；`false` 强制不雪。
  原版群系照旧（引擎里那两处判则本来就恒不成立）。
- **水下色**：`waterFogColor` 改水下的清晰色/雾色（默认 = 原来的硬编码暗蓝）。
- **脚本维度也有群系**：`Biome.distribution(维度编号, fn)` 对任何维度都有效（脚本维度
  同样有 BiomeSource）。脚本维度以前恒不刷怪，现在"该坐标的群系是模组定义的"就按群系
  刷怪表刷 —— 天境那类没定义群系的老模组行为不变。

查询：

```js
Biome.list();          // [{id, name, mod}] —— 含原版 11 个（mod=false）
Biome.get(编号或名字);  // 单条详情：{id, name, mod, temperature, downfall, topMaterial, material, grassColor, foliageColor, fogColor}；没有 = null
level.getBiome(x, z);  // {id, name, mod}   —— 这个坐标实际是哪个群系
level.findBiome(编号或名字 [, x, z [, 搜索半径]]);   // {x, z} 或 null（没找到）
Biome.undistribution(维度);   // 取消某维度的分布回调
Biome.clearCache();           // 分布状态变了（比如开关一个分布）后调它
```

**跨群系事件**（客户端本地玩家，引擎每 tick 检查一次）：

```js
function onBiomeEnter(id, oldId) { ... }   // 踏进一个新群系（oldId = 刚离开的，0 = 无）
function onBiomeLeave(id) { ... }           // 离开旧群系
```

### 4.5 自定义物品 `Item.defineItem`

```js
Item.defineItem(210, {
    name: "魔法宝石",
    icon: "items/gem.png",        // 或 icon: [3, 4]（items.png 网格）
    onUse: function(x, y, z, face) { ... },  // 可选：右键回调
    type: "weapon",               // 可选：item|weapon|pickaxe|axe|shovel|armor
    tier: "emerald"               // 可选：工具/武器材质
});
```

- 物品号范围 **1~255**（方块/物品共用同一张 id 表）。号被占用时引擎会自动向上找
  空闲号（和 `Block.defineBlock` 一样），`Item.defineItem` **返回真实物品号**，
  后续注册配方/生成物品都用返回值。

### 4.6 合成配方 `Recipes`

```js
Recipes.addShapedRecipe(itemId, count, ["ABA","BCB","ABA"], {A:1, B:5});
Recipes.addShapelessRecipe(itemId, count, [1, 1, 5]);
```

---

## 5. 全局函数

```js
modLog("文本");            // 写模组日志（mcpe_mod.log）
setTimeout(fn, ms);        // 定时执行一次
setInterval(fn, ms);       // 定时循环
clearTimeout(handle);
modRequire("other.js");    // 加载 zip 内另一个 js（共享全局）
```

---

## 6. 自定义声音

把 **PCM WAV** 放进 zip 的 `sounds/` 目录（如 `sounds/boom.wav`），自动注册，**文件名即声音名**：

```js
level.playSound("boom", x, y, z, 1.0, 1.0);
```

转换示例：`ffmpeg -i in.mp3 -ar 22050 -ac 1 -sample_fmt s16 out.wav`

---

## 7. 完整示例

```js
// name: 测试模组
// author: 你
// version: 1.0.0

modLog("模组加载了!");

function onJoinWorld() {
    player.sendMessage("欢迎来到世界!");
}

function onChat(msg) {
    if (msg == "/summon") {
        level.spawnVanilla("zombie", player.getX()+2, player.getY(), player.getZ());
        level.playSound("random.explode", player.getX(), player.getY(), player.getZ(), 1, 0.8);
        player.sendMessage("生成了一只僵尸!");
    }
}

function onGuiRender() {
    ui.drawText(10, 10, "你好!", 0xffffffff);
}
```

---

## 8. 调试

- 模组日志：`C:\Users\<你>\AppData\Local\Temp\mcpe_mod.log`
- **`modLog(text)` = 模组自己的日志接口**：随打随写这个文件，不用改游戏本体。
  为防止模组在循环里刷爆磁盘，**每秒最多记 20 条**（超出的直接丢弃）。
  ```js
  modLog("微方块: 放在 " + x + "," + y + "," + z + " 子格 " + ix + "," + iy + "," + iz);
  ```
  想只在异常时记，就自己加条件：`if (bad) modLog("出问题了: " + 细节);`
- JS 报错会写进日志（`JS error in 模组名: ...`）
- 计时/节流请用 **`level.getTicks()`**（游戏刻计数，每 tick +1）；
  `level.getTime()` 是存档里的世界时间，**基本不变**，拿它计时会永远进不去分支。
- 改完 zip 重新进游戏生效（无需重启游戏本体）
