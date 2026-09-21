#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS_CHATINPUTSCREEN_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS_CHATINPUTSCREEN_H__

#include "../Screen.h"
#include "../components/Button.h"
#include <string>
#include <vector>

// In-game chat input screen. Opened from the HUD chat button or the Enter
// key; typing is fed through keyboardNewChar/keyPressed (win32 WM_CHAR +
// WM_KEYDOWN). Enter submits: the message is shown in the chat feed
// (Gui::addMessage) and dispatched to the JS mod engine (onChat event).
//
// 界面（2026-09-18 重做）：
//   · 背景是灰黑半透明蒙板，透出后面的游戏画面 —— 做法与设置/语言/新建世界
//     完全一样（renderGameBehind() 返回 true，Screen::renderBackground 会画
//     fill(0,0,w,h,0x7f000000)）
//   · 顶上一条栏 + 左边“返回”按钮（素材就是新建世界/设置页顶上那条：
//     Touch::THeader + Touch::TButton，不需要新贴图）；只有点“返回”才回游戏
//   · 中间显示聊天记录：最新一条贴着输入框上面，往上翻是更早的；滚轮或 ↑↓
//     翻看历史，来了新消息会自动贴底（自己手动滚过以后就不再自动贴底）
//   · 输入以 '/' 开头时，中间改成显示“可能想输入的指令 + 简述”，已经敲进去的
//     前缀显示成黄色（§e 颜色码，Font::drawSlow 原生支持）；鼠标点某一条就把
//     它填进输入框，可以接着往下敲参数
class ChatInputScreen: public Screen {
	typedef Screen super;
public:
	// 内置命令的执行结果（submit 据此决定给玩家什么提示）
	enum CmdResult {
		CMD_NOT_FOUND = 0,   // 没人认领这条命令
		CMD_OK = 1,          // 执行成功
		CMD_FAILED = 2       // 认领了，但失败了（权限/参数/环境）
	};

	ChatInputScreen();
	~ChatInputScreen();
	void init();
	virtual void removed();
	void setupPositions();
	void render(int xm, int ym, float a);
	virtual void keyPressed(int eventKey);
	virtual void keyboardNewChar(char inputChar);
	virtual void keyboardText(const std::string& text);
	virtual void mouseClicked(int x, int y, int buttonNum);
	virtual void mouseReleased(int x, int y, int buttonNum);
	virtual void onMouseWheel(int dy);
	// Chat takes raw text (no TextBox widget): always counts as text input
	// so the IME engages while typing.
	virtual bool hasTextInput() const { return true; }
	// 灰黑半透明蒙板：透出后面的游戏画面
	virtual bool renderGameBehind() { return true; }
	// 本屏自己画聊天记录（带滚动），让 HUD 别再重复画那 10 行
	virtual bool isChatScreen() const { return true; }

	void submit();
	// 执行内置命令。文字输出写进 out（由 submit 统一按顺序显示），
	// 返回值见 CmdResult。
	CmdResult handleCommand(const std::string& cmd, std::vector<std::string>& out);
protected:
	virtual void buttonClicked(Button* button);
private:
	// 指令补全的一条候选
	struct Suggest {
		std::string name;   // 不含 '/'
		std::string desc;   // 简述（模组没给就是空串）
	};

	bool isSuggesting() const;   // 现在是不是在敲命令名（'/' 开头且还没空格）
	void buildSuggestions();     // 按当前输入刷新候选表
	void updateListLayout();     // 重算列表区上下边界
	int  visibleRows() const;    // 列表区能显示几行
	int  maxScroll() const;      // 最多能往上翻几行
	void drawChatList();         // 画聊天记录
	void drawSuggestList(int ym); // 画指令补全（ym 用来做鼠标悬停高亮）
	void drawInputBar();         // 画底部输入条

	std::string message;
	int _cursorFrame;

	// 顶栏 + 返回按钮（复用现成素材：Touch::THeader / Touch::TButton）
	Touch::THeader* bHeader;
	Touch::TButton* bBack;

	int  scrollOffset;    // 0 = 贴着最新一条；>0 = 往上翻了这么多行
	bool autoScroll;      // 是否跟随最新消息（玩家手动滚动过就是 false）
	int  _lastCount;      // 上次看到的聊天条数（用来发现新消息）
	int  _lastTopTicks;   // 上次最新一条的 ticks（用来发现新消息）
	// 按住拖动（世界列表/设置页那套做法：按下时记锚点，每帧按鼠标位移重算）
	bool _dragging;
	int  _dragStartY;        // 按下时的鼠标 GUI y
	int  _dragStartScroll;   // 按下时的 scrollOffset
	bool _dragMoved;         // 位移超过一行就算拖动，松手时不当点击处理

	std::vector<Suggest> _suggests;
	int _listTop, _listBottom;   // 列表区 y 范围（鼠标点选要用）
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS_CHATINPUTSCREEN_H__*/
