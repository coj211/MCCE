# MinecraftPE-Win 渲染模组 API — 设计文档(相机 / 离屏渲染地基)

- 状态:草案 v0.1
- 基准:主干 `main/MinecraftPE-Win` @ `5a89758`(阶段 09 UI 覆盖后,无任何 PostFX/GL JS 绑定)
- 参照实现:Iris 1.7.6+mc1.20.1、Sodium 0.5.13+mc1.20.1(仅提炼思想,不照搬)
- 已确认范围:先做**引擎 C++ 地基**(相机对象化 + 离屏 FBO 渲染);应用顺序**先镜像水反,后阴影**;JS 模组入口最后。旧 fx_* 测试模组与其代码一律不参考。

---

## 1. 目标与非目标

**目标**
1. 把"世界渲染"从"只认主相机、只画默认窗口缓冲"重构为 **`渲染(任意相机描述, 任意离屏目标)`**。
2. 引擎内置两类"第二相机"用法,验证地基通用性:
   - **镜像相机**(绕水面 Y 平面翻转)→ 平面反射水反;
   - **正交相机**(光源方向)→ shadow map(阴影)。
3. 为最终"强大的模组着色器 API"(滤镜 → 方块级渲染调整 → 水反 → 阴影)留出干净的扩展缝:相机、目标(纹理/FBO)、顶点格式三个维度都可被上层扩展。

**非目标(本轮)**
- 不引入完整 Iris 式 gbuffer/composite 引擎;不推翻现有固定管线渲染;
- 不做 JS API 实现(只留远期形状);
- 不做 SSR / 泛光 / 色调映射等后处理算法。

---

## 2. 基线(主干 5a89758 事实清单)

路径均为 `handheld/src/`。

### 2.1 每帧渲染流程
- `main_win32.h:594` `main` → 循环 `:777-798` → `NinecraftApp::update`(`NinecraftApp.cpp:153` `Minecraft::update`,`:155` `eglSwapBuffers`)。
- `Minecraft::update`(`Minecraft.cpp:536-616`)→ `gameRenderer->render(timer.a)`(`GameRenderer.cpp:137-271`)。
- `GameRenderer::renderLevel(float)`(`GameRenderer.cpp:274-442`)一坨到底:**pick → clear → setupCamera → saveMatrices → 视锥剔除 → 天空/云 → layer0/1 地形 → 实体 → 粒子 → layer2(水/半透明)→ 手持物品**;返回后 HUD/GUI/菜单才画(`:192-234`)。

### 2.2 相机现状(没有相机类)
- 相机 = `Minecraft::cameraTargetPlayer`(`Mob*`,`Minecraft.h:198`);为空时置为 `mc->player`(`GameRenderer.cpp:276-285`)。
- view:每帧 `setupCamera(float a, int eye)`(`GameRenderer.h:37`,`GameRenderer.cpp:96-132`)+ `moveCameraToPlayer`(`:131`,`:470-550`,用 `glRotatef2/glTranslatef2` 堆叠;第 1/3 人称由 `options.thirdPersonView` 决定)。
- proj:`gluPerspective(getFov, mc->width/height, 0.05, renderDistance)`(`GameRenderer.cpp:111-122`);`gluPerspective` 是自定义实现(`gles.cpp:463-488`)。
- 相机插值位置/姿态:renderLevel 内 `:293-295` 的 xOff/yOff/zOff 等;平滑用 `SmoothFloat smoothTurnX/Y`(`GameRenderer.h:65-66`)。
- 矩阵留存:`saveMatrices`(`:987-1000`,`GameRenderer.h:108-110`)读 `GL_PROJECTION/MODELVIEW` 供 pick。

### 2.3 已有"换相机重渲"先例(关键!)
- `LevelRenderer::takePicture(TripodCamera*, Entity*)`(`LevelRenderer.cpp:1210-1236`):临时把 `mc->cameraTargetPlayer = cam`、`hideGui=true`、`thirdPersonView=false`,调 `gameRenderer->renderLevel(0)`,画完恢复——**引擎早就做过"以任意实体当相机渲一整帧"这件事**,只是直写全局、无离屏目标、渲到窗口再截图。

