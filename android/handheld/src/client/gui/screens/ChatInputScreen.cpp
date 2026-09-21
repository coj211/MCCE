#include "ChatInputScreen.h"

#include "../../Minecraft.h"
#include "../../User.h"
#include "../../../platform/input/Keyboard.h"
#include "../../../platform/input/Mouse.h"
#include "../../../locale/I18n.h"
#include "../../../platform/log.h"
#include "../../../AppPlatform.h"
#include "../../renderer/gles.h"
#include "../../../network/RakNetInstance.h"
#include "../../../network/packet/MessagePacket.h"

#include <set>
#include <algorithm>

#ifdef _WIN32
#include "../../../mod/ModEngine.h"
#endif

static const int kMaxLen = 120;

// 颜色码：就是字体本来就认的 §（0xA7），Font::drawSlow 会解析。
// 注意 "\xA7e" 会被 C++ 当成一个十六进制转义（把 e 也吃掉），
// 所以必须拆成两个字符串字面量来写。
static const char kYELLOW[] = "\xA7" "e";   // 黄：已经敲进去的命令名前缀
static const char kGREEN[]  = "\xA7" "a";   // 绿：成功提示
static const char kRED[]    = "\xA7" "c";   // 红：失败 / 未找到
static const char kRESET[]  = "\xA7" "r";   // 回到调用方给的基色

// 方向键：Keyboard 里没有 UP/DOWN 常量，直接用 Windows 虚拟键码
// （main_win32.h 的 transformKey_win32 把 wParam 原样透传下来）。
static const int kKeyUp   = 38;
static const int kKeyDown = 40;

// 引擎内置命令的补全表。
// 内置命令本来就是硬编码的 if 链（本文件的 handleCommand，以及联机/房主侧的
// ServerSideNetworkHandler::handleServerCommand），没有注册表可以枚举 ——
// 所以这张表是“给补全提示看的”，改内置命令时顺手同步这里。
// 模组用 Commands.register 注册的命令不在这里，运行时从 ModEngine 合并。
namespace {
struct BuiltinCmd { const char* name; const char* desc; bool serverOnly; };
// serverOnly=true 的命令只有“连别人的服务器”时才算数（客户端把整行发上去，由
// 服务器侧的 ServerSideNetworkHandler::handleServerCommand 执行）；单机/房主
// 自己的进程跑的是本文件的 handleCommand，不认它们 —— 补全列表按当前环境过滤。
const BuiltinCmd kBuiltinCommands[] = {
	{ "myip",     "显示本机的 IP 地址", false },
	{ "ip",       "显示本机的 IP 地址", false },
	{ "op",       "把玩家设为本世界管理员  /op <玩家名>", false },
	{ "deop",     "取消玩家的本世界管理员  /deop <玩家名>", false },
	{ "ops",      "列出本世界的管理员名单", false },
	{ "gamemode", "切换游戏模式  /gamemode creative|survival [玩家名]", true },
	{ "gm",       "切换游戏模式（同 /gamemode）", true },
	{ "plugins",  "查看 / 重载插件", true },
	{ "list",     "列出在线玩家", true },
};
const int kBuiltinCommandCount = (int)(sizeof(kBuiltinCommands) / sizeof(kBuiltinCommands[0]));

// 聊天/补全每行占的高度（屏幕像素）。字形占地是 11px，行距给 12px，留 1px 缝。
const int kLineH = 12;

// ASCII 字母折成小写（多字节 UTF-8 字节原样保留）
std::string foldAsciiLower(const std::string& s) {
	std::string r = s;
	for (size_t i = 0; i < r.size(); ++i)
		if (r[i] >= 'A' && r[i] <= 'Z') r[i] = (char)(r[i] + 32);
	return r;
}

// 不区分大小写的前缀判断（只对 ASCII 字母折叠大小写；中文按字节相等即可）
bool startsWithNoCase(const std::string& name, const std::string& prefix) {
	if (prefix.size() > name.size()) return false;
	for (size_t i = 0; i < prefix.size(); ++i) {
		char a = name[i], b = prefix[i];
		if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
		if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
		if (a != b) return false;
	}
	return true;
}
}  // namespace

