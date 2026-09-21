# MinecraftPE 0.8.1 Port

Minecraft Pocket Edition **0.8.1**（2013 年的老版本）的三个平台移植，源码和可直接运行的产物都在这个仓库里。

| 目录 | 内容 |
|---|---|
| `android/` | Android 版（NativeActivity + GLES 1.1 固定管线 + ndk-build），含编译好的 `minecraftpe-debug.apk` |
| `win/` | Windows 版（Win32 + OpenGL），含 `MinecraftWin32_GL.exe` |
| `server/` | 独立服务器版，含 `MinecraftPE-Sever.exe` |

## 直接运行（不用编译）

- **Android**：安装 `android/minecraftpe-debug.apk`（含 arm64-v8a 与 armeabi-v7a）
- **Windows**：运行 `win/MinecraftWin32_GL.exe`（需要同目录的 `data/` 与 `OpenAL32.dll`）
- **服务器**：运行 `server/MinecraftPE-Sever.exe`

## 这个移植做了什么

**触摸 HUD 全部重做**
- D-pad / 跳跃 / 潜行 / 飞行升降键；可选**虚拟摇杆**（设置里开关，双击摇杆切潜行）
- 双击跳跃切飞行（窗口 0.25s，避免误触）
- 按住 HUD 按钮不会再把判定成"按住屏幕开始挖方块"

**中文**
- 离线预生成 GB2312 字形图集（`gen_cjk_atlas.py`）：6763 个汉字 + 标点，2048×1024 单张纹理

**模组引擎**（Duktape，脚本模组）
- 脚本可注册物品 / 方块 / 生物 / 命令 / 界面
- HUD 控件 API：
  - `UI.addHudButton` / `UI.addHudKey` —— 自定义屏幕按钮（JS 回调，或映射成鼠标键/键盘键）
  - `UI.setControlStyle` / `UI.getControlRect` —— 由模组决定**原版控件**的位置/大小/隐藏/透明度/形状
  - `ui.fillCircle` / `ui.drawCircleRing` —— 画圆，方便做圆形按钮

**系统输入法**
- 点文本框弹出安卓 IME，文本实时回流到游戏（中文输入、退格、回车提交都能用）

## 构建

### Android
```
cd android
JAVA_HOME=<jdk21> gradle assembleDebug --no-daemon
```
- 需要 JDK 21、Android SDK、NDK r27b、Gradle 8.7
- 原生部分用 `ndk-build`（`android/app/jni/Android.mk` 由 `gen_android_mk.py` 生成，393 个源文件拆成多个静态库）
- **注意**：`app/build.gradle` 里必须保持 `jniDebuggable false` +
  `arguments 'APP_PLATFORM=android-21', 'APP_OPTIM=release', 'NDK_DEBUG=0'`。
  否则 ndk-build 会按 `-O0 -g` 编译，实测帧数只有优化版的 **1/4**（14 fps vs 53.7 fps）。

### Windows / Server
用 Visual Studio 打开对应工程：
- `win/handheld/project/win32_gl/MinecraftWin32_GL.sln`
- `server/MinecraftPE-Sever.vcxproj`

## 已知限制

- Android 渲染距离上限是 **256**：`LevelRenderer` 按上限预分配 VBO，调到 1024 会让 Adreno GPU 分配失败并崩溃
- Android 只支持 GLES 1.1 固定管线；桌面专属特性（离屏 MRT、水面反射、`glClipPlane`、模组 FBO）在 `#if !defined(OPENGL_ES)` 里，Android 不编译

## 版权

Minecraft 及相关素材（贴图、音效、语言文件等）版权归 **Mojang / Microsoft** 所有。
本仓库是对 2013 年 0.8.1 老版本的**移植与研究**，不用于任何商业用途、不收取费用。
如果你是版权方并要求移除，请开 issue，我会立刻处理。
