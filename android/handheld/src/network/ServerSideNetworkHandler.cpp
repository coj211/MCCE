
#include "ServerSideNetworkHandler.h"
#include "../world/level/Level.h"
#include "../world/level/LevelListener.h"
#include "../world/entity/player/Player.h"
#include "../mod/ModEngine.h"
#include <algorithm>
#include "../world/entity/player/Inventory.h"
#include "../world/Container.h"
#include "../world/inventory/BaseContainerMenu.h"
#include "packet/PacketInclude.h"

#include "RakNetInstance.h"
#include "../platform/time.h"          // getTimeMs（聊天限流）
#include "../client/Minecraft.h"
#include "../client/player/LocalPlayer.h"
#include "../client/gamemode/GameMode.h"
#include "../raknet/RakPeerInterface.h"
#include "../raknet/PacketPriority.h"
#ifndef STANDALONE_SERVER
#include "../client/sound/SoundEngine.h"
#endif
#include "../server/ServerPlayer.h"
#include "../world/entity/item/FallingTile.h"
#ifdef STANDALONE_SERVER
#include "../world/level/storage/FolderMethods.h"   // _findfirst / _findclose（服务器 mods 扫描）
#include "../world/level/LevelSettings.h"          // GameType
#include "../client/gamemode/SurvivalMode.h"        // 每个玩家自己的 GameMode 实例
#include "../client/gamemode/CreativeMode.h"
#include "../../../MinecraftPE-Sever/src/PlayerStore.h"   // 一人一份的玩家存档
#include "../../../MinecraftPE-Sever/src/PluginEngine.h"   // 插件（“文员”）
#include "../../../MinecraftPE-Sever/src/ServerPaths.h"    // UTF-8 ↔ UTF-16（写 ops.txt）
#include <fstream>                                       // ops.txt 读写
#include <cmath>                                         // sqrtf（反作弊算速度）
#endif

#define TIMES(x) for(int itc ## __LINE__ = 0; itc ## __LINE__ < x; ++ itc ## __LINE__)

#ifdef STANDALONE_SERVER
// ---------------------------------------------------------------------------
// 独立服务器：mod 文件仓库。
//
// 客户端 / host 进程通过 ModEngine 访问 mods 目录（getEnabledModFiles /
// modFileSize / readModFileBytes）。独立服务器进程里没有 ModEngine（它整块
// 依赖 GL/GUI，编译器把它排除在服务器之外），所以这里直接看磁盘：
//   - 下发给客户端的 mod 列表 = mods 目录里的 *.zip
//   - 供客户端下载的内容    = 这些 zip 的原始字节
// 语义与客户端一致（一个 mod 就是一个 zip）。等第二阶段把 JS 引擎接上服务器，
// 这两个函数换成走 ModEngine 即可，协议和调用点都不用动。
// ---------------------------------------------------------------------------

static const char* kServerModsDir = "mods";   // 相对 CWD（服务器启动时已切到 exe 目录）

static std::vector<std::string> serverModFiles()
{
	std::vector<std::string> out;
	std::string pattern = std::string(kServerModsDir) + "\\*.zip";
	struct _finddata_t fd;
	intptr_t h = _findfirst(pattern.c_str(), &fd);
	if (h == -1)
		return out;
	do {
		if (fd.attrib & _A_SUBDIR)
			continue;
		out.push_back(std::string(fd.name));
	} while (_findnext(h, &fd) == 0);
	_findclose(h);
	std::sort(out.begin(), out.end());
	return out;
}

// 防路径穿越：文件名虽然是从 serverModFiles() 里挑的，但客户端请求是可以乱写的。
static bool isSafeModFileName(const std::string& file)
{
	if (file.empty() || file.size() > 200)
		return false;
	if (file.find('/') != std::string::npos || file.find('\\') != std::string::npos)
		return false;
	if (file.find("..") != std::string::npos)
		return false;
	return true;
}

static long serverModFileSize(const std::string& file)
{
	if (!isSafeModFileName(file))
		return -1;
	FILE* fp = fopen((std::string(kServerModsDir) + "\\" + file).c_str(), "rb");
	if (!fp)
		return -1;
	fseek(fp, 0, SEEK_END);
	long sz = ftell(fp);
	fclose(fp);
	return sz;
}

static bool readServerModFileBytes(const std::string& file, std::vector<unsigned char>& out)
{
	out.clear();
	if (!isSafeModFileName(file))
		return false;
	FILE* fp = fopen((std::string(kServerModsDir) + "\\" + file).c_str(), "rb");
	if (!fp)
		return false;
	fseek(fp, 0, SEEK_END);
	long sz = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if (sz <= 0 || sz > 32 * 1024 * 1024) {
		fclose(fp);
		return false;
	}
	out.resize((size_t)sz);
	size_t rd = fread(out.data(), 1, (size_t)sz, fp);
	fclose(fp);
	if (rd != (size_t)sz) {
		out.clear();
		return false;
	}
	return true;
}

// 按玩家自己的游戏模式设能力（创造：可飞行 / 瞬间破坏 / 无敌）。
// serverGameType < 0 表示“跟随服务器全局配置”，此时保留 GameMode 给的那套。
static void applyPlayerGameType(Player* p)
{
	if (!p || p->serverGameType < 0)
		return;
	const bool creative = (p->serverGameType == GameType::Creative);
	p->abilities.mayfly = creative;
	p->abilities.instabuild = creative;
	p->abilities.invulnerable = creative;
	if (!creative)
		p->abilities.flying = false;
}
#endif // STANDALONE_SERVER

// ---------------------------------------------------------------------------
// World-scoped LevelListener: the handler is attached to every server world
// through one of these bridges so level events can be routed to the players
// that actually live in that world.
// ---------------------------------------------------------------------------
class ServerSideNetworkHandler::WorldListener : public LevelListener
{
public:
	WorldListener(ServerSideNetworkHandler* handler, Level* world)
	:	h(handler), world(world) {}

	virtual void tileChanged(int x, int y, int z) { h->onWorldTileChanged(world, x, y, z); }
	virtual void tileBrightnessChanged(int x, int y, int z) { h->onWorldTileChanged(world, x, y, z); }
	virtual void entityAdded(Entity* e) { h->onWorldEntityAdded(world, e); }
	virtual void entityRemoved(Entity* e) { h->onWorldEntityRemoved(world, e); }
	virtual void levelEvent(Player* source, int type, int x, int y, int z, int data) { h->onWorldLevelEvent(world, source, type, x, y, z, data); }
	virtual void tileEvent(int x, int y, int z, int b0, int b1) { h->onWorldTileEvent(world, x, y, z, b0, b1); }

	ServerSideNetworkHandler* h;
	Level* world;
};

ServerSideNetworkHandler::ServerSideNetworkHandler(Minecraft* minecraft, IRakNetInstance* raknetInstance)
:	minecraft(minecraft),
	raknetInstance(raknetInstance),
	level(NULL),
	_mainWorld(NULL),
	_allowIncoming(false)
#ifdef STANDALONE_SERVER
	, _lastAutosaveMs(0)
	, _opsLoaded(false)
	, _lastPluginTickMs(0)
#endif
{
	rakPeer = raknetInstance->getPeer();
}

ServerSideNetworkHandler::~ServerSideNetworkHandler()
{
	for (std::map<Level*, WorldListener*>::iterator it = _listeners.begin(); it != _listeners.end(); ++it) {
		if (it->first)
			it->first->removeListener(it->second);
		delete it->second;
	}
	_listeners.clear();

	for (unsigned int i = 0; i < _pendingPlayers.size(); ++i)
		delete _pendingPlayers[i];
	_pendingPlayers.clear();
}

// ---------------------------------------------------------------------------
// World registration
// ---------------------------------------------------------------------------
void ServerSideNetworkHandler::attachWorld(Level* world)
{
	if (!world)
		return;
	if (_listeners.find(world) != _listeners.end())
		return;

	// 新维度世界（例如天境）必须拿到网络实例：Level::tick() 里会
	// raknetInstance->send() 发时间包/实体数据。主世界是 Minecraft 建世界时
	// 赋的值，而 getOrCreateDimensionLevel 建出来的世界没人给 —— 空指针，
	// 服务器一 tick 这个新世界就访问违规崩溃（以前维度世界只在客户端本地
	// 建、从没在服务器 tick 过，所以这条雷一直没被踩到）。
	if (minecraft)
		world->raknetInstance = minecraft->raknetInstance;

	int dim = world->dimension ? world->dimension->id : 0;
	_worldByDim[dim] = world;

	WorldListener* l = new WorldListener(this, world);
	world->addListener(l);
	_listeners[world] = l;

	if (!level) {
		level = world;
		_mainWorld = world;
	}
	LOGI("SSNH attached world dim=%d (%p)\n", dim, world);
}

Level* ServerSideNetworkHandler::worldForPlayer(const Player* p) const
{
	if (!p) return _mainWorld;
	std::map<int, Level*>::const_iterator it = _worldByDim.find(p->dimension);
	if (it != _worldByDim.end())
		return it->second;
	return _mainWorld;
}

void ServerSideNetworkHandler::levelGenerated(Level* level)
{
	this->level = level;
	_mainWorld = level;

	if (minecraft->player) {
		minecraft->player->owner = rakPeer->GetMyGUID();
	}

	attachWorld(level);

#ifndef STANDALONE_SERVER
	allowIncomingConnections(minecraft->options.serverVisible);
#else
	allowIncomingConnections(true);
#endif
}

// ---------------------------------------------------------------------------
// Broadcasting
// ---------------------------------------------------------------------------
void ServerSideNetworkHandler::sendToWorld(Level* world, RakNet::BitStream& bitStream, PacketPriority priority, PacketReliability reliability, const RakNet::RakNetGUID* exclude)
{
	if (!world)
		return;
	int dim = world->dimension ? world->dimension->id : 0;
	for (std::map<RakNet::RakNetGUID, Player*>::iterator it = _remotePlayers.begin(); it != _remotePlayers.end(); ++it) {
		Player* p = it->second;
		if (!p)
			continue;
		if (exclude && *exclude == it->first)
			continue;
		if (p->dimension != dim)
			continue;
		rakPeer->Send(&bitStream, priority, reliability, 0, it->first, false);
	}
}

