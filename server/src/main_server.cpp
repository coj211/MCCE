//
// MinecraftPE-Sever —— 独立（无头）服务器入口。
//
// 为什么这里有一个自己的 main，而不是复用引擎的 src/main.cpp：
//   main.cpp 是**客户端**入口：Windows 子系统 + 建窗口 + EGL + 60fps 渲染循环。
//   服务器要的是控制台进程（stdin 敲命令、无窗口、无 GL），所以服务器工程
//   在生成时就把引擎的 main.cpp 排除掉（见 tools/gen_vcxproj.py），改由本文件
//   提供入口。引擎本身一行都不用改。
//
#include "ServerApp.h"
#include "AppPlatform_win32.h"

#include "platform/log.h"
#include "platform/time.h"
#include "network/ServerSideNetworkHandler.h"   // 控制台命令 list / gamemode
#include "world/level/LevelSettings.h"          // GameType
#include "PluginEngine.h"                        // 插件（“文员”）

#include <windows.h>
#include <cstdio>
#include <dbghelp.h>   // MiniDumpWriteDump（崩溃转储）
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

// ---------------------------------------------------------------------------
// 引擎里由 main_win32.h 提供的诊断全局量（客户端靠 main.cpp 那个编译单元定义）。
// 服务器不编译 main.cpp，所以在这里补上，否则 App/PerfRenderer 链接不上。
//   - g_diagUpdateMs 只有 Win32 走 main_win32.h 定义，非 Win32 在 LevelRenderer.cpp
//   - g_diagSwapMs   只在 App::swapBuffers 里写，服务器 NO_EGL 下不写，但被引用
// ---------------------------------------------------------------------------
double g_diagUpdateMs = 0.0;
double g_diagSwapMs   = 0.0;

namespace {

std::mutex               g_consoleMutex;
std::deque<std::string>  g_consoleLines;
volatile bool            g_consoleRunning = true;

// 控制台读线程：独立线程阻塞在 stdin 上，行丢进队列由主循环消费。
// （不能在主循环里直接读，否则会把 tick 卡住。）
void consoleThreadProc()
{
	char buf[512];
	while (g_consoleRunning) {
		if (!fgets(buf, sizeof(buf), stdin)) {
			// EOF（例如被重定向 / 管道关闭）——别再空转
			break;
		}
		std::string line(buf);
		while (!line.empty() && (line[line.size() - 1] == '\n' || line[line.size() - 1] == '\r'))
			line.erase(line.size() - 1);

		std::lock_guard<std::mutex> lock(g_consoleMutex);
		g_consoleLines.push_back(line);
	}
}

// 把工作目录固定到 exe 所在目录，这样 server.properties / 存档 / mods
// 都相对于服务器目录解析，双击或命令行启动都一样。
void setWorkingDirectoryToExe()
{
	char exePath[MAX_PATH];
	DWORD len = GetModuleFileNameA(NULL, exePath, MAX_PATH);
	if (len > 0 && len < MAX_PATH) {
		char* slash = strrchr(exePath, '\\');
		if (slash) {
			*slash = '\0';
			SetCurrentDirectoryA(exePath);
		}
	}
}

} // namespace

