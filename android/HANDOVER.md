# MinecraftPE Android 版 — 交接须知

> 一句话：把 Windows 版 MCPE（`main/MinecraftPE-Win`）移植成 Android 版，产物在 `main/MinecraftPE-And`。
> 状态：**可构建、可安装、能进世界正常游玩，中文已能正常显示**（实测 OPPO PEHT00 / Android 12 / arm64，55–62 fps）。
> 主要遗留：桌面专属渲染特性缺失、视距上限 256、性能没优化过（见第 6 节）。

---

## 1. 血统与路线（先看这段，不然会误判）

- 源码血统：MCPE 0.8.1 的原始 Android/iOS C++ 代码 → 有人把它移植成了 Windows 版（Win32 建窗 + EGL/GLES）→ 现在又搬回 Android。
- **Windows 版的渲染其实是桌面 OpenGL 固定管线**：EGL 只是用来建窗口（PowerVR 模拟层），编译宏只有 `WIN32`，走的是 `gles.h` 的桌面分支 + GLEW，用了固定管线、`GL_QUADS`、`glBegin/glEnd`、`glClipPlane`、FBO、NPOT 纹理。
- **Android 版渲染走 GLES 1.1 固定管线**（`gles.h` 的 `ANDROID` 分支）。这是刻意选的：与电脑版 API 语义同源、改动最小。代价是桌面专属特性（离屏 MRT、水面镜面反射、glClipPlane）在 Android 上不编译——**这些原本就写在 `#if !defined(OPENGL_ES)` 里**，是原作者的平台划分，不算移植遗漏。
- 早期还考虑过"GLES2 + 软件矩阵栈 + shader 模拟固定管线"（项目里有 iOS 用的那套实现），最终没走。

## 2. 目录与工具链

| 是什么 | 在哪 |
|---|---|
| Android 版源码 | `main/MinecraftPE-And/handheld/src`（393 个 cpp） |
| Android 版资源 | `main/MinecraftPE-And/handheld/data` |
| Gradle 工程 | `main/MinecraftPE-And/android` |
| ndk-build 脚本 | `main/MinecraftPE-And/android/app/jni/{Android.mk,Application.mk}` |
| Java 壳 | `main/MinecraftPE-And/android/app/src/main/java/com/reconnern/mcpe/MainActivity.java` |
| APK 产物 | `main/MinecraftPE-And/minecraftpe-debug.apk`（另见 `android/app/build/outputs/apk/debug/app-debug.apk`） |
| 对照用的 Windows 版 | `main/MinecraftPE-Win`（**只读参考，别改它**） |
| 工具链 | `H:\workerspace\ad\`：NDK r27b、JDK 21（`jdk/jdk-21.0.2`）、Gradle 8.7、Android SDK（platform 35 / build-tools 34.0.0 / platform-tools） |
| 生成脚本 | `gen_android_mk.py`（从 vcxproj 清单生成 Android.mk）、`make_icon.py`（从 app.ico 生成 mipmap）、`gen_cjk_atlas.py`（生成中文图集，见第 10 节） |

### ⚠️ 最容易踩的一脚：资源目录

`MinecraftPE-Win` 下有**两个** data：

- `MinecraftPE-Win/data/` ← **36 MB / 253 文件，这才是运行时的真资源**（exe 就在它旁边，`AppPlatform_win32` 用 `fopen("data/...")` 读的相对路径）
- `MinecraftPE-Win/handheld/data/` ← 3.9 MB / 237 文件，**是旧副本**，缺 `images/gui/background/panorama_*.png`、`custom.ttf`、新版 `spritesheet.png`/`title.png`、`zz_ZZ.lang`

Android 版的 `handheld/data` 必须是**前者的内容**。这一条已经踩过（导致主菜单背景显示成方块材质）。

## 3. 怎么构建

```bash
cd main/MinecraftPE-And/android
JAVA_HOME=/h/workerspace/ad/jdk/jdk-21.0.2 \
  /h/workerspace/ad/gradle/gradle-8.7/bin/gradle assembleDebug --no-daemon
