# MCCE —— 多平台移植 + 脚本模组系统

> **MCCE** = MinecraftPE Community Edition

Minecraft Pocket Edition 的 C++ 引擎，从 **0.6.1 的泄露源码**起步，UI 层后来换成了 **0.8.1** 的界面。
现在能跑在 **Windows / Android / 独立服务器**三个平台上 —— 三端共用同一套引擎源码，
也共用同一套**脚本模组系统**。

```
android/     Android 版（NativeActivity + GLES 1.1），含 minecraftpe-debug.apk
win/         Windows 版（Win32 + OpenGL），含 MinecraftWin32_GL.exe
server/      独立服务器版，含 MinecraftPE-Sever.exe
```

血统：0.6.1 泄露源码 → 换上 0.8.1 界面 → 有人移植成 Windows 版 → 扩展到 Android 与独立服务器 → 加上模组系统。

---

## 模组系统

### 它是什么

一个嵌在引擎里的 JavaScript 运行时（Duktape）。**模组就是一个 zip**，丢进 `mods/` 就能改游戏：
加物品、加方块、加生物、注册命令、做自己的界面、监听游戏事件 —— 全程不碰 C++、不用重新编译，
客户端和服务器跑的是同一套 API。

### 模组长什么样

```
mymod.zip
├── main.js              入口，引擎启动时执行
├── blocks/ruby.png      贴图、音效，随便放
└── sounds/dig.ogg
```

`mods/modlist.json` 决定哪些模组启用：

```json
{ "enabled": ["mymod.zip", "another.zip"] }
```

### 能做什么

**① 往游戏里加东西**

```js
// 注册物品（号被别的模组占了会自动往上找空闲号，并把真实号返回给你）
Item.defineItem(400, { name: 'Ruby', icon: 'blocks/ruby.png' });

// 方块、生物、维度、生态域、粒子、投射物
Block.defineBlock(200, { ... });   Block.setState(...);   Block.setName(...);
Block.injectTexture(...);          Mob.defineMob(60, { ... });
Dimension.define('rubyland', { ... });
Biome.define(...);   Biome.distribution(...);   Biome.list();   Biome.get(...);
Particle.define(...);              Projectile.defineProjectile(...);

// 合成表
Recipes.addShapelessRecipe(200, 9, [400]);      // 9 个 400 → 1 个 200
Recipes.addShapedRecipe(...);

// 聊天命令（sender / args 进回调，可选第三个参数是命令简述）
Commands.register('ruby', function (sender, args) { ... }, '发一把红宝石');
```

**② 读写世界**

```
level.getBlock   setBlock   getData   setLight   setSkyColor   setWeather   setTime   getTicks
      getBiome   findBiome  getDifficulty  setGameMode   getCurrentDimension
      changePlayerDimension   saveDimensionState   getModState   setModState
      getBlockEntityData   setBlockEntityData
      spawnMob   spawnVanilla   spawnPainting   spawnParticle   spawnItemEntity   spawnProjectile
      playSound  playMusic      stopMusic        listPlayers      isHost
      isOp   listOps   addOp   removeOp   saveAll   resetChunks
```

**③ 玩家与实体**

`player.*`（30 个）：位置与朝向、生命与伤害、物品增删查、手持物耐久、游戏模式、创造判定、
潜行/疾跑、速度、坐下站起、动作姿势、变焦、传送、发消息。

`mob.*`：位置/速度/生命/伤害/转向/查询附近实体/移除。

`Player.*`：连**玩家动作和模型**都能自定义 —— `defineAction` / `setAction` / `getAction` / `setModel` / `getModel`。

**④ 界面**

- `UI.addButton` —— 自定义屏幕上的按钮
- `UI.override` / `UI.setImage` / `UI.skin` / `Assets.replaceImage` / `Item.replaceImage` —— 覆盖原版界面与贴图
- `UI.addHudButton` —— **HUD 按钮**：游戏进行中悬浮在屏幕上的按钮，带点击 / 按下 / 抬起回调
- `UI.addHudKey` —— HUD 按钮的"按键映射"版：按住它 = 按住某个鼠标键或键盘键
- `UI.setControlStyle` —— **改原版控件**的位置 / 大小 / 隐藏 / 透明度 / 形状
- `ui.drawText` `drawShadow` `fillRect` `fillCircle` `drawCircleRing` `drawImage` —— 2D 绘制
- `ui.openScreen` `closeScreen` `showLoading` `hideLoading` `getWidth` `getHeight` `getScale`