void ServerSideNetworkHandler::redistributePacket(Packet* packet, const RakNet::RakNetGUID& fromPlayer)
{
	// Send to every other player that shares the sender's dimension world.
	Level* w = NULL;
	std::map<RakNet::RakNetGUID, Player*>::iterator it = _remotePlayers.find(fromPlayer);
	if (it != _remotePlayers.end())
		w = worldForPlayer(it->second);
	else
		w = worldForPlayer(minecraft->player); // sender was the host player

	if (!w) {
		delete packet;
		return;
	}

	RakNet::BitStream bitStream;
	packet->write(&bitStream);
	sendToWorld(w, bitStream, packet->priority, packet->reliability, &fromPlayer);
	delete packet;
}

void ServerSideNetworkHandler::displayGameMessage(const std::string& message)
{
#ifndef STANDALONE_SERVER
	minecraft->gui.addMessage(message);
#else
	LOGI("%s\n", message.c_str());
#endif
	MessagePacket packet(message.c_str());
	RakNet::BitStream bitStream;
	packet.write(&bitStream);
	for (std::map<RakNet::RakNetGUID, Player*>::iterator it = _remotePlayers.begin(); it != _remotePlayers.end(); ++it)
		rakPeer->Send(&bitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, it->first, false);
}

// ---------------------------------------------------------------------------
// Chat（服务器权威）
//
// 客户端把聊天原文发上来，服务器负责：
//   1. 校验（非空 / 长度上限 / 频率上限）——自由文本是玩家唯一能随便塞东西的
//      入口，必须在权威端挡住；
//   2. '/' 开头当命令，交给**服务器**执行（不再交给客户端本地执行）；
//   3. 其余广播给同一维度世界的所有人，**包括发送者自己**：客户端发送时不回显，
//      统一等服务器广播回来 —— 所有人看到的内容与顺序以服务器为准。
// ---------------------------------------------------------------------------
void ServerSideNetworkHandler::sendMessageTo(const RakNet::RakNetGUID& source, const std::string& text)
{
	MessagePacket packet(text.c_str());
	RakNet::BitStream bitStream;
	packet.write(&bitStream);
	rakPeer->Send(&bitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, source, false);
}

void ServerSideNetworkHandler::handleServerCommand(Player* p, const RakNet::RakNetGUID& source, const std::string& cmd)
{
	LOGI("[cmd] %s: %s\n", p->name.c_str(), cmd.c_str());

	// 0) 服务器内置管理命令（需要管理员；/gamemode、/plugins、/list）
	{
		std::string body = (!cmd.empty() && cmd[0] == '/') ? cmd.substr(1) : cmd;
		std::string cname = body, cargs;
		size_t sp = body.find(' ');
		if (sp != std::string::npos) {
			cname = body.substr(0, sp);
			cargs = body.substr(sp + 1);
		}

		if (cname == "gamemode" || cname == "gm") {
			if (!isOp(p->name)) {
				sendMessageTo(source, "你没有权限（管理员名单在 ops.txt）");
				return;
			}
			std::string mode = cargs, target = p->name;
			size_t sp2 = cargs.find(' ');
			if (sp2 != std::string::npos) {
				mode = cargs.substr(0, sp2);
				target = cargs.substr(sp2 + 1);
			}
			int type = (mode == "creative" || mode == "c" || mode == "1") ? GameType::Creative
			         : (mode == "survival" || mode == "s" || mode == "0") ? GameType::Survival : -1;
			if (type < 0) {
				sendMessageTo(source, "用法: /gamemode creative|survival [玩家名]（不给名字就是改自己）");
				return;
			}
			std::string msg;
			setPlayerGameType(target, type, msg);
			sendMessageTo(source, msg);
			return;
		}

		if (cname == "op" || cname == "deop") {
			if (!isOp(p->name)) {
				sendMessageTo(source, "你没有权限（管理员名单在 ops.txt）");
				return;
			}
			if (cargs.empty()) {
				sendMessageTo(source, "用法: /" + cname + " <玩家名>");
				return;
			}
			if (cname == "op") {
				makeOp(cargs);
				sendMessageTo(source, cargs + " 现在是管理员了（他重新进服后生效）");
			} else {
				sendMessageTo(source, removeOp(cargs) ? (cargs + " 已被取消管理员")
				                                      : (cargs + " 本来就不是管理员"));
			}
			return;
		}

		if (cname == "plugins") {
#ifdef STANDALONE_SERVER
			if (cargs == "reload") {
				if (!isOp(p->name)) {
					sendMessageTo(source, "你没有权限（管理员名单在 ops.txt）");
					return;
				}
				PluginEngine::loadAll();
				PluginEngine::setHost(this);
				sendMessageTo(source, "插件已重载，共 " + std::to_string(PluginEngine::count()) + " 个");
			} else {
				sendMessageTo(source, "插件:" + PluginEngine::listText());
			}
#else
			sendMessageTo(source, "插件只在服务器上可用");
#endif
			return;
		}

		if (cname == "list") {
			sendMessageTo(source, "在线玩家:" + listOnlinePlayers());
			return;
		}
	}

	// 1) 插件（“文员”）注册的命令
#ifdef STANDALONE_SERVER
	{
		std::string reply;
		if (PluginEngine::runCommand(cmd, p->name, reply)) {
			if (!reply.empty())
				sendMessageTo(source, reply);
			return;
		}
	}
#endif

	// 1) 服务器 mod 注册的命令优先（JS: Commands.register）——"命令由 mod 提供"，
	//    客户端不再本地执行（见 ChatInputScreen::submit）。
#if defined(_WIN32) || defined(__ANDROID__)
	if (minecraft->modEngine) {
		std::string reply;
		if (minecraft->modEngine->runRegisteredCommand(cmd, p->name, reply)) {
			if (!reply.empty())
				sendMessageTo(source, reply);
			return;
		}
	}
#endif

	// 2) 服务器内置命令（/list /say …）后续在这里补；
	// 3) 都不是：明确回一句，免得玩家以为卡住了。
	sendMessageTo(source, "Unknown command: " + cmd);
}

void ServerSideNetworkHandler::handle(const RakNet::RakNetGUID& source, MessagePacket* packet)
{
	Player* p = getPlayer(source);
	if (!p)
		return;

	std::string msg = packet->message.C_String();

	// 长度上限（客户端输入框上限 120，这里放宽到 200 容错；再长直接丢）
	if (msg.empty() || msg.size() > 200)
		return;

	// 频率上限：同一玩家 1 秒内最多 5 条，超出静默丢弃（防刷屏）
	{
		unsigned int now = getTimeMs();
		ChatRate& r = _chatRate[source];
		if (r.windowStart == 0 || now - r.windowStart > 1000) {
			r.windowStart = now;
			r.count = 1;
		} else if (++r.count > 5) {
			return;
		}
	}

	if (msg[0] == '/') {
		// 与客户端保持一致：先给 mod 的 onChat 一次机会，再走服务器内置命令。
		// 模组常用聊天命令实现自己的功能（例如 /giant 召唤生物）。以前服务器
		// 把 /xxx 直接交给内置命令处理，mod 的 onChat 在联机时永远不触发 ——
		// 表现就是"模组命令在服务器里没反应"（生物召唤不出来等）。
		if (ModEngine::instance) {
			// 把“发消息的这个玩家”设为事件上下文，mod 里的 player.getX() 等才拿得到坐标
			ModEngine::instance->setEventPlayer(p);
			ModEngine::instance->fireEvent("onChat", msg);
			ModEngine::instance->setEventPlayer(NULL);
		}
		handleServerCommand(p, source, msg);
		return;
	}

	Level* world = worldForPlayer(p);
	if (!world)
		return;

	// 先让插件（“文员”）格式化这一行（例如给名字加称号前缀），再广播
#ifdef STANDALONE_SERVER
	// 插件（“文员”）可以否决这条消息（登录插件用它挡住未登录玩家的聊天）
	if (!PluginEngine::allowChat(p->name, msg))
		return;
	std::string line = PluginEngine::formatChat(p->name, msg);
#else
	std::string line = p->name + ": " + msg;
#endif
	LOGI("[chat] %s\n", line.c_str());

	// 与客户端一致：普通聊天也让 mod 的 onChat 看到（模组用它做聊天监听）
	if (ModEngine::instance) {
		ModEngine::instance->setEventPlayer(p);
		ModEngine::instance->fireEvent("onChat", msg);
		ModEngine::instance->setEventPlayer(NULL);
	}

	MessagePacket out(line.c_str());
	RakNet::BitStream bitStream;
	out.write(&bitStream);
	sendToWorld(world, bitStream, HIGH_PRIORITY, RELIABLE_ORDERED, NULL);
}

// ---------------------------------------------------------------------------
// World-scoped level events (from WorldListener bridges)
// ---------------------------------------------------------------------------
void ServerSideNetworkHandler::onWorldTileChanged(Level* world, int x, int y, int z)
{
	if (!world)
		return;
	UpdateBlockPacket packet(x, y, z, world->getTile(x, y, z), world->getData(x, y, z));
	RakNet::BitStream bitStream;
	packet.write(&bitStream);
	sendToWorld(world, bitStream, HIGH_PRIORITY, RELIABLE_ORDERED);
}