### 2.4 地形/分层
- 每个 chunk 3 层 VBO:`Chunk.h:23` `RenderChunk renderChunk[NumLayers]`(`:79`);层号 = `Tile::getRenderLayer()`(`Tile.cpp:26-28`;0=不透明,1=alpha test,2=混合),水 = 2(`LiquidTile.h:94-96`)。
- 绘制:`LevelRenderer::render(Mob*, int layer, float alpha)`(`LevelRenderer.h:43`)→ `renderChunks`(`LevelRenderer.cpp:573-617`)→ `RenderList::addR` → `RenderList::render`(`RenderList.cpp:42-99`),逐块 `glTranslatef2(-xOff,…)`(`:52`,平移原点是玩家)。
- 剔除:`FrustumCuller` 从**当前 GL 矩阵**提取(`culling/Frustum.h:70-99`)——谁画就先把谁的矩阵压进 GL。
- 天空:正常路径被注释(`GameRenderer.cpp:321-329`),`renderSky`(`LevelRenderer.cpp:934`)仅在云路径被调(`GameRenderer.cpp:1016`)。

### 2.5 GL / 离屏现状
- 桌面 OpenGL 固定管线 + GLEW(`gles.h:144-156`);EGL 窗口表面(`main_win32.h:686-751`),**窗口深度缓冲仅 16 位**。
- **全仓库零 FBO/RBO/glBlit/glCopyTexImage**;唯一读回是截图 `glReadPixels`(`AppPlatform_win32.h:46-58`)。
- 经验(此前验证):现代 GL(3.3+/shader/FBO)可在现有 libEGL 的 4.6 compat context 内共存;**规则:用 VAO 的 pass 结束后必须 `glBindVertexArray(0)`**。

### 2.6 模组系统
- zip 模组 + Duktape(`handheld/src/mod/ModEngine.cpp`),每 mod 独立 sandbox,事件 = 全局函数;加载 `mods/`。
- JS 侧**无任何 3D 渲染/GL API**,只有 2D `ui`/`UI`/`Assets`(`ModEngine.cpp:2286-2453`)。

---

## 3. 参照系提炼(Iris × Sodium 对移植的启示)

| 层 | 参照 | 机制 | 移植启示 |
|---|---|---|---|
| 几何供给 | Sodium | chunk 后台网格化 → 主线程上传;`RenderSectionManager.update(camera, viewport)` 剔除 → `SortedRenderLists`;`drawChunkLayer(renderType, ChunkRenderMatrices, …)` | 渲染入口只吃"矩阵+视锥",**换相机 = 换参数,不是新代码** |
| 相机参数化 | Sodium `Viewport` + `ChunkRenderMatrices` | frustum + (整数格/小数)相机位 + proj/modelView | 老引擎 float 精度处理 = 整数分块平移(你 RenderList 已在做);frustum 从矩阵现算(你 FrustumCuller 已是) |
| 顶点扩展缝 | Sodium `net.caffeinemc.api.vertex.*` | `VertexFormatRegistry` + attribute offset 描述;Iris 注册 xhfp 扩展格式 | 未来 GBuffer 需要法线等额外属性,顶点格式必须可描述/可扩展 |
| 着色调度 | Iris | 阶段(phase)切 program;世界按材质分桶;gbuffer → composite → final | 你已有"层"概念(0/1/2 + 实体/粒子),天然是阶段雏形 |
| 阴影 | Iris `ShadowRenderer` | 正交投影 + 光源相机,重渲一遍世界到 shadow target;自带视锥 | 与镜像相机共用"第二相机"地基 |
| 水反 | Iris 不实现,shaderpack 自取 | Iris 只保证 depthtex1(无水深度)与 colortex 时序 | 你要做的是**引擎层反射纹理 + 相机**,算法面留给你未来 GLSL/JS |