```

- 首次/全量约 **25 分钟**（双 ABI × 411 编译单元）；改几个 cpp 的增量约 **1 分钟**。
- 只要一个 ABI 会快一半：临时把 `app/build.gradle` 的 `abiFilters` 改成单值。
- 安装：`H:\workerspace\ad\sdk\platform-tools\adb.exe install -r main\MinecraftPE-And\minecraftpe-debug.apk`

包名 `com.reconnern.mcpe`，minSdk 21，targetSdk 34，ABI `arm64-v8a` + `armeabi-v7a`。

## 4. 构建系统的坑（都踩过）

1. **用 ndk-build，不用 CMake** —— NDK 自带 ndk-build，而本机 SDK 里没装 cmake 包。
2. **`jni/Android.mk` 是生成的，别手改**。改清单要改 `gen_android_mk.py` 后重跑：
   `python gen_android_mk.py ../MinecraftPE-Win/handheld/cpp_list.txt android/app/jni/Android.mk`
3. **必须拆成多个静态库**。393 个 `.o` 一次链接，Windows 的 `CreateProcess` 命令行会超 32767 字符，报 `make (e=87) 参数错误`。现在拆成 7 组（core/raknet/world/gui/render/client/gui08）+ libpng，最后再链成一个 `.so`。
4. **`handheld/lib/include` 绝对不要加进 include 路径**。那是 Win32 版用的旧 EGL/GLES/SLES 头，会盖掉 NDK 的系统头，直接编不过（`OpenSLES_Android.h` 报 `unknown type name 'SL_API'`）。
5. **`local.properties` 只写 `sdk.dir`**。NDK 用 `app/build.gradle` 的 `android.ndkPath`，并且必须同时给 `ndkVersion '27.1.12297006'`——两个都设会报 `CXX1100`。
6. **assets 要显式列两个源**：`assets.srcDirs = ['src/main/assets', '../../handheld/data']`。写成 `+= ['../../handheld/data']` 会把默认的 `src/main/assets` 丢掉，内置模组就不进 APK 了。
7. **RakNet 裁剪宏必须跟 Win 版 vcxproj 一致**（8 个 `_RAKNET_SUPPORT_* = 0`），否则缺 `UDPForwarder` 等符号。
8. **libpng 在 ARM 上自动启用 NEON**，要带上 `arm/arm_init.c`、`arm/filter_neon_intrinsics.c`、`arm/palette_neon_intrinsics.c` + `-DPNG_ARM_NEON_IMPLEMENTATION=1` + `LOCAL_ARM_NEON := true`（用 intrinsics，避开 32/64 位汇编差异）。
9. `import-module` 会把 `LOCAL_PATH` 改成 native_app_glue 的目录，之后要恢复（脚本里用 `MPE_ROOT` 存了原路径）。
10. **GLES1 没有 `glBegin/glEnd`，也没有 `GL_QUADS`**（NDK 的 `GLES/gl.h` 里连声明都没有，直接编不过）。`gles.h` 里那句 `#define GL_QUADS 0x0007` 只是让桌面分支的代码能过编译，Android 路径别真用它。要用顶点数组 + `GL_TRIANGLES`/`GL_TRIANGLE_STRIP`（或项目自带的 `glXxx2` 包装宏）。实例：`Font::drawCjkCharImmediate`。

## 5. 我改过/新增的源码（逐条说为什么）

**新增（Android 专属，Win 版没有）**

| 文件 | 作用 |
|---|---|
| `handheld/src/main_android.h` | Android 入口/主循环。照搬 `main_win32.h` 的骨架，把「Win32 消息循环 + EGL(PowerVR)」换成「android_native_app_glue 事件循环 + 原生 EGL/GLES1」。含 `APP_CMD_*` 生命周期、`AInputEvent`→`Multitouch`/`Keyboard` 分发、两个 JNI 回调 |
| `handheld/src/AppPlatform_android.{h,cpp}` | 平台实现：`AAssetManager` 读 assets、libpng 从内存解码贴图、`glReadPixels`+zlib 写截图、屏幕尺寸、`supportsTouchscreen()=true`、JNI 软键盘/震动/设备型号、以及 `androidPickModFile` / `androidInstallBundledMods` |
| `handheld/src/AndroidGlobals.h` | JNI/资产桥接层（`g_androidAssetManager` 等全局 + helper 声明） |

**修改（都为了能在 Android 编过/跑对）**