ChatInputScreen::ChatInputScreen()
:	message(""), _cursorFrame(0),
	bHeader(NULL), bBack(NULL),
	scrollOffset(0), autoScroll(true),
	_lastCount(-1), _lastTopTicks(-1),
	_dragging(false), _dragStartY(0), _dragStartScroll(0), _dragMoved(false),
	_listTop(0), _listBottom(0)
{
}

// 离开聊天屏：把输入法收掉（hideKeyboard 会把 IME 里最终的文本写回 message，
// 此时 message 已经用过/丢弃，无妨）
void ChatInputScreen::removed() {
	if (minecraft && minecraft->platform())
		minecraft->platform()->hideKeyboard();
}

ChatInputScreen::~ChatInputScreen() {
	// 顶栏/返回按钮是本屏 new 的，跟着屏一起销毁
	if (bHeader) { delete bHeader; bHeader = NULL; }
	if (bBack)   { delete bBack;   bBack = NULL;   }
}

void ChatInputScreen::init() {
	message.clear();
	_suggests.clear();
	scrollOffset = 0;
	autoScroll = true;
	_lastCount = -1;
	_lastTopTicks = -1;
	_dragging = false;
	_dragMoved = false;

	// 弹安卓输入法：聊天输入屏没有 TextBox 控件（hasTextInput() 恒为 true），
	// 必须自己把 message 交给 IME —— 不调这个三参版输入法不会出来。
	// 键盘里敲的字符实时回流到 message（onTextChanged 是整体覆盖），
	// 「完成/回车」由 AppPlatform_android::onTextChanged 转成 KEY_RETURN → submit()。
	if (minecraft && minecraft->platform())
		minecraft->platform()->showKeyboard(&message, kMaxLen, false);
	LOGI("[kbd] ChatInputScreen::init done (message len=%d)\n", (int)message.size());

	// 顶栏 + 返回按钮：素材与“新建世界 / 设置”顶上那条栏一模一样
	if (!bHeader)
		bHeader = new Touch::THeader(0, I18n::get("chat.title"));
	if (!bBack)
		bBack = new Touch::TButton(1, I18n::get("gui.back"), minecraft);
	bBack->width = 38;
	bBack->height = 18;

	// init() 有可能被再调一次，按钮只挂一遍
	bool hasHeader = false, hasBack = false;
	for (size_t i = 0; i < buttons.size(); ++i) {
		if (buttons[i] == bHeader) hasHeader = true;
		if (buttons[i] == bBack)   hasBack = true;
	}
	if (!hasHeader) buttons.push_back(bHeader);
	if (!hasBack)   buttons.push_back(bBack);
}

void ChatInputScreen::setupPositions() {
	// 与 CreateWorldScreen 的顶栏布局一致：栏铺满宽度，高 = 按钮高 + 8
	if (bHeader) {
		bHeader->x = 0;
		bHeader->y = 0;
		bHeader->width = width;
		bHeader->height = (bBack ? bBack->height : 18) + 8;
	}
	if (bBack) {
		bBack->x = 4;
		bBack->y = 4;
	}
	updateListLayout();
}

void ChatInputScreen::updateListLayout() {
	int top = (bHeader ? bHeader->height : 26) + 3;
	int bottom = height - 34 - 3;   // 底部输入条从 height-34 开始
	if (bottom < top + kLineH) bottom = top + kLineH;   // 至少留出一行
	_listTop = top;
	_listBottom = bottom;
}

int ChatInputScreen::visibleRows() const {
	int rows = (_listBottom - _listTop) / kLineH;
	return rows < 1 ? 1 : rows;
}

int ChatInputScreen::maxScroll() const {
	int m = (int)minecraft->gui.getGuiMessages().size() - visibleRows();
	return m < 0 ? 0 : m;
}