**三条核心启示**
1. **相机做成"描述/快照"而非类**:主相机、镜像相机、阴影相机是同一参数化的三个实例。主干无相机类、`takePicture` 已有换实体先例,重构阻力小。
2. **几何供给与着色消费分家**:chunk 几何(3 层 VBO)先原样复用;需要额外顶点属性(法线等)时再走"顶点格式描述符"扩展,不推倒重来。
3. **把 `renderLevel` 的相机部分参数化**是全部工作的咽喉,先做这一个纯函数级重构,后面全是调用它。

---

## 4. 总体架构

### 4.1 概念分层

```
[ JS 模组层(远期 M3) ]  ← 相机/目标/回调,非本轮
─────────────────────────────────────
[ 引擎 API 面(M0-M2,C++) ]  RenderTarget, RenderCameraDesc, renderWorld(...)
─────────────────────────────────────
[ 渲染内核 ]  GameRenderer::renderLevel 重构 → renderWorld(cam, target)
             LevelRenderer::render(Mob*, layer, alpha) 不动
             chunk VBO / RenderList / FrustumCuller 不动
─────────────────────────────────────
[ GL 层 ]  GLEW 桌面 GL;新增 FBO/RTT 封装;现有 EGL 4.6 compat context
```

### 4.2 相机对象化(核心重构)

把 `setupCamera/moveCameraToPlayer` 里"从 Mob + 选项 → 矩阵"的逻辑抽成**纯函数**:

```
RenderCameraDesc {
    Vec3  pos;            // 相机世界坐标(整数+小数拆解沿用 CameraTransform 思路)
    float yaw, pitch;     // 姿态(与 Mob 头一致:yaw 绕 Y,pitch 绕 X)
    float fov;            // 纵向/横向视场(沿用 getFov 语义,默认 70)
    float aspect;         // mc->width / mc->height,离屏时=目标宽高比
    float near = 0.05f;   // 与 gluPerspective 现值一致
    float far;            // renderDistance
    int   eye;            // 0=主眼/1=副眼,透传 renderLevel(a, eye)
    bool  ortho;          // false=透视(镜像);true=正交(阴影,配 orthoSize)
    float orthoSize;      // 正交半宽(阴影用)
}
```

- **主相机 = 每帧由 `mc->cameraTargetPlayer` + 选项生成的默认实例**(保持现状语义,含平滑/SmoothFloat/第三人称/手持相机逻辑——这些仍属 `renderLevel` 主路径私有,不进 desc)。
- 镜像/阴影相机 = 引擎内部按规则从主相机导出(见 4.5/4.6)。
- desc 里不含 `options.hideGui/thirdPersonView` 之类副作用:离屏渲染天然不画 GUI,HUD 在主路径末尾才画。

### 4.3 离屏目标(RenderTarget 与 FBO 封装)

新增 `handheld/src/client/renderer/gl/RenderTarget.h/.cpp`(桌面 GLEW):

```
struct RenderTargetDesc {
    int width, height;         // 离屏尺寸(默认 = 逻辑分辨率,可 0.25x~1x)
    bool withDepth;            // 挂 D24 深度(绕开窗口 EGL 16 位)
    GLenum colorInternal = GL_RGBA8;
};
class RenderTarget {           // RAII:创建→glGenFramebuffers/Textures/RBO,销毁→删除
    GLuint fbo, colorTex, depthBuf;
    void bind();               // 保存当前 FBO/视口,绑自己,设 glViewport
    void unbind();             // 恢复(glBindFramebuffer(0)+原视口)
    GLuint colorTexture();     // 给后续合成采样
    void resizeIfNeeded(w,h);
};
```

- 深度的"读回采样"不纳入本轮(固定管线无法在 fragment 里采样深度;等 GLSL 路径再补)。

### 4.4 渲染入口参数化

把 `GameRenderer::renderLevel(float a)` 重构为内部 **`renderWorld(const RenderCameraDesc& cam, RenderTarget* target)`**,主路径与离屏共用:

```
void GameRenderer::renderLevel(float a) {          // 主路径(现签名不变,调用方零改动)
    renderWorld(makeMainCameraDesc(a), nullptr);   // nullptr = 渲到窗口缓冲
    ... HUD/GUI 仍在 renderLevel 外层 ...
}

void GameRenderer::renderWorld(const RenderCameraDesc& cam, RenderTarget* target) {
    // 1. target? target->bind() : (确保窗口 0 绑定)
    // 2. 投影:把 gluPerspective(或正交)那一段改为按 cam 生成矩阵(与现 :111-122 等价)
    // 3. 视图:把 moveCameraToPlayer 的堆叠改为按 cam.pos/yaw/pitch 生成(固定管线 glRotatef2/Translatef2 保留,只是输入来自 cam;同步重构 FrustumCuller 依赖:先压矩阵再建/复用)
    // 4. 按需剔除/updateDirtyChunks → 分层渲染(全部沿用 LevelRenderer::render(Mob*,layer,a))
    //    但把"从 mc->cameraTargetPlayer 取位置"改成从 cam.pos 取(renderChunks/RenderList 平移原点)
    // 5. 天空:离屏且允许时按需渲(见 4.7)
    // 6. target? target->unbind() : (无)
}
```

> 侵入点提示:`takePicture` 模式(`LevelRenderer.cpp:1213-1225`)证明"临时改 cameraTargetPlayer + renderLevel(0)"能工作;参数化后**不再需要**这种全局替换,`Mob*` 参数语义保留(镜像可用 TripodCamera 式宿主,也可纯 desc)。

### 4.5 镜像相机(平面反射,M1)

给定水面高度 `wy` 与主相机:

```
RenderCameraDesc mirrorOf(const RenderCameraDesc& main, float wy) {
    RenderCameraDesc m = main;
    // 相机位置关于水面(平面 y=wy)镜像
    m.pos.y = 2*wy - main.pos.y;
    m.pitch = -main.pitch;       // 俯仰取反
    m.yaw   = main.yaw;          // 偏航不变
    return m;
}
```

渲染规则(标准平面反射,固定管线可做):
1. 渲染顺序:先渲镜像画面到反射 target(反射里**不含水/半透明层自身**,即 layer 只画 0/1 + 实体/天空),再渲正常画面,最后合成时在水像素上取反射纹理;
2. **winding 翻转**:镜像把几何绕到水面下、可见性翻转,画反射场景时 `glFrontFace` 取反(逆→顺)补偿,画完还原;
3. **clip 平面**:理想上用水面 `y=wy` 作 `GL_CLIP_PLANE0` 裁掉水面以上的几何(桌面 GL 兼容管线支持 `glClipPlane`,GLEW 可用);若该路径不稳,退化为"层 2 不画 + 远处容差",见坑 #6;
4. 只画水面下可见部分时水面自身(层 2)不渲,避免无限递归。

**水 mask**:layer2(水)需要一张"哪些像素是水"的 mask,供合成阶段区分。M1 提供引擎离屏 mask(在渲 layer2 前把 mask 目标清黑,渲 layer2 时同时写 mask 颜色目标);合成仍先由 C++ 内置 shader-less 方式(CPU 每像素?太慢)——**实际采用**:复用反射纹理合成 pass 需要一个最小 GLSL 全屏 pass(见 4.7 决策),这是引擎第一个 GLSL pass,也是后续模组着色 API 的种子。

### 4.6 阴影相机(M2,后续里程碑)

```
RenderCameraDesc sunCamera(const RenderCameraDesc& main, const Vec3& sunDir, float shadowDistance, float orthoSize) {
    RenderCameraDesc c = main; c.ortho = true;
    c.orthoSize = ...;                     // 覆盖主相机视锥的包围盒
    c.pos = main.pos + sunDir*shadowDistance;  // 或包围盒中心 - 太阳方向*far
    c.yaw/pitch = 由 sunDir 反解(朝向太阳);
    c.near/far  = 包住 shadowDistance 包围盒;
    return c;
}
```