// ---------------------------------------------------------------------------
// 崩溃转储：服务器原本没有任何异常捕获 —— 静默崩溃（堆损坏那类）时日志里
// 什么都不留，只能靠猜。这里挂一个 VEH，崩溃时把 minidump 写到 exe 旁边。
// dbghelp.dll 动态加载，不引入链接依赖。
// ---------------------------------------------------------------------------
static LONG WINAPI ServerCrashVeh(EXCEPTION_POINTERS* ep)
{
	static volatile LONG dumping = 0;
	if (dumping) return EXCEPTION_CONTINUE_SEARCH;
	if (!ep || !ep->ExceptionRecord) return EXCEPTION_CONTINUE_SEARCH;

	DWORD code = ep->ExceptionRecord->ExceptionCode;
	bool fatal = (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_STACK_OVERFLOW ||
	              code == EXCEPTION_ILLEGAL_INSTRUCTION || code == EXCEPTION_PRIV_INSTRUCTION ||
	              code == EXCEPTION_INT_DIVIDE_BY_ZERO || code == 0xC0000374 /*堆损坏*/);
	if (!fatal) return EXCEPTION_CONTINUE_SEARCH;
	dumping = 1;

	HMODULE dbg = LoadLibraryA("dbghelp.dll");
	if (dbg) {
		typedef BOOL (WINAPI *MiniDumpWriteDumpFn)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
		                                           void*, void*, void*);
		MiniDumpWriteDumpFn fn = (MiniDumpWriteDumpFn)GetProcAddress(dbg, "MiniDumpWriteDump");
		if (fn) {
			char exePath[MAX_PATH] = {0};
			GetModuleFileNameA(NULL, exePath, MAX_PATH);
			std::string path(exePath);
			size_t dot = path.find_last_of('.');
			if (dot != std::string::npos) path = path.substr(0, dot);
			path += "_crash.dmp";
			HANDLE f = CreateFileA(path.c_str(), GENERIC_WRITE, 0, NULL,
			                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (f != INVALID_HANDLE_VALUE) {
				MINIDUMP_EXCEPTION_INFORMATION mei;
				mei.ThreadId = GetCurrentThreadId();
				mei.ExceptionPointers = ep;
				mei.ClientPointers = FALSE;
				fn(GetCurrentProcess(), GetCurrentProcessId(), f,
				   (MINIDUMP_TYPE)(MiniDumpWithDataSegs | MiniDumpWithHandleData),
				   &mei, NULL, NULL);
				CloseHandle(f);
				fprintf(stderr, "[server] crash dump written: %s (code=%08lx)\n", path.c_str(), (unsigned long)code);
			}
		}
	}
	return EXCEPTION_CONTINUE_SEARCH;
}