bool ChatInputScreen::isSuggesting() const {
	if (message.empty() || message[0] != '/') return false;
	// 敲了空格说明已经在写参数了，这时回到正常的聊天记录显示
	if (message.find(' ') != std::string::npos) return false;
	return true;
}

void ChatInputScreen::buildSuggestions() {
	_suggests.clear();
	if (!isSuggesting()) return;

	std::string prefix = message.substr(1);
	std::set<std::string> seen;   // 名字去重（模组可能与内置命令重名）

	// 1) 内置命令（按当前环境过滤：服务器侧命令只在连别人服务器时列）
	bool onlineClient = minecraft->isOnlineClient();
	for (int i = 0; i < kBuiltinCommandCount; ++i) {
		const BuiltinCmd& c = kBuiltinCommands[i];
		if (c.serverOnly != onlineClient) continue;
		std::string name = c.name;
		if (!startsWithNoCase(name, prefix)) continue;
		if (!seen.insert(name).second) continue;
		Suggest s;
		s.name = name;
		s.desc = c.desc;
		_suggests.push_back(s);
	}

	// 2) 模组用 Commands.register 注册的命令
	//    （名字来自 ModEngine::_commandNames，简述来自它可选的第三个参数）
#ifdef _WIN32
	if (ModEngine::instance) {
		const std::set<std::string>& names = ModEngine::instance->_commandNames;
		for (std::set<std::string>::const_iterator it = names.begin(); it != names.end(); ++it) {
			if (!startsWithNoCase(*it, prefix)) continue;
			if (!seen.insert(*it).second) continue;
			Suggest s;
			s.name = *it;
			s.desc = ModEngine::instance->commandDesc(*it);
			_suggests.push_back(s);
		}
	}
#endif

	std::sort(_suggests.begin(), _suggests.end(),
	          [](const Suggest& a, const Suggest& b) { return a.name < b.name; });
}

void ChatInputScreen::keyPressed(int eventKey) {
	if (eventKey == Keyboard::KEY_RETURN) {
		submit();
	} else if (eventKey == Keyboard::KEY_ESCAPE) {
		minecraft->setScreen(NULL);
	} else if (eventKey == Keyboard::KEY_BACKSPACE) {
		// UTF-8 aware backspace: erase one complete character. A plain
		// message.erase(size-1) would chop a multi-byte CJK char in half,
		// leaving a garbage glyph.
		if (!message.empty()) {
			size_t last = message.size();
			// Skip trailing continuation bytes (0x80-0xBF) of the last
			// UTF-8 sequence, then remove the leading byte too.
			while (last > 0 && ((unsigned char)message[last - 1] & 0xC0) == 0x80)
				--last;
			if (last > 0)
				--last;
			message.erase(last);
		}
		buildSuggestions();
	} else if (eventKey == kKeyUp || eventKey == kKeyDown) {
		// ↑ 往回看更早的，↓ 回到最新
		scrollOffset += (eventKey == kKeyUp) ? 3 : -3;
		if (scrollOffset < 0) scrollOffset = 0;
		int m = maxScroll();
		if (scrollOffset > m) scrollOffset = m;
		// 自己翻了就不再自动贴底；翻回最底恢复跟随
		autoScroll = (scrollOffset <= 0);
	} else {
		super::keyPressed(eventKey);
	}
}

void ChatInputScreen::keyboardNewChar(char inputChar) {
	if (inputChar >= 32 && (int)message.size() < kMaxLen) {
		message += inputChar;
		buildSuggestions();
	}
}

void ChatInputScreen::keyboardText(const std::string& text) {
	// UTF-8 aware: append the whole (possibly multi-byte) string.
	if ((int)(message.size() + text.size()) <= kMaxLen) {
		message += text;
		buildSuggestions();
	}
}