Packet* ServerSideNetworkHandler::getAddPacketFromEntity( Entity* entity ) {
	if (entity->isMob() && !entity->isPlayer()) { //@fix: This code is duplicated. See if it can be unified.
		// 注意：这里原本有一层 "if (minecraft->player)" 保护。那是**客户端房主
		// 模式**的假设（客户端自己有本地玩家）；独立服务器没有本地玩家
		// （minecraft->player 为 NULL），于是这个包永远不发 —— 表现就是
		// **服务器生成的生物（僵尸等）对客户端完全不可见**。
		// AddMobPacket 构造只读实体自身的数据，不需要 player，去掉这层即可。
		return new AddMobPacket((Mob*)entity);
	}
	else if (entity->isPlayer()) {

	} else if (entity->isItemEntity()) {
		AddItemEntityPacket* packet = new AddItemEntityPacket((ItemEntity*)entity);
		entity->xd = packet->xa();
		entity->yd = packet->ya();
		entity->zd = packet->za();
		return packet;
	} else if(entity->isHangingEntity()) {
		return new AddPaintingPacket((Painting*) entity);
	} else {
        int type = entity->getEntityTypeId();
        int data = entity->getAuxData();

        if (EntityTypes::IdFallingTile == type) {
            FallingTile* ft = (FallingTile*) entity;
            data = -(ft->tile | (ft->data << 16));
        }

		AddEntityPacket* packet = new AddEntityPacket(entity, data);
		return packet;
	}
	return NULL;
}

void ServerSideNetworkHandler::onWorldEntityAdded(Level* world, Entity* e)
{
	// Players are announced explicitly (join/dimension-switch flows); only
	// world entities (mobs, items, paintings...) are broadcast here.
	if (!e || e->isPlayer())
		return;
	Packet* packet = getAddPacketFromEntity(e);
	if (packet) {
		RakNet::BitStream bitStream;
		packet->write(&bitStream);
		sendToWorld(world, bitStream, packet->priority, packet->reliability);
		delete packet;
	}
}

void ServerSideNetworkHandler::onWorldEntityRemoved(Level* world, Entity* e)
{
	if (!e || e->isPlayer())
		return;
	RemoveEntityPacket packet(e->entityId);
	RakNet::BitStream bitStream;
	packet.write(&bitStream);
	sendToWorld(world, bitStream, packet.priority, packet.reliability);
}

void ServerSideNetworkHandler::onWorldLevelEvent(Level* world, Player* source, int type, int x, int y, int z, int data)
{
	if (!world) return;
	LevelEventPacket packet(type, x, y, z, data);
	RakNet::BitStream bitStream;
	packet.write(&bitStream);
	sendToWorld(world, bitStream, HIGH_PRIORITY, RELIABLE_ORDERED, source ? &source->owner : NULL);
}

void ServerSideNetworkHandler::onWorldTileEvent(Level* world, int x, int y, int z, int b0, int b1)
{
	if (!world) return;
	TileEventPacket packet(x, y, z, b0, b1);
	RakNet::BitStream bitStream;
	packet.write(&bitStream);
	sendToWorld(world, bitStream, HIGH_PRIORITY, RELIABLE_ORDERED);
}

void ServerSideNetworkHandler::onNewClient(const RakNet::RakNetGUID& clientGuid)
{
	LOGI("onNewClient, client guid: %s\n", clientGuid.ToString());
}

void ServerSideNetworkHandler::onDisconnect(const RakNet::RakNetGUID& guid)
{
	LOGI("onDisconnect\n");
	removeRemotePlayer(guid);
}

Player* ServerSideNetworkHandler::getPlayer(const RakNet::RakNetGUID& source)
{
	std::map<RakNet::RakNetGUID, Player*>::iterator it = _remotePlayers.find(source);
	if (it != _remotePlayers.end())
		return it->second;
	return NULL;
}

Player* ServerSideNetworkHandler::findServerPlayerById(int entityId)
{
	for (std::map<RakNet::RakNetGUID, Player*>::iterator it = _remotePlayers.begin(); it != _remotePlayers.end(); ++it) {
		if (it->second && it->second->entityId == entityId)
			return it->second;
	}
	return NULL;
}

void ServerSideNetworkHandler::removeRemotePlayer(const RakNet::RakNetGUID& guid)
{
	std::map<RakNet::RakNetGUID, Player*>::iterator it = _remotePlayers.find(guid);
	if (it == _remotePlayers.end())
		return;

	Player* player = it->second;
	_remotePlayers.erase(it);
	_modReadySent.erase(guid);

	if (player) {
		Level* w = worldForPlayer(player);
#ifdef STANDALONE_SERVER
		// 退出即落盘：位置/背包/血量/游戏模式写回 players/<名字>.dat
		if (w && PlayerStore::save(w, player))
			LOGI("[player] %s: saved on disconnect (pos=%.1f,%.1f,%.1f)\n",
			     player->name.c_str(), player->x, player->y, player->z);

		// 插件（“文员”）事件
		PluginEngine::fireLeave(player->name);

		// 释放这个玩家自己的 GameMode 实例（避免泄漏）
		{
			std::map<RakNet::RakNetGUID, GameMode*>::iterator git = _playerGameModes.find(guid);
			if (git != _playerGameModes.end()) {
				delete git->second;
				_playerGameModes.erase(git);
			}
		}
#endif
		std::string message = player->name;
		message += " disconnected from the game";
		displayGameMessage(message);

		// tell everyone else in the same world they left
		RemovePlayerPacket rp(player);
		RakNet::BitStream bs;
		rp.write(&bs);
		sendToWorld(w, bs, rp.priority, rp.reliability, &guid);

		player->reallyRemoveIfPlayer = true;
		if (w)
			w->removeEntity(player);
	}
}

// ---------------------------------------------------------------------------
// Dimension switching (server authoritative)
// ---------------------------------------------------------------------------
void ServerSideNetworkHandler::switchHostToDimension(int dimensionId, float x, float y, float z)
{
	if (minecraft->player)
		teleportPlayerToDimension(minecraft->player, dimensionId, x, y, z);
}

void ServerSideNetworkHandler::teleportPlayerToDimension(Player* p, int dimensionId, float x, float y, float z)
{
	// 维度传送诊断（定位服务器闪退）：每一步落一条到 logs/mod.log
#define DIMLOG(msg) do { if (ModEngine::instance) ModEngine::instance->log(std::string("[dim] ") + (msg)); } while (0)
	if (!p)
		return;
	DIMLOG(std::string("teleport ") + p->name + " -> dim " + std::to_string(dimensionId)
	       + " at " + std::to_string((int)x) + "," + std::to_string((int)y) + "," + std::to_string((int)z));

	bool isHost = (p == minecraft->player);

	Level* oldWorld = worldForPlayer(p);

	// Resolve (or lazily create) the target world.
	Level* newWorld = NULL;
	std::map<int, Level*>::iterator it = _worldByDim.find(dimensionId);
	if (it != _worldByDim.end())
		newWorld = it->second;
	else
		newWorld = minecraft->getOrCreateDimensionLevel(dimensionId);
	if (!newWorld) {
		LOGW("teleportPlayerToDimension: no world for dim %d\n", dimensionId);
		return;
	}

	DIMLOG(std::string("target world ") + (newWorld ? "ok" : "null")
	       + " old=" + (oldWorld ? "ok" : "null")
	       + " same=" + ((newWorld == oldWorld) ? "1" : "0"));
	if (newWorld == oldWorld) {
		// Same world: plain teleport; tell a remote player's client about it.
		p->moveTo(x, y, z, p->yRot, p->xRot);
		if (!isHost) {
			MovePlayerPacket mp(p->entityId, x, y - p->heightOffset, z, p->yRot, p->xRot);
			RakNet::BitStream bitStream;
			mp.write(&bitStream);
			rakPeer->Send(&bitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, p->owner, false);
		}
		return;
	}

	// 1) tell the players of the world we are leaving
	{
		RemovePlayerPacket rp(p);
		RakNet::BitStream bs;
		rp.write(&bs);
		sendToWorld(oldWorld, bs, rp.priority, rp.reliability, &p->owner);
	}

	DIMLOG("step1 leave-announce done");

	// 2) 房主自己换维度：统一走引擎的“进入世界”流程（后台加载 + ProgressScreen 加载页），
	//    与单机路径一致。以前那条就地 switchActiveLevel 是同步的、没有加载页。
	//    实体搬运、存档、玩家落点都由 beginDimensionTravel 负责，这里不再重复。
	if (isHost) {
		attachWorld(newWorld);
		minecraft->beginDimensionTravel(dimensionId, x, y, z);
		DIMLOG("step2 host travel started (with loading screen)");
		// 4a) 把房主公告给新世界的远端玩家（远端还没切过来时这包是空发，无害）
		AddPlayerPacket ap(p);
		RakNet::BitStream bs;
		ap.write(&bs);
		sendToWorld(newWorld, bs, ap.priority, ap.reliability);
		DIMLOG("step3 host advertised to new world");
		return;
	}

	// —— 远端玩家：仍走"发维度包让客户端自己加载"那条。
	{
		SetDimensionPacket sd(dimensionId, x, y, z);
		RakNet::BitStream bs;
		sd.write(&bs);
		rakPeer->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, p->owner, false);
	}
	DIMLOG("step2 dimension packet sent");
	attachWorld(newWorld);

	// 3) move the entity between worlds (entity id preserved, no delete)
	p->dimension = dimensionId;
	p->moveTo(x, y, z, p->yRot, p->xRot);
	if (oldWorld->getEntity(p->entityId) == p)
		oldWorld->moveEntityTo(p, newWorld);
	else
		newWorld->addEntity(p);

	DIMLOG(std::string("step3 entity moved, dim now ") + std::to_string(p->dimension));

	// 4b) announce the arrival to the other players of the new world
	{
		AddPlayerPacket ap(p);
		RakNet::BitStream bs;
		ap.write(&bs);
		sendToWorld(newWorld, bs, ap.priority, ap.reliability, &p->owner);
	}

	// 5) snapshot: current players + entities of the new world
	DIMLOG("step4 arrival announced, sending snapshot");
	sendSnapshotTo(p);
	DIMLOG("step5 snapshot sent, teleport complete");
