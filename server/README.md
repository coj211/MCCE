# MinecraftPE-Sever —— 独立（无头）专用服务器

## 术语（全项目统一）

| 叫法 | 指什么 |
|---|---|
| **服务器** | 本工程产出的独立进程 `MinecraftPE-Sever.exe`（无窗口、无 GL、控制台程序） |
| **客户端** | 玩家的游戏本体 `MinecraftPE-Win`（`MinecraftWin32_GL.exe`） |
| 「房主模式」 | 原版局域网联机：客户端进程自己兼任服务器（历史行为，本工程不改它） |
| **mod**（“武将”） | 双端加载的扩展：改游戏内容（方块/怪物/维度/物品/生成器），放 `mods/*.zip` |
| **插件**（“文员”） | **只在服务器**跑的扩展：管服务器事务（配置/账户/ops/聊天格式/命令），放 `plugins/*.js` |

> 不再用"服务端 / 玩家端"来指这两个东西。

一个**没有房主的世界存档**：把 MinecraftPE 的世界/实体/方块/背包/网络逻辑放进一个
独立的控制台进程里跑，客户端（`MinecraftPE-Win`）直接连进来就能玩 —— 不需要谁
开着游戏当房主。

```
   MinecraftPE-Sever.exe  (无窗口 / 无 GL / 控制台进程)
   ├── 世界生成与存档        ← 引擎 world/  (ServerLevel / ChunkCache / RegionFile)
   ├── 实体、怪物 AI、方块    ← 引擎 world/entity, world/level
   ├── 背包、容器、合成       ← 引擎 world/inventory, world/item
   ├── 联机协议与玩家管理     ← 引擎 network/ServerSideNetworkHandler
   └── mod 引擎（JS/Duktape） ← 引擎 mod/ModEngine
                 ▲
                 │ MCPE 0.6.1 原版协议（RakNet UDP）
                 ▼
   MinecraftWin32_GL.exe（原版客户端）—— 只负责画面与输入
```

服务器与客户端**共用同一份引擎源码**（`../MinecraftPE-Win/handheld/src`），靠
`STANDALONE_SERVER` 宏做无头构建。所有为服务器做的引擎改动都用这个宏隔离，
**客户端构建逐字不变**。

---

## 1. 构建

```bash
cd main/MinecraftPE-Sever

# 1) 生成工程文件（从客户端工程机械翻译：加 STANDALONE_SERVER、Console 子系统、
#    剔除 145 个纯客户端渲染/GUI 源文件与引擎 main.cpp）
python tools/gen_vcxproj.py

# 2) 编译（VS18 BuildTools / 工具集 v145 / Win32）
"/c/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/MSBuild/Current/Bin/MSBuild.exe" \
    MinecraftPE-Sever.vcxproj -p:Configuration=Release -p:Platform=Win32 -p:PlatformToolset=v145 -m -v:m

# 3) 产物同步到服务器根目录（服务器的工作目录就是 exe 所在目录）
cp build/Release/MinecraftPE-Sever.exe .
```

> 为什么用生成脚本而不是手写工程：客户端工程有 390 条 `ClCompile` 逐条列出，
> 手抄必然漂移。`gen_vcxproj.py` 做机械翻译，引擎加了新文件重跑一次即可。

运行时依赖的 DLL（从客户端目录复制，与服务器 exe 同目录）：

```
glew32.dll  libpng3.dll  zlib1.dll  OpenAL32.dll  libEGL.dll  libgles_cm.dll  libpng12.dll
```

## 2. 运行

```bash
cd main/MinecraftPE-Sever
./MinecraftPE-Sever.exe
```

控制台命令：`stop`（退出）。启动后会打印：

```
========================================
 MinecraftPE-Sever (standalone)
   name    : A Minecraft PE Server
   bind    : 0.0.0.0:19132 (udp/raknet)
   level   : world
   players : up to 16
========================================
```

### 后台能看到玩家在干什么

玩家的行为都会打到服务器控制台（前缀 `[act]` / `[chat]`）：

