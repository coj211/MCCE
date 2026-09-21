#ifndef MINECRAFT_SEVER_SERVERCONFIG_H__
#define MINECRAFT_SEVER_SERVERCONFIG_H__

#include <string>

//
// 服务器配置（server.properties）。
//
// 读法沿用 Java 版服务器的习惯：key=value、'#' 开头的行是注释、缺失的键用默认值。
// 首次运行时若文件不存在，会写出一份带注释的默认配置，方便直接手改。
//
struct ServerConfig
{
	std::string serverName;    // 客户端服务器列表里显示的名字
	int         port;          // UDP 监听端口（RakNet）
	int         maxPlayers;
	std::string levelId;       // 存档 id（也是存档目录名）
	std::string levelName;     // 存档显示名
	long        seed;          // 0 = 每次启动随机
	int         gameType;      // GameType::Survival(0) / GameType::Creative(1)
	int         worldType;     // WorldType::Old(0) 有限 256x256 / WorldType::Infinite(1)
	std::string motd;          // 进服时发给玩家的欢迎语（可为空）

	// ---- 世界规则（类 Java 版 gamerule，服务器启动时应用）----
	bool        daylightCycle;  // 昼夜是否更替（false = 时间冻结）
	bool        spawnAnimals;   // 是否生成和平生物
	bool        spawnMonsters;  // 是否生成敌对生物
	int         difficulty;     // Difficulty::PEACEFUL / EASY / NORMAL / HARD

	// 旧版遗留的 TCP 命令端口（CommandServer::init）。它**没有任何鉴权**，
	// 任何能连上端口的人都能改世界，所以默认关闭。
	bool        enableCommandServer;
	int         commandPort;

	ServerConfig();

	// 读取配置；文件不存在时用默认值并顺手写一份模板出来。
	static ServerConfig load(const std::string& path);

	// 写出配置（覆盖）。
	bool save(const std::string& path) const;
};

#endif // MINECRAFT_SEVER_SERVERCONFIG_H__