void ChatInputScreen::buttonClicked(Button* button) {
	if (button == bBack) {
		// 只有点“返回”才回游戏（以前是点屏幕任何地方都关）
		minecraft->setScreen(NULL);
		return;
	}
	super::buttonClicked(button);
}

void ChatInputScreen::mouseClicked(int x, int y, int buttonNum) {
	// 在聊天记录区域按下左键：起手拖动（世界列表/设置页那套做法，位移在
	// render() 里每帧按鼠标当前位置重算）。补全列表不长，不参与拖动。
	if (buttonNum == MouseAction::ACTION_LEFT && !isSuggesting()
			&& y >= _listTop && y < _listBottom) {
		_dragging = true;
		_dragStartY = y;
		_dragStartScroll = scrollOffset;
		_dragMoved = false;
	}
	// 其余交给基类做按钮命中（顶栏/返回按钮的按下与释放）
	super::mouseClicked(x, y, buttonNum);

	// 安卓：点击输入框以外的地方时系统会自动收起输入法，于是聊天屏里点一下
	// 画面就没法接着打字了（只能一次打完）。这里补叫一次键盘。
	// 注意放在 super 之后：如果刚点的是“返回”按钮，屏已经关了就不该再弹。
	if (minecraft->screen == this && minecraft->platform())
		minecraft->platform()->reshowKeyboard();
}

void ChatInputScreen::mouseReleased(int x, int y, int buttonNum) {
	bool wasDrag = _dragMoved;
	_dragging = false;
	_dragMoved = false;

	// 松手时一动不动 = 一次点击：补全列表里点在哪条就把它填进输入框。
	// （以前在 mouseClicked 里就填，会和“按下后拖走”打架，所以挪到松手判定。
	//   命中范围与 drawSuggestList 的“这一行画得出来”保持一致。）
	if (!wasDrag && buttonNum == MouseAction::ACTION_LEFT && isSuggesting()
			&& !_suggests.empty() && y >= _listTop && y + kLineH <= _listBottom) {
		int row = (y - _listTop) / kLineH;
		if (row >= 0 && row < (int)_suggests.size()) {
			message = "/" + _suggests[row].name;
			buildSuggestions();
		}
	}
	super::mouseReleased(x, y, buttonNum);
}

void ChatInputScreen::onMouseWheel(int dy) {
	if (dy == 0) return;
	// 往上滚 = 看更早的（方向与 ModsScreen::onMouseWheel 一致）
	scrollOffset += (dy > 0) ? 3 : -3;
	if (scrollOffset < 0) scrollOffset = 0;
	int m = maxScroll();
	if (scrollOffset > m) scrollOffset = m;
	autoScroll = (scrollOffset <= 0);
}