- 渲 shadow map = 同一 `renderWorld(cam, target)`,只是 target 用 D24 深度(颜色可 RGBA8 占位,未来 shadowcolor)。
- 主画面地形 pass 需要接收阴影:这要求地形渲染进入 GLSL(把阴影纹理喂给 shader),因此 **M2 的落地依赖 4.7 的 GLSL 化地形路径**,作为里程碑里明确的前置。

### 4.7 双渲染路径策略(重要决策)

| 路径 | 何时用 | 内容 |
|---|---|---|
| **A:固定管线(现有)** | M0-M1 主路径 + 镜像/离屏 | 原样画 chunk/实体;离屏只进 FBO,不加 shader |
| **B:GLSL 最小路径(新增)** | M1 合成 pass 起,逐步扩大 | 引擎内置最小 GLSL 130 全屏 pass(VAO+VBO+program,遵守 `glBindVertexArray(0)` 规则);后续方块级渲染/顶点扩展/阴影/GBuffer 都在这条路径上长 |

演进触发点:
- M1 要"在水像素上合成反射纹理" → 需要 B 的第一个全屏采样 pass;
- 需要法线/阴影 → 地形迁移到 B(带 attribute 的 VBO 布局 = Sodium `VertexFormatDescription` 思路的简化版);
- 天空:正常路径天空被注释。**镜像画面没天空会很假**,M1 前置项 = 恢复/提供天空渲染(要么恢复原 `renderSky` 逻辑在反射 target 画一次,要么先渲天空色→远处地平线渐变,足够倒影用)。

### 4.8 状态隔离与回退
- 所有新 GL 对象(FBO/纹理/program/VAO)集中在 `renderWorld` 一进一出之间创建/销毁,**结束后必须还原**:当前 FBO→0、视口、`glFrontFace`、clip 平面、深度掩码、绑定纹理/VAO。
- 提供总开关(如 `options` 或编译宏):镜像/阴影关闭时,`renderWorld(camDesc, nullptr)` 与旧 `renderLevel` 行为等价 → 每次改动有回退点,不弄坏原画面(用户铁律)。

---

## 5. API 草案(C++,引擎内部,M0-M2 实施)

以下为**首个里程碑就位后**的内核形态;JS 层见 §7。

```cpp
// ---- handheld/src/client/renderer/gl/RenderCamera.h ----
struct RenderCameraDesc {
    Vec3  pos{0,0,0};            // 世界坐标
    float yaw=0, pitch=0;        // 姿态
    float fov=70.f, aspect=1.f;  // 透视
    float near=0.05f, far=512.f;
    bool  ortho=false;
    float orthoSize=64.f;        // 正交半宽(阴影)
};

RenderCameraDesc makeMainCamera(GameRenderer&, float alpha);      // 主相机(现逻辑封装)
RenderCameraDesc mirrorOf(const RenderCameraDesc&, float waterY); // 镜像(M1)
RenderCameraDesc sunCamera(const RenderCameraDesc&, const Vec3& sunDir, float dist); // 阴影(M2)

// ---- handheld/src/client/renderer/gl/RenderTarget.h ----
class RenderTarget;   // 见 §4.3:bind/unbind/colorTexture/resizeIfNeeded

// ---- GameRenderer 扩展 ----
// void renderWorld(const RenderCameraDesc&, RenderTarget* target=nullptr);
//   主路径 renderLevel(float) 保持签名与行为;内部委托 renderWorld(makeMainCamera(a), nullptr)
```

**内置序列(伪代码,主画面一帧)**

```
renderShadows(sunCam, shadowTarget);      // M2 起,在世界 pass 之前(可隔帧/低分辨率)
renderWorld(mainCam, sceneFboOrNull);     // 正常世界
if (waterReflectionEnabled && waterY>0) {
    renderWorld(mirrorOf(mainCam, waterY), reflTarget);   // 反射:层0/1+实体+天空,winding 翻转,clip 水面
}
compositePass:                             // M1 起,GLSL 全屏 pass
    // 输入:sceneTex(或主 FBO 颜色)、waterMaskTex、reflTex
    // 输出:屏幕缓冲 —— 水像素=反射+扰动,其余原样
```