| 文件 | 改了什么 / 为什么 |
|---|---|
| `client/renderer/gles.h` | ANDROID 分支补 `glDepthRange→glDepthRangef`、`glClearDepth→glClearDepthf` 映射（GLES1 只有 float 版）。VBO 声明不用补——NDK 的 `GLES/gl.h` 已声明 `glGenBuffers` 等，且 stub 库确实导出 |
| `client/renderer/RenderTarget.h` | `drawToFirst/Second/Both/Third/Fourth` 用了 `GL_COLOR_ATTACHMENT0-3`（GLES1 没有），加 `#if !defined(OPENGL_ES)` 条件编译 |
| `client/renderer/Chunk.h`、`Chunk.cpp`、`Tesselator.h`、`Tesselator.cpp`、`Options.cpp` | 这些文件的 `#if defined(MACOS)||defined(LINUX)||defined(WIN32)` 条件补上 `|| defined(ANDROID)`，**打开 VBO + CPU 网格 + 植被摆动路径**。原先 Android 被排除在外，导致 `Chunk` 的成员（`_swayAmp`/`_cpuMesh` 等）不存在而编不过 |
| `client/renderer/LevelRenderer.cpp` | ① 第 842 行 sway 路径同上加 ANDROID；② **`MAX_RENDER_DIST` 与 `dist` 上限刻意保持非桌面值 256**（原因见第 7 节）。顺带把 `chunkBuffers` 改成值初始化 `new GLuint[n]()`，并在 `glGenBuffers` 后加 `glGetError` 日志 |
| `client/renderer/GameRenderer.cpp` | 同上：`renderDistance` 上限保持非桌面值 **400**（别改成 1024） |
| `client/gui/Font.cpp` | ① 非 `_WIN32` 下补声明 `gw`（CJK 字宽测量用的变量只在 Win 分支里存在，Android 编译会报未声明）；② **中文支持**：把 `#ifdef _WIN32` 里的 CJK 逻辑放宽成 `#if defined(_WIN32) || defined(ANDROID)`，`initCjkFont()` 增加 Android 分支（从 `assets/fonts/cjk_atlas.bmp` 读预生成图集）；`drawCjkCharImmediate()` 原来的 `glBegin(GL_QUADS)` 换成顶点数组 + `GL_TRIANGLE_STRIP`。详见第 10 节 |
| `client/gui/Font.h` | 同上：CJK 的成员与 `initCjkFont/buildCjkChar/drawCjkCharImmediate` 声明从 `#ifdef _WIN32` 放宽到 `_WIN32 || ANDROID`。`_latinTexture`/`_glyphL`/`_glyphW`（自定义拉丁图集那套，GDI 专用）仍只在 `_WIN32` |
| `gui08/include/I18n.hpp` | `#include "../../../locale/I18n.h"` 多了一级，改成 `../../locale/I18n.h` |
| `raknet/FileList.cpp` | `#if defined(ANDROID)` 分支里的 `#include <asm/io.h>` 在 NDK 不存在（而且此文件根本没用 inb/outb），删掉 |
| `util/FrameProf.h` | 无条件 `#include <windows.h>` + `QueryPerformanceCounter`，改成 `_WIN32` 用 QPC、其他平台用 `clock_gettime(CLOCK_MONOTONIC)` |

另外：`handheld/data` 整个替换成了 `MinecraftPE-Win/data` 的内容（去掉 16MB 的 `custom - 副本.ttf`）。

## 6. 已知问题 / 未完成（按优先级）

1. **桌面专属渲染特性缺失**：水面镜面反射（`renderReflectionTo`）、离屏合成/MRT（`RenderTarget`）、`glClipPlane` 裁剪、模组 FBO 着色器接口（`GlJsBindings` 在 GLES 平台是空实现）。都在 `#if !defined(OPENGL_ES)` 里，要让它们"和电脑版一模一样"得额外实现（GLES1 需 `GL_OES_framebuffer_object`）。
2. **渲染距离上限 256**（桌面 1024），所以视距比电脑版近。原因是 VBO 预分配数量（见第 7 节），要放宽得改成分批/按需分配 VBO。
3. **中文图集是离线预生成的**：字集固定为 GB2312 的 6763 个汉字 + 39 个常用标点/符号（见第 10 节）。生僻字、emoji、以及没列进 `kExtraChars` 的符号仍会画成占位符（图集第 0 格）。改字集或换字体都要重跑 `gen_cjk_atlas.py` 并重新打包。
4. 软键盘只做了文本外发（`nativeOnTextChanged`），中文输入法在游戏内是否顺畅没实测。
5. 触摸方向键/转向的布局只做了"能跑"，没针对横屏分辨率微调过。
6. 性能没优化过（现在 55–62 fps，Chunk 流式加载时偶尔掉到 26 fps）。

## 7. 已修问题存档（别再把它们弄回去）

