#include "GuiElement.h"
#include "NinePatch.h"
#include "../../Minecraft.h"
#include "../../renderer/Tesselator.h"

GuiElement::GuiElement( bool active/*=false*/, bool visible/*=true*/, int x /*= 0*/, int y /*= 0*/, int width/*=24*/, int height/*=24*/ )
: active(active),
  visible(visible),
  x(x),
  y(y),
  width(width),
  height(height),
  // 0.8.1 GUI 移植：背景字段
  color(0),
  field_1C(NULL) {
		
}

GuiElement::~GuiElement() {
	// 0.8.1 GUI 移植：释放九宫格背景
	if (field_1C) {
		delete field_1C;
		field_1C = NULL;
	}
}

bool GuiElement::pointInside( int x, int y ) {
	if(x >= this->x && x < this->x + this->width) {
		if(y >= this->y && y < this->y + this->height) {
			return true;
		}
	}
	return false;
}

// 0.8.1 GUI 移植：带背景渲染
void GuiElement::render( Minecraft* minecraft, int xm, int ym ) {
	(void)minecraft; (void)xm; (void)ym;
	if (color) {
		fill(x, y, x + width, y + height, (int)color);
	} else if (field_1C) {
		field_1C->setSize((float)width, (float)height);
		field_1C->draw(Tesselator::instance, (float)x, (float)y);
	}
}

void GuiElement::setActiveAndVisibility( bool active ) {
	this->active = active;
	this->visible = active;
}
void GuiElement::setActiveAndVisibility( bool active, bool visible ) {
	this->active = active;
	this->visible = visible;
}
void GuiElement::setBackground( Minecraft* mc, const std::string& path, const IntRectangle& rect, int a5, int a6 ) {
	if (field_1C) { delete field_1C; field_1C = NULL; }
	color = 0;
	NinePatchFactory factory(mc->textures, path);
	field_1C = factory.createSymmetrical(rect, a5, a6, (float)width, (float)height);
}
void GuiElement::setBackground( unsigned int c ) {
	if (field_1C) { delete field_1C; field_1C = NULL; }
	color = c;
}
void GuiElement::clearBackground() {
	if (field_1C) { delete field_1C; field_1C = NULL; }
	color = 0;
}
