#include "Button.h"
#include "NinePatch.h"
#include "../../Minecraft.h"
#include "../../renderer/Textures.h"
#include "../../../mod/ModEngine.h"  // modLookupTexture via ModEngine::instance

// 09 · UI 覆盖：查 zip mod 注册的贴图 GL id（跨编译单元给 Button 用）
unsigned int modLookupTexture(const std::string& path) {
	ModEngine* me = ModEngine::instance;
	if (!me) return 0;
	return me->getModTexture(path);
}
// 09b · 查 zip mod 贴图像素尺寸（九宫格皮肤角尺寸用）
bool modLookupTextureSize(const std::string& path, int& w, int& h) {
	ModEngine* me = ModEngine::instance;
	if (!me) return false;
	return me->getModTextureSize(path, w, h);
}

Button::Button(int id, const std::string& msg)
:	GuiElement(true, true, 0, 0, 200, 24),
    id(id),
	msg(msg),
	selected(false),
	pressed(false),
	overridingScreenRendering(false),
	text(false),
	modImageTexId(0),
	hasModSkin(false),
	modAlpha(-1.f),
    _currentlyDown(false)
{
	for (int i = 0; i < 9; ++i) modSkinTex[i] = 0;

}

Button::Button( int id, int x, int y, const std::string& msg )
:	GuiElement(true, true, x, y, 200, 24),
    id(id),
	msg(msg),
	selected(false),
	pressed(false),
	overridingScreenRendering(false),
	text(false),
	modImageTexId(0),
	hasModSkin(false),
	modAlpha(-1.f),
    _currentlyDown(false)
{
	for (int i = 0; i < 9; ++i) modSkinTex[i] = 0;

}

Button::Button( int id, int x, int y, int w, int h, const std::string& msg )
:	GuiElement(true, true, x, y, w, h),
    id(id),
	msg(msg),
	selected(false),
	pressed(false),
	overridingScreenRendering(false),
	text(false),
	modImageTexId(0),
	hasModSkin(false),
	modAlpha(-1.f),
    _currentlyDown(false)
{
	for (int i = 0; i < 9; ++i) modSkinTex[i] = 0;

}

void Button::render( Minecraft* minecraft, int xm, int ym )
{
	if (!visible) return;

	/*
	minecraft->textures->loadAndBindTexture("gui/gui.png");
	glColor4f2(1, 1, 1, 1);

	//printf("ButtonId: %d - Hovered? %d (cause: %d, %d, %d, %d, <> %d, %d)\n", id, hovered, x, y, x+w, y+h, xm, ym);
	int yImage = getYImage(hovered || selected);

	blit(x, y, 0, 46 + yImage * 20, w / 2, h, 0, 20);
	blit(x + w / 2, y, 200 - w / 2, 46 + yImage * 20, w / 2, h, 0, 20);
	*/

	renderBg(minecraft, xm, ym);
	renderFace(minecraft, xm , ym);
}

void Button::released( int mx, int my ) {
    _currentlyDown = false;
    pressed = false;
}

bool Button::clicked( Minecraft* minecraft, int mx, int my )
{
	return active && mx >= x && my >= y && mx < x + width && my < y + height;
}

// 09b · 画一张 mod 纹理到指定矩形（helper）
static void drawModTexRect(Minecraft* mc, unsigned int tex, float x0, float y0, float x1, float y1) {
	glEnable2(GL_TEXTURE_2D);
	glEnable2(GL_BLEND);
	glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBindTexture(GL_TEXTURE_2D, tex);
	glColor4f2(1, 1, 1, 1);
	Tesselator& t = Tesselator::instance;
	t.begin();
	t.color(0xffffffff);
	t.vertexUV(x0, y1, 0, 0, 1);
	t.vertexUV(x1, y1, 0, 1, 1);
	t.vertexUV(x1, y0, 0, 1, 0);
	t.vertexUV(x0, y0, 0, 0, 0);
	t.draw();
}

