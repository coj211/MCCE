#ifndef NET_MINECRAFT_CLIENT_GUI__GuiElement_H__
#define NET_MINECRAFT_CLIENT_GUI__GuiElement_H__
#include "../GuiComponent.h"

class Tesselator;
class Minecraft;
class NinePatchLayer;
struct IntRectangle;

class GuiElement : public GuiComponent {
public:
	GuiElement(bool active=false, bool visible=true, int x = 0, int y = 0, int width=24, int height=24);
    virtual ~GuiElement();
    virtual void tick(Minecraft* minecraft) {}
    virtual void render(Minecraft* minecraft, int xm, int ym);
	virtual void setupPositions() {}
	virtual void mouseClicked(Minecraft* minecraft, int x, int y, int buttonNum) {}
	virtual void mouseReleased(Minecraft* minecraft, int x, int y, int buttonNum) {}
	virtual bool pointInside(int x, int y);
	void setVisible(bool visible);

	// 0.8.1 GUI 移植补充（0.8.1 GuiElement 的方法集）：
	virtual void topRender(Minecraft* minecraft, int xm, int ym) {}
	virtual void focusuedMouseClicked(Minecraft* minecraft, int x, int y, int buttonNum) {}
	virtual void focusuedMouseReleased(Minecraft* minecraft, int x, int y, int buttonNum) {}
	virtual void keyPressed(Minecraft* minecraft, int key) {}
	virtual void keyboardNewChar(Minecraft* minecraft, const std::string& text, bool isChar) {}
	virtual bool backPressed(Minecraft* minecraft, bool isDown) { return false; }
	virtual bool suppressOtherGUI() { return false; }
	virtual void setTextboxText(const std::string& text) {}
	void setActiveAndVisibility(bool active);
	void setActiveAndVisibility(bool active, bool visible);
	void setBackground(Minecraft* minecraft, const std::string& path, const IntRectangle& rect, int a5, int a6);
	void setBackground(unsigned int color);
	void clearBackground();

	bool active;
	bool visible;
	int x;
	int y;
	int width;
	int height;
	// 0.8.1 GUI 移植：背景（纯色或九宫格）
	unsigned int color;
	NinePatchLayer* field_1C;
};

#endif /*NET_MINECRAFT_CLIENT_GUI__GuiElement_H__*/