1. **主菜单 6 图全景背景显示成方块材质** → 资源目录复制错了（拿了 `handheld/data` 旧副本）。全景图加载失败后 GL 还绑着上一个纹理（terrain.png），所以看着像方块材质。
2. **游戏是竖屏** → `AndroidManifest.xml` 里写的是 `sensorPortrait`，改成 `sensorLandscape`。
3. **进入世界必崩（SIGSEGV，fault addr 0x14）** → 这是最花时间的一个。`LevelRenderer` 构造时 `glGenBuffers(numListsOrBuffers, chunkBuffers)`，而 `numListsOrBuffers` 按渲染距离上限算：桌面 1024 → **135200 个 VBO**，Adreno 的 GLES1 驱动分配不了；`chunkBuffers = new GLuint[n]` 又不初始化，于是绑定了垃圾 id，`glBindBuffer` 实际解绑，`glVertexPointer/glColorPointer` 的"偏移"被当成客户端数组地址（`NULL+0x14`），`glDrawArrays` 里驱动 memcpy 读 `0x14` 直接崩。修法：Android 走非桌面上限（256 → 9248 个 buffer）+ 数组值初始化 + `glGetError` 日志。
   → **推论**：以后再引入"桌面才有的上限/预分配"，务必确认移动驱动能承受。
4. **没有 app 图标** → 从 Windows 版的 `handheld/project/win32_gl/app.ico` 用 PIL 生成 5 种密度的 `mipmap-*/ic_launcher.png`。
5. **中文全部不显示（界面/聊天里汉字空白，英文数字正常）** → 汉字字形在 Windows 上是运行期用 GDI 现渲染的，整段 CJK 逻辑都写在 `#ifdef _WIN32` 里，Android 根本没编译，于是 UTF-8 汉字的每个字节都落到拉丁字形取样里。
   修法：双管齐下 —— ① 把 CJK 逻辑的条件从 `_WIN32` 放宽到 `_WIN32 || ANDROID`；② 用 `gen_cjk_atlas.py` 走**与 Win 版同一套 GDI 调用**离线渲染出 `assets/fonts/cjk_atlas.bmp`，Android 侧只负责读它、上传纹理、建 unicode→格子 索引（详见第 10 节）。
   → **推论**：这个项目里凡是被 `#ifdef _WIN32` 包住的「字体/字形渲染」代码，Android 都没有替身 —— 要么离线做成数据，要么换实现，别指望直接编过就有。
6. **世界内 HUD 按钮点不动（右上角聊天键、物品栏格子…）** → `Gui::handleClick` 只挂在 `Minecraft::tickInput()` 的 `while (Mouse::next())` 事件队列上，而那个队列在 Android 世界内收不到触摸点击（原版挖/放方块那两行就是因此被注释掉的），所以点击永远到不了 `handleClick`。
   修法：Android 下改成**轮询** `Mouse::isButtonDown(ACTION_LEFT)` 做按下边沿检测，在「刚按下」那一帧补一次 `gui.handleClick`（数据源与挖方块同源，坐标取同一次 `Mouse::feed` 写进去的 `Mouse::getX/getY`）；原来队列里那句用 `#ifndef ANDROID` 关掉，避免真收到队列事件时点两次。见 `client/Minecraft.cpp` 的 `tickInput()`。
   → **推论**：这个项目里凡是「只靠 `Mouse::next()` 事件队列」的输入响应，Android 世界内都可能失效 —— 要么改成轮询状态，要么先查清事件为什么没进队列。
7. **屏幕最右边的 HUD / 开关「看得见、点不着」**（右上角聊天按钮就是它；设置里的开关也难按）→ 横屏时屏幕右侧那条导航栏属于**系统**：游戏的 EGL surface 画到了它下面（本机实测 surface 宽 2290 = 屏幕 2400 − 左侧挖孔 110，而 `mAppBounds` 只有 `Rect(110,0-2268,1080)`，右边界 2268），所以**渲染画得进去**（看得见）但**触摸由系统接管、不派发给应用**（点不着）。
   修法（两层，缺一不可）：
   ① **沉浸式全屏** —— `MainActivity.applyImmersiveFullscreen()`（`onCreate` + `onWindowFocusChanged` 都调）隐藏状态栏/导航栏，把整屏交还应用，右侧不再被半透明导航栏盖住。
   ② **把系统接管区当成「屏幕外」** —— 不要硬编码一个右边缘距（那样按钮会永远缩在中间）。用窗口 insets 把右侧宽度推给 native：`MainActivity.pushSystemInsetsToNative()` → `nativeSetRightInset()` → `AppPlatform::getUiRightInset()`，然后 **UI 布局统一扣掉它**（`Gui::uiScreenWidth()` 是唯一的宽度来源；`Minecraft::setSize`、`GameRenderer::setupGuiScreen`、`Gui::render/handleClick/getSlotIdAt/getRectangleArea` 全部走它）。沉浸式生效时 inset = 0，右上角按钮就**回到最右上角**；导航栏露出时 UI 自动内缩，仍然点得到。
   → **推论**：① 「能画出来」和「能摸到」是两套边界（surface 尺寸 vs 系统 TouchableRegion），贴边 UI 必须按后者布局；② UI 宽度**只能有一个来源**（`Gui::uiScreenWidth()`），渲染与命中判定各写一份公式迟早错位。
