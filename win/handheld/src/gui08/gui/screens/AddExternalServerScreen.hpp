#pragma once
#include <gui/Screen.hpp>

namespace Touch { class TButton; class THeader; }
struct TextBox081;
struct OptionsPane;
struct JavaToggleButton;

// 添加服务器界面（与语言界面/新世界界面同一套样式：顶栏 back/ok + 可滚动列表）
//   第 1 行：服务器名称（输入框）
//   第 2 行：IP 地址（输入框）
//   第 3 行：端口（输入框，默认 19132）
//   第 4 行：这是不是 Java 版(1.8.x)服务器（on/off 开关）
struct AddExternalServerScreen: Screen
{
	Touch::THeader* bHeader;
	Touch::TButton* bBack;   // 左上：返回
	Touch::TButton* bAdd;    // 右上：完成（添加服务器）
	OptionsPane* pane;
	TextBox081* nameBox;
	TextBox081* addressBox;
	TextBox081* portBox;
	JavaToggleButton* javaToggle;
	bool_t isJavaServer;

	AddExternalServerScreen();

	void closeScreen();
	void addServer();
	void toggleJava();

	virtual ~AddExternalServerScreen();
	virtual void render(int32_t, int32_t, float);
	virtual void init();
	virtual void setupPositions();
	virtual void tick();
	virtual void mouseClicked(int32_t, int32_t, int32_t);
	virtual void mouseReleased(int32_t, int32_t, int32_t);
	virtual void keyPressed(int32_t);
	virtual void keyboardNewChar(const std::string&, bool_t);
	virtual void keyboardNewChar(char inputChar);
	virtual void setTextboxText(const std::string&);
	virtual bool_t handleBackEvent(bool_t);
	virtual void buttonClicked(Button*);
	// 与语言界面一致：背景是主界面那个 6 图全景
	virtual bool renderGameBehind() { return true; }
	// 输入框不在 textBoxes 里，必须声明有文本输入，
	// 否则 Win32 层不激活 IME、不生成字符 → 无法输入
	virtual bool hasTextInput() const { return true; }
};
