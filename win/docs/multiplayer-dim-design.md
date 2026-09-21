# 多人联机:无限世界 + 按玩家分维度(设计记录)

分支:本仓库 current(main, 渲染/维度 mod 版)。原版 0.6.1 联机局限:
1. 加入者永远进入有限 256×256(host 开无限也没用)——`StartGamePacket` 不带 worldType,
   客户端 `MultiPlayerLevel` 缺省 `WorldType::Old`,区块请求写死 16×16 网格。
2. 维度是 Level 级单例(`Level::dimension`+单一 chunk 缓存+单一存储 tag),全服务器共享一个
   世界,玩家无法各自处于不同维度。

## 已完成(阶段1, commit a84b2dc)
- `StartGamePacket` 尾部追加 `worldType`+`dimensionId`(host 侧 Login 填充;
  joiner 侧传入 `LevelSettings`)→ joiner `levelData.isInfinite()` 生效,本地无限生成放开。
- joiner 侧新增"随玩家滚动区块请求窗口"(ClientSideNetworkHandler,仅 infinite 激活):
  `rebuildChunkWindow/updateChunkStream/chunkReadyCheck`,每帧由 `Minecraft::update →
  netCallback->tick()` 驱动;有限世界保持原固定 16×16 请求逻辑不变。
- 存储/渲染无需改:joiner chunk cache 与单机同为 ChunkCache+生成器,服务器块数据覆盖本地。

## 阶段2 设计:服务器多世界 + 按玩家维度路由
- 服务器进程 = Minecraft(host)。世界池:dim0 = 主存档 world(唯一落盘 world);
  其它维度 world = 惰性创建的 ServerLevel(dimTag 隔离区块区,只读盘、改动保存在进程内存)。
- 每玩家维度:`Player::dimension`(已有字段)权威。玩家所在世界 = 池[dimension]。
  迁移 = 从旧 world `removeEntity` → 设 `dimension` → 目标 world `addEntity`(entityId 保留)。
- 路由:ServerSideNetworkHandler 记录所有远程玩家(guid→Player)。广播一律按"玩家当前维度
  world"过滤(同 world 互见,跨 world 不可见/收不到数据)。LevelListener 改为每 world 一个
  桥接 listener,事件携带 world 上下文。
- 远程玩家维度切换:服务器发 `PACKET_SETDIMENSION(dim,x,y,z)` → 客户端清实体/本地
  `resetChunks(dim)`/teleport/重开区块窗口 → 服务器随后补发该 world 快照(AddPlayer/实体)。
- host 玩家(渲染侧)迁移:引擎新 `Minecraft::switchActiveLevel()` 轻量换渲染 world(不重跑
  完整生成/onJoinWorld),服务端 tick 扩展到池内所有活跃 world。

## 验收(同机双开)
A 开无限世界 host;B 加入:能走出 256 边界、相互方块/移动同步。
A 进 aether(维度 20):B 仍在主世界且数据不串;A 回主世界;B 进 aether 同理(远程玩家触发
由 aether 多人化阶段提供:mod 通过 `server.getPlayers()`+维度 API 按玩家驱动)。

## 遗留/取舍(本轮范围外,需另开任务)
- 非 0 维度 world 不落盘(进程重启丢天域改动);实体/存档跨维度细节。
- Old(有限)主档的维度世界:RegionFile 无 dimTag 后缀,可能冲突(本项目主档为 Infinite)。