8. **触摸按钮布局：跳跃键挪到右侧 + 飞行升降键独立** → 原来跳跃键是左侧 D-pad 的**中心格**（`AREA_DPAD_C`），而飞行时的"上飞/下飞"靠的是**先按住跳跃键、再按 D-pad 的前进/后退**（`_allowHeightChange` + `wantUp/wantDown`），副作用是飞行中按住跳跃键走路会让方向键改图标、改作用。
   改法：① 跳跃键（原本的 `aJump`）挪到屏幕另一侧、与原先位置**左右对称**处（`xx = w - BaseX - 2*Bw`，右手模式自动镜像）；② 新增 `AREA_FLY_UP = 106` / `AREA_FLY_DOWN = 107` 两个**独立**升降键，贴在跳跃键正上/正下方，飞行时才显示（渲染用图集第二行的上升/下降图标），按了直接升降；③ 删掉"按住跳跃键 + 前进 → areaId 改成 `AREA_DPAD_N`"那条，`isChangingFlightHeight` 也不再由 `heldJump` 触发。
   ⚠ **任何新增的触摸区域都要注册在 `AREA_TURN` 之前** —— `TouchAreaModel::getPointerId` 是**按注册顺序返回第一个命中**，而 `AREA_TURN` 覆盖"全屏减去各控件区"；后注册会被转向区吃掉。另外 `_buttons[]` 是按 `areaId - AREA_DPAD_FIRST` 索引的，加区域记得同步扩数组与 `releaseAllKeys()` 的循环。

## 8. 调试方法

```bash
ADB=/h/workerspace/ad/sdk/platform-tools/adb.exe
$ADB devices -l                     # 设备；无输出说明线/授权/模式（要选"传输文件"）有问题
$ADB logcat -c && $ADB shell am start -n com.reconnern.mcpe/.MainActivity
$ADB logcat -d > log.txt            # native 日志 tag 是 MinecraftPE
grep -n "beginning of crash" log.txt   # 崩溃点在这里之后
$ADB exec-out screencap -p > shot.png  # 截图
```

- 崩溃堆栈里 `libminecraftpe.so (符号名+偏移)` 是靠符号表还原的，很好用。
- 存档 / options / 模组 / 日志目录：`/sdcard/Android/data/com.reconnern.mcpe/files`
  （首次启动会把 `assets/mods/{taczgun.zip,time_control.zip,modlist.json}` 释放到这里）
- 卡在"adb 看不到设备"：换 USB 口、换数据线（很多线只充电）、手机通知栏把 USB 用途切成「传输文件」。

## 9. 约定（重要）

- **不要参考 `H:\workerspace\MinecraftPE-android`**。用户明确说过那是一年前的旧项目，"大多都不通用"。本次移植是**从当前 Win 版重做**的。
- 包名 `com.reconnern.mcpe` 是**刻意**跟 `ModEngine.cpp` 里硬编码的 Android 路径（`/sdcard/Android/data/com.reconnern.mcpe/files`）对齐的，改名要同步改那里。
- Win 版与 And 版是**两份独立源码**（`main/MinecraftPE-Win` / `main/MinecraftPE-And`），本次移植的所有改动只落在 And 这一份，Win 那份没动。两边的 `handheld/src` 往后会各自演进——如果要在 Win 版修同一个 bug，记得手动同步。
- 改完源码记得 `adb install -r` 覆盖安装（同签名同版本号，直接覆盖即可）。
- Windows 上跑 `gradle` 前如果上一次构建被中断，先清残留进程（`taskkill //F //IM java.exe //IM make.exe //IM clang++.exe`），否则会因文件占用报 `Unable to delete directory ... build/intermediates`。

---

## 10. 中文（CJK）字形是怎么来的