```
[act] 小奇怪 spawned at (8.5, 65.0, -2.5) dim=0
[act] 小奇怪 moved to (12.5, 64.0, -3.0) dim=0
[act] 小奇怪 broke tile.grass(id 2) @ (12, 64, -3) dim=0
[act] 小奇怪 placed tile.stone(id 1) @ (12, 64, -2) dim=0
[chat] 小奇怪: 大家好
[chat] 小奇怪: /smoke 你好            ← 命令也在这里留痕
[cmd] 小奇怪: /smoke 你好             ← 命令分发
小奇怪 joined the game
小奇怪 disconnected from the game
```

`moved` 是**节流**后的：位移超过 3 格、或距上次记录超过 5 秒才记一条
（移动包每 tick 都来，不节流会把控制台刷爆）。方块名取自 tile 的
`getDescriptionId()`，后面跟 id，名字缺失时也能看出是什么。

客户端连接方式：多人游戏 → External / Add Server → 填服务器 IP（本机 `127.0.0.1`）。

## 3. 配置 `server.properties`

首次启动自动生成，改完重启生效。

| 键 | 说明 |
|---|---|
| `server-name` | 客户端服务器列表里显示的名字 |
| `server-port` | UDP 监听端口（默认 19132） |
| `max-players` | 上限（默认 16） |
| `level-name` | 存档 id，也是存档目录名（`./world/`） |
| `level-display-name` | 存档显示名 |
| `level-seed` | `0` = 每次启动随机 |
| `gamemode` | `survival` / `creative` |
| `level-type` | `old`（有限 256×256）/ `infinite` |
| `motd` | 进服提示语 |
| `do-daylight-cycle` | 昼夜是否更替；`false` = 时间冻结（太阳不动） |
| `spawn-animals` | 是否生成和平生物（猪/牛/羊/鸡…） |
| `spawn-monsters` | 是否生成敌对生物（僵尸/骷髅/蜘蛛…） |
| `difficulty` | `peaceful` / `easy` / `normal` / `hard`；`peaceful` 时不刷敌对生物、已存在的会消失 |
| `enable-command-server` | 旧版无鉴权 TCP 调试端口，默认关 |

`server-name` 就是**客户端服务器列表里显示的名字**（改完重启服务器生效）。

存档根目录 = 服务器 exe 所在目录（引擎在 `STANDALONE_SERVER` 下把存档路径直接
定为外部存储根，不再套 `games/com.mojang/minecraftWorlds/`）。

## 4. mod 机制

### 4.1 服务器为准 + 客户端自动补齐（已实现）

服务器 `mods/*.zip` 就是这个世界启用的 mod 列表：

1. 玩家登录 → 服务器下发 `ModListPacket`（`mods/*.zip` 的文件名 + 大小）
2. 客户端比对自己 `mods/` 目录 → 缺哪个就 `ModRequestPacket` 要哪个
3. 服务器分块发 `ModFilePacket`（48KB/块）→ 客户端写盘 → `applyEnabledList`
   **自动启用**（无需玩家去设置里手动勾选）
4. 客户端 `ModsReadyPacket` → 服务器才发 `StartGamePacket` 放行进世界

服务器侧的实现不依赖 `ModEngine`（它读的是磁盘上的 `mods/*.zip`，并带路径穿越防护），
所以即使服务器还没接上 JS 执行，这个"下发 + 自动启用"链路也是通的。

### 4.2 同一份 zip 双端加载 + `isServer` / `isClient`（已实现）

一份 mod zip 两端都加载：服务器跑权威逻辑（方块/事件/命令/数值），客户端跑表现
（贴图/UI/光影/音效），代码里用全局 `isServer` / `isClient` 区分：

```js
// name: MyMod
modLog("side: isServer=" + isServer + " isClient=" + isClient);

if (isClient) {
    // 只在客户端：贴图 / 界面 / 光影
    Assets.replaceBlockIcon(41, "gold_block.png");
}

// 服务器命令：客户端输入 /heal Steve，由服务器执行
Commands.register("heal", function (sender, args) {
    // sender = 执行者名字，args = 参数串；返回的字符串会作为聊天回复发给他
    return "回复了 " + sender + " 10 点血";
});

function onTick() { /* 每 tick（服务器也跑） */ }
function onJoinWorld() { /* 世界就绪 */ }
```

