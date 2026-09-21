#ifndef NET_MINECRAFT_CLIENT_GUI_COMPONENTS__Button_H__
#define NET_MINECRAFT_CLIENT_GUI_COMPONENTS__Button_H__

//package net.minecraft.client.gui;

#include <string>
#include "GuiElement.h"
#include "../../Options.h"

class Font;
class Minecraft;
struct IntRectangle;

class Button: public GuiElement
{
public:
	Button(int id, const std::string& msg);
    Button(int id, int x, int y, const std::string& msg);
    Button(int id, int x, int y, int w, int h, const std::string& msg);
	virtual ~Button() {}
    virtual void render(Minecraft* minecraft, int xm, int ym);

	virtual bool clicked(Minecraft* minecraft, int mx, int my);
    virtual void released(int mx, int my);
    virtual void setPressed();

	// 0.8.1 GUI 移植新增：
	bool isOverrideScreenRendering() { return overridingScreenRendering; }
	void setOverrideScreenRendering(bool v) { overridingScreenRendering = v; }
	bool isPressed(int mx, int my) { return pressed && isInside(mx, my); }
	void setMsg(const std::string& m) { msg = m; }

	bool isInside(int xm, int ym);
protected:
	virtual int getYImage(bool hovered);
	virtual void renderBg(Minecraft* minecraft, int xm, int ym);

	virtual void renderFace(Minecraft* minecraft, int xm, int ym);
	// True when the button should draw its highlight state. On mouse
	// platforms only the pointer hovering highlights (Java edition look);
	// the keyboard-tab "selected" flag is ignored so the first button in a
	// list does not stay lit. Touchscreen platforms keep hover||selected.
	bool isHighlighted(Minecraft* minecraft, int xm, int ym);
	bool hovered(Minecraft* minecraft, int xm, int ym);	
public:
	std::string msg;
	int id;

	bool selected;
	// 0.8.1 GUI 移植：按下状态（0.8.1 控件直接读这个字段）
	bool pressed;
	// 0.8.1 GUI 移植：是否覆盖屏幕自身渲染（供弹层/子窗口用）
	bool overridingScreenRendering;
	// 0.8.1 GUI 移植：键盘导航选中标志（0.8.1 屏幕用 b->text）
	bool text;
	// 09 · UI 覆盖：自定义底图（zip 内 png 路径 -> GL 纹理，0 = 未设）
	std::string modImagePath;
	unsigned int modImageTexId;
	// 09b · UI 覆盖：九宫格皮肤（9 张 png：TL,T,TR,L,C,R,BL,B,BR；"" = 不用该块）
	std::string modSkinPaths[9];
	unsigned int modSkinTex[9];
	bool hasModSkin;
	// 09 · UI 覆盖：按钮整体透明度（-1 = 未设/不透明）
	float modAlpha;
protected:
    bool _currentlyDown;
public:
	// 09 · UI 覆盖：renderBg 前调用；返回 true = 已画 mod 底图（跳过原版九宫格）
	bool drawModBgIfSet(Minecraft* minecraft);
	// 09b · 九宫格皮肤绘制（drawModBgIfSet 内部自动优先 skin）
	void drawModSkin(Minecraft* minecraft);
};

// @note: A bit backwards, but this is a button that
//        only reacts to clicks, but isn't rendered.
class BlankButton: public Button
{
	typedef Button super;
public:
	BlankButton(int id);
	BlankButton(int id, int x, int y, int w, int h);
};


namespace Touch {
class TButton: public Button
{
	typedef Button super;
public:
	TButton(int id, const std::string& msg);
	TButton(int id, int x, int y, const std::string& msg);
	TButton(int id, int x, int y, int w, int h, const std::string& msg);
	// 0.8.1 GUI 移植：带 Minecraft* 的构造（0.8.1 屏幕用）
	TButton(int id, const std::string& msg, Minecraft* mc);
	TButton(int id, int x, int y, const std::string& msg, Minecraft* mc);
	TButton(int id, int x, int y, int w, int h, const std::string& msg, Minecraft* mc);
	virtual ~TButton();
	// 0.8.1 GUI 移植：init（九宫格背景）
	void init(Minecraft* mc);
	void init(Minecraft* mc, const std::string& img, const IntRectangle& r1, const IntRectangle& r2, int a, int b, int c, int d);
protected:
	virtual void renderBg(Minecraft* minecraft, int xm, int ym);
	// 0.8.1 GUI 移植：普通/按下两个九宫格背景层
	NinePatchLayer* field_2C;
	NinePatchLayer* field_30;
};

// "Header" in Touchscreen mode
class THeader: public Button {
	typedef Button super;
public:
	THeader(int id, const std::string& msg);
	THeader(int id, int x, int y, const std::string& msg);
	THeader(int id, int x, int y, int w, int h, const std::string& msg);
protected:
	virtual void renderBg(Minecraft* minecraft, int xm, int ym);
	void render( Minecraft* minecraft, int xm, int ym );
public:
	int xText;
};
}

#endif /*NET_MINECRAFT_CLIENT_GUI_COMPONENTS__Button_H__*/
