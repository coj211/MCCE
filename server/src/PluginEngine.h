#ifndef MINECRAFT_SEVER_PLUGINENGINE_H__
#define MINECRAFT_SEVER_PLUGINENGINE_H__

#include <string>
#include <vector>

#include "../../MinecraftPE-Win/handheld/thirdparty/duktape/duktape.h"

class ServerSideNetworkHandler;

//
// 插件（"文员"）：**只跑在服务器**上的小工具。
//
// 和 mod（"武将"）分开的理由：服务器有些东西客户端根本不存在 —— server.properties、
// 玩家账户、ops 名单…… 而 mod 是双端加载的（客户端也要跑同一份），它一旦去碰这些
// 就会出现"服务器能跑、客户端不兼容"。所以：
//   mod    = 改游戏内容（方块/怪物/维度/物品），两端都要有
//   plugin = 管服务器事务（配置/账户/ops/聊天格式/命令），客户端完全不知道它存在
//
// 插件目录：plugins/*.js（放进去就自动加载；改完 server 控制台敲 `plugins reload`）
//
// 插件里可用的 API 见 PluginEngine.cpp 顶部的注释。
//
namespace PluginEngine
{
	// 服务器实例（插件通过它广播/私聊/查在线玩家/改模式…）
	void setHost(ServerSideNetworkHandler* ssn);

	// 扫描 plugins/*.js 全部加载（reload 也是重走这个）
	void loadAll();
	void unloadAll();

	int count();
	std::vector<std::string> names();
	std::string listText();     // 控制台 `plugins` 用

	// ---- 服务器在相应时机调用的事件 ----
	void fireJoin(const std::string& playerName);
	void fireLeave(const std::string& playerName);
	void fireChat(const std::string& playerName, const std::string& message);
	void fireTick();                                    // 低频（约每秒一次）
	void tickScheduler();                               // 每 tick 驱动插件的 setTimeout/setInterval

	// 登录准入：封禁 / 白名单（插件能改这两份名单，所以判定放在插件引擎里）。
	// 返回 false 表示不许进，rejectReason 是要发给客户端的原因。
	bool checkJoinAllowed(const std::string& playerName, std::string& rejectReason);

	// ---- 可取消事件 ----
	// 插件在这些事件里 return false，服务器就丢弃这次操作。
	// 用途：登录插件在玩家没登录前拦住挖/放/打/说话。
	// （移动**不**在这里拦：客户端是本地预测位置，服务器拒绝移动包会让玩家漂移回弹，
	//   要做"锁在登录点"得靠服务端每 tick 拉回位置，那是另一件事。）
	bool allowBlockBreak(const std::string& playerName, int x, int y, int z);
	bool allowBlockPlace(const std::string& playerName, int x, int y, int z);
	bool allowAttack(const std::string& playerName, int targetEntityId);
	bool allowChat(const std::string& playerName, const std::string& message);

	// 聊天行格式化：插件可以用 chat.setFormat(fn) 接管（比如给名字加称号前缀）。
	// 没有插件接管时原样返回 "名字: 消息"。
	std::string formatChat(const std::string& playerName, const std::string& message);

	// 插件用 Commands.register 注册的命令；返回 true 表示已被处理（reply 是要回给玩家的文本）。
	bool runCommand(const std::string& line, const std::string& senderName, std::string& reply);
}

#endif // MINECRAFT_SEVER_PLUGINENGINE_H__