void ChatInputScreen::render(int xm, int ym, float a) {
	// 背景：灰黑半透明蒙板（renderGameBehind()==true），透出后面的游戏画面
	renderBackground();
	updateListLayout();

	// 来了新消息就贴到最新（玩家自己滚过就不打扰他）
	const GuiMessageList& msgs = minecraft->gui.getGuiMessages();
	int grew = 0;
	if ((int)msgs.size() != _lastCount) {
		grew = (int)msgs.size() - _lastCount;
		if (autoScroll) scrollOffset = 0;
		// 玩家正翻着旧消息看的时候，新消息不该把画面整体顶下去一行：
		// 消息是插在表头的，偏移量跟着加同样多就能把视线锚在原地。
		else if (_lastCount >= 0 && grew > 0) {
			scrollOffset += grew;
			// 拖动中：锚点也要跟着揄，否则下面按锚点重算会把这次补偿盖掉。
			if (_dragging) _dragStartScroll += grew;
		}
		_lastCount = (int)msgs.size();
	}
	if (!msgs.empty()) {
		int topTicks = msgs[0].ticks;
		// 条数没变时也可能来了新消息：到 200 上限以后会挤掉最旧一条，条数
		// 不变，但最新一条的 ticks 会从一个较大的值回落，据此发现它。
		// （条数变了的情形上面已经处理过，这里不重复加偏移。）
		if (grew == 0 && _lastTopTicks >= 0 && topTicks < _lastTopTicks) {
			if (autoScroll) scrollOffset = 0;
			else {
				scrollOffset += 1;
				if (_dragging) _dragStartScroll += 1;   // 同上：拖动中锚点一起揄
			}
		}
		_lastTopTicks = topTicks;
	}
	{
		int m = maxScroll();
		if (scrollOffset > m) scrollOffset = m;
		if (scrollOffset < 0) scrollOffset = 0;
	}

	// 按住拖动：按下时的锚点 + 现在鼠标的位置 = 翻了多少行。放在新消息处理
	// 之后，拖动中就听手指/鼠标的（不会被自动贴底拉走）。
	if (_dragging) {
		int rows = (_dragStartY - ym) / kLineH;   // 往上拖 = 看更早的
		if (rows != 0) _dragMoved = true;
		int ns = _dragStartScroll + rows;
		if (ns < 0) ns = 0;
		int m = maxScroll();
		if (ns > m) ns = m;
		scrollOffset = ns;
		autoScroll = (scrollOffset <= 0);
	}

	if (isSuggesting())
		drawSuggestList(ym);
	else
		drawChatList();

	drawInputBar();

	// 顶栏 + 返回按钮最后画，保证盖在蒙板/列表之上
	Screen::render(xm, ym, a);
}

void ChatInputScreen::drawChatList() {
	const GuiMessageList& msgs = minecraft->gui.getGuiMessages();
	if (msgs.empty()) return;

	Font* f = minecraft->font;
	// 最新的一条贴在最下面一行，往上是更早的；scrollOffset 越大翻得越早
	float y = (float)(_listBottom - kLineH);
	int idx = scrollOffset;
	for (; idx < (int)msgs.size(); ++idx, y -= (float)kLineH) {
		if (y < (float)_listTop) break;
		const std::string& m = msgs[idx].message;
		float w = (float)f->width(m);
		// 底衬（做法与 HUD 那条聊天一样：半透明黑）
		fill(0.0f, y - 1.0f, 2.0f + w + 2.0f, y + (float)(kLineH - 1), 0x80000000);
		f->drawShadow(m, 2, y, 0xffffffff);
	}
}

void ChatInputScreen::drawSuggestList(int ym) {
	Font* f = minecraft->font;
	std::string prefix = message.size() > 1 ? message.substr(1) : std::string();

	if (_suggests.empty()) {
		// 一条都没匹配上：直接说清楚，别让玩家以为是卡了
		std::string line = std::string(kRED) + I18n::get("chat.cmdNotFound") + ": " + message;
		f->drawShadow(line, 4.0f, (float)_listTop, 0xffffffff);
		return;
	}

	// 简述统一对齐到“最长的那条指令”右边
	float descX = 4.0f;
	for (size_t i = 0; i < _suggests.size(); ++i) {
		float w = (float)f->width("/" + _suggests[i].name);
		if (w > descX) descX = w;
	}
	descX += 10.0f;

	float y = (float)_listTop;
	for (size_t i = 0; i < _suggests.size(); ++i, y += (float)kLineH) {
		if (y + (float)kLineH > (float)_listBottom) break;   // 放不下的就不画了

		// 鼠标停在哪一行，哪一行就有淡底，提示“这条可以点”
		if (ym >= (int)y - 1 && ym < (int)y + (kLineH - 1))
			fill(0.0f, y - 1.0f, (float)width, y + (float)(kLineH - 1), 0x40ffffff);

		const Suggest& s = _suggests[i];
		// 已经敲进去的那一段显示成黄色（§e），后面照常
		size_t hl = prefix.size();
		if (hl > s.name.size()) hl = s.name.size();
		std::string label = "/";
		if (hl > 0) {
			label += kYELLOW;
			label += s.name.substr(0, hl);
			label += kRESET;
		}
		label += s.name.substr(hl);

		f->drawShadow(label, 4.0f, y, 0xffffffff);
		if (!s.desc.empty())
			f->drawShadow(s.desc, descX, y, 0xffa0a0a0);
	}
}