- 服务器**能跑**：mod 扫描/加载、`onTick`/`onJoinWorld`/方块/实体/维度等逻辑钩子、
  `Commands.register` 注册的服务器命令、`modLog`/文件读写/`setConfig`/`setTimeout`。
- 服务器**静默忽略**：`ui.*`、`GL.*`/`Render.*`、贴图注入、玩家皮肤、粒子、声音
  —— 它们依赖渲染模块，只对画面有意义（服务器调用不会崩，只是没效果）。

> 服务器进程里 `modEngine` 是真实存在的：引擎核心逻辑（`Level::saveLevelData`、
> `GameMode::destroyBlock`、`MobSpawner`、`Dimension`）本身就回调它的钩子，
> 所以 mod 的权威逻辑在服务器是真跑的，不是空壳。

写服务器 mod 时注意：服务器没有"本地玩家"，`player.*` 指的是本地玩家对象（服务器
为 NULL）。面向具体玩家的操作要用命令的 `sender` 参数，或等第 6 节第 2 项的
`server.*` API。

### 4.3 玩家数据（一人一份存档）

`world/players/<名字>.dat` —— 每个玩家自己的位置、朝向、血量、饥饿、经验、背包、
游戏模式都记在这里（NBT，和 `level.dat` 同一套容器格式）。

- **写入时机**：玩家退出/断线时立即写 + 每 5 分钟自动写 + 关服前写
- **读取时机**：登录握手完成、进入世界之前；没有文件就是新玩家（默认出生点、空背包）
- **游戏模式是 per-player 的**：玩家文件里的 `ServerGameType` 决定他自己的模式
  （创造：可飞行/瞬间破坏/无敌），`-1` 表示跟随 `server.properties` 的 `gamemode`

> 为什么引擎自带的存档不够用：它只给"本地玩家"留了一个坑位
> （`LevelData::createTag(players)` 里只取 `players[0]`，见 `LevelData.cpp:157`），
> 多人时不仅别人不存，`level.dat` 里那份还只属于第一个进服的人。

⚠️ **名字就是身份**：引擎没有账号体系，玩家名来自客户端登录包自报，服务器不验证。
所以「按名字存数据」意味着任何人都能用别人的名字进服、拿到那个人的背包。
要做权限/防冒名，得另做（白名单、op 名单、进服口令）。

### 4.5 mod 的日志在哪

服务器上 mod 的日志有**两份**：

- **控制台**：直接打出来 —— 服务器没有客户端的 F3 面板，日志看不到就没法排查
- **文件**：`logs/mod.log`（服务器目录下，追加写）

> `ModEngine::log()` 原本只写文件、不写控制台，而且那个文件路径在客户端是
> `C:\Users\<用户名>\AppData\Local\Temp\mcpe_mod.log`（**把用户名写死了**，
> 换台电脑就失效）——所以服务器这一侧单独走 `logs/mod.log`，客户端行为保持不变。
> 服务器还不受"只在启动阶段记日志"那个开关限制（客户端靠 F3 重开）。

启动时控制台会先给一份摘要：

```
[server] 已加载 mod: 1 个
[server]   - smoke_test.zip
```

## 5. 插件机制（“文员”）

**mod 和插件是两套东西，别混用：**

| | mod（“武将”） | 插件（“文员”） |
|---|---|---|
| 跑在哪 | **双端**（服务器跑逻辑、客户端跑表现） | **只在服务器** |
| 管什么 | 游戏内容：方块、怪物、维度、物品、生成器 | 服务器事务：配置、账户、ops、聊天格式、命令、运维 |
| 客户端知道吗 | 知道（客户端要下载同一份 zip） | **完全不知道**（不影响兼容性） |
| 目录 | `mods/*.zip` | `plugins/*.js` |