#undef DIMLOG
}

// ---------------------------------------------------------------------------
// Packet handling
// ---------------------------------------------------------------------------
void ServerSideNetworkHandler::handle(const RakNet::RakNetGUID& source, LoginPacket* packet)
{
	if (!level) return;
	if (!_allowIncoming) return;

	LOGI("LoginPacket\n");
	// 落一条到 logs/mod.log：以后"客户端到底连上没有"一眼可查（LOGI 只到控制台）
	if (ModEngine::instance)
		ModEngine::instance->log(std::string("[srv] login: ") + packet->clientName.C_String());

	// 登录准入：封禁 / 白名单（插件维护的名单，所以判定走插件引擎）
#ifdef STANDALONE_SERVER
	{
		std::string reject;
		if (!PluginEngine::checkJoinAllowed(packet->clientName.C_String(), reject)) {
			LOGI("[server] 拒绝 %s 进服：%s\n", packet->clientName.C_String(), reject.c_str());
			RakNet::BitStream bs;
			MessagePacket(reject.c_str()).write(&bs);
			rakPeer->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, source, false);
			rakPeer->CloseConnection(source, true);
			return;
		}
	}
#endif

	int loginStatus = LoginStatus::Success;
	bool oldClient = packet->clientNetworkVersion < SharedConstants::NetworkProtocolLowestSupportedVersion;
	bool oldServer = packet->clientNetworkLowestSupportedVersion > SharedConstants::NetworkProtocolVersion;
	if (oldClient || oldServer)
		loginStatus = oldClient? LoginStatus::Failed_ClientOld : LoginStatus::Failed_ServerOld;

	RakNet::BitStream bitStream;
	LoginStatusPacket(loginStatus).write(&bitStream);
	rakPeer->Send(&bitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, source, false);

	if (LoginStatus::Success != loginStatus)
		return;

	//
	// Valid client version - the player joins the main (dimension 0) world.
	//
	Level* main = _mainWorld ? _mainWorld : level;
	Player* newPlayer = new ServerPlayer(minecraft, main);
	newPlayer->dimension = main->dimension ? main->dimension->id : 0;

	minecraft->gameMode->initAbilities(newPlayer->abilities);
	newPlayer->owner = source;
	newPlayer->name = packet->clientName.C_String();

#ifdef STANDALONE_SERVER
	// 读回这个人上次的存档（位置/朝向/血量/饥饿/经验/背包/游戏模式）。
	// 没有文件 = 新玩家，保持上面刚初始化好的默认值。
	if (PlayerStore::load(main, newPlayer)) {
		applyPlayerGameType(newPlayer);
		LOGI("[player] %s: loaded save (mode=%d pos=%.1f,%.1f,%.1f)\n",
		     newPlayer->name.c_str(), newPlayer->serverGameType,
		     newPlayer->x, newPlayer->y, newPlayer->z);
	} else {
		LOGI("[player] %s: new player\n", newPlayer->name.c_str());
	}
#endif

	_pendingPlayers.push_back(newPlayer);
	_remotePlayers[source] = newPlayer;

	// Reset the player so he doesn't spawn inside blocks
	while (newPlayer->y > 0) {
		newPlayer->setPos(newPlayer->x, newPlayer->y, newPlayer->z);
		if (main->getCubes(newPlayer, newPlayer->bb).size() == 0) break;
		newPlayer->y += 1;
	}
	newPlayer->moveTo(newPlayer->x, newPlayer->y - newPlayer->heightOffset, newPlayer->z, newPlayer->yRot, newPlayer->xRot);

	//
	// Mod auto-sync handshake: tell the client which mods the host world
	// uses. StartGame is only sent once the client reports everything is
	// installed & enabled (ModsReadyPacket).
	//
	_modReadySent[source] = false;
	sendModListTo(source);
}

void ServerSideNetworkHandler::handle(const RakNet::RakNetGUID& source, ModListPacket* packet)
{
	// The server never receives mod lists.
}

void ServerSideNetworkHandler::handle(const RakNet::RakNetGUID& source, ModFilePacket* packet)
{
	// The server never receives mod file chunks.
}

void ServerSideNetworkHandler::handle(const RakNet::RakNetGUID& source, ModsReadyPacket* packet)
{
	if (!getPlayer(source))
		return;
	if (_modReadySent[source])
		return;
	LOGI("ModsReadyPacket: sending StartGame\n");
	_modReadySent[source] = true;
	sendStartGame(source);
}

void ServerSideNetworkHandler::handle(const RakNet::RakNetGUID& source, ModRequestPacket* packet)
{
	Player* p = getPlayer(source);
	if (!p)
		return;
#if defined(_WIN32) || defined(__ANDROID__)
#if defined(STANDALONE_SERVER)
	// 独立服务器：mod 源是 mods/*.zip（没有 ModEngine）
	std::vector<std::string> enabled = serverModFiles();
#else
	if (!minecraft->modEngine)
		return;
	std::vector<std::string> enabled = minecraft->modEngine->getEnabledModFiles();
#endif
	std::vector<unsigned char> bytes;
	const int kChunk = 48 * 1024;
	for (size_t i = 0; i < packet->files.size() && i < 256; ++i) {
		const std::string& file = packet->files[i];
		bool allowed = false;
		for (size_t j = 0; j < enabled.size(); ++j)
			if (enabled[j] == file) { allowed = true; break; }
		if (!allowed)
			continue;
#if defined(STANDALONE_SERVER)
		if (!readServerModFileBytes(file, bytes))
			continue;
#else
		if (!minecraft->modEngine->readModFileBytes(file, bytes))
			continue;
#endif
		int total = (int)bytes.size();
		for (int off = 0; off < total; off += kChunk) {
			int len = (std::min)(kChunk, total - off);
			ModFilePacket fp(file, off, total, bytes.data() + off, len);
			RakNet::BitStream bitStream;
			fp.write(&bitStream);
			rakPeer->Send(&bitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, source, false);
		}
	}
#endif
}

void ServerSideNetworkHandler::sendModListTo(const RakNet::RakNetGUID& source)
{
	ModListPacket packet;
#if defined(_WIN32) || defined(__ANDROID__)
#if defined(STANDALONE_SERVER)
	// 独立服务器：下发 mods/*.zip（没有 ModEngine）
	{
		std::vector<std::string> files = serverModFiles();
		for (size_t i = 0; i < files.size() && i < 256; ++i) {
			long sz = serverModFileSize(files[i]);
			if (sz < 0)
				continue;
			ModListPacket::Entry e;
			e.file = files[i];
			e.size = (int)sz;
			packet.entries.push_back(e);
		}
	}
#else
	if (minecraft->modEngine) {
		std::vector<std::string> files = minecraft->modEngine->getEnabledModFiles();
		for (size_t i = 0; i < files.size() && i < 256; ++i) {
			long sz = minecraft->modEngine->modFileSize(files[i]);
			if (sz < 0)
				continue;
			ModListPacket::Entry e;
			e.file = files[i];
			e.size = (int)sz;
			packet.entries.push_back(e);
		}
	}
#endif
#endif
	LOGI("sendModListTo: %d mod(s)\n", (int)packet.entries.size());
	RakNet::BitStream bitStream;
	packet.write(&bitStream);
	rakPeer->Send(&bitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, source, false);
}

void ServerSideNetworkHandler::sendStartGame(const RakNet::RakNetGUID& source)
{
	Player* newPlayer = getPlayer(source);
	if (!newPlayer)
		return;
	Level* main = worldForPlayer(newPlayer);
	if (!main)
		return;

	RakNet::BitStream bitStream;
	// 发给**这个玩家**的游戏模式：优先用他自己的（存档里记着的 / 控制台改过的），
	// 否则用服务器全局配置。客户端收到 StartGame 后会按这个值重建自己的玩家
	// （isCreative → 能飞行 / 创造界面），所以“给某人单独开创造”就是靠这里生效。
	int gameType = (newPlayer->serverGameType >= 0)
		? newPlayer->serverGameType
		: (minecraft->isCreativeMode() ? GameType::Creative : GameType::Survival);

	StartGamePacket(
		main->getSeed(),
		main->getLevelData()->getGeneratorVersion(),
		gameType,
		newPlayer->entityId,
		newPlayer->x, newPlayer->y - newPlayer->heightOffset, newPlayer->z,
		main->getLevelData()->getWorldType(),
		newPlayer->dimension
	).write(&bitStream);

	rakPeer->Send(&bitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, source, false);
}

void ServerSideNetworkHandler::handle(const RakNet::RakNetGUID& source, ReadyPacket* packet)
{
	if (!level) return;

	if (packet->type == ReadyPacket::READY_CLIENTGENERATION)
		onReady_ClientGeneration(source);

	if (packet->type == ReadyPacket::READY_REQUESTEDCHUNKS)
		onReady_RequestedChunks(source);
}

