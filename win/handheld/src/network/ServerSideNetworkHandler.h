#ifndef _MINECRAFT_NETWORK_SERVERSIDENETWORKHANDLER_H_
#define _MINECRAFT_NETWORK_SERVERSIDENETWORKHANDLER_H_


#include "NetEventCallback.h"
#include "../raknet/RakNetTypes.h"
#include "../raknet/BitStream.h"
#include "../raknet/PacketPriority.h"
#include <vector>
#include <map>
#include <set>
#include <deque>
#include <string>

class Minecraft;
class Level;
class ServerLevel;
class IRakNetInstance;
class Packet;
class Player;
class Entity;
class SetDimensionPacket;
class GameMode;

class ServerSideNetworkHandler : public NetEventCallback
{
public:
	ServerSideNetworkHandler(Minecraft* minecraft, IRakNetInstance* raknetInstance);
	virtual ~ServerSideNetworkHandler();

	virtual void levelGenerated(Level* level);

	// LevelListener 事件改由每 world 一个 WorldListener 桥接转发进来,
	// 以便知道事件发生在哪个维度 world(跨 world 不广播)。
	virtual void onNewClient(const RakNet::RakNetGUID& clientGuid);
	virtual void onDisconnect(const RakNet::RakNetGUID& guid);

	void onReady_ClientGeneration(const RakNet::RakNetGUID& source);
	void onReady_RequestedChunks(const RakNet::RakNetGUID& source);
	Packet* getAddPacketFromEntity(Entity* entity);

	virtual void handle(const RakNet::RakNetGUID& source, LoginPacket* packet);
	virtual void handle(const RakNet::RakNetGUID& source, ReadyPacket* packet);
	virtual void handle(const RakNet::RakNetGUID& source, ModListPacket* packet);   // not expected server-side
	virtual void handle(const RakNet::RakNetGUID& source, ModRequestPacket* packet);
	virtual void handle(const RakNet::RakNetGUID& source, ModFilePacket* packet);    // not expected server-side
	virtual void handle(const RakNet::RakNetGUID& source, ModsReadyPacket* packet);
	virtual void handle(const RakNet::RakNetGUID& source, MessagePacket* packet);
	virtual void handle(const RakNet::RakNetGUID& source, MovePlayerPacket* packet);
	//virtual void handle(const RakNet::RakNetGUID& source, PlaceBlockPacket* packet);
	virtual void handle(const RakNet::RakNetGUID& source, RemoveBlockPacket* packet);
	// 客户端 mod 直接设置方块（天境模组把水点亮成传送门方块就靠它）
	virtual void handle(const RakNet::RakNetGUID& source, PlaceBlockPacket* packet);
	//virtual void handle(const RakNet::RakNetGUID& source, ExplodePacket* packet);
	virtual void handle(const RakNet::RakNetGUID& source, RequestChunkPacket* packet);
	virtual void handle(const RakNet::RakNetGUID& source, PlayerEquipmentPacket* packet);
	virtual void handle(const RakNet::RakNetGUID& source, PlayerArmorEquipmentPacket* packet);
	virtual void handle(const RakNet::RakNetGUID& source, SetHealthPacket* packet);
	//virtual void handle(const RakNet::RakNetGUID& source, TeleportEntityPacket* packet);
	virtual void handle(const RakNet::RakNetGUID& source, InteractPacket* packet);
	virtual void handle(const RakNet::RakNetGUID& source, AnimatePacket* packet);
	virtual void handle(const RakNet::RakNetGUID& source, UseItemPacket* packet);
	virtual void handle(const RakNet::RakNetGUID& source, EntityEventPacket* packet);
	virtual void handle(const RakNet::RakNetGUID& source, PlayerActionPacket* packet);
	virtual void handle(const RakNet::RakNetGUID& source, RespawnPacket* packet);
	virtual void handle(const RakNet::RakNetGUID& source, SendInventoryPacket* packet);
	virtual void handle(const RakNet::RakNetGUID& source, DropItemPacket* packet);
	virtual void handle(const RakNet::RakNetGUID& source, ContainerSetSlotPacket* packet);
	virtual void handle(const RakNet::RakNetGUID& source, ContainerClosePacket* packet);
	virtual void handle(const RakNet::RakNetGUID& source, SignUpdatePacket* packet);
	virtual void handle(const RakNet::RakNetGUID& source, SetDimensionPacket* packet); // unexpected client->server; ignore
	// 模组（玩家姿态/动作）：客户端上报自己的动作，服务器记下并广播给同世界其他人。
	virtual void handle(const RakNet::RakNetGUID& source, PlayerPosePacket* packet);