**为什么必须分开**：服务器有些东西客户端根本不存在（`server.properties`、玩家账户、
ops 名单）。而 mod 是双端加载的，它一旦去碰这些，就会出现“服务器能跑、客户端不兼容”。
所以插件单独一套引擎、单独一套 API，只跑在服务器。

### 5.1 写一个插件

把一个 `.js` 放追 `plugins/` 就自动加载（服务器启动时扫一遍）。改完不用重启：
控制台敲 `plugins reload`，游戏内（管理员）敲 `/plugins reload`。

插件里能用的：

| API | 作用 |
|---|---|
| `server.log(msg)` | 写服务器日志 |
| `server.broadcast(text)` | 给所有人发一条聊天 |
| `server.sendTo(玩家名, text)` | 给某个玩家私聊 |
| `server.players()` | 在线玩家数组 `[{name, mode, x, y, z, health}]` |
| `server.getConfig(key)` / `server.setConfig(key, value)` | 读/写 `server.properties`（改完重启生效） |
| `server.deletePlayerSave(名字)` | 删掉这个玩家的存档（账户/背包一起清） |
| `server.setGameMode(名字, "creative")` | 改在线玩家的游戏模式（当场生效） |
| `chat.setFormat(fn)` | `fn(玩家名, 消息)` 返回完整聊天行 —— **加称号前缀就靠它** |
| `Commands.register(name, fn)` | 注册 `/name`，`fn(sender, args)` 的返回值回给玩家 |
| `files.read / write / append / list / delete / exists(path)` | 服务器目录下的文件读写。**沙箱**：`..` 与绝对路径会被拒 |
| `ops.list() / add(name) / remove(name)` | 管理员名单（ops.txt） |
| `bans.list() / add(name, reason) / remove(name) / isBanned(name)` | 封禁名单（bans.txt）；**登录时直接拒掉** |
| `whitelist.list() / add / remove / enabled() / enable(bool)` | 白名单（whitelist.txt，配 `white-list=true` 生效） |
| `setTimeout(fn, ms) / setInterval(fn, ms) / clearTimer(id)` | 定时任务（服务器每 tick 驱动，最小 50ms） |
| 事件 | `onPlayerJoin(name)` / `onPlayerLeave(name)` / `onTick()`（每秒一次） |
| **可取消事件** | `onBlockBreak(name,x,y,z)` / `onBlockPlace(name,x,y,z)` / `onAttack(name,targetId)` / `onChat(name,msg)` —— **return false 就否决这次操作**（登录插件靠它拦住未登录玩家） |

完整例子见 `plugins/称号.js`（给聊天加称号 + `/在线` 命令）。

> 插件**拿不到**改动游戏内容的那套 API（加方块/怪物/维度）——那是 mod 的活。

### 5.2 管理员（ops）

`ops.txt` 一行一个玩家名。**名单为空时，第一个进服的玩家自动成为管理员**
（否则服务器后台启动、没有控制台窗口时就没人能用管理命令）。

**怎么加/去管理员**（三条路）：

| 在哪敲 | 命令 | 谁能用 |
|---|---|---|
| 服务器窗口 | `op <玩家名>` / `deop <玩家名>` | 控制台天然有权限 |
| 游戏内 | `/op <玩家名>` / `/deop <玩家名>` | 需要管理员 |
| 插件 | `ops.add(名字)` / `ops.remove(名字)` | 插件自己 |

**插件命令也能声明“只有管理员能用”**（第三个参数）：

```js
Commands.register("封禁", function (sender, args) {
    bans.add(args);
    return "已封禁 " + args;
}, { op: true });        // ← 普通玩家敲这条会被回“你没有权限”
```

游戏内命令：`/gamemode creative|survival [玩家名]`（需管理员）、
`/plugins [reload]`（reload 需管理员）、`/op` `/deop`（需管理员）、`/list`（所有人）。

> ⚠️ **这套权限的根基是「名字 = 身份」**，而 0.6.1 没有账号系统：玩家名是客户端
> 自己报上来的（`ServerSideNetworkHandler.cpp` 里 `newPlayer->name = packet->clientName`），
> 服务器不验证。所以它**只能防手滑，防不住冒名**——谁把客户端名字改成你的名字，
> 连上来就是管理员。真要防冒名得加进服密码，那需要改客户端。

