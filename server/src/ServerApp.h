#ifndef MINECRAFT_SEVER_SERVERAPP_H__
#define MINECRAFT_SEVER_SERVERAPP_H__

#include "NinecraftApp.h"
#include "ServerConfig.h"

//
// 独立服务器进程的 App。
//
// 为什么要继承 NinecraftApp：
//   App::init(AppContext&) 会调用虚函数 init()，而 NinecraftApp::init() 在
//   STANDALONE_SERVER 下只是"初始化静态表 + hostMultiplayer()"——没有世界、
//   没有主循环收尾。服务器需要在这条链上补：读配置 → 按配置端口监听 →
//   载入/创建世界。用一个子类 override init() 就能拿到引擎既有的初始化，
//   不必改动引擎源码（这是刻意的：原版/客户端保持不动）。
//
class ServerApp : public NinecraftApp
{
public:
	// NinecraftApp::init()（无参）把 App::init(AppContext&) 遮蔽了 —— 同名不同
	// 签名，派生类作用域一找到 init 就不再往上找。这里把带参数的版本重新引入，
	// 否则外面没法 app->init(ctx)。
	using App::init;

	ServerApp();
	virtual ~ServerApp();

	// 服务器主循环每轮调用一次（App::update 的 override）。
	void update();

	const ServerConfig& config() const { return _config; }
	void requestShutdown() { _shutdownRequested = true; }

protected:
	// App::init() -> init() 的 override：无头服务器启动流程。
	void init();

private:
	void bootServer();
	void applyWorldRules();   // 把 server.properties 的世界规则应用到世界
	void printBanner();

	ServerConfig _config;
	bool         _shutdownRequested;
	bool         _booted;
};

#endif // MINECRAFT_SEVER_SERVERAPP_H__