void ServerSideNetworkHandler::onReady_ClientGeneration(const RakNet::RakNetGUID& source)
{
	Player* newPlayer = popPendingPlayer(source);
	if (!newPlayer) {
		for (int i = 0; i < 3; ++i)
			LOGE("We don't have a user associated with this player!\n");
		return;
	}
	Level* main = worldForPlayer(newPlayer); // the main world on join

	RakNet::BitStream bitStream;

	// send level time
	SetTimePacket(main->getTime()).write(&bitStream);
	rakPeer->Send(&bitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, source, false);

	// send all pre-existing players (incl. the host player) in this world
	const PlayerList& players = main->players;
	for (unsigned int i = 0; i < players.size(); i++) {
		Player* player = players[i];
		if (player == newPlayer) continue;

        bitStream.Reset();
        AddPlayerPacket(player).write(&bitStream);
        rakPeer->Send(&bitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, source, false);

        if (player->getArmorTypeHash()) {
            bitStream.Reset();
            PlayerArmorEquipmentPacket(player).write(&bitStream);
            rakPeer->Send(&bitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, source, false);
        }
	}

	main->addEntity(newPlayer);
	_remotePlayers[source] = newPlayer;
	// 加入消息广播给同一世界的所有人（客户端显示 + 服务器控制台各一条）
	displayGameMessage(newPlayer->name + " joined the game");

#ifdef STANDALONE_SERVER
	// 管理员名单为空时把第一个进服的人设为管理员（否则没控制台窗口就没人能管服）
	maybeAutoOp(newPlayer);
	// 插件（“文员”）事件
	PluginEngine::fireJoin(newPlayer->name);
#endif

	// 玩家行为日志：进服时的位置（后续挖/放/移动都会跟着记）
	{
		Level* jw = worldForPlayer(newPlayer);
		LOGI("[act] %s spawned at (%.1f, %.1f, %.1f) dim=%d\n",
		     newPlayer->name.c_str(), newPlayer->x, newPlayer->y, newPlayer->z,
		     (jw && jw->dimension) ? jw->dimension->id : 0);
	}

	// 进服欢迎语（server.properties 的 motd）：只发给刚进来的这个人
	if (!_motd.empty())
		sendMessageTo(source, _motd);

	// Send all Entities in this world to the new player
	for (unsigned int i = 0; i < main->entities.size(); ++i) {
		Entity* e = main->entities[i];
		if (e->isPlayer() || e == newPlayer) continue;
		Packet* packet = getAddPacketFromEntity(e);
		if(packet != NULL) {
			bitStream.Reset();
			packet->write(&bitStream);
			rakPeer->Send(&bitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, source, false);
			delete packet;
		}
	}

	// broadcast the new player to the other players of this world
	{
		bitStream.Reset();
		AddPlayerPacket ap(newPlayer);
		ap.write(&bitStream);
		sendToWorld(main, bitStream, ap.priority, ap.reliability, &source);
	}
}

//
// Messages to be sent after client has finished applying changes
//
void ServerSideNetworkHandler::onReady_RequestedChunks(const RakNet::RakNetGUID& source)
{
	Player* p = getPlayer(source);
	if (!p) return;
	Level* w = worldForPlayer(p);

	RakNet::BitStream bitStream;
	// Send all TileEntities of the player's world to the player
	for (unsigned int i = 0; i < w->tileEntities.size(); ++i) {
		TileEntity* e = w->tileEntities[i];
		Packet* packet = e->getUpdatePacket();
		if (packet != NULL) {
			bitStream.Reset();
			packet->write(&bitStream);
			rakPeer->Send(&bitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, source, false);
			delete packet;
		}
	}
}

void ServerSideNetworkHandler::handle(const RakNet::RakNetGUID& source, MovePlayerPacket* packet)
{
	Player* p = getPlayer(source);
	if (!p) return;
	Level* w = worldForPlayer(p);

	if (Entity* entity = w->getEntity(packet->entityId))
	{
		entity->xd = entity->yd = entity->zd = 0;
		entity->lerpTo(packet->x, packet->y, packet->z, packet->yRot, packet->xRot, 3);

		// broadcast this packet to other clients in the same world
		RakNet::BitStream bitStream;
		packet->write(&bitStream);
		sendToWorld(w, bitStream, packet->priority, packet->reliability, &source);
	}

	// 反作弊：移动速度异常只记日志，不拉回（传送/卡顿/重连都会造成大位移）
#ifdef STANDALONE_SERVER
	{
		SpeedSample& sp = _speedCheck[source];
		unsigned int now = getTimeMs();
		if (sp.ms != 0 && now > sp.ms) {
			float dx = packet->x - sp.x, dy = packet->y - sp.y, dz = packet->z - sp.z;
			float dist = sqrtf(dx * dx + dy * dy + dz * dz);
			float secs = (now - sp.ms) / 1000.0f;
			if (secs > 0.05f && dist / secs > 25.0f)   // 跑跳大约 6 格/秒，25 已很宽松
				logCheat(source, p, "移动速度异常", packet->x, packet->y, packet->z);
		}
		sp.x = packet->x; sp.y = packet->y; sp.z = packet->z; sp.ms = now;
	}
#endif

	// 玩家行为日志：谁移动到哪。移动包每 tick 都来 → 节流：位移超过 3 格、
	// 或距上次记录超过 5 秒才记一条，否则控制台会被刷爆。
	{
		unsigned int now = getTimeMs();
		MoveLog& ml = _moveLog[source];
		float dx = packet->x - ml.x, dy = packet->y - ml.y, dz = packet->z - ml.z;
		if (ml.lastMs == 0 || dx * dx + dy * dy + dz * dz > 9.0f || now - ml.lastMs > 5000) {
			ml.x = packet->x; ml.y = packet->y; ml.z = packet->z; ml.lastMs = now;
			LOGI("[act] %s moved to (%.1f, %.1f, %.1f) dim=%d\n",
			     p->name.c_str(), packet->x, packet->y, packet->z,
			     w->dimension ? w->dimension->id : 0);
		}
	}
}

void ServerSideNetworkHandler::handle(const RakNet::RakNetGUID& source, RemoveBlockPacket* packet){
	Player* player = getPlayer(source);
	if (!player) return;
	Level* w = worldForPlayer(player);

	player->swing();

	int x = packet->x, y = packet->y, z = packet->z;

#ifdef STANDALONE_SERVER
	// 反作弊：够不着 / 手速过快，直接丢弃这个包（服务器说了算）
	if (!rateAllow(_breakRate, source, 25) || !withinReach(player, x + 0.5f, y + 0.5f, z + 0.5f)) {
		logCheat(source, player, "破坏方块被拒（距离或频率）", x + 0.5f, y + 0.5f, z + 0.5f);
		return;
	}
	// 插件（“文员”）可以否决这次破坏（登录插件靠它拦住未登录的玩家）
	if (!PluginEngine::allowBlockBreak(player->name, x, y, z))
		return;
#endif

	// code copied from GameMode.cpp
	int oldId = w->getTile(x, y, z);
	int data = w->getData(x, y, z);
	Tile* oldTile = Tile::tiles[oldId];
	bool changed = w->setTile(x, y, z, 0);
	if (oldTile != NULL && changed) {
		w->playSound(x + 0.5f, y + 0.5f, z + 0.5f, oldTile->soundType->getBreakSound(), (oldTile->soundType->getVolume() + 1) / 2, oldTile->soundType->getPitch() * 0.8f);

		// 玩家行为日志：谁在哪里挖掉了什么方块
		LOGI("[act] %s broke %s(id %d) @ (%d, %d, %d) dim=%d\n",
		     player->name.c_str(), oldTile->getDescriptionId().c_str(), oldId, x, y, z,
		     w->dimension ? w->dimension->id : 0);

		if (gameModeFor(source, player)->isSurvivalType() && player->canDestroy(oldTile))
			oldTile->playerDestroy(w, player, x, y, z, data);

		oldTile->destroy(w, x, y, z, data);
	}
}

void ServerSideNetworkHandler::handle(const RakNet::RakNetGUID& source, RequestChunkPacket* packet)
{
	Player* p = getPlayer(source);
	if (!p)
        return;
	Level* w = worldForPlayer(p);
#ifdef STANDALONE_SERVER
	// 排队，由 tick() 每 tick 限量生成（见 _pendingChunks 说明）
	{
		std::pair<int, int> key(packet->x, packet->z);
		if (_pendingChunkKeys.insert(key).second) {
			PendingChunk pc;
			pc.guid = source;
			pc.x = packet->x;
			pc.z = packet->z;
			_pendingChunks.push_back(pc);
			if (_pendingChunks.size() > 600) {   // 防积压
				_pendingChunkKeys.erase(std::make_pair(_pendingChunks.front().x,
				                                       _pendingChunks.front().z));
				_pendingChunks.pop_front();
			}
		}
	}
	return;
#else
    LevelChunk* chunk = w->getChunk(packet->x, packet->z);

	if (!chunk)
        return;

	ChunkDataPacket cpacket(chunk->x, chunk->z, chunk);

	RakNet::BitStream bitStream;
	cpacket.write(&bitStream);
	rakPeer->Send(&bitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, source, false);

    const LevelChunk::TEMap& teMap = chunk->getTileEntityMap();
    for (LevelChunk::TEMapCIterator cit = teMap.begin(); cit != teMap.end(); ++cit) {
        TileEntity* te = cit->second;
        if (Packet* p = te->getUpdatePacket()) {
            bitStream.Reset();
            p->write(&bitStream);
            rakPeer->Send(&bitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, source, false);
            delete p;
        }
    }
#endif
}

void ServerSideNetworkHandler::handle(const RakNet::RakNetGUID& source, PlayerEquipmentPacket* packet)
{
	Player* player = getPlayer(source);
	if (!player) return;
	if (rakPeer->GetMyGUID() == player->owner) return;
	Level* w = worldForPlayer(player);

	// override the player's inventory
	int slot = Inventory::MAX_SELECTION_SIZE;
	if (slot >= 0) {
		if (packet->itemId == 0) {
			player->inventory->clearSlot(slot);
		} else {
			// @note: 128 is an ugly hack for depletable items.
			// @todo: fix
			ItemInstance newItem(packet->itemId, 128, packet->itemAuxValue);
			player->inventory->replaceSlot(slot, &newItem);
		}
		player->inventory->moveToSelectedSlot(slot, true);
		RakNet::BitStream bitStream;
		packet->write(&bitStream);
		sendToWorld(w, bitStream, packet->priority, packet->reliability, &source);
	} else {
		LOGW("Warning: Remote player doesn't have his thing, Odd!\n");
	}
}