void ChatInputScreen::drawInputBar() {
	// 输入条：沿用原来的样子（半透明黑条 + 一条分隔线）
	int boxY = height - 34;
	int boxH = 26;
	fillGradient(0, boxY, width, boxY + boxH, 0x90000000, 0xb0000000);
	fillGradient(0, boxY + boxH, width, boxY + boxH + 1, 0xffaaaaaa, 0xffaaaaaa);

	Font* f = minecraft->font;
	std::string display = "> " + message;
	// Cursor: block after the text, blinking every 20 ticks.
	_cursorFrame++;
	if ((_cursorFrame / 20) % 2 == 0) {
		display += "_";
	}
	f->drawShadow(display, 6, boxY + 8, 0xffffffff);
}

void ChatInputScreen::submit() {
	// 空回车不做任何事（以前是关屏回游戏；现在只有“返回”才回游戏）
	if (message.empty())
		return;

	std::string sent = message;
	message.clear();
	_suggests.clear();
	scrollOffset = 0;
	autoScroll = true;

	// ---- 联机时聊天与命令都归服务器（服务器权威）----
	// 连别人的服务器：整行原样发上去（含 '/' 命令），由服务器校验、执行命令，
	// 并把聊天广播回来。本地**不回显**：服务器会把消息发给同一世界的所有人
	// （包括发送者自己），这样大家看到的内容和顺序都以服务器为准。
	if (minecraft->isOnlineClient()) {
		MessagePacket packet(RakNet::RakString(sent.c_str()));
		minecraft->raknetInstance->send(packet);
		return;   // 留在聊天界面：服务器的回显/回复会进聊天记录
	}

	// ---- 单机 / 自己当房主：房主进程本身就是服务器，命令本地执行 ----
	if (sent[0] == '/') {
		// Slash commands: give mods first refusal via onChat (so other
		// mods' /commands keep working), then try the mods' registered
		// commands, then the game's built-in commands.
		//
		// 命令优先级与服务器侧完全一致
		// （ServerSideNetworkHandler::handleMessage -> handleServerCommand）：
		//   1) 先给所有 mod 的 onChat 一次机会（别的 mod 的 /command 靠这里活着）
		//   2) 再让 mod 用 Commands.register 登记过的命令执行 ——
		//      runRegisteredCommand() 第一件事就是 isRegisteredCommand(name)，
		//      没登记的名字直接返回 false、什么都不碰
		//   3) 最后才是游戏内置命令（/myip /op /deop /ops）
#ifdef _WIN32
		if (ModEngine::instance) {
			ModEngine::instance->fireEvent("onChat", sent);

			// 模组命令名是大小写敏感的（Commands.register 存的是原样名字），而补全
			// 列表是不区分大小写匹配的 —— 把命令词纠正成注册表里的确切写法，
			// 免得“/Time 在补全里明明列着、敲下去却报未找到”。
			std::string cmdLine = sent;
			{
				size_t sp = cmdLine.find(' ');
				std::string word = (sp == std::string::npos) ? cmdLine.substr(1) : cmdLine.substr(1, sp - 1);
				std::string rest = (sp == std::string::npos) ? std::string() : cmdLine.substr(sp);
				std::string folded = foldAsciiLower(word);
				const std::set<std::string>& names = ModEngine::instance->_commandNames;
				for (std::set<std::string>::const_iterator it = names.begin(); it != names.end(); ++it) {
					if (foldAsciiLower(*it) == folded) { cmdLine = "/" + *it + rest; break; }
				}
			}

			std::string reply;
			if (ModEngine::instance->runRegisteredCommand(
					cmdLine, minecraft->user->name, reply)) {
				// 模组命令认领了这条：它返回的字符串就是给玩家的回复
				if (reply.compare(0, 15, "Command error: ") == 0) {
					minecraft->gui.addMessage(std::string(kRED) + I18n::get("chat.cmdFailed"));
					minecraft->gui.addMessage(reply);
				} else {
					minecraft->gui.addMessage(std::string(kGREEN) + I18n::get("chat.cmdOk"));
					if (!reply.empty())
						minecraft->gui.addMessage(reply);
				}
				return;
			}
		}
#endif
		std::vector<std::string> out;
		CmdResult r = handleCommand(sent, out);
		if (r == CMD_OK) {
			// 先报“输入成功”，再跟这条命令自己的输出
			minecraft->gui.addMessage(std::string(kGREEN) + I18n::get("chat.cmdOk"));
			for (size_t i = 0; i < out.size(); ++i)
				minecraft->gui.addMessage(out[i]);
		} else if (r == CMD_FAILED) {
			minecraft->gui.addMessage(std::string(kRED) + I18n::get("chat.cmdFailed"));
			for (size_t i = 0; i < out.size(); ++i)
				minecraft->gui.addMessage(out[i]);
		} else {
			// 没人认领这条命令：以前这里故意静默（怕误报模组的 onChat），
			// 现在按玩家要求明确提示一句
			minecraft->gui.addMessage(
				std::string(kRED) + I18n::get("chat.cmdNotFound") + ": " + sent);
		}
		return;
	}

	// 普通聊天
	std::string line = minecraft->user->name + ": " + sent;
	minecraft->gui.addMessage(line);
	// 房主：聊天还得广播给连进来的其他玩家（RakNet 服务器模式下 send 即广播）
	if (minecraft->raknetInstance && minecraft->raknetInstance->isServer()) {
		MessagePacket packet(RakNet::RakString(line.c_str()));
		minecraft->raknetInstance->send(packet);
	}
#ifdef _WIN32
	if (ModEngine::instance)
		ModEngine::instance->fireEvent("onChat", sent);
#endif
}