int main(int argc, char** argv)
{
	// 崩溃时写 minidump（否则静默崩溃无从查起）
	AddVectoredExceptionHandler(1, ServerCrashVeh);

	// 控制台切到 UTF-8：源码里的中文、插件名、玩家名全是 UTF-8 字节，
	// 不切的话老控制台按 GBK 解读，日志全是乱码（插件名也一样）。
	// 输出被重定向到文件时这个调用无害（文件里本来就是 UTF-8 字节）。
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);

	// 控制台输出必须即时可见：stdout 一旦被重定向（> server.log）默认就是块缓冲，
	// 启动日志会一直卡在缓冲区里，出问题时什么都看不到。
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	setWorkingDirectoryToExe();

	LOGI("\n");
	LOGI("[server] MinecraftPE-Sever starting (pid=%lu)\n", (unsigned long)GetCurrentProcessId());

	AppContext ctx = {};
	ctx.doRender = false;                       // 无头：不换帧、不碰 GL
	ctx.platform = new AppPlatform_win32();     // 平台层只用它的文件/时间/字符串接口

	ServerApp* app = new ServerApp();
	app->externalStoragePath = ".";             // 存档根目录 = 服务器 exe 目录
	app->externalCacheStoragePath = ".";

	// App::init(AppContext&) 会调用虚函数 init() -> ServerApp::init()
	app->init(ctx);

	// 插件（“文员”）：服务器已经起来了，加载 plugins/*.js。
	// 插件拿不到“改动游戏内容”的那套 API（那是 mod 的事），只管服务器事务：
	// 配置、账户、ops、聊天格式、命令。
	if (ServerSideNetworkHandler* ssn = dynamic_cast<ServerSideNetworkHandler*>(app->netCallback)) {
		PluginEngine::setHost(ssn);
		PluginEngine::loadAll();
	} else {
		LOGI("[plugin] 没有拿到服务器处理器，插件未加载\n");
	}

	// 控制台（stdin）命令线程
	std::thread consoleThread(consoleThreadProc);

	// 主循环。节拍由引擎内部的 Timer 决定（20 tick/s），这里只做最小让出，
	// 避免空转把一个核心跑满。
	while (!app->wantToQuit()) {
		app->update();

		{
			std::deque<std::string> pending;
			{
				std::lock_guard<std::mutex> lock(g_consoleMutex);
				pending.swap(g_consoleLines);
			}
			for (size_t i = 0; i < pending.size(); ++i) {
				const std::string& cmd = pending[i];
				if (cmd == "stop" || cmd == "exit" || cmd == "quit") {
					LOGI("[server] console: shutting down\n");
					app->requestShutdown();
				} else if (cmd == "list" || cmd == "players") {
					if (ServerSideNetworkHandler* ssn = dynamic_cast<ServerSideNetworkHandler*>(app->netCallback))
						LOGI("[server] 在线玩家:%s\n", ssn->listOnlinePlayers().c_str());
				} else if (cmd == "help") {
					LOGI("[server] 控制台命令:\n"
					     "  list                          在线玩家与各自模式\n"
					     "  gamemode <survival|creative> <玩家名>   改某个玩家的游戏模式\n"
					     "  plugins [reload]              看/重载插件（plugins/*.js）\n"
					     "  stop                          关服（先把在线玩家写回存档）\n");
				} else if (cmd.compare(0, 3, "op ") == 0) {
					// 控制台天然有权限 —— 能敲到服务器窗口的人本来就能改一切
					if (ServerSideNetworkHandler* ssn = dynamic_cast<ServerSideNetworkHandler*>(app->netCallback))
						ssn->makeOp(cmd.substr(3));
				} else if (cmd.compare(0, 5, "deop ") == 0) {
					if (ServerSideNetworkHandler* ssn = dynamic_cast<ServerSideNetworkHandler*>(app->netCallback)) {
						const std::string name = cmd.substr(5);
						LOGI("[server] %s\n", ssn->removeOp(name) ? (name + " 已被取消管理员").c_str()
						                                        : (name + " 本来就不是管理员").c_str());
					}
				} else if (cmd.compare(0, 9, "gamemode ") == 0 || cmd.compare(0, 3, "gm ") == 0) {
					// gamemode <survival|creative> <玩家名>（玩家名可以带空格，取剩余整段）
					std::string rest = (cmd.compare(0, 3, "gm ") == 0) ? cmd.substr(3) : cmd.substr(9);
					size_t sp = rest.find(' ');
					int type = -1;
					std::string name;
					if (sp != std::string::npos) {
						std::string mode = rest.substr(0, sp);
						name = rest.substr(sp + 1);
						while (!name.empty() && name[0] == ' ')
							name.erase(0, 1);
						if (mode == "creative" || mode == "c" || mode == "1")      type = GameType::Creative;
						else if (mode == "survival" || mode == "s" || mode == "0") type = GameType::Survival;
					}
					if (type < 0 || name.empty()) {
						LOGI("[server] 用法: gamemode <survival|creative> <玩家名>\n");
					} else if (ServerSideNetworkHandler* ssn = dynamic_cast<ServerSideNetworkHandler*>(app->netCallback)) {
						std::string msg;
						ssn->setPlayerGameType(name, type, msg);
						LOGI("[server] %s\n", msg.c_str());
					}
				} else if (cmd == "plugins" || cmd.compare(0, 8, "plugins ") == 0) {
					if (cmd == "plugins reload") {
						PluginEngine::loadAll();
						LOGI("[server] 插件已重载，共 %d 个\n", PluginEngine::count());
					} else {
						LOGI("[server] 插件（%d 个）:%s\n", PluginEngine::count(), PluginEngine::listText().c_str());
					}
				} else if (!cmd.empty()) {
					LOGI("[server] unknown console command: %s (try: help)\n", cmd.c_str());
				}
			}
		}

		Sleep(1);
	}

	g_consoleRunning = false;
	consoleThread.detach();

	LOGI("[server] bye\n");
	return 0;
}