void ServerSideNetworkHandler::handle(const RakNet::RakNetGUID& source, PlayerArmorEquipmentPacket* packet) {
    Player* player = getPlayer(source);
    if (!player) return;
    if (rakPeer->GetMyGUID() == player->owner) return;
	Level* w = worldForPlayer(player);

    packet->fillIn(player);
	RakNet::BitStream bitStream;
	packet->write(&bitStream);
	sendToWorld(w, bitStream, packet->priority, packet->reliability, &source);
}

void ServerSideNetworkHandler::handle(const RakNet::RakNetGUID& source, InteractPacket* packet) {
	Player* p = getPlayer(source);
	if (!p) return;
	Level* w = worldForPlayer(p);

	Entity* src = w->getEntity(packet->sourceId);
	Entity* entity = w->getEntity(packet->targetId);
	if (src && entity && src->isPlayer())
	{
		Player* player = (Player*) src;
		if (InteractPacket::Attack == packet->action) {
#ifdef STANDALONE_SERVER
			// 反作弊：打得到才生效（距离 + 攻击频率）
			if (!rateAllow(_interactRate, source, 8) ||
			    !withinReach(player, entity->x, entity->y + 1.0f, entity->z)) {
				logCheat(source, player, "攻击被拒（距离或频率）", entity->x, entity->y, entity->z);
				return;
			}
			// 插件（“文员”）可以否决这次攻击
			if (!PluginEngine::allowAttack(player->name, packet->targetId))
				return;
#endif
			player->swing();
			gameModeFor(source, player)->attack(player, entity);
		}
		if (InteractPacket::Interact == packet->action) {
			player->swing();
			gameModeFor(source, player)->interact(player, entity);
		}

		RakNet::BitStream bitStream;
		packet->write(&bitStream);
		sendToWorld(w, bitStream, packet->priority, packet->reliability, &source);
	}
}

void ServerSideNetworkHandler::handle(const RakNet::RakNetGUID& source, AnimatePacket* packet)
{
	Player* p = getPlayer(source);
	if (!p)
        return;
	Level* w = worldForPlayer(p);

    // Own player -> invalid
    if (minecraft->player && minecraft->player->entityId == packet->entityId) {
        return;
    }

	Entity* entity = w->getEntity(packet->entityId);
	if (!entity || !entity->isPlayer())
        return;

    Player* player = (Player*) entity;

	switch (packet->action) {
	case AnimatePacket::Swing:
		player->swing();
		break;
	default:
		LOGW("Unknown Animate action: %d\n", packet->action);
		break;
	}
	RakNet::BitStream bitStream;
	packet->write(&bitStream);
	sendToWorld(w, bitStream, packet->priority, packet->reliability, &source);
}

void ServerSideNetworkHandler::handle(const RakNet::RakNetGUID& source, PlaceBlockPacket* packet)
{
	Player* p = getPlayer(source);
	if (!p) return;
	Level* w = worldForPlayer(p);
	if (!w) return;

	int id = packet->blockId;
	if (id < 0 || id >= 256) return;
	if (Tile::tiles[id] == NULL) {
		LOGI("[mod] ignored client block id=%d (no such tile)\n", id);
		return;
	}
	int x = packet->x, y = packet->y, z = packet->z;
	if (!w->setTileAndData(x, y, z, id, packet->blockData))
		return;
	// 让同一个世界的其它玩家也看到这块（例如天境传送门方块）
	UpdateBlockPacket up(x, y, z, id, packet->blockData);
	RakNet::BitStream bs;
	up.write(&bs);
	sendToWorld(w, bs, up.priority, up.reliability, &p->owner);
	LOGI("[mod] client set block id=%d @(%d,%d,%d) by %s\n", id, x, y, z, p->name.c_str());
}

void ServerSideNetworkHandler::handle(const RakNet::RakNetGUID& source, UseItemPacket* packet)
{
	Player* p = getPlayer(source);
	if (!p) return;
	Level* w = worldForPlayer(p);

	LOGI("UseItemPacket\n");
	Entity* entity = w->getEntity(packet->entityId);
	if (entity && entity->isPlayer()) {
		Player* player = (Player*) entity;
		int x = packet->x, y = packet->y, z = packet->z;
		Tile* t = Tile::tiles[w->getTile(x, y, z)];

		if (t == Tile::invisible_bedrock) return;
		if (t && t->use(w, x, y, z, player)) return;
		if (packet->item.isNull()) return;

		ItemInstance* item = &packet->item;

		if(packet->face == 255) {
            // Special case: x,y,z means direction-of-action
            player->aimDirection.set(packet->x / 32768.0f, packet->y / 32768.0f, packet->z / 32768.0f);
			gameModeFor(source, player)->useItem(player, w, item);
		}
		else {
#ifdef STANDALONE_SERVER
			// 反作弊：放置/使用也要看距离与频率
			if (!rateAllow(_placeRate, source, 25) || !withinReach(player, x + 0.5f, y + 0.5f, z + 0.5f)) {
				logCheat(source, player, "放置/使用被拒（距离或频率）", x + 0.5f, y + 0.5f, z + 0.5f);
				return;
			}
			// 插件（“文员”）可以否决这次放置
			if (!PluginEngine::allowBlockPlace(player->name, x, y, z))
				return;
#endif
			// 玩家行为日志：放置/交互。useItemOn 可能只是开箱子/开门，所以比较目标
			// 格子的方块有没有变：变了才是真放置。
			int beforeId = w->getTile(x, y, z);
			gameModeFor(source, player)->useItemOn(player, w, item, packet->x, packet->y, packet->z, packet->face,
				Vec3(packet->clickX + packet->x, packet->clickY + packet->y, packet->clickZ + packet->z));
			int afterId = w->getTile(x, y, z);
			if (afterId != beforeId) {
				Tile* placed = Tile::tiles[afterId];
				LOGI("[act] %s placed %s(id %d) @ (%d, %d, %d) dim=%d\n",
				     player->name.c_str(), placed ? placed->getDescriptionId().c_str() : "?", afterId,
				     x, y, z, w->dimension ? w->dimension->id : 0);
			}
		}
	}
}

void ServerSideNetworkHandler::handle(const RakNet::RakNetGUID& source, EntityEventPacket* packet) {
	Player* p = getPlayer(source);
	if (!p) return;
	Level* w = worldForPlayer(p);

	if (Entity* e = w->getEntity(packet->entityId))
		e->handleEntityEvent(packet->eventId);
}

void ServerSideNetworkHandler::handle(const RakNet::RakNetGUID& source, PlayerActionPacket* packet)
{
	Player* p = getPlayer(source);
	if (!p) return;
	LOGI("PlayerActionPacket\n");
	Level* w = worldForPlayer(p);
	Entity* entity = w->getEntity(packet->entityId);
	if (entity && entity->isPlayer()) {
		Player* player = (Player*) entity;
		if(packet->action == PlayerActionPacket::RELEASE_USE_ITEM) {
			gameModeFor(source, player)->releaseUsingItem(player);
			return;
		}
		else if(packet->action == PlayerActionPacket::STOP_SLEEPING) {
			player->stopSleepInBed(true, true, true);
		}
	}
}

// 模组（玩家姿态/动作）：某客户端上报“我的动作变了”。
//   - 只认“汇报自己”：playerId 一律取发送者的实体 id，不信包里的值（防冒充）。
//   - 服务器侧也记一份（服务器上的 mod 可查询/驱动表现）。
//   - 广播给同世界的其他玩家（排除发送者：他自己那边已经立即生效了）。
void ServerSideNetworkHandler::handle(const RakNet::RakNetGUID& source, PlayerPosePacket* packet)
{
	Player* p = getPlayer(source);
	if (!p) return;
	const int pid = p->entityId;
	if (ModEngine::instance)
		ModEngine::instance->applyAppearance(pid, packet->action.C_String(), packet->model.C_String());
	Level* w = worldForPlayer(p);
	if (!w) return;
	PlayerPosePacket out(pid, packet->action, packet->model);
	RakNet::BitStream bs;
	out.write(&bs);
	sendToWorld(w, bs, HIGH_PRIORITY, RELIABLE_ORDERED, &source);
}

void ServerSideNetworkHandler::handle( const RakNet::RakNetGUID& source, RespawnPacket* packet )
{
	Player* p = getPlayer(source);
	if (!p) return;
	Level* w = worldForPlayer(p);

	NetEventCallback::handle(w, source, packet );
	RakNet::BitStream bitStream;
	packet->write(&bitStream);
	sendToWorld(w, bitStream, packet->priority, packet->reliability, &source);
}

void ServerSideNetworkHandler::handle( const RakNet::RakNetGUID& source, SendInventoryPacket* packet )
{
	Player* p = getPlayer(source);
	if (!p) return;
	Level* w = worldForPlayer(p);

	Entity* entity = w->getEntity(packet->entityId);
	if (entity && entity->isPlayer()) {
		Player* pl = (Player*)entity;
		pl->inventory->replace(packet->items, packet->numItems);
		if ((packet->extra & SendInventoryPacket::ExtraDrop) != 0) {
			pl->inventory->dropAll(false);
            //@todo @armor : Drop armor
		}
	}
}

void ServerSideNetworkHandler::handle( const RakNet::RakNetGUID& source, DropItemPacket* packet )
{
	Player* p = getPlayer(source);
	if (!p) return;
	Level* w = worldForPlayer(p);

	Entity* entity = w->getEntity(packet->entityId);
	if (entity && entity->isPlayer()) {
		Player* pl = (Player*)entity;
		pl->drop(new ItemInstance(packet->item), packet->dropType != 0);
	}
}

void ServerSideNetworkHandler::handle(const RakNet::RakNetGUID& source, ContainerClosePacket* packet) {
	Player* p = getPlayer(source);
	if (!p) return;
	if (p != minecraft->player)
		static_cast<ServerPlayer*>(p)->doCloseContainer();
}