// 09b · 用 9 张贴图按九宫格填满矩形 (x,y,w,h)。角图按 cornerW/H（=角图像素
// 尺寸，调用方传入）不拉伸；边沿轴拉伸、中心双向拉伸。texs 顺序
// TL,T,TR,L,C,R,BL,B,BR，tex=0 表示该块缺省跳过。
static void modDrawNinePatchRects(Minecraft* mc, const unsigned int texs[9],
	int cornerW, int cornerH, float x, float y, float w, float h)
{
	if (cornerW <= 0 || cornerH <= 0) return;
	const float cw = (float)cornerW, ch = (float)cornerH;
	const float x0 = x, x1 = x + cw, x2 = x + w - cw, x3 = x + w;
	const float y0 = y, y1 = y + ch, y2 = y + h - ch, y3 = y + h;
	struct R { float x0, y0, x1, y1; } r[9];
	r[0] = { x0, y0, x1, y1 };      r[1] = { x1, y0, x2, y1 };      r[2] = { x2, y0, x3, y1 };
	r[3] = { x0, y1, x1, y2 };      r[4] = { x1, y1, x2, y2 };      r[5] = { x2, y1, x3, y2 };
	r[6] = { x0, y2, x1, y3 };      r[7] = { x1, y2, x2, y3 };      r[8] = { x2, y2, x3, y3 };
	for (int i = 0; i < 9; ++i) {
		if (!texs[i]) continue;
		if (r[i].x1 <= r[i].x0 || r[i].y1 <= r[i].y0) continue;  // 拉伸区域为负（太小）跳过
		drawModTexRect(mc, texs[i], r[i].x0, r[i].y0, r[i].x1, r[i].y1);
	}
}

// 09b · 九宫格皮肤（9 张 png）。四角按自身像素尺寸绘制（不拉伸），
// 边沿轴向拉伸、中心双向拉伸，填满按钮矩形。角图全部必须同尺寸。
void Button::drawModSkin(Minecraft* minecraft)
{
	if (!hasModSkin) return;
	// lazy resolve all 9 textures + corner pixel size from part 0 (TL)
	int cornerW = 0, cornerH = 0;
	unsigned int texs[9];
	for (int i = 0; i < 9; ++i) {
		texs[i] = modSkinTex[i];
		if (modSkinPaths[i].empty()) { texs[i] = 0; continue; }
		if (!texs[i])
			texs[i] = modLookupTexture(modSkinPaths[i]);
		if (i == 0 && texs[i]) {
			int tw = 0, th = 0;
			if (modLookupTextureSize(modSkinPaths[i], tw, th)) { cornerW = tw; cornerH = th; }
		}
	}
	if (!texs[0] || cornerW <= 0 || cornerH <= 0) return;
	modDrawNinePatchRects(minecraft, texs, cornerW, cornerH,
		(float)x, (float)y, (float)width, (float)height);
}

// 09b · 面板九宫格皮肤入口：按 "<screen>.<elem>" 键查 mod 注册表，若有
// UI.skin 覆盖则用 mod 9 图重绘 (x,y,w,h)。返回 true = 已绘制（调用方跳过原版）。
// 供 NinePatchLayer::draw 等面板绘制点调用。
bool modDrawNinePatchSkin(const std::string& screenKey,
	float x, float y, float w, float h)
{
	ModEngine* me = ModEngine::instance;
	if (!me) return false;
	// split "<screen>.<elem>" (same syntax as UI.skin / UI.setImage)
	size_t dot = screenKey.find('.');
	if (dot == std::string::npos || dot == 0 || dot + 1 >= screenKey.size()) return false;
	std::string screen = screenKey.substr(0, dot);
	std::string elem = screenKey.substr(dot + 1);
	ModEngine::UiElementOverride o;
	if (!me->uiQuery(screen, elem, o) || !o.hasSkin) return false;
	unsigned int texs[9];
	int cornerW = 0, cornerH = 0;
	for (int i = 0; i < 9; ++i) {
		texs[i] = 0;
		if (o.skin[i].empty()) continue;
		texs[i] = modLookupTexture(o.skin[i]);
		if (i == 0 && texs[i]) {
			int tw = 0, th = 0;
			if (modLookupTextureSize(o.skin[i], tw, th)) { cornerW = tw; cornerH = th; }
		}
	}
	if (!texs[0] || cornerW <= 0 || cornerH <= 0) return false;
	Minecraft* mc = me->minecraft();
	if (!mc) return false;
	modDrawNinePatchRects(mc, texs, cornerW, cornerH, x, y, w, h);
	return true;
}

