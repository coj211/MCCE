#include "ServerApp.h"

#include "platform/log.h"
#include "platform/time.h"
#include "world/level/LevelSettings.h"
#include "client/player/LocalPlayer.h"
#include "world/level/Level.h"
#include "world/level/storage/LevelData.h"
#include "world/Difficulty.h"
#include "network/ServerSideNetworkHandler.h"
#include "mod/ModEngine.h"                       // mod 引擎（“武将”）
#ifdef _WIN32
#include <direct.h>                              // _mkdir（mod 日志目录）
#endif

namespace {
// 难度常量 → 配置里的写法（日志用）
const char* diffName(int d)
{
	switch (d) {
	case Difficulty::PEACEFUL: return "peaceful";
	case Difficulty::EASY:     return "easy";
	case Difficulty::HARD:     return "hard";
	default:                   return "normal";
	}
}
} // namespace

ServerApp::ServerApp()
:	_shutdownRequested(false),
	_booted(false)
{
}

ServerApp::~ServerApp()
{
}

void ServerApp::init()
{
	// 1) 读配置（不存在就落一份模板）
	_config = ServerConfig::load("server.properties");

	// 2) 引擎初始化。STANDALONE_SERVER 下的 NinecraftApp::init() 会建好静态表、
	//    存档源，并 hostMultiplayer()（用的是默认端口）——下面会按配置重绑。
	NinecraftApp::init();

	bootServer();
}

void ServerApp::bootServer()
{
	// mod 日志目录（服务器下 ModEngine 把日志写到 logs/mod.log，见 ModEngine.cpp）
	_mkdir("logs");
	if (_booted)
		return;
	_booted = true;

	// 服务器名（客户端服务器列表里显示的就是它）
	if (user)
		user->name = _config.serverName.c_str();

	// 3) 按配置端口监听。hostMultiplayer() 内部会 disconnect 上一次的 host 并
	//    重建 ServerSideNetworkHandler，所以这里等价于"换端口重开"。
	hostMultiplayer(_config.port);

	// 4) 把 server.properties 里的进服欢迎语交给服务器处理器（玩家进入世界时
	//    单独发给他）。
	if (ServerSideNetworkHandler* ssn = dynamic_cast<ServerSideNetworkHandler*>(netCallback))
		ssn->setMotd(_config.motd);

	// 4) 载入（或首次生成）世界。种子为 0 表示每次随机。
	long seed = _config.seed;
	if (seed == 0)
		seed = (long)getEpochTimeS();

	LevelSettings settings(seed, _config.gameType, _config.worldType);

	printBanner();
	LOGI("[server] loading level '%s' (id=%s seed=%ld type=%s gamemode=%s)\n",
	     _config.levelName.c_str(), _config.levelId.c_str(), seed,
	     _config.worldType == WorldType::Infinite ? "infinite" : "old",
	     _config.gameType == GameType::Creative ? "creative" : "survival");

	// 5) mod 引擎（“武将”）：服务器必须真的建实例并加载 mods/ —— 服务端权威逻辑
	//    （方块/实体/命令/事件）全挂在它上面，没有实例等于所有 mod 钩子空跑。
	//    为什么在这里而不是靠 Minecraft::init()：服务器走的是 ServerApp::init()，
	//    把 Minecraft::init() 整个覆盖掉了，那里面建 modEngine 的代码根本不会跑。
	//    时机与客户端一致：进世界之前建好，这样世界生成期间的钩子
	//    （onJoinWorld / 方块快照 / 维度生成器）才有反应。
#if defined(_WIN32) || defined(__ANDROID__)
	if (!modEngine) {
		modEngine = new ModEngine(this);
		if (modEngine->init())
			modEngine->loadEnabledMods();
		else
			modEngine->log("ModEngine init failed");
	}
#endif

	selectLevel(_config.levelId, _config.levelName, settings);

	// 6) 应用世界规则（server.properties）：昼夜更替 / 生物生成
	applyWorldRules();

	// 7) 把 mod 加载结果打到控制台。ModEngine 自己的日志只写文件（客户端靠 F3 面板
	//    查看），服务器没有 F3 —— 所以这里在控制台给一份摘要，不然看不出 mod 到底
	//    加载了没。
#if defined(_WIN32) || defined(__ANDROID__)
	if (modEngine) {
		std::vector<std::string> mods = modEngine->getEnabledModFiles();
		LOGI("[server] 已加载 mod: %d 个\n", (int)mods.size());
		for (size_t i = 0; i < mods.size(); ++i)
			LOGI("[server]   - %s\n", mods[i].c_str());
		if (mods.empty())
			LOGI("[server]   (mods/ 里没有启用的 mod：把 .zip 放进去，并写进 mods/modlist.json)\n");
	}
#endif
}

// 把 server.properties 里的世界规则应用到服务器世界。
// 引擎侧对应：Level::setDaylightCycle（时间是否推进）、Level::setSpawnSettings
// （运行期刷怪开关）、LevelData::setSpawnMobs（记录进存档的刷怪总开关）。
void ServerApp::applyWorldRules()
{
	if (!level)
		return;

	level->setDaylightCycle(_config.daylightCycle);
	level->setSpawnSettings(_config.spawnMonsters, _config.spawnAnimals);   // (敌对, 和平)
	// 难度：服务器的难度只能由服务器配置决定。引擎原本在 Minecraft::tick 里每 tick
	// 用客户端选项覆盖它，STANDALONE_SERVER 下那段已经排除（见那里的注释）。
	level->difficulty = _config.difficulty;
	if (level->getLevelData())
		level->getLevelData()->setSpawnMobs(_config.spawnAnimals || _config.spawnMonsters);

	LOGI("[server] world rules: difficulty=%s daylight-cycle=%s spawn-animals=%s spawn-monsters=%s\n",
	     diffName(_config.difficulty),
	     _config.daylightCycle ? "on" : "off",
	     _config.spawnAnimals ? "on" : "off",
	     _config.spawnMonsters ? "on" : "off");
}

void ServerApp::printBanner()
{
	LOGI("========================================\n");
	LOGI(" MinecraftPE-Sever (standalone)\n");
	LOGI("   name    : %s\n", _config.serverName.c_str());
	LOGI("   bind    : 0.0.0.0:%d (udp/raknet)\n", _config.port);
	LOGI("   level   : %s\n", _config.levelId.c_str());
	LOGI("   players : up to %d\n", _config.maxPlayers);
	LOGI("========================================\n");
}

void ServerApp::update()
{
	NinecraftApp::update();

	if (_shutdownRequested) {
		// 关服前把在线玩家写回存档（退出即存之外的兜底：玩家还没退出就关服时）
		if (ServerSideNetworkHandler* ssn = dynamic_cast<ServerSideNetworkHandler*>(netCallback))
			ssn->saveAllPlayers();
		quit();
	}
}