**⑤ 事件**（在脚本里定义同名全局函数即可收到）

```
onChat        onKey         onMouse       onTick        onGuiRender   onText
onAttack      onUse         onPlace       onPlaceBlock  onPlaceAttempt
onBreakBlock  onBreakAttempt onEatFood    onStepOn      onNeighborChanged
onJoinWorld   onLeaveWorld  onChunkGenerate  onPlayerTick  onMobTick
onJump        onGround      onMobJump     onBiomeEnter  onBiomeLeave
onRender      onRenderComposite            onRemove
onButton      onCell        onClick       onDown        onUp          onClose
```

**⑥ 模组之间**：`mods.set(key, value)` / `mods.get(key)` 共享数据，`getConfig` / `setConfig` 存自己的配置，
`isClient()` / `isServer()` 判断当前跑在哪一端。

### HUD 控件 API（手机端专属）

模组能自己排布手机屏幕上的按钮，甚至改掉原版按钮：

```js
// 自定义按钮：位置大小自己给，按下 / 抬起 / 点击回调进 JS
UI.addHudButton({ key: 'fire', x, y, w, h, onDown, onUp, onClick });

// 或者做成"按键映射"：按住 = 按下鼠标键 / 键盘键，模组照旧收 onMouse / onKey
UI.addHudKey({ key: 'lmb', x, y, w, h, map: 'mouse0' });   // 'key:82' 就是 R

// 改原版控件：位置 / 大小 / 隐藏 / 透明度 / 形状 / 颜色
UI.setControlStyle('jump', {
    x, y, w, h,          // GUI 坐标；不给就保持引擎算出来的位置
    hidden: false,
    opacity: 0.55,       // 0~1
    shape: 2,            // 0 = 原版贴图, 1 = 矩形, 2 = 圆
    fill: 0x40ffffff, border: 0xffffffff, borderWidth: 2
});
UI.getControlRect('jump');     // { x, y, w, h, visible } —— 原版控件当前矩形
UI.clearControlStyle('jump');  // 还原成原版
```

可改的原版控件：`jump`、`sneak`、`fly_up`、`fly_down`、`dpad_up/down/left/right`。
坐标口径与 `ui.getWidth()` 一致；要做圆形按钮就用 `ui.fillCircle` / `ui.drawCircleRing` 自己画。

### 模组的输入框能直接用系统输入法

模组做的输入界面同样接上了安卓系统输入法：中文输入法上屏、退格、回车提交都正常，
文本实时回流到脚本的 `onText`。

---

## 其它

- **中文显示**：离线预生成 GB2312 字形图集（6763 个汉字 + 标点，一张 2048×1024 纹理）。换字体只需替换 `data/fonts/custom.ttf` 后重跑 `gen_cjk_atlas.py`，C++ 不动。
- **Android 触摸**：十字键 / 虚拟摇杆二选一（双击摇杆切潜行、双击跳跃切飞行）；按住 HUD 按钮不会再被误判成"按住屏幕挖方块"。
- **性能**：原生代码以 `-O2` 编译。要点在 `android/app/build.gradle` 保持 `jniDebuggable false` + `NDK_DEBUG=0` —— 误按 debug 编译（`-O0 -g`）会让帧数只剩 1/4（实测 14 fps → 53.7 fps，同一设备同一场景）。

## 直接跑

- **Android**：安装 `android/minecraftpe-debug.apk`（含 arm64-v8a 与 armeabi-v7a）
- **Windows**：运行 `win/MinecraftWin32_GL.exe`（需要同目录的 `data/` 与 `OpenAL32.dll`）
- **服务器**：运行 `server/MinecraftPE-Sever.exe`

## 编译

- **Android**：`cd android && gradle assembleDebug` —— 需要 JDK 21 + Android SDK + NDK r27b + Gradle 8.7，原生部分走 `ndk-build`
- **Windows / Server**：用 Visual Studio 打开 `win/handheld/project/win32_gl/MinecraftWin32_GL.sln` 或 `server/MinecraftPE-Sever.vcxproj`

## 说明

引擎与素材源自 Minecraft Pocket Edition（版权归 Mojang / Microsoft）。本仓库是对老版本的移植与模组系统研究，非商业、不收费；版权方要求即删。