	//
	// ---- per-player dimension worlds (server) ----
	//

	// Register a server world (dimension) and attach its LevelListener bridge.
	void attachWorld(Level* world);
	// World a player currently lives in (resolved by player->dimension).
	Level* worldForPlayer(const Player* p) const;
	// Server-side migration entry: moves `p` into the dimension world
	// `dimensionId`, keeps entity id, re-syncs every affected client.
	// `p` may be a remote ServerPlayer or the host LocalPlayer.
	void teleportPlayerToDimension(Player* p, int dimensionId, float x, float y, float z);
	// Convenience: host player (LocalPlayer) dimension travel.
	void switchHostToDimension(int dimensionId, float x, float y, float z);
	// Forget a remote player (connection lost / kicked).
	void removeRemotePlayer(const RakNet::RakNetGUID& guid);

	// WorldListener bridge target (called with the source world).
	void onWorldTileChanged(Level* world, int x, int y, int z);
	void onWorldEntityAdded(Level* world, Entity* e);
	void onWorldEntityRemoved(Level* world, Entity* e);
	void onWorldLevelEvent(Level* world, Player* source, int type, int x, int y, int z, int data);
	void onWorldTileEvent(Level* world, int x, int y, int z, int b0, int b1);

	bool allowsIncomingConnections() { return _allowIncoming; }
	void allowIncomingConnections(bool doAllow);

	// 进服欢迎语（server.properties 的 motd）：玩家进入世界后单独发给他。
	void setMotd(const std::string& motd) { _motd = motd; }

	// 把在线玩家数据全部写回存档（关服前调用；实现见 PlayerStore）。
	void saveAllPlayers();

	// 这个玩家该用哪个 GameMode 实例。
	//   服务器：**每个玩家各一个**（创造/生存一套，各自的破坏进度等状态不再串）；
	//   客户端/房主：仍是全局那一个（行为不变）。
	GameMode* gameModeFor(const RakNet::RakNetGUID& source, Player* p);

	// 改某个在线玩家的游戏模式（控制台 / 以后接 /gamemode 命令）。
	// 返回是否找到并改成功；msg 里是给操作者看的说明。
	bool setPlayerGameType(const std::string& playerName, int gameType, std::string& msg);

	// 在线玩家列表（控制台 `list` 用）
	std::string listOnlinePlayers();

	// 游戏内命令（聊天里以 / 开头的整行）用：该玩家是不是管理员。
	bool isOp(const std::string& playerName);
	// 插件改了 ops.txt 之后让服务器重读名单（不然还在用旧缓存）。
	void reloadOps();
	// 管理员增删：控制台（天然有权限）与游戏内 /op、/deop 共用
	void makeOp(const std::string& playerName);
	bool removeOp(const std::string& playerName);
	// 第一个进服的玩家自动成为管理员（避免“没窗口就没人能用管理命令”）。
	void maybeAutoOp(Player* p);

	// ---- 给插件（"文员"）用的服务器能力 ----
	// 插件不该碰游戏内容（那是 mod 的事），但需要这些"服务器事务"能力。
	struct PluginPlayerInfo {
		std::string name;
		int gameType;
		float x, y, z;
		int health;
	};
	std::vector<PluginPlayerInfo> pluginPlayerSnapshot();
	Level* pluginWorld();                                        // 主世界（插件算存档路径用）
	void broadcastMessage(const std::string& text);               // 广播一条聊天
	bool sendToPlayerByName(const std::string& name, const std::string& text);

	Player* popPendingPlayer(const RakNet::RakNetGUID& source);
	// send one packet to every connected player who currently lives in `world`
	void sendToWorld(Level* world, RakNet::BitStream& bitStream, PacketPriority priority, PacketReliability reliability, const RakNet::RakNetGUID* exclude = NULL);

	Level* getMainWorld() const { return _mainWorld; }
	// every active server world (all dimensions with any state)
	const std::map<int, Level*>& getWorlds() const { return _worldByDim; }
	// remote players (guid -> ServerPlayer)
	const std::map<RakNet::RakNetGUID, Player*>& getRemotePlayers() const { return _remotePlayers; }
	// lookup a connected remote player by its entity id (used by mod API)
	Player* findServerPlayerById(int entityId);

	// per-frame tick: advance non-rendered server worlds (entities/levels)
	virtual void tick();

private:

	void redistributePacket(Packet* packet, const RakNet::RakNetGUID& fromPlayer);
	void displayGameMessage(const std::string& message);

	// ---- 聊天（服务器权威） ----
	// 客户端发上来的自由文本在这里校验（长度/频率）：斜杠命令走服务器命令，
	// 普通消息广播给同一维度世界的所有人（含发送者 —— 客户端不回显）。
	void sendMessageTo(const RakNet::RakNetGUID& source, const std::string& text);
	void handleServerCommand(Player* p, const RakNet::RakNetGUID& source, const std::string& cmd);

	struct ChatRate { unsigned int windowStart; int count; };
	std::map<RakNet::RakNetGUID, ChatRate> _chatRate;

	// 玩家行为日志：记录玩家移动到哪。移动包每 tick 都来，必须节流，
	// 否则控制台会被刷爆（见 handle(MovePlayerPacket)）。
	struct MoveLog { float x, y, z; unsigned int lastMs; };
	std::map<RakNet::RakNetGUID, MoveLog> _moveLog;

	// 每个玩家自己的 GameMode 实例（服务器用；见 gameModeFor）
	std::map<RakNet::RakNetGUID, GameMode*> _playerGameModes;

#ifdef STANDALONE_SERVER
	// 管理员名单（文件 ops.txt，一行一个名字）
	void loadOps();
	std::set<std::string> _ops;
	bool _opsLoaded;
	unsigned int _lastPluginTickMs;   // 插件 onTick 限成每秒一次

	// ---- 反作弊：距离与频率（说明见 .cpp）----
	struct RateLimit { unsigned int windowStart; int count; };
	struct SpeedSample { float x, y, z; unsigned int ms; };
	std::map<RakNet::RakNetGUID, RateLimit> _breakRate;
	std::map<RakNet::RakNetGUID, RateLimit> _placeRate;
	std::map<RakNet::RakNetGUID, RateLimit> _interactRate;
	std::map<RakNet::RakNetGUID, SpeedSample> _speedCheck;
	std::map<RakNet::RakNetGUID, unsigned int> _cheatLogMs;
	bool rateAllow(std::map<RakNet::RakNetGUID, RateLimit>& m, const RakNet::RakNetGUID& g, int maxPerSecond);
	bool withinReach(Player* p, float tx, float ty, float tz);
	void logCheat(const RakNet::RakNetGUID& source, Player* p, const char* what, float x, float y, float z);

	// 区块请求排队：天境地形是 JS 一块块算出来的（一块 100+ms），客户端进世界
	// 会一口气请求几百块；同步全做会把主线程占住几十秒（表现成服务器卡死）。
	// 这里入队，每 tick 只生成少量，保证服务器随时能响应。
	struct PendingChunk { RakNet::RakNetGUID guid; int x, z; };
	std::deque<PendingChunk> _pendingChunks;
	std::set<std::pair<int, int> > _pendingChunkKeys;
#endif

#ifdef STANDALONE_SERVER
	// 每 5 分钟把在线玩家写回存档（崩溃/异常断线最多丢 5 分钟）
	void autosavePlayers();
	unsigned int _lastAutosaveMs;
#endif

	std::string _motd;

	Player* getPlayer(const RakNet::RakNetGUID& source);   // remote-only lookup
	Level* worldOf(const RakNet::RakNetGUID& source);
	void sendSnapshotTo(Player* p);   // full current-world state for a player
	void sendStartGame(const RakNet::RakNetGUID& source); // after mod sync is done
	// host's enabled mod list for the joining player (files + sizes)
	void sendModListTo(const RakNet::RakNetGUID& source);

	// world-scoped LevelListener: knows its world, forwards to the handler
	class WorldListener;

	Minecraft*					minecraft;
	Level*						level;              // main world (dimension 0), set by levelGenerated()
	Level*						_mainWorld;         // same as level — persistent overworld
	IRakNetInstance*			raknetInstance;
	RakNet::RakPeerInterface*	rakPeer;

	std::vector<Player*> _pendingPlayers;
	bool _allowIncoming;

	std::map<int, Level*>			_worldByDim;   // dimensionId -> Level (world pool)
	std::map<Level*, WorldListener*> _listeners;   // world -> bridge listener
	std::map<RakNet::RakNetGUID, Player*> _remotePlayers; // guid -> ServerPlayer
	std::map<RakNet::RakNetGUID, bool> _modReadySent;     // joiners awaiting mod sync
};

#endif
