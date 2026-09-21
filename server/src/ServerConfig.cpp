#include "ServerConfig.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

// 引擎侧的类型（GameType / WorldType 常量在 LevelSettings.h 里）
#include "world/level/LevelSettings.h"
#include "world/Difficulty.h"

namespace {

std::string trim(const std::string& s)
{
	size_t b = s.find_first_not_of(" \t\r\n");
	if (b == std::string::npos)
		return std::string();
	size_t e = s.find_last_not_of(" \t\r\n");
	return s.substr(b, e - b + 1);
}

int toInt(const std::string& s, int fallback)
{
	if (s.empty())
		return fallback;
	char* end = NULL;
	long v = strtol(s.c_str(), &end, 10);
	if (end == s.c_str())
		return fallback;
	return (int)v;
}

long toLong(const std::string& s, long fallback)
{
	if (s.empty())
		return fallback;
	char* end = NULL;
	long long v = _strtoi64(s.c_str(), &end, 10);
	if (end == s.c_str())
		return fallback;
	return (long)v;
}

bool toBool(const std::string& s, bool fallback)
{
	if (s.empty())
		return fallback;
	if (s == "true" || s == "1" || s == "yes" || s == "on")
		return true;
	if (s == "false" || s == "0" || s == "no" || s == "off")
		return false;
	return fallback;
}

} // namespace

ServerConfig::ServerConfig()
:	serverName("A Minecraft PE Server"),
	port(19132),
	maxPlayers(16),
	levelId("world"),
	levelName("world"),
	seed(0),
	gameType(GameType::Survival),
	worldType(WorldType::Infinite),
	motd("Welcome!"),
	daylightCycle(true),
	spawnAnimals(true),
	spawnMonsters(true),
	difficulty(Difficulty::NORMAL),
	enableCommandServer(false),
	commandPort(4711)
{
}

ServerConfig ServerConfig::load(const std::string& path)
{
	ServerConfig c;

	std::ifstream in(path.c_str());
	if (!in.is_open()) {
		// 第一次跑：落一份模板，用户改了下次就生效
		c.save(path);
		return c;
	}

	std::string line;
	while (std::getline(in, line)) {
		std::string t = trim(line);
		if (t.empty() || t[0] == '#')
			continue;

		size_t eq = t.find('=');
		if (eq == std::string::npos)
			continue;

		std::string key = trim(t.substr(0, eq));
		std::string val = trim(t.substr(eq + 1));

		if      (key == "server-name")          c.serverName = val.empty() ? c.serverName : val;
		else if (key == "server-port")          c.port = toInt(val, c.port);
		else if (key == "max-players")          c.maxPlayers = toInt(val, c.maxPlayers);
		else if (key == "level-name")           c.levelId = val.empty() ? c.levelId : val;
		else if (key == "level-display-name")   c.levelName = val.empty() ? c.levelName : val;
		else if (key == "level-seed")           c.seed = toLong(val, c.seed);
		else if (key == "gamemode")             c.gameType = (val == "creative" || val == "1") ? GameType::Creative
		                                                    : (val == "survival" || val == "0") ? GameType::Survival
		                                                    : c.gameType;
		else if (key == "level-type")           c.worldType = (val == "infinite" || val == "1") ? WorldType::Infinite
		                                                     : (val == "old" || val == "0") ? WorldType::Old
		                                                     : c.worldType;
		else if (key == "motd")                 c.motd = val;
		else if (key == "do-daylight-cycle")    c.daylightCycle = toBool(val, c.daylightCycle);
		else if (key == "spawn-animals")        c.spawnAnimals = toBool(val, c.spawnAnimals);
		else if (key == "spawn-monsters")       c.spawnMonsters = toBool(val, c.spawnMonsters);
		else if (key == "difficulty")           c.difficulty = (val == "peaceful") ? Difficulty::PEACEFUL
		                                                       : (val == "easy")     ? Difficulty::EASY
		                                                       : (val == "hard")     ? Difficulty::HARD
		                                                       : Difficulty::NORMAL;
		else if (key == "enable-command-server") c.enableCommandServer = toBool(val, c.enableCommandServer);
		else if (key == "command-port")         c.commandPort = toInt(val, c.commandPort);
	}

	// 存档名缺省跟随存档 id，省得两份配置打架
	if (c.levelName.empty())
		c.levelName = c.levelId;

	return c;
}

bool ServerConfig::save(const std::string& path) const
{
	std::ofstream out(path.c_str());
	if (!out.is_open())
		return false;

	out << "# MinecraftPE-Sever 配置\n"
	    << "# 改完重启服务器生效。\n"
	    << "\n"
	    << "# 客户端服务器列表里显示的名字\n"
	    << "server-name=" << serverName << "\n"
	    << "\n"
	    << "# UDP 监听端口\n"
	    << "server-port=" << port << "\n"
	    << "max-players=" << maxPlayers << "\n"
	    << "\n"
	    << "# 存档\n"
	    << "level-name=" << levelId << "\n"
	    << "level-display-name=" << levelName << "\n"
	    << "# 0 = 每次启动随机；非 0 固定种子\n"
	    << "level-seed=" << seed << "\n"
	    << "\n"
	    << "# survival | creative\n"
	    << "gamemode=" << (gameType == GameType::Creative ? "creative" : "survival") << "\n"
	    << "# old(有限 256x256) | infinite\n"
	    << "level-type=" << (worldType == WorldType::Infinite ? "infinite" : "old") << "\n"
	    << "\n"
	    << "# 玩家进服提示语\n"
	    << "motd=" << motd << "\n"
	    << "\n"
	    << "# ---- 世界规则 ----\n"
	    << "# 昼夜是否更替（false = 时间冻结，太阳不动）\n"
	    << "do-daylight-cycle=" << (daylightCycle ? "true" : "false") << "\n"
	    << "# 是否生成和平生物（猪/牛/羊/鸡…）\n"
	    << "spawn-animals=" << (spawnAnimals ? "true" : "false") << "\n"
	    << "# 是否生成敌对生物（僵尸/骷髅/蜘蛛…）\n"
	    << "spawn-monsters=" << (spawnMonsters ? "true" : "false") << "\n"
	    << "# 难度：peaceful | easy | normal | hard（peaceful 时敌对生物不刷、已存在的会消失）\n"
	    << "difficulty=" << (difficulty == Difficulty::PEACEFUL ? "peaceful"
	                         : difficulty == Difficulty::EASY   ? "easy"
	                         : difficulty == Difficulty::HARD   ? "hard" : "normal") << "\n"
	    << "\n"
	    << "# 旧版 TCP 命令端口（无鉴权，默认关）\n"
	    << "enable-command-server=" << (enableCommandServer ? "true" : "false") << "\n"
	    << "command-port=" << commandPort << "\n";

	return true;
}