### 5.3 反作弊（服务器权威）

客户端发上来的包是**可以伪造的**——改过的客户端能隔着一百格挖方块、一秒钟拆一百块。
服务器的原则是：「不信客户端说自己够得着，只信自己算的」。

| 检查 | 阈值 | 处理 |
|---|---|---|
| 破坏 / 放置的距离 | 6 格 | 丢弃这个包 + 记 `[cheat]` |
| 破坏 / 放置的频率 | 25 次 / 秒 | 同上 |
| 攻击的距离 / 频率 | 6 格 / 8 次每秒 | 同上 |
| 移动速度 | 25 格 / 秒 | **只记日志，不拉回** |

作弊日志有节流（同一玩家 3 秒最多一条），被刷也不怕。

> 移动速度异常**故意不拉回**：传送、卡顿、重连都会造成大位移，拉回很容易误伤玩家。
> 先观察一段时间，确认误判率再决定要不要收紧。

阈值都在 `network/ServerSideNetworkHandler.cpp` 的 `withinReach / rateAllow` 与各调用点
（想放宽就把 `6.0f`、`25`、`8` 调大）。

### 5.4 自带示例：`plugins/登录.js`

进服要登录，没登录前**不能挖/放/打/聊天**（可以走路）。

| 游戏内命令 | 作用 |
|---|---|
| `/设置密码 <密码>` | 新玩家注册（设完自动登录） |
| `/login <密码>` | 老玩家登录 |
| `/改密码 <旧> <新>` | 改密码 |
| `/删账号 <玩家名>` | 管理员用：删掉某人的账号（忘了密码时） |

实现方式：`onBlockBreak/onBlockPlace/onAttack/onChat` 里判断“这个人登录了吗”，
没登录就 `return false` —— 服务器收到 false 会**直接丢弃这次操作**。

> 两条限制要说清：① 密码是**明文**存在 `plugin_data/登录.txt`（管理员看得到）；
> ② 身份基础仍是“玩家名”，猜不到密码就冒充不了，**但你没设过密码时别人能抢先注册**。
>
> 另外**移动拦不了**：位置是客户端本地预测的，服务器硬拦会让角色原地漂移回弹。
> 真要“锁死在登录点”，得靠服务器每 tick 把没登录的人拉回原位置。

| 文件 | 改动 |
|---|---|
| `client/renderer/gles.h` | 服务器分支补 GL 类型/常量（只取声明不调函数），`glXXX2` 状态包装给成空操作 |
| `client/Minecraft.cpp` | 服务器排除：渲染截图存档、GUI 选项屏、`locateMultiplayer`、`ExternalServerFile`、旧版 TCP `CommandServer`（无鉴权后门）、mod 引擎的创建与皮肤/事件调用 |
| `network/ServerSideNetworkHandler.cpp` | 独立服务器用 `mods/*.zip` 作为 mod 源完成 `ModList`/`ModFile` 下发（含文件名安全校验） |
| `mod/ModEngine.cpp` | 31 个贴图/UI/皮肤/声音/粒子入口在服务器掏空；3 处非线性段（贴图还原、ScriptedScreen、裸 GL 绑定注册）条件编译；怪物定义在服务器只注册碰撞箱不建渲染模型 |
| `client/renderer/Textures.h` / `Tesselator.h` 等 | 无需改动（GL 类型补齐后自然可编译） |

服务器专属代码：`src/main_server.cpp`（控制台入口 + stdin 命令线程）、
`src/ServerApp.{h,cpp}`（启动流程）、`src/ServerConfig.{h,cpp}`（配置读写）。

## 7. 已完成 / 下一步

### 已完成

- 独立无头服务器：配置、世界生成与存档、控制台 `stop`、UDP 监听
- 原版客户端登录握手 + 进入世界（实测通过）
- 服务器为准的 mod 下发 + 客户端自动下载启用
- **服务器跑 mod**：同一份 zip 双端加载 + `isServer`/`isClient`；
  `onTick`/`onJoinWorld`/方块/实体/维度等钩子在服务器真跑