// ---------------------------------------------------------------------------
// In-game commands
// ---------------------------------------------------------------------------
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")

// Collect the machine's IPv4 / IPv6 addresses into out (one per line).
static void collectLocalIps(std::string& out) {
	ULONG bufLen = 0;
	GetAdaptersAddresses(AF_UNSPEC, 0, NULL, NULL, &bufLen);
	if (bufLen == 0) return;
	IP_ADAPTER_ADDRESSES* addrs = (IP_ADAPTER_ADDRESSES*)malloc(bufLen);
	if (!addrs) return;
	ULONG rc = GetAdaptersAddresses(AF_UNSPEC, 0, NULL, addrs, &bufLen);
	if (rc != NO_ERROR) { free(addrs); return; }

	char host[NI_MAXHOST];
	for (IP_ADAPTER_ADDRESSES* a = addrs; a; a = a->Next) {
		if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
		// Skip non-operational / non-public adapters? Show all but loopback.
		if (a->OperStatus != IfOperStatusUp) continue;
		for (IP_ADAPTER_UNICAST_ADDRESS* u = a->FirstUnicastAddress; u; u = u->Next) {
			int family = u->Address.lpSockaddr->sa_family;
			if (family != AF_INET && family != AF_INET6) continue;
			if (getnameinfo(u->Address.lpSockaddr, u->Address.iSockaddrLength,
			                host, sizeof(host), NULL, 0, NI_NUMERICHOST) != 0)
				continue;
			// Skip link-local IPv6 (fe80::) and 0.0.0.0 — not connectable.
			if (family == AF_INET6 && strncmp(host, "fe80", 4) == 0) continue;
			if (strcmp(host, "0.0.0.0") == 0 || strcmp(host, "::") == 0) continue;
			std::string fam = (family == AF_INET) ? "[IPv4] " : "[IPv6] ";
			out += fam + host + "\n";
		}
	}
	free(addrs);
}
#endif