> 反射的隔帧渲染降频(反射画面两帧间变化小)放入 M1 优化项,不做默认。

---

## 6. 里程碑与验收

| | 内容 | 涉及 | 验收(用户实测) |
|---|---|---|---|
| **M0** | FBO/RTT 封装 + `RenderCameraDesc` + `renderWorld` 参数化重构 + 离屏"第二视口"调试 | `GameRenderer.cpp/.h`、`LevelRenderer`、新增 `gl/RenderTarget.*`、`gl/RenderCamera.h` | 主画面与 5a89758 逐像素无差异(回退开关关闭时);调试模式角落小视口显示"镜像相机所见"(临时取景验证相机可换) |
| **M1** | 恢复/补天空(离屏用)→ 镜像相机渲反射纹理 → 水 mask(layer2)→ 最小 GLSL 全屏合成 pass → 水反开/关 | 上述 + `Tile::getRenderLayer`/layer2 标注、新 `gl/program` 最小封装 | 站在水边:水面出现真实倒影(岸/天/实体),非水画面不变;/镜像关 恢复原样 |
| **M2** | 正交阴影相机 → shadow target → 地形 GLSL 化以采样阴影(依赖 B 路径铺开) | `ShadowRenderer`(新) | 太阳方向出现随地形起伏的阴影;可关 |
| **M3** | 把 Camera/RenderTarget/合成回调暴露给 Duktape JS(远期,形状见 §7) | `ModEngine` 注册 | fx_* 级别的能力由模组实现,不靠引擎内置 |

每里程碑交付:构建走 `tools/build/build_win32_release.bat`(v145,/Od),exe 同步仓库根;截图存 `tools/*.png` 交用户目检。

---

## 7. JS 模组 API 远期形状(仅设计,不实现)

> 目的:锁住 M0-M2 的 C++ 结构不与未来 JS 面冲突。形状继承"能力清单",但引擎全新实现、签名可能微调。

```
Render.enable(offscreen: bool)                       // 引擎离屏渲染场景纹理
Render.renderScene(cam: Camera, targetFbo)           // = renderWorld 暴露(镜像/阴影同源)
Render.waterLevel(): float                           // 探测/给定水面 y
Render.waterMaskTexture(): GLTexture                 // layer2 mask
GL 对象(纹理/FBO/program/…)裸绑定(与既有 gles 封装同风格)
回调:onRenderComposite(sceneTex, w, h, maskTex, reflTex)
Camera 对象:{ pos:[x,y,z], yaw, pitch, fov, ortho?, orthoSize? }
```

---

## 8. 已知坑与对策

1. **窗口深度 16 位**:离屏一律自建 D24;主路径不动。
2. **`RenderList` 平移原点 = 玩家位置**:参数化时"原点"取自 `cam.pos`,须把层内 `glTranslatef2(-xOff,…)` 的 xOff 来源从实体换成 desc,小心保持整数/小数拆分精度(仿 Sodium `CameraTransform` 思路)。
3. **剔除依赖当前 GL 矩阵**(`Frustum.h:70-99`):离屏渲染前先把该相机的 P*M 压进 GL 再建 FrustumCuller(与现主路径次序一致)。
4. **天空被注释**:镜像无天空=倒影黑;M1 前置恢复天空(至少水面以上区域)。
5. **winding 与 clip**:镜像需 `glFrontFace` 反转 + `y=wy` clip 平面;若 `glClipPlane` 在 PowerVR 模拟驱动不稳,退化为"不画层2+近水面几何轻微上抬容差",并把正确性标记为 open issue。
6. **EGL 表面与 offscreen 混用**:离屏 pass 结束必须绑回 `eglSurface` 对应 FBO 0 + 还原视口;状态机统一在 `renderWorld` 进出口处理。
7. **VAO 规则**:任何新 GLSL pass 结束 `glBindVertexArray(0)`;固定管线路径不要碰 VAO 绑定。
8. **性能**:镜像 = 每帧多渲一遍世界;M1 先全分辨率实现正确,优化项=降分辨率(0.5x)+隔帧,阴影同理低分辨率。