// 09 · UI 覆盖：mod 自定义底图——优先九宫格皮肤，否则整图拉伸铺满按钮。
// 返回 true 表示已画（调用方跳过原版九宫格/图集底图）。
bool Button::drawModBgIfSet(Minecraft* minecraft)
{
	if (hasModSkin && modSkinPaths[0].size()) {
		drawModSkin(minecraft);
		if (modSkinTex[0]) return true;
	}
	if (modImagePath.empty()) return false;
	if (!modImageTexId) {
		// resolve lazily: the png was registered by the zip loader
		modImageTexId = modLookupTexture(modImagePath);
	}
	if (!modImageTexId) return false;
	drawModTexRect(minecraft, modImageTexId, (float)x, (float)y, (float)(x + width), (float)(y + height));
	return true;
}

void Button::setPressed() {
    _currentlyDown = true;
    pressed = true;
}

int Button::getYImage( bool hovered )
{
	int res = 1;
	if (!active) res = 0;
	else if (hovered) res = 2;
	return res;
}

void Button::renderFace(Minecraft* mc, int xm, int ym) {
	Font* font = mc->font;
	if (!active) {
		drawCenteredString(font, msg, x + width / 2, y + (height - 8) / 2, 0xffa0a0a0);
	} else {
		if (isHighlighted(mc, xm, ym)) {
			drawCenteredString(font, msg, x + width / 2, y + (height - 8) / 2, 0xffffa0);
		} else {
			drawCenteredString(font, msg, x + width / 2, y + (height - 8) / 2, 0xe0e0e0);
		}
	}
}

void Button::renderBg( Minecraft* minecraft, int xm, int ym )
{
	if (drawModBgIfSet(minecraft)) return;
	minecraft->textures->loadAndBindTexture("gui/gui.png");
	glColor4f2(1, 1, 1, 1);

	//printf("ButtonId: %d - Hovered? %d (cause: %d, %d, %d, %d, <> %d, %d)\n", id, hovered, x, y, x+w, y+h, xm, ym);
	int yImage = getYImage(isHighlighted(minecraft, xm, ym));;

	blit(x, y, 0, 46 + yImage * 20, width / 2, height, 0, 20);
	blit(x + width / 2, y, 200 - width / 2, 46 + yImage * 20, width / 2, height, 0, 20);
}

bool Button::isHighlighted(Minecraft* minecraft, int xm, int ym) {
	// Mouse platforms: only pointer hover highlights. Touchscreen platforms
	// keep hover||selected (selected is never set there anyway).
	if (minecraft->useTouchscreen())
		return hovered(minecraft, xm, ym) || selected;
	return hovered(minecraft, xm, ym);
}

bool Button::hovered(Minecraft* minecraft, int xm , int ym) {
	// Mouse (non-touchscreen) platforms: hover = pointer inside the button
	// (Java edition behaviour — highlights the button on mouse-over).
	// Touchscreen platforms: hover only while pressed.
	return minecraft->useTouchscreen()? (_currentlyDown && isInside(xm, ym)) : isInside(xm, ym);
}

bool Button::isInside( int xm, int ym ) {
	return xm >= x && ym >= y && xm < x + width && ym < y + height;
}

//
// BlankButton
//
BlankButton::BlankButton(int id)
:	super(id, "")
{
	visible = false;
}

BlankButton::BlankButton(int id, int x, int y, int w, int h)
:	super(id, x, y, w, h, "")
{
	visible = false;
}

