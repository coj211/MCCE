#ifndef NET_MINECRAFT_CLIENT_GUI__Screen_H__
#define NET_MINECRAFT_CLIENT_GUI__Screen_H__

//package net.minecraft.client.gui;

#include <vector>
#include <map>
#include "GuiComponent.h"
// 0.8.1 GUI 移植：Screen 基类扩展所需的 mco 状态枚举
#include "mco/MCOEvent.hpp"
#include "mco/MojangConnectionStatus.hpp"

class Font;
class Minecraft;
class Button;
class TextBox;
class GuiElement;
struct IntRectangle;

class Screen: public GuiComponent
{
public:
	Screen();
	virtual ~Screen();

    virtual void render(int xm, int ym, float a);

    void init(Minecraft* minecraft, int width, int height);
	virtual void init();

    void setSize(int width, int height);
	virtual void setupPositions() {};

	virtual void updateEvents();
    virtual void mouseEvent();
    virtual void keyboardEvent();
	virtual void keyboardTextEvent();
	virtual bool handleBackEvent(bool isDown);

    virtual void tick() {}

    virtual void removed() {}

    virtual void renderBackground();
    virtual void renderBackground(int vo);
    virtual void renderDirtBackground(int vo);
	// query
	virtual bool hasClippingArea(IntRectangle& out);

    virtual bool isPauseScreen();
	virtual bool isErrorScreen();
	virtual bool isInGameScreen();
    virtual bool closeOnPlayerHurt();

    virtual void confirmResult(bool result, int id) {}
	virtual void lostFocus();
	virtual void toGUICoordinate(int& x, int& y);
protected:
	void updateTabButtonSelection();

	virtual void buttonClicked(Button* button) {}
	virtual void mouseClicked(int x, int y, int buttonNum);
	virtual void mouseReleased(int x, int y, int buttonNum);

	virtual void keyPressed(int eventKey);
	virtual void keyboardNewChar(char inputChar);
	// UTF-8 text input (multi-byte aware). Default implementation feeds the
	// bytes to the legacy single-char handler; chat input overrides this to
	// append the whole string directly.
	virtual void keyboardText(const std::string& text);
public:
	int width;
	int height;
	bool passEvents;
	// True when this screen has a text-input box / field (chat, server
	// address, scripted screens with input boxes, etc.). The platform uses
	// it to enable the IME only while typing, so a CJK input method never
	// swallows gameplay keys. Screens without a TextBox but with text input
	// (ChatInputScreen) override this to return true.
	virtual bool hasTextInput() const { return !textBoxes.empty(); }
	// Mouse wheel: default no-op (vanilla uses the wheel for hotbar while
	// in-game); screens that scroll call super and use e.dy themselves.
	virtual void onMouseWheel(int dy);
	//GuiParticles* particles;

	// 0.8.1 GUI 移植：控件树（0.8.1 屏幕用它做元素渲染/事件分发）
	std::vector<GuiElement*> elements;
	// 0.8.1 GUI 移植：新虚函数（mco 状态/文本回调，默认空实现）
	virtual void renderMenuBackground(float a);
	// 0.8.1 GUI 移植：是否渲染游戏画面在菜单后（设置页用）
	virtual bool renderGameBehind() { return false; }
	// 聊天屏重写为 true：HUD（Gui::render 里的 renderChatMessages）不再重复画
	// 聊天记录，改由聊天屏自己画一份（带滚动 / 指令补全）。
	virtual bool isChatScreen() const { return false; }
	virtual void feedMCOEvent(MCOEvent ev) { (void)ev; }
	virtual bool supppressedBySubWindow() { return false; }
	virtual void onTextBoxUpdated(int index) { (void)index; }
	virtual void onMojangConnectorStatus(MojangConnectionStatus status) { (void)status; }
	virtual void setTextboxText(const std::string& text) { (void)text; }
	virtual void onInternetUpdate() {}

	// 09 · UI 覆盖系统：参与覆盖的屏返回其逻辑名（如 "mainmenu"），
	// 返回空串 = 该屏不接受 mod UI 覆盖（默认）。
	virtual const char* uiScreenId() const { return ""; }
	// 09 · 在 setupPositions() 之后调用：把 mod 的 UI 覆盖（挪按钮/隐藏/
	// 透明/改字/加按钮）应用到本屏已有的按钮上，并挂载 mod 新增按钮。
	// 引擎在 init() 末尾自动调用一次（此时原版位置已算好）。
	void applyUiOverrides();

protected:
	Minecraft* minecraft;
	std::vector<Button*> buttons;
	std::vector<TextBox*> textBoxes;

	std::vector<Button*> tabButtons;
	int tabButtonIndex;

	Font* font;
private:
	Button* clickedButton;
	// 09 · UI 覆盖：mod 新增按钮（key -> button id），由 applyUiOverrides 挂载。
	struct UiModButton { std::string key; Button* button; };
	std::vector<UiModButton> _uiModButtons;
	int _uiNextButtonId;   // id 分配器（从 100000 起，避开原版按钮 id）
};

#endif /*NET_MINECRAFT_CLIENT_GUI__Screen_H__*/
