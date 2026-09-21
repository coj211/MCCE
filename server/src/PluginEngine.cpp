#include "PluginEngine.h"

#include "PlayerStore.h"
#include "ServerPaths.h"
#include "network/ServerSideNetworkHandler.h"
#include "platform/log.h"
#include "platform/time.h"

#ifdef WIN32
#include <direct.h>   // _wmkdir
#include <io.h>       // _wfindfirst / _wremove / _waccess
#endif

#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <vector>

//
// 插件（"文员"）里可用的 API —— 只跑在服务器，客户端完全不知道它的存在。
//
//   server.log(msg)                          写服务器日志
//   server.broadcast(text)                   给所有人发一条聊天
//   server.sendTo(玩家名, text)               给某个玩家私聊
//   server.players()                         在线玩家 [{name, mode, x, y, z, health}]
//   server.getConfig(key) / setConfig(k,v)   读/写 server.properties（改完重启生效）
//   server.deletePlayerSave(名字)             删掉这个玩家的存档（账户/背包一起清）
//   server.setGameMode(名字, "creative")      改在线玩家的游戏模式（当场生效）
//
//   files.read(path) / write(path,text) / append(path,text) / list(dir) / delete(path) / exists(path)
//                                            服务器目录下的读写（**沙箱**：不许 .. 与绝对路径）
//
//   ops.list() / add(name) / remove(name)    管理员名单（ops.txt）
//   bans.list() / add(name,reason) / remove(name) / isBanned(name)     封禁名单（bans.txt）
//   whitelist.list() / add(name) / remove(name) / enabled() / enable(bool)   白名单（whitelist.txt）
//
//   setTimeout(fn, 毫秒) / setInterval(fn, 毫秒) / clearTimer(id)
//
//   chat.setFormat(fn)                       fn(玩家名, 消息) 返回完整聊天行 —— 加称号靠它
//   Commands.register(name, fn)              注册 /name，fn(sender, args) 的返回值回给玩家
//
// 事件（在插件顶层定义同名函数即可）：
//   onPlayerJoin(name) / onPlayerLeave(name) / onChat(name, message) / onTick()
//
// 一个写得对的插件长这样：
//
//   // plugins/称号.js
//   var 称号 = {};
//   onPlayerJoin = function (name) {
//       server.broadcast("欢迎来到服务器：" + name);
//   };
//   Commands.register("称号", function (sender, args) {
//       称号[sender] = args;
//       return "已把你的称号设为 " + args;
//   });
//   chat.setFormat(function (name, msg) {
//       return (称号[name] ? "[" + 称号[name] + "] " : "") + name + ": " + msg;
//   });
//

struct Plugin {
	std::string name;      // 文件名（不含 .js）
	duk_context* ctx;
	std::string path;

	// 这个插件的定时任务（由服务器每 tick 驱动）
	struct Timer {
		int id;
		unsigned int dueMs;
		int intervalMs;    // 0 = 一次性
	};
	std::vector<Timer> timers;
	int nextTimerId;

	// 声明成“只有管理员能用”的命令（Commands.register(name, fn, { op: true })）
	std::set<std::string> opOnly;
};

static std::vector<Plugin> g_plugins;
static ServerSideNetworkHandler* g_host = NULL;
static Plugin* g_current = NULL;    // 正在执行回调的插件（setTimeout 要知道自己属于谁）

// 名单（插件改的就是这几份文件）
static std::set<std::string> g_ops;
static std::set<std::string> g_bans;
static std::map<std::string, std::string> g_banReasons;
static std::set<std::string> g_whitelist;
static bool g_listsLoaded = false;

// ---------------------------------------------------------------------------
// 小工具
// ---------------------------------------------------------------------------
static std::string strOf(duk_context* ctx, duk_idx_t idx)
{
	const char* s = duk_safe_to_string(ctx, idx);
	return std::string(s ? s : "");
}

static void trim(std::string& s)
{
	size_t b = s.find_first_not_of(" \t\r\n");
	size_t e = s.find_last_not_of(" \t\r\n");
	s = (b == std::string::npos) ? std::string() : s.substr(b, e - b + 1);
}

// 沙箱：插件只能碰服务器目录下的**相对路径**。
// 插件是"文员"——能改服务器目录里的东西，但不该跑到系统盘乱写，
// 也别想用 .. 爬出服务器目录。
static bool safeRelPath(const std::string& p)
{
	if (p.empty() || p.size() > 260)
		return false;
	if (p.find("..") != std::string::npos)
		return false;
	if (p[0] == '/' || p[0] == '\\')
		return false;
	if (p.size() >= 2 && p[1] == ':')      // C:\...
		return false;
	return true;
}

// 逐级创建目录（write 的时候顺手建，免得插件自己操心）
static void makeDirs(const std::string& relPath)
{
	size_t pos = relPath.find_last_of("/\\");
	if (pos == std::string::npos)
		return;
	std::string dir = relPath.substr(0, pos);
	if (dir.empty())
		return;

	std::string cur;
	for (size_t i = 0; i <= dir.size(); ++i) {
		if (i == dir.size() || dir[i] == '/' || dir[i] == '\\') {
			if (!cur.empty()) {
#ifdef WIN32
				_wmkdir(ServerPaths::toWide(cur).c_str());
#else
				mkdir(cur.c_str(), 0777);
#endif
			}
		}
		if (i < dir.size())
			cur += dir[i];
	}
}