//
// The Touch-interface button
//
namespace Touch {

TButton::TButton(int id, const std::string& msg)
:	super(id, msg),
	field_2C(NULL),
	field_30(NULL)
{
	width = 66;
	height = 26;
}

TButton::TButton( int id, int x, int y, const std::string& msg )
:	super(id, x, y, msg),
	field_2C(NULL),
	field_30(NULL)
{
	width = 66;
	height = 26;
}

TButton::TButton( int id, int x, int y, int w, int h, const std::string& msg )
:	super(id, x, y, w, h, msg),
	field_2C(NULL),
	field_30(NULL)
{
}

TButton::~TButton() {
	if (field_2C) { delete field_2C; field_2C = NULL; }
	if (field_30) { delete field_30; field_30 = NULL; }
}

TButton::TButton(int id, const std::string& msg, Minecraft* mc)
:	super(id, msg),
	field_2C(NULL),
	field_30(NULL)
{
	width = 66;
	height = 26;
	if (mc) init(mc);
}

TButton::TButton( int id, int x, int y, const std::string& msg, Minecraft* mc )
:	super(id, x, y, msg),
	field_2C(NULL),
	field_30(NULL)
{
	width = 66;
	height = 26;
	if (mc) init(mc);
}

TButton::TButton( int id, int x, int y, int w, int h, const std::string& msg, Minecraft* mc )
:	super(id, x, y, w, h, msg),
	field_2C(NULL),
	field_30(NULL)
{
	if (mc) init(mc);
}

// 0.8.1 GUI 移植：spritesheet.png 九宫格按钮背景
void TButton::init(Minecraft* mc) {
	NinePatchFactory npf(mc->textures, "gui/spritesheet.png");
	IntRectangle r;
	r.y = 32; r.x = 8; r.w = 8; r.h = 8;
	field_2C = npf.createSymmetrical(r, 2, 2, (float)width, (float)height);
	r.x = 0; r.y = 32; r.w = 8; r.h = 8;
	field_30 = npf.createSymmetrical(r, 2, 2, (float)width, (float)height);
}

void TButton::init(Minecraft* mc, const std::string& img, const IntRectangle& r1, const IntRectangle& r2, int a, int b, int c, int d) {
	width = c;
	height = d;
	NinePatchFactory npf(mc->textures, img);
	field_2C = npf.createSymmetrical(r1, a, b, (float)c, (float)d);
	field_30 = npf.createSymmetrical(r2, a, b, (float)c, (float)d);
}

void TButton::renderBg( Minecraft* minecraft, int xm, int ym )
{
	if (drawModBgIfSet(minecraft)) return;
	// 0.8.1 GUI 移植：classicGUI 时用 spritesheet.png 九宫格（iOS 圆角按钮）
	if (minecraft->options.classicGUI && field_2C && field_30) {
		NinePatchLayer* layer;
		float px, py;
		field_2C->setSize((float)width, (float)height);
		field_30->setSize((float)width, (float)height);
		if (active && (selected || (_currentlyDown && xm >= x && ym >= y && xm < x + width && ym < y + height))) {
			layer = field_30;
			px = (float)x;
			py = (float)y;
		} else {
			layer = field_2C;
			px = (float)x;
			py = (float)y;
		}
		layer->draw(Tesselator::instance, px, py);
		return;
	}

	bool hovered = active && (minecraft->useTouchscreen()? (_currentlyDown && xm >= x && ym >= y && xm < x + width && ym < y + height) : false);

	minecraft->textures->loadAndBindTexture("gui/touchgui.png");

	//printf("ButtonId: %d - Hovered? %d (cause: %d, %d, %d, %d, <> %d, %d)\n", id, hovered, x, y, x+w, y+h, xm, ym);
	if (active)
		glColor4f2(1, 1, 1, 1);
	else
		glColor4f2(0.5f, 0.5f, 0.5f, 1);

	blit(x, y, hovered?66:0, 0, width, height, 66, 26);
	//blit(x + w / 2, y, 200 - w / 2, 46 + yImage * 20, w / 2, h, 0, 20);
}


//
// Header spacing in Touchscreen mode
//
THeader::THeader(int id, const std::string& msg)
:	super(id, msg),
	xText(-99999)
{
	active = false;
	width = 66;
	height = 26;
}

THeader::THeader( int id, int x, int y, const std::string& msg )
:	super(id, x, y, msg),
	xText(-99999)
{
	active = false;
	width = 66;
	height = 26;
}

THeader::THeader( int id, int x, int y, int w, int h, const std::string& msg )
:	super(id, x, y, w, h, msg),
	xText(-99999)
{
	active = false;
}

void THeader::render( Minecraft* minecraft, int xm, int ym ) {
	Font* font = minecraft->font;
	renderBg(minecraft, xm, ym);
	
	int xx = x + width/2;
	if (xText != -99999)
		xx = xText;
	drawCenteredString(font, msg, xx, y + (height - 8) / 2, 0xe0e0e0);
}

void THeader::renderBg( Minecraft* minecraft, int xm, int ym )
{
	if (drawModBgIfSet(minecraft)) return;
	minecraft->textures->loadAndBindTexture("gui/touchgui.png");

	//printf("ButtonId: %d - Hovered? %d (cause: %d, %d, %d, %d, <> %d, %d)\n", id, hovered, x, y, x+w, y+h, xm, ym);
	glColor4f2(1, 1, 1, 1);

	// Left cap
	blit(x, y, 150, 26, 2, height-1, 2, 25);
	// Middle
	blit(x+2, y, 153, 26, width-3, height-1, 8, 25);
	// Right cap
	blit(x+width-2, y, 162, 26, 2, height-1, 2, 25);
	// Shadow
	glEnable2(GL_BLEND);
	glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	blit(x, y+height-1, 153, 52, width, 3, 8, 3);
}

};