**一句话**：字形的「渲染」发生在 Windows 上（离线预生成），Android 只是把结果读进来当纹理用。

### 为什么这么做

Windows 版的中文是运行期用 GDI 现渲染的（`Font.cpp` 的 `initCjkFont()`：`CreateFontW(-16, …, NONANTIALIASED_QUALITY)` + `DrawTextW(DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOCLIP)`，16px 格子铺进 2048×1024 图集）。Android 没有 GDI，所以把这一步挪到打包前做。

### 数据流

```
handheld/data/fonts/custom.ttf ┐
                               ├─> gen_cjk_atlas.py ─> handheld/data/fonts/cjk_atlas.bmp
handheld/src/client/gui/       │      （在 Windows 上跑，走 GDI）
  cjk_gb2312.h (6763 个汉字)   │
  Font.cpp     (kExtraChars)   ┘                │
                                                ▼  （随 assets 打进 APK）
                    Font::initCjkFont() 的 ANDROID 分支：读它 → glTexImage2D → 建索引
```

- `gen_cjk_atlas.py` 走的是**和 Win 版一模一样的那套 GDI 调用**（字体族名从 ttf 的 `name` 表里读出来；字体优先 `data/fonts/custom.ttf` = "Minecraft AE Pixel"，没有就回退系统字体），所以离线产物与 Win 版运行期生成的图集一致。
- 图集格式：54 字节 BMP 头（`biHeight = -1024`，top-down）+ 2048×1024 的 32bpp 像素，共 8388662 字节。`R=G=B=A=字形覆盖率`（0/255 硬像素），所以拿图片查看器直接打开这个 BMP 就能看到字形；C++ 侧只取每像素第 4 字节当 alpha。
- 格子编号（**C++ 与 Python 两边必须一致**）：
  - `[0 .. 6762]` → `cjk_gb2312.h` 的 `g_cjkChars`（GB2312 一二级汉字全集，6763 个）
  - `[6763 .. 6801]` → `Font.cpp` 的 `kExtraChars`（39 个常用标点/符号；GB2312 汉字表里没有标点，Win 版当年就补了这一列，不补全角标点会画成占位符）
  - 图集 8192 格，用了 6802 格，剩下是空格子
- 生成脚本**直接解析 `Font.cpp` 里的 `kExtraChars`**（文件里有两份：Android 分支和 `_WIN32` 分支各一份），两份不一致会直接报错，避免漂移。

### 改字体 / 加字

```bash
cd main/MinecraftPE-And
python gen_cjk_atlas.py --preview 中文   # 生成图集；--preview 顺便把指定字的字形打成字符画核对
# 然后照常 assembleDebug 重新打包
```

- **换字体**：把要用的 ttf 覆盖到 `handheld/data/fonts/custom.ttf`，重跑脚本即可，C++ 不用动。想用系统黑体就把 `custom.ttf` 挪走 —— 脚本和 Win 版都会回退到 `Microsoft YaHei`。
- **加字**：往 `Font.cpp` 的两份 `kExtraChars` 里加码位（两份要一样），重跑脚本。要扩汉字全集就得改 `cjk_gb2312.h`（C++ 和生成脚本共用它）并同步图集容量（`COLS/ROWS`）。
- 脚本依赖：Windows + Python（`ctypes` 调 gdi32/user32），不需要 PIL/FreeType。

### 显示端的几个参数（改之前先读）

- `_cjkGlyph = 16`（图集每格像素）vs `_cjkDraw = 11`（屏幕上每个字的占地面积；拉丁字母是 9px）。16→11 是 `GL_NEAREST` 采样，所以是硬像素风、笔画偏细。**别把 `_cjkDraw` 调到 9**：中文在 9px 下会糊成一团。
- 普通 UI 走 `Tesselator` 批量画（切纹理时 `FLUSH_BATCH()`）；Touch UI 那种 override 模式（`t.isOverridden()`）下 `Tesselator::draw()` 是空操作，所以汉字改走 `drawCjkCharImmediate()` 直接画（用顶点数组 + `GL_TRIANGLE_STRIP`，GLES1 没有立即模式）。
- `_cjkReady` 是安全阀：图集没读到时汉字直接跳过，不会去绑 0 号纹理画出白块。

### 排查

```bash
$ADB logcat -d | grep -i "\[Font\]"     # 应有 "[Font] CJK atlas 2048x1024 uploaded (tex=N)"
$ADB exec-out screencap -p > shot.png    # 看界面/聊天里的中文
```