void ServerSideNetworkHandler::handle(const RakNet::RakNetGUID& source, ContainerSetSlotPacket* packet) {
	Player* p = getPlayer(source);
	if (!p) return;

	if (p->containerMenu == NULL) {
		LOGW("User has no container!\n");
		return;
	}
	if (p->containerMenu->containerId != packet->containerId)
	{
		LOGW("Wrong container id: %d vs %d\n", p->containerMenu->containerId, packet->containerId);
		return;
	}

	if (ContainerType::FURNACE == p->containerMenu->containerType) {
		p->containerMenu->setSlot(packet->slot, &packet->item);
	}
	if (ContainerType::CONTAINER == p->containerMenu->containerType) {
		p->containerMenu->setSlot(packet->slot, &packet->item);
	}
}

void ServerSideNetworkHandler::handle( const RakNet::RakNetGUID& source, SetHealthPacket* packet )
{
	Player* p = getPlayer(source);
	if (!p) return;
	Player* pl = p;
	if (packet->health <= -32) {
		int diff = packet->health - SetHealthPacket::HEALTH_MODIFY_OFFSET;
		if (diff > 0) pl->hurt(NULL, diff);
		else if (diff < 0) pl->heal(-diff);
	}
}

void ServerSideNetworkHandler::handle( const RakNet::RakNetGUID& source, SignUpdatePacket* packet ) {
	Player* p = getPlayer(source);
	if (!p) return;
	Level* w = worldForPlayer(p);

	RakNet::BitStream bitStream;
	packet->write(&bitStream);
	sendToWorld(w, bitStream, packet->priority, packet->reliability, &source);

	TileEntity* te = w->getTileEntity(packet->x, packet->y, packet->z);
	if (TileEntity::isType(te, TileEntityType::Sign)) {
		SignTileEntity* ste = (SignTileEntity*) te;
		if (ste->isEditable()) {
			for (int i = 0; i < SignTileEntity::NUM_LINES; i++) {
				ste->messages[i] = packet->lines[i];
			}
		}
	}
}

void ServerSideNetworkHandler::handle(const RakNet::RakNetGUID& source, SetDimensionPacket* packet) {
	// Clients never send this; ignore.
}

void ServerSideNetworkHandler::allowIncomingConnections( bool doAllow )
{
	if (doAllow) {
#if defined(STANDALONE_SERVER)
		// 独立服务器：客户端服务器列表里播的是**服务器名**
		// （server.properties 的 server-name，由 ServerApp 写进 user->name）。
		// 不能再用 options.username —— 那个默认就是 "Steve"，会顶着服务器名显示。
		raknetInstance->announceServer(minecraft->user ? minecraft->user->name : std::string());
#else
		raknetInstance->announceServer(minecraft->options.username);
#endif
	} else {
		raknetInstance->announceServer("");
	}
	_allowIncoming = doAllow;
}

Player* ServerSideNetworkHandler::popPendingPlayer( const RakNet::RakNetGUID& source )
{
	if (!level) {
		LOGE("Could not add player since Level is NULL!\n");
		return NULL;
	}

	for (unsigned int i = 0; i < _pendingPlayers.size(); ++i) {
		Player* p = _pendingPlayers[i];
		if (p->owner == source) {
			_pendingPlayers.erase(_pendingPlayers.begin() + i);
			return p;
		}
	}
	return NULL;
}

void ServerSideNetworkHandler::sendSnapshotTo(Player* p)
{
	if (!p)
		return;
	Level* w = worldForPlayer(p);
	if (!w)
		return;

	// players currently in this world (so the joiner sees who is around)
	const PlayerList& players = w->players;
	for (unsigned int i = 0; i < players.size(); i++) {
		Player* other = players[i];
		if (other == p)
			continue;
		AddPlayerPacket ap(other);
		RakNet::BitStream bitStream;
		ap.write(&bitStream);
		rakPeer->Send(&bitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, p->owner, false);
	}

	// all entities in this world
	for (unsigned int i = 0; i < w->entities.size(); ++i) {
		Entity* e = w->entities[i];
		if (e->isPlayer())
			continue;
		Packet* packet = getAddPacketFromEntity(e);
		if (packet != NULL) {
			RakNet::BitStream bitStream;
			packet->write(&bitStream);
			rakPeer->Send(&bitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, p->owner, false);
			delete packet;
		}
	}

	// tile entities
	for (unsigned int i = 0; i < w->tileEntities.size(); ++i) {
		TileEntity* e = w->tileEntities[i];
		Packet* packet = e->getUpdatePacket();
		if (packet != NULL) {
			RakNet::BitStream bitStream;
			packet->write(&bitStream);
			rakPeer->Send(&bitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, p->owner, false);
			delete packet;
		}
	}
}

// Tick any server world that isn't the one being rendered locally (players
// may live in several dimension worlds at once).
// ---------------------------------------------------------------------------
// 每个玩家自己的 GameMode（游戏模式按玩家分别）
//
// 引擎里 GameMode 是“全局一个”，而且内部直接读 minecraft->player（本机玩家）。
// 服务器上本机玩家是空的，所有玩家又共用一个实例（连“破坏进度”都串）。
// 做法：服务器给每个玩家各建一个自己的 GameMode；客户端/房主仍用全局那一个。
//
// 模式的权威来源：玩家存档里的 ServerGameType（≥ 0）→ 否则服务器全局配置。
// ---------------------------------------------------------------------------
GameMode* ServerSideNetworkHandler::gameModeFor(const RakNet::RakNetGUID& source, Player* p)
{
#ifndef STANDALONE_SERVER
	return minecraft->gameMode;   // 客户端/房主：行为不变
#else
	std::map<RakNet::RakNetGUID, GameMode*>::iterator it = _playerGameModes.find(source);
	if (it != _playerGameModes.end())
		return it->second;

	int type = (p && p->serverGameType >= 0)
		? p->serverGameType
		: (minecraft->gameMode->isCreativeType() ? GameType::Creative : GameType::Survival);

	GameMode* gm = (type == GameType::Creative) ? (GameMode*)new CreativeMode(minecraft)
	                                            : (GameMode*)new SurvivalMode(minecraft);
	_playerGameModes[source] = gm;
	return gm;
#endif
}

bool ServerSideNetworkHandler::setPlayerGameType(const std::string& playerName, int gameType, std::string& msg)
{
#ifndef STANDALONE_SERVER
	msg = "该入口只在独立服务器上可用";
	return false;
#else
	for (std::map<RakNet::RakNetGUID, Player*>::iterator it = _remotePlayers.begin(); it != _remotePlayers.end(); ++it) {
		Player* p = it->second;
		if (!p || p->name != playerName)
			continue;

		p->serverGameType = gameType;
		applyPlayerGameType(p);          // 立即生效：飞行/无敌/瞬间破坏

		// 丢掉旧的 GameMode 实例，下次取用时按新模式重建
		std::map<RakNet::RakNetGUID, GameMode*>::iterator git = _playerGameModes.find(it->first);
		if (git != _playerGameModes.end()) {
			delete git->second;
			_playerGameModes.erase(git);
		}

		Level* w = worldForPlayer(p);
		if (w)
			PlayerStore::save(w, p);     // 立刻落盘，重启/重进也不丢

		// 热切换：当场告诉这个客户端"你的模式变了"，它收到后就地切 GameMode
		// 并重算能力，不用重连。（旧客户端不认识这个包 → createPacket 返回 NULL
		// → 直接忽略，最多是没当场生效，不会崩。）
		{
			SetPlayerGameTypePacket pkt(gameType);
			RakNet::BitStream gmBs;
			pkt.write(&gmBs);
			rakPeer->Send(&gmBs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, it->first, false);
		}

		msg = std::string("已把 ") + playerName + " 的游戏模式设为 "
			+ (gameType == GameType::Creative ? "创造" : "生存")
			+ "（服务器判定立即生效；客户端要重进一次才会看到飞行/创造界面）";
		return true;
	}
	msg = std::string("找不到在线玩家: ") + playerName;
	return false;
#endif
}

std::string ServerSideNetworkHandler::listOnlinePlayers()
{
	std::string out;
	int n = 0;
	for (std::map<RakNet::RakNetGUID, Player*>::iterator it = _remotePlayers.begin(); it != _remotePlayers.end(); ++it) {
		Player* p = it->second;
		if (!p)
			continue;
		++n;
		out += "\n  " + p->name + "  mode=";
		out += (p->serverGameType < 0) ? "(跟服务器配置)"
		                                    : (p->serverGameType == GameType::Creative ? "创造" : "生存");
		out += "  pos=(" + std::to_string((int)p->x) + "," + std::to_string((int)p->y)
		     + "," + std::to_string((int)p->z) + ")";
	}
	if (n == 0)
		return "\n  (没有在线玩家)";
	return out;
}

// ---------------------------------------------------------------------------
// 管理员名单（ops.txt）
//
// 为什么需要：游戏内命令（/gamemode、/plugins reload）不能人人可用。
// 名单就是一个文本文件，一行一个名字；没有这个文件时，**第一个进服的玩家
// 自动成为管理员**（否则没控制台窗口时就没人能用管理命令）。
// ---------------------------------------------------------------------------
#ifdef STANDALONE_SERVER
void ServerSideNetworkHandler::loadOps()
{
	_ops.clear();
	_opsLoaded = true;
	std::ifstream in("ops.txt");
	std::string line;
	while (std::getline(in, line)) {
		while (!line.empty() && (line[line.size() - 1] == '\r' || line[line.size() - 1] == '\n'))
			line.erase(line.size() - 1);
		if (!line.empty())
			_ops.insert(line);
	}
}

void ServerSideNetworkHandler::makeOp(const std::string& playerName)
{
	if (playerName.empty() || _ops.find(playerName) != _ops.end())
		return;
	_ops.insert(playerName);
	std::ofstream out("ops.txt", std::ios::app);
	if (out.is_open())
		out << playerName << "\n";
	LOGI("[server] %s 已被设为管理员\n", playerName.c_str());
}