ChatInputScreen::CmdResult ChatInputScreen::handleCommand(const std::string& cmd, std::vector<std::string>& out) {
	std::string name = cmd.substr(1);  // strip '/'
	// 命令词不区分大小写（与补全列表的匹配方式一致， /OP 也是 /op）；
	// 参数原样保留（玩家名的大小写是有意义的）。
	{
		size_t sp = name.find(' ');
		if (sp == std::string::npos)
			name = foldAsciiLower(name);
		else
			name = foldAsciiLower(name.substr(0, sp)) + name.substr(sp);
	}
	// "/myip" / "/ip" — show this machine's local IPv4 + IPv6 addresses.
	if (name == "myip" || name == "ip") {
#ifdef _WIN32
		std::string ips;
		collectLocalIps(ips);
		if (ips.empty()) {
			out.push_back(I18n::get("chat.ipNotFound"));
			return CMD_FAILED;
		}
		// addMessage renders one message; split lines.
		size_t pos = 0;
		while (pos < ips.size()) {
			size_t nl = ips.find('\n', pos);
			if (nl == std::string::npos) nl = ips.size();
			out.push_back("[IP] " + ips.substr(pos, nl - pos));
			pos = nl + 1;
		}
		return CMD_OK;
#else
		out.push_back(I18n::get("chat.ipWindowsOnly"));
		return CMD_FAILED;
#endif
	}

	// "/op <名字>" / "/deop <名字>" / "/ops" —— 本世界的管理员名单。
	// 名单每个世界一份（存在世界目录的 ops.txt 里），原版游戏不用它，是给模组
	// 判断权限用的。
	// 注意：这里只“多接一个命令”，**没有另做一套命令系统** —— 模组的 onChat
	// 仍然在 handleCommand 之前先拿到整行（见 submit()），别的模组命令不受影响。
#ifdef _WIN32
	{
		std::string word = name, arg;
		size_t sp = name.find(' ');
		if (sp != std::string::npos) {
			word = name.substr(0, sp);
			arg  = name.substr(sp + 1);
			// 去首尾空白
			size_t a = 0, b = arg.size();
			while (a < b && (arg[a] == ' ' || arg[a] == '\t')) ++a;
			while (b > a && (arg[b - 1] == ' ' || arg[b - 1] == '\t')) --b;
			arg = arg.substr(a, b - a);
		}
		if (word == "op" || word == "deop" || word == "ops") {
			ModEngine* me = ModEngine::instance;
			if (!me) {
				out.push_back("[op] 模组引擎未就绪");
				return CMD_FAILED;
			}
			// 权限：只有已经在名单里的人能改名单
			const std::string who = minecraft->user->name;
			if (!me->isOp(who)) {
				out.push_back("[op] 你不是这个世界的 op，不能改名单");
				return CMD_FAILED;
			}
			if (word == "ops") {
				std::set<std::string>& ops = me->worldOps();
				if (ops.empty()) {
					out.push_back("[op] 这个世界的 op 名单是空的");
				} else {
					std::string line;
					for (std::set<std::string>::const_iterator it = ops.begin(); it != ops.end(); ++it) {
						if (!line.empty()) line += ", ";
						line += *it;
					}
					out.push_back("[op] 本世界 op: " + line);
				}
				return CMD_OK;
			}
			if (arg.empty()) {
				out.push_back(word == "op" ? "[op] 用法: /op <玩家名字>" : "[op] 用法: /deop <玩家名字>");
				return CMD_FAILED;
			}
			if (word == "op") {
				me->addOp(arg);
				out.push_back("[op] 已把 " + arg + " 设为本世界的 op");
			} else {
				if (me->removeOp(arg))
					out.push_back("[op] 已去掉 " + arg + " 的 op");
				else
					out.push_back("[op] " + arg + " 本来就不在名单里");
			}
			return CMD_OK;
		}
	}
#endif

	// 没人认领。调用方会提示“未找到该指令”。
	return CMD_NOT_FOUND;
}
