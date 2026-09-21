#ifndef NET_MINECRAFT_CLIENT_GUI__GuiElementContainer_H__
#define NET_MINECRAFT_CLIENT_GUI__GuiElementContainer_H__
#include "GuiElement.h"
#include <vector>
class Tesselator;
class Minecraft;

class GuiElementContainer : public GuiElement {
public:
	GuiElementContainer(bool active=false, bool visible=true, int x = 0, int y = 0, int width=24, int height=24);
    virtual ~GuiElementContainer();
    virtual void render(Minecraft* minecraft, int xm, int ym);
	virtual void setupPositions();
	virtual void addChild(GuiElement* element);
	virtual void removeChild(GuiElement* element);
	virtual void clearAll();

	virtual void tick( Minecraft* minecraft );

	virtual void mouseClicked( Minecraft* minecraft, int x, int y, int buttonNum );

	virtual void mouseReleased( Minecraft* minecraft, int x, int y, int buttonNum );

	// 0.8.1 GUI 移植：键盘/焦点/顶层渲染事件向 children 分发
	// （TextBox081 等在 OptionsPane/OptionsGroup 里收不到输入，NAME 无法修改）
	virtual void topRender(Minecraft* minecraft, int xm, int ym);
	virtual void focusuedMouseClicked(Minecraft* minecraft, int x, int y, int buttonNum);
	virtual void focusuedMouseReleased(Minecraft* minecraft, int x, int y, int buttonNum);
	virtual void keyPressed(Minecraft* minecraft, int key);
	virtual void keyboardNewChar(Minecraft* minecraft, const std::string& text, bool isChar);
	virtual bool backPressed(Minecraft* minecraft, bool isDown);
	virtual bool suppressOtherGUI();
	virtual void setTextboxText(const std::string& text);

protected:
	std::vector<GuiElement*> children;
};

#endif /*NET_MINECRAFT_CLIENT_GUI__GuiElementContainer_H__*/