bool ServerSideNetworkHandler::removeOp(const std::string& playerName)
{
	if (playerName.empty() || _ops.find(playerName) == _ops.end())
		return false;
	_ops.erase(playerName);

	// 重写整份名单（中文名字要走宽字符 API，见 ServerPaths.h）
	std::string out;
	for (std::set<std::string>::iterator it = _ops.begin(); it != _ops.end(); ++it)
		out += *it + "\n";
	FILE* f = _wfopen(ServerPaths::toWide("ops.txt").c_str(), L"wb");
	if (f) {
		fwrite(out.data(), 1, out.size(), f);
		fclose(f);
	}
	LOGI("[server] %s 已被取消管理员\n", playerName.c_str());
	return true;
}
#else
// 客户端构建没有 ops 名单（服务器专属），但 makeOp/removeOp 有公开声明，
// 命令处理代码会引用它们 —— 给两个空实现，避免链接失败。
void ServerSideNetworkHandler::makeOp(const std::string&) {}
bool ServerSideNetworkHandler::removeOp(const std::string&) { return false; }
#endif
//
// 为什么必须在服务器做：客户端发上来的包是可以伪造的 —— 改过的客户端能隔着
// 上百格挖方块、一秒钟拆一百块。服务器不能相信“客户端说自己够得着”。
// 这里只做两件低误判的事：
//   1. 距离：目标不在够得着的范围内 → 丢弃这个包
//   2. 频率：手速超过人类能点出来的上限 → 丢弃这个包
// 移动速度异常只记日志、不拉回 —— 传送/卡顿/重连都能造成大位移，拉回很容易
// 误伤玩家，先观察一段时间再说。
// ---------------------------------------------------------------------------
#ifdef STANDALONE_SERVER
bool ServerSideNetworkHandler::rateAllow(std::map<RakNet::RakNetGUID, RateLimit>& m, const RakNet::RakNetGUID& g, int maxPerSecond)
{
	unsigned int now = getTimeMs();
	RateLimit& rl = m[g];
	if (rl.windowStart == 0 || now - rl.windowStart > 1000) {
		rl.windowStart = now;
		rl.count = 1;
		return true;
	}
	++rl.count;
	return rl.count <= maxPerSecond;
}

bool ServerSideNetworkHandler::withinReach(Player* p, float tx, float ty, float tz)
{
	const float kReach = 6.0f;   // 原版大约 5 格，给走动和网络抖动留点余量
	float dx = tx - p->x;
	float dy = ty - (p->y + 1.2f);   // 以身子中段为准，别拿脚底算
	float dz = tz - p->z;
	return (dx * dx + dy * dy + dz * dz) <= (kReach * kReach);
}

// 作弊日志节流：同一玩家 3 秒最多一条，否则作弊者能把控制台刷爆
void ServerSideNetworkHandler::logCheat(const RakNet::RakNetGUID& source, Player* p, const char* what, float x, float y, float z)
{
	unsigned int now = getTimeMs();
	unsigned int& last = _cheatLogMs[source];
	if (last != 0 && now - last < 3000)
		return;
	last = now;
	LOGI("[cheat] %s %s (目标 %.1f,%.1f,%.1f / 他在 %.1f,%.1f,%.1f)\n",
	     p ? p->name.c_str() : "?", what, x, y, z,
	     p ? p->x : 0.f, p ? p->y : 0.f, p ? p->z : 0.f);
}
#endif

void ServerSideNetworkHandler::reloadOps()
{
#ifdef STANDALONE_SERVER
	_opsLoaded = false;
#endif
}

bool ServerSideNetworkHandler::isOp(const std::string& playerName)
{
#ifdef STANDALONE_SERVER
	if (!_opsLoaded)
		loadOps();
	return _ops.find(playerName) != _ops.end();
#else
	(void)playerName;
	return false;
#endif
}

void ServerSideNetworkHandler::maybeAutoOp(Player* p)
{
#ifdef STANDALONE_SERVER
	if (!p || !p->name.empty() == false)
		return;
	if (!_opsLoaded)
		loadOps();
	if (_ops.empty()) {
		LOGI("[server] 管理员名单为空，把第一个进服的玩家 %s 设为管理员\n", p->name.c_str());
		makeOp(p->name);
	}
#endif
}

// ---- 给插件用的服务器能力 ----
std::vector<ServerSideNetworkHandler::PluginPlayerInfo> ServerSideNetworkHandler::pluginPlayerSnapshot()
{
	std::vector<PluginPlayerInfo> out;
	for (std::map<RakNet::RakNetGUID, Player*>::iterator it = _remotePlayers.begin(); it != _remotePlayers.end(); ++it) {
		Player* p = it->second;
		if (!p)
			continue;
		PluginPlayerInfo info;
		info.name = p->name;
		info.gameType = p->serverGameType;
		info.x = p->x;
		info.y = p->y;
		info.z = p->z;
		info.health = p->health;
		out.push_back(info);
	}
	return out;
}

Level* ServerSideNetworkHandler::pluginWorld()
{
	return _mainWorld ? _mainWorld : level;
}

void ServerSideNetworkHandler::broadcastMessage(const std::string& text)
{
	displayGameMessage(text);
}

bool ServerSideNetworkHandler::sendToPlayerByName(const std::string& name, const std::string& text)
{
	for (std::map<RakNet::RakNetGUID, Player*>::iterator it = _remotePlayers.begin(); it != _remotePlayers.end(); ++it) {
		Player* p = it->second;
		if (p && p->name == name) {
			sendMessageTo(it->first, text);
			return true;
		}
	}
	return false;
}

// ---------------------------------------------------------------------------
// 玩家数据落盘（服务器）
//
// 引擎自带的存档只给“本地玩家”留了一个坑位（LevelData::createTag 只用 players[0]，
// 见 LevelData.cpp:157），所以独立服务器自己按玩家名存：
// <世界目录>/players/<名字>.dat（实现见 PlayerStore）。
// 时机：退出即存 + 每 5 分钟自动存 + 关服存。
// ---------------------------------------------------------------------------
void ServerSideNetworkHandler::saveAllPlayers()
{
#ifndef STANDALONE_SERVER
	return;   // 客户端/房主模式不写玩家分档（保持原版行为）
#else
	int n = 0;
	for (std::map<RakNet::RakNetGUID, Player*>::iterator it = _remotePlayers.begin(); it != _remotePlayers.end(); ++it) {
		Player* p = it->second;
		Level* w = p ? worldForPlayer(p) : NULL;
		if (w && PlayerStore::save(w, p))
			++n;
	}
	LOGI("[player] shutdown save: %d player(s)\n", n);
#endif
}

#ifdef STANDALONE_SERVER
void ServerSideNetworkHandler::autosavePlayers()
{
	unsigned int now = getTimeMs();
	if (_lastAutosaveMs == 0) {
		_lastAutosaveMs = now;
		return;
	}
	if (now - _lastAutosaveMs < 5 * 60 * 1000)
		return;
	_lastAutosaveMs = now;

	int n = 0;
	for (std::map<RakNet::RakNetGUID, Player*>::iterator it = _remotePlayers.begin(); it != _remotePlayers.end(); ++it) {
		Player* p = it->second;
		Level* w = p ? worldForPlayer(p) : NULL;
		if (w && PlayerStore::save(w, p))
			++n;
	}
	LOGI("[player] autosave: %d player(s)\n", n);
}
#endif

void ServerSideNetworkHandler::tick()
{
#ifdef STANDALONE_SERVER
	// 限量生成排队中的区块请求（每 tick 2 块）：天境那种 JS 生成的地形一块要
	// 100+ms，一次做几百块会把主线程占住几十秒。限量后服务器始终能响应，
	// 天境会一块一块慢慢长出来。
	{
		int budget = 2;
		while (budget-- > 0 && !_pendingChunks.empty()) {
			PendingChunk pc = _pendingChunks.front();
			_pendingChunks.pop_front();
			_pendingChunkKeys.erase(std::make_pair(pc.x, pc.z));
			Player* p = getPlayer(pc.guid);
			if (!p) continue;
			Level* w = worldForPlayer(p);
			if (!w) continue;
			LevelChunk* chunk = w->getChunk(pc.x, pc.z);
			if (!chunk) continue;
			ChunkDataPacket cpacket(chunk->x, chunk->z, chunk);
			RakNet::BitStream bitStream;
			cpacket.write(&bitStream);
			rakPeer->Send(&bitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, pc.guid, false);
			const LevelChunk::TEMap& teMap = chunk->getTileEntityMap();
			for (LevelChunk::TEMapCIterator cit = teMap.begin(); cit != teMap.end(); ++cit) {
				TileEntity* te = cit->second;
				if (Packet* up = te->getUpdatePacket()) {
					bitStream.Reset();
					up->write(&bitStream);
					rakPeer->Send(&bitStream, HIGH_PRIORITY, RELIABLE_ORDERED, 0, pc.guid, false);
					delete up;
				}
			}
		}
	}
	autosavePlayers();
	// 插件的定时任务：每 tick 只比对到期时间，到点了才进 JS
	PluginEngine::tickScheduler();
	// 插件（“文员”）的 onTick：每秒一次就好 —— 每秒 20 次去调 JS 没必要，
	// 而且“文员”的活（改配置/发通知）本来就不高频。
	{
		unsigned int now = getTimeMs();
		if (_lastPluginTickMs == 0)
			_lastPluginTickMs = now;
		else if (now - _lastPluginTickMs >= 1000) {
			_lastPluginTickMs = now;
			PluginEngine::fireTick();
		}
	}
#endif
	Level* rendered = minecraft ? minecraft->level : NULL;
	for (std::map<int, Level*>::iterator it = _worldByDim.begin(); it != _worldByDim.end(); ++it) {
		Level* w = it->second;
		if (!w || w == rendered)
			continue;
		if (w->players.empty() && w->entities.empty())
			continue;
		w->tickEntities();
		w->tick();
	}
}