---

## 9. 未决问题(需后续决策)

1. 反射 target 的默认分辨率策略:跟随主分辨率 vs 固定 0.5x?(建议 0.5x 起步,选项暴露)
2. 镜像 pass 是否画实体/粒子?(建议:画地形+实体+天空;粒子/手持不画——反射里不需要手)
3. 阴影 M2 是否要求"地形迁 GLSL"作为前置(影响地形几何供给重构的排期,建议是)
4. 水面 Y 的获取:M1 用"探测到水面方块的最高 y" vs 用户设定值?(建议:引擎自动探测 layer2 最低高度面,配 `/wl` 覆写)
5. 合成 pass 的 shader 位置:内置 GLSL 资源(仿 Sodium `ShaderLoader`)vs 外置文件?

---

## 10. 实施记录 · 阴影落地(rc5,2026-09-05,相对主干 5a89758 之后的工作区)

**已实现(与 §4.6/§6 M2 草案不同,记录最终形态):**

- M0-M1 已按 §4.3/§4.5 落地为 `RenderTarget`(RGBA8+D24 双通道 scene target + planar reflection)+ JS `onRenderComposite(scene,mask,refl)`;光影包 demos/rc4_shader = 纯合成级(调色/空气雾/水反)。
- **M2 阴影没有走"地形迁移 GLSL"路线**,而是在合成 pass 中做**深度重建 shadow mapping**:
  1. 主 scene 深度纹理(D24)已经是"水面以下几何"深度 → 合成 shader 用 CPU 算好的 `inverse(proj·view)` 逐像素重建世界坐标(再加回 RenderList 平移原点 = 相机 xOff);
  2. 模组自建 D24 阴影图 FBO,用 `Render.world([...9元正交])` 从太阳方向把层 0/1 渲进阴影图(低频:玩家移动 >0.6m / 方块改动 / 光照参数变化才重渲,站定零开销);
  3. 合成 shader 把世界坐标投影进光空间,3×3 PCF 软阴影,图边缘 smoothstep 淡出。
- 水面像素不吃阴影(其 depth 是水底,重建会错位;且反射纹理本身无阴影)。

**引擎/JS 接口增量:**
- `GameRenderer::renderWorldScripted(…, orthoHalf)` 支持正交相机(glOrtho ±orthoHalf,near/far 可为负排序无关);渲完把实际压栈的 PROJ/MODELVIEW 矩阵与渲染原点拷进成员,经 getter 暴露 —— 模组拿到的光空间矩阵与渲染时**零错位**。
- JS `Render.world([x,y,z,yaw,pitch,fov,near,far,orthoHalf])`(orthoHalf>0 切正交)、`Render.getWorldMatrices()` → `{proj:[16],view:[16],x,y,z}`(列主序)。
- JS `GL.glUniformMatrix4fv(loc, values[16])`(列主序,无 transpose)、`GL.glColorMask(r,g,b,a)`。
- 未新增文件;新光影包 demos/rc5_shadow → mods/rc5_shadow.zip(rc5 = rc4 超集 + 阴影)。

**与草案的差异与取舍:**
- near/far 用正数(SH_N=3,SH_F≈190),相机悬在玩家太阳侧 96m,避免负 near 影响 FrustumCuller;正交半宽 64(边缘 86%~100% 淡出)。
- 阴影图分辨率 1024²,覆盖玩家周围约 128×128×190m;实体不投影(只渲层 0/1)。
- 昼夜:强度乘 `dayFactor`(与引擎亮度 br=2cos(2π·td)+0.5 同相位),深夜≈0.06。
- 未决问题 §9.3 由本方案**否决**:阴影不需要地形迁 GLSL。

**开发手册入口**:光影/渲染模组开发的**权威文档与示例在工作区
`H:\workerspace\workapace\main\the mod develop`(README + docs/10-着色器渲染API.md
§10.10 实时阴影 rc5 / §10.11 开发工作流)。引擎侧本文档只记设计决策与增量。
