# MinecraftPE 0.8.1 Port

Minecraft Pocket Edition **0.8.1**（2013 年）在三个平台上的移植。源码和可直接运行的产物都在这里。

```
android/     Android 版（NativeActivity + GLES 1.1），含 minecraftpe-debug.apk
win/         Windows 版（Win32 + OpenGL），含 MinecraftWin32_GL.exe
server/      独立服务器版，含 MinecraftPE-Sever.exe
```

## 这个版本做了什么

### Android 触摸操作（整套重写）

- 十字键 / 虚拟摇杆二选一（设置里开关）。摇杆模式下**双击摇杆切换潜行**
- **双击跳跃切换飞行**，判定窗口收紧到 0.25 秒，不再误触
- 飞行时跳跃键上/下方各有一个独立的升降键
- 按住任何 HUD 按钮都不会再被判定成「按住屏幕挖方块」—— 老版本里按方向键会把准星对着的方块挖掉

### 中文显示

- 离线预生成 GB2312 字形图集（`gen_cjk_atlas.py`）：6763 个汉字 + 39 个标点，合成一张 2048×1024 纹理
- 换字体只需替换 `data/fonts/custom.ttf` 后重跑脚本，C++ 不用改

### 系统输入法

- 点文本框直接弹出安卓输入法，支持中文上屏、退格、回车提交
- 文本实时回流到游戏；点画面空白处键盘不会再丢
- 聊天屏、告示牌、世界名、服务器地址等所有输入框走同一条路

### 性能

- 原生代码以 `-O2` 编译。要点在 `android/app/build.gradle` 保持
  `jniDebuggable false` + `NDK_DEBUG=0`：误按 debug 编译（`-O0 -g`）会让帧数只剩 **1/4**（实测 14 fps → 53.7 fps，同一台设备同一场景）

### 模组引擎（脚本模组，无需重编译）

脚本里可以直接拿到这些全局对象：

| 对象 | 用途 |
|---|---|
| `Item` / `Block` / `Mob` | 注册自定义物品、方块、生物 |
| `Recipes` | 合成表（有序 / 无序） |
| `Commands` | 注册聊天命令 |
| `Player` / `player` / `level` / `mob` | 玩家、世界、实体读写 |
| `Biome` / `Dimension` / `Particle` / `Projectile` | 生态域、维度、粒子、投射物 |
| `UI` / `ui` | 界面：自定义屏幕、HUD 控件、2D 绘制 |
| `Assets` / `skins` | 贴图资源 |
| `setTimeout` / `setInterval` | 定时器 |
| `getConfig` / `setConfig` | 模组自己的配置存储 |

**HUD 控件 API** —— 让模组自己排布手机上的按钮：

```js
// 自定义按钮：位置大小自己给，按下/抬起回调进 JS
UI.addHudButton({ key: 'fire', x, y, w, h, onDown, onUp, onClick });

// 或者做成「按键映射」：按住 = 按下鼠标键 / 键盘键，模组照旧收 onMouse / onKey
UI.addHudKey({ key: 'mouse_l', x, y, w, h, map: 'mouse0' });   // 'key:82' 就是 R

// 改原版控件：位置 / 大小 / 隐藏 / 透明度 / 形状 / 颜色
UI.setControlStyle('jump', {
    x, y, w, h,                 // 不给就保持引擎算出的位置
    hidden: false,
    opacity: 0.55,              // 0~1
    shape: 2,                   // 0 = 原版贴图, 1 = 矩形, 2 = 圆
    fill: 0x40ffffff, border: 0xffffffff, borderWidth: 2
});
UI.getControlRect('jump');      // { x, y, w, h, visible } —— 拿原版控件的当前矩形
UI.clearControlStyle('jump');   // 还原

// 画圆，方便做圆形按钮
ui.fillCircle(cx, cy, r, 0xAARRGGBB);
ui.drawCircleRing(cx, cy, r, thickness, 0xAARRGGBB);
```

- 可改的原版控件：`jump`、`sneak`、`fly_up`、`fly_down`、`dpad_up`、`dpad_down`、`dpad_left`、`dpad_right`
- 坐标口径是 **GUI 坐标**，与 `ui.getWidth()` 同一套（引擎内部换算成物理像素）
- `ui.*` 另有：`drawText`、`drawShadow`、`fillRect`、`drawImage`、`getWidth`、`getHeight`、`getScale`、`openScreen`、`closeScreen`、`showLoading`、`hideLoading`

**脚本能接的事件**：`onChat`、`onKey`、`onMouse`、`onTick`、`onGuiRender`、`onAttack`、`onBreakBlock`、`onBreakAttempt`、`onChunkGenerate`、`onJoinWorld`、`onLeaveWorld`、`onEatFood`、`onJump`、`onGround`、`onBiomeEnter`、`onBiomeLeave`、`onMobJump` 等。

## 直接运行

- **Android**：安装 `android/minecraftpe-debug.apk`（含 arm64-v8a 与 armeabi-v7a）
- **Windows**：运行 `win/MinecraftWin32_GL.exe`（需要同目录的 `data/` 与 `OpenAL32.dll`）
- **服务器**：运行 `server/MinecraftPE-Sever.exe`

## 怎么编译

- **Android**：`cd android && gradle assembleDebug` —— 需要 JDK 21 + Android SDK + NDK r27b + Gradle 8.7，原生部分走 `ndk-build`（`android/app/jni/Android.mk` 由 `gen_android_mk.py` 生成）
- **Windows / Server**：用 Visual Studio 打开 `win/handheld/project/win32_gl/MinecraftWin32_GL.sln` 或 `server/MinecraftPE-Sever.vcxproj`

## 说明

Minecraft 及相关素材版权归 Mojang / Microsoft 所有。本仓库是对 2013 年 0.8.1 老版本的移植研究，非商业、不收费；版权方要求即删。