- **命令由服务器执行**：客户端连服务器时 `/xxx` 整行发给服务器，服务器先查
  mod 注册的命令表（`Commands.register`，返回值即回复），未命中才回
  "Unknown command"；单机/房主仍走本地执行
- **聊天服务器转发**：服务器校验（非空 / 200 字节上限 / 每人 5 条每秒）后广播给
  同维度世界的所有人（含发送者，客户端不再本地回显，避免重复）
- 玩家加入/离开广播、`motd` 进服提示
- **玩家行为日志**：控制台用 `[act]` / `[chat]` 记录谁在哪里挖/放了什么方块、移动到哪
  （移动做了节流）
- **世界规则可配**：`do-daylight-cycle`（昼夜更替）、`spawn-animals`、
  `spawn-monsters`（生物生成）
- 服务器名广播取自 `server-name`（修掉了客户端列表里显示成 "Steve" 的问题）
- 关闭旧版无鉴权 TCP 调试端口（4711）
- 服务器 mod 日志常开（客户端靠 F3 按需打开，服务器没有 F3）
- **插件系统（“文员”）**：`plugins/*.js` 自动加载 + `plugins reload` 热重载；
  独立 JS 环境（拿不到改动游戏内容的 API），管服务器事务：
  `server.log/broadcast/sendTo/players/getConfig/setConfig/deletePlayerSave/setGameMode`、
  `chat.setFormat`（聊天加称号前缀）、`Commands.register`、
  事件 `onPlayerJoin/onPlayerLeave/onChat/onTick`
- **游戏内管理命令**：`/gamemode`（需管理员）、`/plugins [reload]`（reload 需管理员）、
  `/list`；命令优先级：内置 → 插件 → mod → 未知
- **管理员机制**：`ops.txt`（一行一名），名单为空时**第一个进服的玩家自动成为管理员**

> ⚠️ 客户端侧也有改动（`ChatInputScreen::submit` 把聊天与命令发给服务器），
> **需要重新编译客户端 exe** 才能生效。

### 下一步

1. 插件 API 补完：写文件/读目录（目前只有 `server.properties`）、`ops.add/remove`、
   封禁/白名单、定时任务（`setTimeout` 风格）
2. 服务器内置命令继续补（`/say /kick /tp /time` …）
3. 服务器 mod API 补齐"面向某个玩家"的操作（`server.sendMessage(name, text)`、
   踢人、传送）—— 服务器没有本地玩家对象，`player.*` 用不上
4. 服务器权威校验：方块/背包/交互的距离与频率校验（反作弊）
5. 运维：白名单/封禁、多世界管理、定时重启、日志落盘

## 8. 编码约定（UTF-8）

Windows 上“字符串编码”是三个世界，服务器必须自己统一，否则中文一律乱码：

| 哪个世界 | 编码 |
|---|---|
| 源码 / 网络包 / 日志 / 插件 JS | **UTF-8** |
| 文件系统内核 | UTF-16 |
| 窄字符 CRT（`fopen` / `_findfirst` / `std::ifstream`） | 本地代码页（中文系统 = GBK） |

所以有两条硬规定：

1. **进程启动时把控制台切到 UTF-8**（`src/main_server.cpp` 里 `SetConsoleOutputCP(CP_UTF8)`）
   —— 否则用老控制台（conhost）启动时，日志里的中文、插件名全是乱码。
2. **一切碰文件系统的地方走宽字符 API**，UTF-8 先转 UTF-16（`src/ServerPaths.h`）：
   `_wfopen` / `_wfindfirst` / `_wremove` / `_wrename` / `std::ifstream(const wchar_t*)`。
   中文插件文件名（`称号.js`）与中文玩家名（存档文件名）靠这条才能找到文件。

另外 `ServerPaths.h` 里 include `<windows.h>` 时挡掉了 `winsock.h`
（`WIN32_LEAN_AND_MEAN` + `_WINSOCKAPI_`），否则会和 RakNet 的 `winsock2.h` 冲突
（`sockaddr` 重定义）。