static std::string readTextFile(const std::string& relPath)
{
#ifdef WIN32
	std::ifstream in(ServerPaths::toWide(relPath).c_str(), std::ios::binary);
#else
	std::ifstream in(relPath.c_str(), std::ios::binary);
#endif
	if (!in.is_open())
		return std::string();
	std::ostringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

static bool writeTextFile(const std::string& relPath, const std::string& text, bool append)
{
	makeDirs(relPath);
#ifdef WIN32
	FILE* f = _wfopen(ServerPaths::toWide(relPath).c_str(), append ? L"ab" : L"wb");
#else
	FILE* f = fopen(relPath.c_str(), append ? "ab" : "wb");
#endif
	if (!f)
		return false;
	bool ok = (text.empty() || fwrite(text.data(), 1, text.size(), f) == text.size());
	fclose(f);
	return ok;
}

// 一行一个名字的名单文件
static std::set<std::string> loadNameList(const std::string& relPath)
{
	std::set<std::string> out;
	std::string all = readTextFile(relPath);
	std::istringstream ss(all);
	std::string line;
	while (std::getline(ss, line)) {
		trim(line);
		if (!line.empty() && line[0] != '#')
			out.insert(line);
	}
	return out;
}

static void saveNameList(const std::string& relPath, const std::set<std::string>& names)
{
	std::string out;
	for (std::set<std::string>::const_iterator it = names.begin(); it != names.end(); ++it)
		out += *it + "\n";
	writeTextFile(relPath, out, false);
}

static void loadListsIfNeeded()
{
	if (g_listsLoaded)
		return;
	g_listsLoaded = true;
	g_ops = loadNameList("ops.txt");
	g_whitelist = loadNameList("whitelist.txt");

	// bans.txt 每行 "名字" 或 "名字|原因"
	g_bans.clear();
	g_banReasons.clear();
	std::istringstream ss(readTextFile("bans.txt"));
	std::string line;
	while (std::getline(ss, line)) {
		trim(line);
		if (line.empty() || line[0] == '#')
			continue;
		size_t bar = line.find('|');
		if (bar == std::string::npos) {
			g_bans.insert(line);
		} else {
			std::string name = line.substr(0, bar);
			trim(name);
			g_bans.insert(name);
			g_banReasons[name] = line.substr(bar + 1);
		}
	}
}

static void saveBans()
{
	std::string out;
	for (std::map<std::string, std::string>::const_iterator it = g_banReasons.begin(); it != g_banReasons.end(); ++it)
		out += it->first + "|" + it->second + "\n";
	for (std::set<std::string>::const_iterator it = g_bans.begin(); it != g_bans.end(); ++it)
		if (g_banReasons.find(*it) == g_banReasons.end())
			out += *it + "\n";
	writeTextFile("bans.txt", out, false);
}

// 调某个插件的全局函数（存在且是函数才调）。参数由调用方先压栈。
static bool callGlobal(Plugin& p, const char* fn, int nargs)
{
	Plugin* prev = g_current;
	g_current = &p;

	duk_context* ctx = p.ctx;
	duk_get_global_string(ctx, fn);
	if (!duk_is_function(ctx, -1)) {
		duk_pop(ctx);
		g_current = prev;
		return false;
	}
	duk_insert(ctx, -1 - nargs);
	bool ok = true;
	if (duk_pcall(ctx, nargs) != 0) {
		LOGW("[plugin:%s] %s 出错: %s\n", p.name.c_str(), fn, strOf(ctx, -1).c_str());
		ok = false;
	}
	duk_pop(ctx);
	g_current = prev;
	return ok;
}

// 可取消事件用：调插件的 fn(args...)，插件 return false 表示“我反对这次操作”。
// 没定义这个函数的插件视为不反对。
static bool callGlobalBool(Plugin& p, const char* fn, int nargs)
{
	Plugin* prev = g_current;
	g_current = &p;

	duk_context* ctx = p.ctx;
	duk_get_global_string(ctx, fn);
	if (!duk_is_function(ctx, -1)) {
		duk_pop(ctx);
		g_current = prev;
		return true;
	}
	duk_insert(ctx, -1 - nargs);
	bool allow = true;
	if (duk_pcall(ctx, nargs) != 0) {
		LOGW("[plugin:%s] %s 出错: %s\n", p.name.c_str(), fn, strOf(ctx, -1).c_str());
	} else {
		allow = (duk_to_boolean(ctx, -1) != 0);
	}
	duk_pop(ctx);
	g_current = prev;
	return allow;
}

// ---------------------------------------------------------------------------
// JS API: server.*
// ---------------------------------------------------------------------------
static duk_ret_t jsLog(duk_context* ctx)
{
	LOGI("[plugin] %s\n", strOf(ctx, 0).c_str());
	return 0;
}

static duk_ret_t jsBroadcast(duk_context* ctx)
{
	if (g_host)
		g_host->broadcastMessage(strOf(ctx, 0));
	return 0;
}

static duk_ret_t jsSendTo(duk_context* ctx)
{
	bool ok = false;
	if (g_host)
		ok = g_host->sendToPlayerByName(strOf(ctx, 0), strOf(ctx, 1));
	duk_push_boolean(ctx, ok);
	return 1;
}

static duk_ret_t jsPlayers(duk_context* ctx)
{
	duk_push_array(ctx);
	if (!g_host)
		return 1;
	std::vector<ServerSideNetworkHandler::PluginPlayerInfo> list = g_host->pluginPlayerSnapshot();
	for (size_t i = 0; i < list.size(); ++i) {
		duk_push_object(ctx);
		duk_push_string(ctx, list[i].name.c_str());   duk_put_prop_string(ctx, -2, "name");
		duk_push_int(ctx, list[i].gameType);          duk_put_prop_string(ctx, -2, "mode");
		duk_push_number(ctx, list[i].x);              duk_put_prop_string(ctx, -2, "x");
		duk_push_number(ctx, list[i].y);              duk_put_prop_string(ctx, -2, "y");
		duk_push_number(ctx, list[i].z);              duk_put_prop_string(ctx, -2, "z");
		duk_push_int(ctx, list[i].health);            duk_put_prop_string(ctx, -2, "health");
		duk_put_prop_index(ctx, -2, (duk_uarridx_t)i);
	}
	return 1;
}

// ---- server.properties 读写 ----
static const char* kConfigPath = "server.properties";

static std::string readConfigValue(const std::string& key)
{
	std::istringstream in(readTextFile(kConfigPath));
	std::string line;
	while (std::getline(in, line)) {
		trim(line);
		if (line.empty() || line[0] == '#')
			continue;
		size_t eq = line.find('=');
		if (eq == std::string::npos)
			continue;
		std::string k = line.substr(0, eq);
		trim(k);
		if (k == key) {
			std::string v = line.substr(eq + 1);
			trim(v);
			return v;
		}
	}
	return std::string();
}

static bool writeConfigValue(const std::string& key, const std::string& value)
{
	std::vector<std::string> lines;
	bool replaced = false;
	{
		std::istringstream in(readTextFile(kConfigPath));
		std::string line;
		while (std::getline(in, line)) {
			std::string t = line;
			trim(t);
			if (!t.empty() && t[0] != '#') {
				size_t eq = t.find('=');
				if (eq != std::string::npos) {
					std::string k = t.substr(0, eq);
					trim(k);
					if (k == key) {
						lines.push_back(key + "=" + value);
						replaced = true;
						continue;
					}
				}
			}
			lines.push_back(line);
		}
	}
	if (!replaced)
		lines.push_back(key + "=" + value);

	std::string out;
	for (size_t i = 0; i < lines.size(); ++i)
		out += lines[i] + "\n";
	return writeTextFile(kConfigPath, out, false);
}

static duk_ret_t jsGetConfig(duk_context* ctx)
{
	duk_push_string(ctx, readConfigValue(strOf(ctx, 0)).c_str());
	return 1;
}

static duk_ret_t jsSetConfig(duk_context* ctx)
{
	std::string key = strOf(ctx, 0);
	std::string value = strOf(ctx, 1);
	bool ok = !key.empty() && writeConfigValue(key, value);
	if (ok)
		LOGI("[plugin] setConfig: %s=%s（重启服务器后生效）\n", key.c_str(), value.c_str());
	duk_push_boolean(ctx, ok);
	return 1;
}

static duk_ret_t jsDeletePlayerSave(duk_context* ctx)
{
	std::string name = strOf(ctx, 0);
	bool ok = false;
	if (g_host) {
		Level* w = g_host->pluginWorld();
		if (w) {
			std::string path = PlayerStore::dirFor(w) + "/" + PlayerStore::safeFileName(name) + ".dat";
#ifdef WIN32
			ok = (_wremove(ServerPaths::toWide(path).c_str()) == 0);
#else
			ok = (remove(path.c_str()) == 0);
#endif
			LOGI("[plugin] deletePlayerSave: %s -> %s\n", name.c_str(), ok ? "已删除" : "文件不存在");
		}
	}
	duk_push_boolean(ctx, ok);
	return 1;
}

static duk_ret_t jsSetGameMode(duk_context* ctx)
{
	std::string name = strOf(ctx, 0);
	std::string mode = strOf(ctx, 1);
	int type = (mode == "creative" || mode == "创造" || mode == "1") ? 1
	         : (mode == "survival" || mode == "生存" || mode == "0") ? 0 : -1;

	bool ok = false;
	std::string msg;
	if (type >= 0 && g_host)
		ok = g_host->setPlayerGameType(name, type, msg);
	else
		msg = "用法: server.setGameMode(玩家名, \"creative\"|\"survival\")";
	if (!ok)
		LOGI("[plugin] setGameMode 失败: %s\n", msg.c_str());
	duk_push_string(ctx, msg.c_str());
	return 1;
}

// ---------------------------------------------------------------------------
// JS API: files.*（沙箱在服务器目录内）
// ---------------------------------------------------------------------------
static duk_ret_t jsFilesRead(duk_context* ctx)
{
	std::string path = strOf(ctx, 0);
	if (!safeRelPath(path)) {
		LOGW("[plugin] files.read 拒绝越界路径: %s\n", path.c_str());
		duk_push_string(ctx, "");
		return 1;
	}
	duk_push_string(ctx, readTextFile(path).c_str());
	return 1;
}

static duk_ret_t jsFilesWrite(duk_context* ctx)
{
	std::string path = strOf(ctx, 0);
	if (!safeRelPath(path)) {
		LOGW("[plugin] files.write 拒绝越界路径: %s\n", path.c_str());
		duk_push_boolean(ctx, false);
		return 1;
	}
	duk_push_boolean(ctx, writeTextFile(path, strOf(ctx, 1), false));
	return 1;
}

static duk_ret_t jsFilesAppend(duk_context* ctx)
{
	std::string path = strOf(ctx, 0);
	if (!safeRelPath(path)) {
		LOGW("[plugin] files.append 拒绝越界路径: %s\n", path.c_str());
		duk_push_boolean(ctx, false);
		return 1;
	}
	duk_push_boolean(ctx, writeTextFile(path, strOf(ctx, 1), true));
	return 1;
}

static duk_ret_t jsFilesExists(duk_context* ctx)
{
	std::string path = strOf(ctx, 0);
	bool ok = false;
#ifdef WIN32
	ok = safeRelPath(path) && (_waccess(ServerPaths::toWide(path).c_str(), 0) == 0);
#endif
	duk_push_boolean(ctx, ok);
	return 1;
}

static duk_ret_t jsFilesDelete(duk_context* ctx)
{
	std::string path = strOf(ctx, 0);
	bool ok = false;
	if (safeRelPath(path)) {
#ifdef WIN32
		ok = (_wremove(ServerPaths::toWide(path).c_str()) == 0);
#endif
	}
	duk_push_boolean(ctx, ok);
	return 1;
}

static duk_ret_t jsFilesList(duk_context* ctx)
{
	std::string dir = strOf(ctx, 0);
	duk_push_array(ctx);
	if (!safeRelPath(dir))
		return 1;
#ifdef WIN32
	std::wstring pattern = ServerPaths::toWide(dir);
	if (!pattern.empty() && pattern[pattern.size() - 1] != L'/' && pattern[pattern.size() - 1] != L'\\')
		pattern += L'\\';
	pattern += L'*';

	_wfinddata_t fd;
	intptr_t h = _wfindfirst(pattern.c_str(), &fd);
	if (h == -1)
		return 1;
	duk_uarridx_t idx = 0;
	do {
		if (fd.attrib & _A_SUBDIR)
			continue;      // 只要文件，先不递归
		duk_push_string(ctx, ServerPaths::toUtf8(fd.name).c_str());
		duk_put_prop_index(ctx, -2, idx++);
	} while (_wfindnext(h, &fd) == 0);
	_findclose(h);
#endif
	return 1;
}

// ---------------------------------------------------------------------------
// JS API: ops / bans / whitelist
// ---------------------------------------------------------------------------
static duk_ret_t jsOpsList(duk_context* ctx)
{
	loadListsIfNeeded();
	duk_push_array(ctx);
	duk_uarridx_t i = 0;
	for (std::set<std::string>::const_iterator it = g_ops.begin(); it != g_ops.end(); ++it) {
		duk_push_string(ctx, it->c_str());
		duk_put_prop_index(ctx, -2, i++);
	}
	return 1;
}

static duk_ret_t jsOpsAdd(duk_context* ctx)
{
	loadListsIfNeeded();
	std::string name = strOf(ctx, 0);
	if (name.empty()) {
		duk_push_boolean(ctx, false);
		return 1;
	}
	g_ops.insert(name);
	saveNameList("ops.txt", g_ops);
	if (g_host)
		g_host->reloadOps();          // 让服务器重新读名单（不然它还用旧的缓存）
	LOGI("[plugin] ops.add: %s\n", name.c_str());
	duk_push_boolean(ctx, true);
	return 1;
}

static duk_ret_t jsOpsRemove(duk_context* ctx)
{
	loadListsIfNeeded();
	std::string name = strOf(ctx, 0);
	bool ok = (g_ops.erase(name) > 0);
	if (ok) {
		saveNameList("ops.txt", g_ops);
		if (g_host)
			g_host->reloadOps();
		LOGI("[plugin] ops.remove: %s\n", name.c_str());
	}
	duk_push_boolean(ctx, ok);
	return 1;
}

static duk_ret_t jsBansList(duk_context* ctx)
{
	loadListsIfNeeded();
	duk_push_array(ctx);
	duk_uarridx_t i = 0;
	for (std::set<std::string>::const_iterator it = g_bans.begin(); it != g_bans.end(); ++it) {
		duk_push_string(ctx, it->c_str());
		duk_put_prop_index(ctx, -2, i++);
	}
	return 1;
}

static duk_ret_t jsBansAdd(duk_context* ctx)
{
	loadListsIfNeeded();
	std::string name = strOf(ctx, 0);
	std::string reason = strOf(ctx, 1);
	if (name.empty()) {
		duk_push_boolean(ctx, false);
		return 1;
	}
	g_bans.insert(name);
	if (!reason.empty())
		g_banReasons[name] = reason;
	saveBans();
	LOGI("[plugin] bans.add: %s（%s）\n", name.c_str(), reason.c_str());
	duk_push_boolean(ctx, true);
	return 1;
}

static duk_ret_t jsBansRemove(duk_context* ctx)
{
	loadListsIfNeeded();
	std::string name = strOf(ctx, 0);
	g_banReasons.erase(name);
	bool ok = (g_bans.erase(name) > 0);
	if (ok) {
		saveBans();
		LOGI("[plugin] bans.remove: %s\n", name.c_str());
	}
	duk_push_boolean(ctx, ok);
	return 1;
}

static duk_ret_t jsBansIsBanned(duk_context* ctx)
{
	loadListsIfNeeded();
	duk_push_boolean(ctx, g_bans.find(strOf(ctx, 0)) != g_bans.end());
	return 1;
}

static duk_ret_t jsWhitelistList(duk_context* ctx)
{
	loadListsIfNeeded();
	duk_push_array(ctx);
	duk_uarridx_t i = 0;
	for (std::set<std::string>::const_iterator it = g_whitelist.begin(); it != g_whitelist.end(); ++it) {
		duk_push_string(ctx, it->c_str());
		duk_put_prop_index(ctx, -2, i++);
	}
	return 1;
}

static duk_ret_t jsWhitelistAdd(duk_context* ctx)
{
	loadListsIfNeeded();
	std::string name = strOf(ctx, 0);
	if (name.empty()) {
		duk_push_boolean(ctx, false);
		return 1;
	}
	g_whitelist.insert(name);
	saveNameList("whitelist.txt", g_whitelist);
	LOGI("[plugin] whitelist.add: %s\n", name.c_str());
	duk_push_boolean(ctx, true);
	return 1;
}

static duk_ret_t jsWhitelistRemove(duk_context* ctx)
{
	loadListsIfNeeded();
	bool ok = (g_whitelist.erase(strOf(ctx, 0)) > 0);
	if (ok)
		saveNameList("whitelist.txt", g_whitelist);
	duk_push_boolean(ctx, ok);
	return 1;
}

static duk_ret_t jsWhitelistEnabled(duk_context* ctx)
{
	std::string v = readConfigValue("white-list");
	duk_push_boolean(ctx, (v == "true" || v == "1" || v == "on"));
	return 1;
}

static duk_ret_t jsWhitelistEnable(duk_context* ctx)
{
	bool on = duk_to_boolean(ctx, 0) != 0;
	bool ok = writeConfigValue("white-list", on ? "true" : "false");
	duk_push_boolean(ctx, ok);
	return 1;
}

// ---------------------------------------------------------------------------
// JS API: 定时任务（服务器每 tick 驱动）
// ---------------------------------------------------------------------------
static void storeTimerHandler(duk_context* ctx, int id)
{
	duk_push_global_object(ctx);
	duk_dup(ctx, 0);
	duk_put_prop_string(ctx, -2, ("__timer_" + std::to_string(id)).c_str());
	duk_pop(ctx);
}

static duk_ret_t jsSetTimeout(duk_context* ctx)
{
	if (!g_current || !duk_is_function(ctx, 0)) {
		duk_push_int(ctx, -1);
		return 1;
	}
	int ms = duk_to_int(ctx, 1);
	if (ms < 0)
		ms = 0;
	int id = g_current->nextTimerId++;
	storeTimerHandler(ctx, id);

	Plugin::Timer t;
	t.id = id;
	t.dueMs = getTimeMs() + (unsigned int)ms;
	t.intervalMs = 0;
	g_current->timers.push_back(t);
	duk_push_int(ctx, id);
	return 1;
}

static duk_ret_t jsSetInterval(duk_context* ctx)
{
	if (!g_current || !duk_is_function(ctx, 0)) {
		duk_push_int(ctx, -1);
		return 1;
	}
	int ms = duk_to_int(ctx, 1);
	if (ms < 50)
		ms = 50;              // 别让插件把服务器拖死
	int id = g_current->nextTimerId++;
	storeTimerHandler(ctx, id);

	Plugin::Timer t;
	t.id = id;
	t.dueMs = getTimeMs() + (unsigned int)ms;
	t.intervalMs = ms;
	g_current->timers.push_back(t);
	duk_push_int(ctx, id);
	return 1;
}

static duk_ret_t jsClearTimer(duk_context* ctx)
{
	if (!g_current) {
		duk_push_boolean(ctx, false);
		return 1;
	}
	int id = duk_to_int(ctx, 0);
	bool found = false;
	for (size_t i = 0; i < g_current->timers.size(); ++i) {
		if (g_current->timers[i].id == id) {
			g_current->timers.erase(g_current->timers.begin() + i);
			found = true;
			break;
		}
	}
	if (found) {
		duk_push_global_object(g_current->ctx);
		duk_del_prop_string(g_current->ctx, -1, ("__timer_" + std::to_string(id)).c_str());
		duk_pop(g_current->ctx);
	}
	duk_push_boolean(ctx, found);
	return 1;
}

// ---------------------------------------------------------------------------
// JS API: chat.* / Commands.*
// ---------------------------------------------------------------------------
static duk_ret_t jsChatSetFormat(duk_context* ctx)
{
	if (!duk_is_function(ctx, 0)) {
		duk_push_boolean(ctx, false);
		return 1;
	}
	duk_push_global_object(ctx);
	duk_dup(ctx, 0);
	duk_put_prop_string(ctx, -2, "__chatFormat");
	duk_pop(ctx);
	duk_push_boolean(ctx, true);
	return 1;
}

static duk_ret_t jsCommandsRegister(duk_context* ctx)
{
	std::string name = strOf(ctx, 0);
	if (!name.empty() && name[0] == '/')
		name = name.substr(1);
	if (name.empty() || !duk_is_function(ctx, 1)) {
		duk_push_boolean(ctx, false);
		return 1;
	}
	duk_push_global_object(ctx);
	duk_dup(ctx, 1);
	duk_put_prop_string(ctx, -2, ("__cmd_" + name).c_str());
	duk_pop(ctx);

	// 第三个参数可选：{ op: true } → 只有管理员能用这条命令
	bool opOnly = false;
	if (duk_is_object(ctx, 2)) {
		duk_get_prop_string(ctx, 2, "op");
		opOnly = (duk_to_boolean(ctx, -1) != 0);
		duk_pop(ctx);
	}
	if (opOnly && g_current)
		g_current->opOnly.insert(name);

	LOGI("[plugin] 注册命令 /%s%s\n", name.c_str(), opOnly ? "（需管理员）" : "");
	duk_push_boolean(ctx, true);
	return 1;
}

// ---------------------------------------------------------------------------
// 加载 / 卸载
// ---------------------------------------------------------------------------
static void registerApi(duk_context* ctx)
{
	duk_push_global_object(ctx);

	// server.*
	duk_push_object(ctx);
	duk_push_c_function(ctx, jsLog, 1);              duk_put_prop_string(ctx, -2, "log");
	duk_push_c_function(ctx, jsBroadcast, 1);        duk_put_prop_string(ctx, -2, "broadcast");
	duk_push_c_function(ctx, jsSendTo, 2);           duk_put_prop_string(ctx, -2, "sendTo");
	duk_push_c_function(ctx, jsPlayers, 0);          duk_put_prop_string(ctx, -2, "players");
	duk_push_c_function(ctx, jsGetConfig, 1);        duk_put_prop_string(ctx, -2, "getConfig");
	duk_push_c_function(ctx, jsSetConfig, 2);        duk_put_prop_string(ctx, -2, "setConfig");
	duk_push_c_function(ctx, jsDeletePlayerSave, 1); duk_put_prop_string(ctx, -2, "deletePlayerSave");
	duk_push_c_function(ctx, jsSetGameMode, 2);      duk_put_prop_string(ctx, -2, "setGameMode");
	duk_put_prop_string(ctx, -2, "server");

	// files.*
	duk_push_object(ctx);
	duk_push_c_function(ctx, jsFilesRead, 1);        duk_put_prop_string(ctx, -2, "read");
	duk_push_c_function(ctx, jsFilesWrite, 2);       duk_put_prop_string(ctx, -2, "write");
	duk_push_c_function(ctx, jsFilesAppend, 2);      duk_put_prop_string(ctx, -2, "append");
	duk_push_c_function(ctx, jsFilesList, 1);        duk_put_prop_string(ctx, -2, "list");
	duk_push_c_function(ctx, jsFilesDelete, 1);      duk_put_prop_string(ctx, -2, "delete");
	duk_push_c_function(ctx, jsFilesExists, 1);      duk_put_prop_string(ctx, -2, "exists");
	duk_put_prop_string(ctx, -2, "files");

	// ops.* / bans.* / whitelist.*
	duk_push_object(ctx);
	duk_push_c_function(ctx, jsOpsList, 0);          duk_put_prop_string(ctx, -2, "list");
	duk_push_c_function(ctx, jsOpsAdd, 1);           duk_put_prop_string(ctx, -2, "add");
	duk_push_c_function(ctx, jsOpsRemove, 1);        duk_put_prop_string(ctx, -2, "remove");
	duk_put_prop_string(ctx, -2, "ops");

	duk_push_object(ctx);
	duk_push_c_function(ctx, jsBansList, 0);         duk_put_prop_string(ctx, -2, "list");
	duk_push_c_function(ctx, jsBansAdd, 2);          duk_put_prop_string(ctx, -2, "add");
	duk_push_c_function(ctx, jsBansRemove, 1);       duk_put_prop_string(ctx, -2, "remove");
	duk_push_c_function(ctx, jsBansIsBanned, 1);     duk_put_prop_string(ctx, -2, "isBanned");
	duk_put_prop_string(ctx, -2, "bans");

	duk_push_object(ctx);
	duk_push_c_function(ctx, jsWhitelistList, 0);    duk_put_prop_string(ctx, -2, "list");
	duk_push_c_function(ctx, jsWhitelistAdd, 1);     duk_put_prop_string(ctx, -2, "add");
	duk_push_c_function(ctx, jsWhitelistRemove, 1);  duk_put_prop_string(ctx, -2, "remove");
	duk_push_c_function(ctx, jsWhitelistEnabled, 0); duk_put_prop_string(ctx, -2, "enabled");
	duk_push_c_function(ctx, jsWhitelistEnable, 1);  duk_put_prop_string(ctx, -2, "enable");
	duk_put_prop_string(ctx, -2, "whitelist");

	// chat.* / Commands.*
	duk_push_object(ctx);
	duk_push_c_function(ctx, jsChatSetFormat, 1);    duk_put_prop_string(ctx, -2, "setFormat");
	duk_put_prop_string(ctx, -2, "chat");

	duk_push_object(ctx);
	duk_push_c_function(ctx, jsCommandsRegister, 2); duk_put_prop_string(ctx, -2, "register");
	duk_put_prop_string(ctx, -2, "Commands");

	// 定时任务（顶层函数，像浏览器那样直接用）
	duk_push_c_function(ctx, jsSetTimeout, 2);       duk_put_global_string(ctx, "setTimeout");
	duk_push_c_function(ctx, jsSetInterval, 2);      duk_put_global_string(ctx, "setInterval");
	duk_push_c_function(ctx, jsClearTimer, 1);       duk_put_global_string(ctx, "clearTimer");

	duk_pop(ctx);
}

static bool loadOne(const std::string& fileName)
{
	std::string path = std::string("plugins/") + fileName;
	std::string src = readTextFile(path);
	if (src.empty()) {
		LOGW("[plugin] 读不到或空文件: %s\n", path.c_str());
		return false;
	}

	duk_context* ctx = duk_create_heap_default();
	if (!ctx) {
		LOGW("[plugin] 创建 JS 环境失败: %s\n", fileName.c_str());
		return false;
	}
	registerApi(ctx);

	std::string pluginName = fileName;
	if (pluginName.size() > 3)
		pluginName = pluginName.substr(0, pluginName.size() - 3);   // 去掉 .js

	// 先入列表，再 eval —— 顶层就调用 setTimeout / Commands.register 时才认得出
	// 这些注册属于哪个插件（g_current）。
	Plugin p;
	p.name = pluginName;
	p.ctx = ctx;
	p.path = path;
	p.nextTimerId = 1;
	g_plugins.push_back(p);
	g_current = &g_plugins.back();

	if (duk_peval_lstring(ctx, src.data(), src.size()) != 0) {
		LOGW("[plugin] %s 加载出错: %s\n", fileName.c_str(), strOf(ctx, -1).c_str());
		g_current = NULL;
		duk_destroy_heap(ctx);
		g_plugins.pop_back();
		return false;
	}
	duk_pop(ctx);   // 丢掉 eval 的返回值
	g_current = NULL;

	LOGI("[plugin] 已加载 %s\n", fileName.c_str());
	return true;
}

void PluginEngine::setHost(ServerSideNetworkHandler* ssn)
{
	g_host = ssn;
}

void PluginEngine::unloadAll()
{
	for (size_t i = 0; i < g_plugins.size(); ++i) {
		if (g_plugins[i].ctx)
			duk_destroy_heap(g_plugins[i].ctx);
	}
	g_plugins.clear();
	g_current = NULL;
}

void PluginEngine::loadAll()
{
	unloadAll();
	g_listsLoaded = false;   // 名单文件可能被外面改过，reload 时重读
#ifdef WIN32
	_wmkdir(L"plugins");     // 没有就建一个，用户知道往哪放
#endif

#ifdef WIN32
	// 用宽字符扫描：文件名在文件系统里是 UTF-16，窄字符 API 拿到的是本地代码页
	// （GBK）字节，混进 UTF-8 字符串里会乱码，中文文件名也打不开。
	_wfinddata_t fd;
	intptr_t h = _wfindfirst(L"plugins\\*.js", &fd);
	if (h == -1) {
		LOGI("[plugin] plugins/ 下没有 .js 插件\n");
		return;
	}
	do {
		if (fd.attrib & _A_SUBDIR)
			continue;
		loadOne(ServerPaths::toUtf8(fd.name));
	} while (_wfindnext(h, &fd) == 0);
	_findclose(h);
#endif

	loadListsIfNeeded();
	LOGI("[plugin] 共加载 %d 个插件（ops %d / 封禁 %d / 白名单 %d）\n",
	     (int)g_plugins.size(), (int)g_ops.size(), (int)g_bans.size(), (int)g_whitelist.size());
}

int PluginEngine::count()
{
	return (int)g_plugins.size();
}

std::vector<std::string> PluginEngine::names()
{
	std::vector<std::string> out;
	for (size_t i = 0; i < g_plugins.size(); ++i)
		out.push_back(g_plugins[i].name);
	return out;
}

std::string PluginEngine::listText()
{
	std::string out;
	for (size_t i = 0; i < g_plugins.size(); ++i) {
		out += "\n  " + g_plugins[i].name;
		if (!g_plugins[i].timers.empty())
			out += "（定时任务 " + std::to_string(g_plugins[i].timers.size()) + " 个）";
	}
	if (g_plugins.empty())
		return "\n  (没有插件，把 .js 放进 plugins/ 后敲 plugins reload)";
	return out;
}

// ---------------------------------------------------------------------------
// 登录准入
// ---------------------------------------------------------------------------
bool PluginEngine::checkJoinAllowed(const std::string& playerName, std::string& rejectReason)
{
	loadListsIfNeeded();

	std::map<std::string, std::string>::const_iterator it = g_banReasons.find(playerName);
	if (g_bans.find(playerName) != g_bans.end()) {
		rejectReason = "你被这个服务器封禁了";
		if (it != g_banReasons.end() && !it->second.empty())
			rejectReason += "：" + it->second;
		return false;
	}

	std::string wl = readConfigValue("white-list");
	if (wl == "true" || wl == "1" || wl == "on") {
		if (g_whitelist.find(playerName) == g_whitelist.end()) {
			rejectReason = "服务器只允许白名单玩家进入";
			return false;
		}
	}
	return true;
}

// ---------------------------------------------------------------------------
// 可取消事件：任一插件 return false 就取消这次操作（登录插件靠它拦住未登录玩家）
// ---------------------------------------------------------------------------
bool PluginEngine::allowBlockBreak(const std::string& playerName, int x, int y, int z)
{
	for (size_t i = 0; i < g_plugins.size(); ++i) {
		Plugin& p = g_plugins[i];
		duk_push_string(p.ctx, playerName.c_str());
		duk_push_int(p.ctx, x);
		duk_push_int(p.ctx, y);
		duk_push_int(p.ctx, z);
		if (!callGlobalBool(p, "onBlockBreak", 4))
			return false;
	}
	return true;
}

bool PluginEngine::allowBlockPlace(const std::string& playerName, int x, int y, int z)
{
	for (size_t i = 0; i < g_plugins.size(); ++i) {
		Plugin& p = g_plugins[i];
		duk_push_string(p.ctx, playerName.c_str());
		duk_push_int(p.ctx, x);
		duk_push_int(p.ctx, y);
		duk_push_int(p.ctx, z);
		if (!callGlobalBool(p, "onBlockPlace", 4))
			return false;
	}
	return true;
}

bool PluginEngine::allowAttack(const std::string& playerName, int targetEntityId)
{
	for (size_t i = 0; i < g_plugins.size(); ++i) {
		Plugin& p = g_plugins[i];
		duk_push_string(p.ctx, playerName.c_str());
		duk_push_int(p.ctx, targetEntityId);
		if (!callGlobalBool(p, "onAttack", 2))
			return false;
	}
	return true;
}

bool PluginEngine::allowChat(const std::string& playerName, const std::string& message)
{
	for (size_t i = 0; i < g_plugins.size(); ++i) {
		Plugin& p = g_plugins[i];
		duk_push_string(p.ctx, playerName.c_str());
		duk_push_string(p.ctx, message.c_str());
		if (!callGlobalBool(p, "onChat", 2))
			return false;
	}
	return true;
}

// ---------------------------------------------------------------------------
// 事件
// ---------------------------------------------------------------------------
void PluginEngine::fireJoin(const std::string& playerName)
{
	for (size_t i = 0; i < g_plugins.size(); ++i) {
		duk_push_string(g_plugins[i].ctx, playerName.c_str());
		callGlobal(g_plugins[i], "onPlayerJoin", 1);
	}
}

void PluginEngine::fireLeave(const std::string& playerName)
{
	for (size_t i = 0; i < g_plugins.size(); ++i) {
		duk_push_string(g_plugins[i].ctx, playerName.c_str());
		callGlobal(g_plugins[i], "onPlayerLeave", 1);
	}
}

void PluginEngine::fireChat(const std::string& playerName, const std::string& message)
{
	for (size_t i = 0; i < g_plugins.size(); ++i) {
		duk_push_string(g_plugins[i].ctx, playerName.c_str());
		duk_push_string(g_plugins[i].ctx, message.c_str());
		callGlobal(g_plugins[i], "onChat", 2);
	}
}

void PluginEngine::fireTick()
{
	for (size_t i = 0; i < g_plugins.size(); ++i)
		callGlobal(g_plugins[i], "onTick", 0);
}

// 定时任务：服务器每 tick（50ms）调一次。只比对到期时间，到点了才进 JS，
// 所以哪怕插件一个没设也几乎不花时间。
void PluginEngine::tickScheduler()
{
	if (g_plugins.empty())
		return;

	unsigned int now = getTimeMs();
	for (size_t i = 0; i < g_plugins.size(); ++i) {
		Plugin& p = g_plugins[i];
		if (p.timers.empty())
			continue;

		Plugin* prev = g_current;
		g_current = &p;

		// 记下这一轮到期的 id：回调里可能又改了定时器列表，所以调用后按 id 重新查找。
		std::vector<int> due;
		for (size_t k = 0; k < p.timers.size(); ++k)
			if (now >= p.timers[k].dueMs)
				due.push_back(p.timers[k].id);

		for (size_t d = 0; d < due.size(); ++d) {
			std::string key = "__timer_" + std::to_string(due[d]);
			duk_context* ctx = p.ctx;
			duk_get_global_string(ctx, key.c_str());
			if (duk_is_function(ctx, -1)) {
				if (duk_pcall(ctx, 0) != 0) {
					LOGW("[plugin:%s] 定时任务出错: %s\n", p.name.c_str(), strOf(ctx, -1).c_str());
				}
				duk_pop(ctx);
			} else {
				duk_pop(ctx);
			}

			// 还在列表里：一次性任务删掉，循环任务排下一次
			for (size_t k = 0; k < p.timers.size(); ++k) {
				if (p.timers[k].id != due[d])
					continue;
				if (p.timers[k].intervalMs > 0) {
					p.timers[k].dueMs = now + (unsigned int)p.timers[k].intervalMs;
				} else {
					p.timers.erase(p.timers.begin() + k);
					duk_push_global_object(ctx);
					duk_del_prop_string(ctx, -1, key.c_str());
					duk_pop(ctx);
				}
				break;
			}
		}
		g_current = prev;
	}
}

std::string PluginEngine::formatChat(const std::string& playerName, const std::string& message)
{
	std::string fallback = playerName + ": " + message;
	// 后加载的插件优先（方便覆盖前一个）
	for (size_t k = g_plugins.size(); k > 0; --k) {
		Plugin& p = g_plugins[k - 1];
		duk_context* ctx = p.ctx;
		duk_get_global_string(ctx, "__chatFormat");
		if (!duk_is_function(ctx, -1)) {
			duk_pop(ctx);
			continue;
		}
		Plugin* prev = g_current;
		g_current = &p;
		duk_push_string(ctx, playerName.c_str());
		duk_push_string(ctx, message.c_str());
		if (duk_pcall(ctx, 2) != 0) {
			LOGW("[plugin:%s] chatFormat 出错: %s\n", p.name.c_str(), strOf(ctx, -1).c_str());
			duk_pop(ctx);
			g_current = prev;
			continue;
		}
		if (duk_is_string(ctx, -1)) {
			std::string out = strOf(ctx, -1);
			duk_pop(ctx);
			g_current = prev;
			return out;
		}
		duk_pop(ctx);
		g_current = prev;
	}
	return fallback;
}

bool PluginEngine::runCommand(const std::string& line, const std::string& senderName, std::string& reply)
{
	reply.clear();
	std::string body = line;
	if (!body.empty() && body[0] == '/')
		body = body.substr(1);
	std::string name = body, args;
	size_t sp = body.find(' ');
	if (sp != std::string::npos) {
		name = body.substr(0, sp);
		args = body.substr(sp + 1);
	}
	if (name.empty())
		return false;

	for (size_t i = 0; i < g_plugins.size(); ++i) {
		Plugin& p = g_plugins[i];
		duk_context* ctx = p.ctx;
		duk_get_global_string(ctx, ("__cmd_" + name).c_str());
		if (!duk_is_function(ctx, -1)) {
			duk_pop(ctx);
			continue;
		}
		// 声明成 op 专用的命令，普通玩家不许用（权限判定统一走服务器的 ops 名单）
		if (p.opOnly.find(name) != p.opOnly.end()) {
			if (!g_host || !g_host->isOp(senderName)) {
				reply = "你没有权限用 /" + name + "（需要管理员）";
				return true;
			}
		}
		Plugin* prev = g_current;
		g_current = &p;
		duk_push_string(ctx, senderName.c_str());
		duk_push_string(ctx, args.c_str());
		if (duk_pcall(ctx, 2) != 0) {
			reply = "插件命令出错: " + strOf(ctx, -1);
			LOGW("[plugin:%s] /%s 出错: %s\n", p.name.c_str(), name.c_str(), reply.c_str());
			duk_pop(ctx);
			g_current = prev;
			return true;
		}
		if (duk_is_string(ctx, -1))
			reply = strOf(ctx, -1);
		duk_pop(ctx);
		g_current = prev;
		return true;
	}
	return false;
}
