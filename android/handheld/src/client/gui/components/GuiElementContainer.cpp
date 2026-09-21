#include "GuiElementContainer.h"
#include <algorithm>
GuiElementContainer::GuiElementContainer( bool active/*=false*/, bool visible/*=true*/, int x /*= 0*/, int y /*= 0*/, int width/*=24*/, int height/*=24*/ )
: GuiElement(active, visible, x, y, width, height) {

}

GuiElementContainer::~GuiElementContainer() {
	while(!children.empty()) {
		GuiElement* element = children.back();
		children.pop_back();
		delete element;
	}
}

void GuiElementContainer::render( Minecraft* minecraft, int xm, int ym ) {
	for(std::vector<GuiElement*>::iterator it = children.begin(); it != children.end(); ++it) {
		(*it)->render(minecraft, xm, ym);
	}
}

void GuiElementContainer::setupPositions() {
	for(std::vector<GuiElement*>::iterator it = children.begin(); it != children.end(); ++it) {
		(*it)->setupPositions();
	}
}

void GuiElementContainer::addChild( GuiElement* element ) {
	children.push_back(element);
}

void GuiElementContainer::removeChild( GuiElement* element ) {
	std::vector<GuiElement*>::iterator it = std::find(children.begin(), children.end(), element);
	if(it != children.end())
		children.erase(it);
}

void GuiElementContainer::clearAll() {
	children.clear();
}

void GuiElementContainer::tick( Minecraft* minecraft ) {
	for(std::vector<GuiElement*>::iterator it = children.begin(); it != children.end(); ++it) {
		(*it)->tick(minecraft);
	}
}

void GuiElementContainer::mouseClicked( Minecraft* minecraft, int x, int y, int buttonNum ) {
	for(std::vector<GuiElement*>::iterator it = children.begin(); it != children.end(); ++it) {
		(*it)->mouseClicked(minecraft, x, y, buttonNum);
	}
}

void GuiElementContainer::mouseReleased( Minecraft* minecraft, int x, int y, int buttonNum ) {
	for(std::vector<GuiElement*>::iterator it = children.begin(); it != children.end(); ++it) {
		(*it)->mouseReleased(minecraft, x, y, buttonNum);
	}
}

// 0.8.1 GUI 移植：键盘/焦点/顶层渲染事件向 children 分发
void GuiElementContainer::topRender( Minecraft* minecraft, int xm, int ym ) {
	for(std::vector<GuiElement*>::iterator it = children.begin(); it != children.end(); ++it) {
		(*it)->topRender(minecraft, xm, ym);
	}
}
void GuiElementContainer::focusuedMouseClicked( Minecraft* minecraft, int x, int y, int buttonNum ) {
	for(std::vector<GuiElement*>::iterator it = children.begin(); it != children.end(); ++it) {
		(*it)->focusuedMouseClicked(minecraft, x, y, buttonNum);
	}
}
void GuiElementContainer::focusuedMouseReleased( Minecraft* minecraft, int x, int y, int buttonNum ) {
	for(std::vector<GuiElement*>::iterator it = children.begin(); it != children.end(); ++it) {
		(*it)->focusuedMouseReleased(minecraft, x, y, buttonNum);
	}
}
void GuiElementContainer::keyPressed( Minecraft* minecraft, int key ) {
	for(std::vector<GuiElement*>::iterator it = children.begin(); it != children.end(); ++it) {
		(*it)->keyPressed(minecraft, key);
	}
}
void GuiElementContainer::keyboardNewChar( Minecraft* minecraft, const std::string& text, bool isChar ) {
	for(std::vector<GuiElement*>::iterator it = children.begin(); it != children.end(); ++it) {
		(*it)->keyboardNewChar(minecraft, text, isChar);
	}
}
bool GuiElementContainer::backPressed( Minecraft* minecraft, bool isDown ) {
	bool ret = false;
	for(std::vector<GuiElement*>::iterator it = children.begin(); it != children.end(); ++it) {
		if ((*it)->backPressed(minecraft, isDown)) ret = true;
	}
	return ret;
}
bool GuiElementContainer::suppressOtherGUI() {
	for(std::vector<GuiElement*>::iterator it = children.begin(); it != children.end(); ++it) {
		if ((*it)->suppressOtherGUI()) return true;
	}
	return false;
}
void GuiElementContainer::setTextboxText( const std::string& text ) {
	for(std::vector<GuiElement*>::iterator it = children.begin(); it != children.end(); ++it) {
		(*it)->setTextboxText(text);
	}
}
