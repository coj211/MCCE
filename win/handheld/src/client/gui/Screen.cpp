#include "Screen.h"
#include "components/Button.h"
#include "components/TextBox.h"
#include "../Minecraft.h"
#include "../renderer/Tesselator.h"
#include "../sound/SoundEngine.h"
#include "../../platform/input/Keyboard.h"
#include "../../platform/input/Mouse.h"
#include <stdio.h>
#include <typeinfo>
#include <string>
#include "../renderer/Textures.h"
#include "../../util/Mth.h"
// 09 · UI 覆盖系统：applyUiOverrides 需要读 mod 引擎注册表
#include "../../mod/ModEngine.h"
// 0.8.1 GUI 移植：panorama 背景依赖
#include <rendering/states/DisableState.hpp>
#include <util/Color4.hpp>

Screen::Screen()
:   passEvents(false),
	clickedButton(NULL),
	tabButtonIndex(0),
	width(1),
	height(1),
	minecraft(NULL),
	font(NULL),
	_uiNextButtonId(100000)
{
}

Screen::~Screen() {
	// 09 · UI 覆盖：释放本屏挂载的 mod 新增按钮（本类 new 的）
	for (size_t i = 0; i < _uiModButtons.size(); ++i)
		delete _uiModButtons[i].button;
	_uiModButtons.clear();
}

void Screen::render( int xm, int ym, float a )
{
	// 09 · UI 覆盖：每帧应用（幂等）——PlayScreen 等在运行中会重建
	// buttons 容器，init 时挂载的 mod 按钮可能被清掉，渲染前补挂最稳。
	applyUiOverrides();
	// 0.8.1 GUI 移植：先渲染控件树元素
	for (unsigned int i = 0; i < elements.size(); i++) {
		GuiElement* e = elements[i];
		if (e) e->render(minecraft, xm, ym);
	}
	for (unsigned int i = 0; i < buttons.size(); i++) {
		Button* button = buttons[i];
		if (button && !button->isOverrideScreenRendering()) {
			// 09 · UI 覆盖：半透明按钮（modAlpha 0..1）——渲染前开混合设 alpha；
			// 0.8.1 九宫格按钮（TButton classicGUI）不吃内部颜色重置，可生效；
			// 文字/底图绘制内部会重设 glColor 的路径保持不透明（可接受的近似）。
			if (button->modAlpha >= 0.f && button->modAlpha < 0.999f) {
				float al = button->modAlpha;
				if (al < 0.001f) al = 0.001f;
				glEnable2(GL_BLEND);
				glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glColor4f2(1, 1, 1, al);
				button->render(minecraft, xm, ym);
				glColor4f2(1, 1, 1, 1);
			} else {
				button->render(minecraft, xm, ym);
			}
		}
	}
}

void Screen::init( Minecraft* minecraft, int width, int height )
{
	//particles = /*new*/ GuiParticles(minecraft);
	this->minecraft = minecraft;
	this->font = minecraft->font;
	this->width = width;
	this->height = height;
	init();
	setupPositions();
	updateTabButtonSelection();
	// 09 · UI 覆盖系统：原版位置已由 setupPositions 算好，最后套 mod 覆盖
	// （挪按钮/隐藏/透明/改字/加按钮）。非目标屏 uiScreenId() 为空 -> 空操作。
	applyUiOverrides();
}

void Screen::init()
{
}

void Screen::setSize( int width, int height )
{
	this->width = width;
	this->height = height;
	setupPositions();
	applyUiOverrides();
}

// 09 · UI 覆盖系统：把 mod 声明的覆盖应用到本屏。
// - 元素键 = "button.<id>"，覆盖 x/y/width/height/text/visible/alpha/image；
// - 追加 UI.addButton 注册的新按钮（key -> 大 id，点击走 uiNotifyButtonClick）。
void Screen::applyUiOverrides()
{
	const char* sid = uiScreenId();
	if (!sid || !sid[0] || !minecraft || !minecraft->modEngine) return;
	ModEngine* me = minecraft->modEngine;
	// 覆盖已有按钮
	for (unsigned int i = 0; i < buttons.size(); ++i) {
		Button* b = buttons[i];
		if (!b) continue;
		std::string key = "button." + std::to_string(b->id);
		ModEngine::UiElementOverride o;
		if (!me->uiQuery(sid, key, o)) continue;
		if (o.hasPos)   { b->x = o.x; b->y = o.y; }
		if (o.hasSize)  { b->width = o.w; b->height = o.h; }
		if (o.hasText)  b->setMsg(o.text);
		if (o.hasVisible) b->visible = o.visible;
		if (o.hasAlpha) b->modAlpha = o.alpha;
		if (o.hasImage) {
			b->modImagePath = o.imagePath;
			b->modImageTexId = 0;  // lazy resolve on next render
		}
		if (o.hasSkin) {
			b->hasModSkin = true;
			for (int si = 0; si < ModEngine::kSkinParts; ++si) {
				b->modSkinPaths[si] = o.skin[si];
				b->modSkinTex[si] = 0;
			}
		}
	}
	// 挂载 mod 新增按钮（幂等：key 已挂载且按钮仍在容器中则跳过；
	// PlayScreen 等运行中会 buttons.clear()，被清掉的重新补挂）。
	std::map<std::string, ModEngine::UiScreenDef>::iterator it = me->_uiScreens.find(sid);
	if (it == me->_uiScreens.end()) return;
	const std::vector<ModEngine::UiAddedButton>& adds = it->second.addButtons;
	for (size_t i = 0; i < adds.size(); ++i) {
		Button* nb = NULL;
		for (size_t j = 0; j < _uiModButtons.size(); ++j) {
			if (_uiModButtons[j].key == adds[i].key) { nb = _uiModButtons[j].button; break; }
		}
		if (!nb) {
			nb = new Button(_uiNextButtonId++, adds[i].x, adds[i].y, adds[i].w, adds[i].h, adds[i].text);
			if (!adds[i].imagePath.empty()) {
				nb->modImagePath = adds[i].imagePath;
				nb->modImageTexId = 0;
			}
			_uiModButtons.push_back(UiModButton());
			_uiModButtons.back().key = adds[i].key;
			_uiModButtons.back().button = nb;
		}
		// 已在 buttons 容器中则不重复 push（容器被清空后补挂）
		bool inContainer = false;
		for (unsigned int k = 0; k < buttons.size(); ++k)
			if (buttons[k] == nb) { inContainer = true; break; }
		if (!inContainer) {
			buttons.push_back(nb);
			tabButtons.push_back(nb);
		}
	}
}

bool Screen::handleBackEvent( bool isDown )
{
	return false;
}

void Screen::updateEvents()
{
	if (passEvents)
		return;

	while (Mouse::next()) {
		const MouseAction& e = Mouse::getEvent();
		if (e.action == MouseAction::ACTION_WHEEL)
			onMouseWheel(e.dy);
		else
			mouseEvent();
	}

	while (Keyboard::next())
		keyboardEvent();

	// Process the accumulated UTF-8 input text as one unit. The legacy
	// byte-by-byte loop (nextTextChar/getChar) cannot be mixed with
	// whole-string handling: getText() returns ALL accumulated bytes each
	// call, so a per-byte loop would append duplicates, and _textIndex is
	// not rewound between frames, which swallowed/delayed keystrokes.
	// consumeText() takes and clears the buffer so each keystroke is
	// delivered exactly once even if updateEvents runs multiple times a
	// frame (multi-tick) or a reset lands between frames.
	Keyboard::rewind();
	keyboardTextEvent();
}

void Screen::mouseEvent()
{
	const MouseAction& e = Mouse::getEvent();
	if (!e.isButton())
		return;

	if (Mouse::getEventButtonState()) {
		int xm = e.x * width / minecraft->width;
		int ym = e.y * height / minecraft->height - 1;
		mouseClicked(xm, ym, Mouse::getEventButton());
	} else {
		int xm = e.x * width / minecraft->width;
		int ym = e.y * height / minecraft->height - 1;
		mouseReleased(xm, ym, Mouse::getEventButton());
	}
}

void Screen::keyboardEvent()
{
	if (Keyboard::getEventKeyState()) {
		//if (Keyboard.getEventKey() == Keyboard.KEY_F11) {
		//    minecraft->toggleFullScreen();
		//    return;
		//}
		keyPressed(Keyboard::getEventKey());
	}
}
void Screen::keyboardTextEvent()
{
	keyboardText(Keyboard::consumeText());
}

void Screen::keyboardText(const std::string& text)
{
	// Legacy path: feed each byte to the single-char handler.
	for (size_t i = 0; i < text.size(); ++i)
		keyboardNewChar(text[i]);
}
void Screen::keyboardNewChar(char inputChar)
{
	// 0.8.1 GUI 移植：把字符分发给 elements（TextBox081 的字符输入，支持 UTF-8 逐字节拼接）
	std::string s(1, inputChar);
	for (std::vector<GuiElement*>::iterator it = elements.begin(); it != elements.end(); ++it) {
		(*it)->keyboardNewChar(minecraft, s, false);
	}
}

void Screen::renderBackground()
{
	renderBackground(0);
}

void Screen::renderBackground( int vo )
{
	// 0.8.1 GUI 移植：0.8.1 的背景逻辑（renderGameBehind 时半透明黑透出下层画面，否则 dirt）
	if (renderGameBehind()) {
		fill(0, 0, width, height, 0x7f000000);
	} else {
		renderDirtBackground(vo);
	}
}

void Screen::renderDirtBackground( int vo )
{
	//glDisable2(GL_LIGHTING);
	glDisable2(GL_FOG);
	Tesselator& t = Tesselator::instance;
	minecraft->textures->loadAndBindTexture("gui/background.png");
	glColor4f2(1, 1, 1, 1);
	float s = 32;
	float fvo = (float) vo;
	t.begin();
	t.color(0x404040);
	t.vertexUV(0, (float)height, 0, 0, height / s + fvo);
	t.vertexUV((float)width, (float)height, 0, width / s, (float)height / s + fvo);
	t.vertexUV((float)width, 0, 0, (float)width / s, 0 + fvo);
	t.vertexUV(0, 0, 0, 0, 0 + fvo);
	t.draw();
}

bool Screen::isPauseScreen()
{
	return true;
}

bool Screen::isErrorScreen()
{
	return false;
}

bool Screen::isInGameScreen()
{
	return true;
}

bool Screen::closeOnPlayerHurt() {
    return false;
}

void Screen::keyPressed( int eventKey )
{
	// 0.8.1 GUI 移植：先分发按键到 elements（TextBox081 的退格/回车）
	for (std::vector<GuiElement*>::iterator it = elements.begin(); it != elements.end(); ++it) {
		(*it)->keyPressed(minecraft, eventKey);
	}
	if (eventKey == Keyboard::KEY_ESCAPE) {
		minecraft->setScreen(NULL);
		//minecraft->grabMouse();
	}
	if (minecraft->useTouchscreen())
		return;

	// "Tabbing" the buttons (walking with keys)
	const int tabButtonCount = tabButtons.size();
	if (!tabButtonCount)
		return;

	Options& o = minecraft->options;
	if (eventKey == o.keyMenuNext.key)
		if (++tabButtonIndex == tabButtonCount) tabButtonIndex = 0;
	if (eventKey == o.keyMenuPrevious.key)
		if (--tabButtonIndex == -1) tabButtonIndex = tabButtonCount-1;
	if (eventKey == o.keyMenuOk.key) {
		Button* button = tabButtons[tabButtonIndex];
		if (button->active) {
			minecraft->soundEngine->playUI("random.click", 1, 1);
			buttonClicked(button);
		}
	}

	updateTabButtonSelection();
}

void Screen::updateTabButtonSelection()
{
	if (minecraft->useTouchscreen())
		return;

	for (unsigned int i = 0; i < tabButtons.size(); ++i)
		tabButtons[i]->selected = (i == tabButtonIndex);
}

void Screen::mouseClicked( int x, int y, int buttonNum )
{
	if (buttonNum == MouseAction::ACTION_LEFT) {
		// 0.8.1 GUI 移植：先分发到 elements（列表/滚动容器等），再命中 buttons
		for (std::vector<GuiElement*>::iterator it = elements.begin(); it != elements.end(); ++it) {
			(*it)->mouseClicked(minecraft, x, y, buttonNum);
		}
		for (unsigned int i = 0; i < buttons.size(); ++i) {
			Button* button = buttons[i];
            //LOGI("Hit-testing button: %p\n", button);
			if (button->clicked(minecraft, x, y)) {
                button->setPressed();

                //LOGI("Hit-test successful: %p\n", button);
				clickedButton = button;
			}
		}
	}
}

void Screen::mouseReleased( int x, int y, int buttonNum )
{
	// 0.8.1 GUI 移植：先分发到 elements（列表/滚动容器等）
	for (std::vector<GuiElement*>::iterator it = elements.begin(); it != elements.end(); ++it) {
		(*it)->mouseReleased(minecraft, x, y, buttonNum);
	}
	//LOGI("b_id: %d, (%p), text: %s\n", buttonNum, clickedButton, clickedButton?clickedButton->msg.c_str():"<null>");
	if (!clickedButton || buttonNum != MouseAction::ACTION_LEFT) return;

	// Take the reference out FIRST: buttonClicked()/mod callbacks may
	// setScreen()/delete buttons (use-after-free otherwise), so after that
	// point we must not touch clickedButton/buttons again.
	Button* down = clickedButton;
	clickedButton = NULL;

	// Release always clears the pressed visual state — including the "drag
	// out and release" case that used to leave the button stuck highlighted.
	// (Do it before any callback: at this point the button is still alive.)
	down->released(x, y);

	// Was the release inside the button? Then it activates.
	bool activated = false;
	for (unsigned int i = 0; i < buttons.size(); ++i) {
		if (buttons[i] == down && down->clicked(minecraft, x, y)) {
			activated = true;
			break;
		}
	}
	if (activated) {
		// 09 · UI 覆盖：mod 新增按钮直接走它的 onClick（不进屏 buttonClicked）
		bool isModBtn = false;
		for (size_t m = 0; m < _uiModButtons.size(); ++m) {
			if (_uiModButtons[m].button == down) {
				isModBtn = true;
				if (minecraft->modEngine)
					minecraft->modEngine->uiNotifyButtonClick(_uiModButtons[m].key);
				break;
			}
		}
		if (!isModBtn)
			buttonClicked(down);
		minecraft->soundEngine->playUI("random.click", 1, 1);
	}
	// NOTE: after buttonClicked()/uiNotifyButtonClick() this screen or its
	// buttons may have been destroyed — do not dereference down/buttons here.
	clickedButton = NULL;
}

bool Screen::hasClippingArea( IntRectangle& out )
{
	return false;
}

void Screen::lostFocus() {
	for(std::vector<TextBox*>::iterator it = textBoxes.begin(); it != textBoxes.end(); ++it) {
		TextBox* tb = *it;
		tb->loseFocus(minecraft);
	}
}

void Screen::toGUICoordinate( int& x, int& y ) {
	// Guard against a zeroed minecraft framebuffer size (window minimized /
	// WM_SIZE(0,0) races): the division below would raise 0xC0000094.
	if (minecraft == NULL || minecraft->width <= 0 || minecraft->height <= 0)
		return;
	x = x * width / minecraft->width;
	y = y * height / minecraft->height - 1;
}

void Screen::onMouseWheel(int dy) {
	// Default: nothing (vanilla uses the wheel for hotbar selection only
	// when no screen is open).
}


// 0.8.1 GUI 移植：本地透视矩阵（gles.cpp 的 gluPerspective 逻辑内联，
// 绕开 Windows SDK glu.h 的 stdcall 声明与 gles.h cdecl 声明的符号冲突）
static void menuPerspective(GLfloat fovy, GLfloat aspect, GLfloat zNear, GLfloat zFar) {
    GLfloat m[4][4];
    GLfloat sine, cotangent, deltaZ;
    GLfloat radians = (GLfloat)(fovy / 2.0f * 3.14159265358979323846f / 180.0f);
    deltaZ = zFar - zNear;
    sine = (GLfloat)sin(radians);
    if ((deltaZ == 0.0f) || (sine == 0.0f) || (aspect == 0.0f))
        return;
    cotangent = (GLfloat)(cos(radians) / sine);
    memset(m, 0, sizeof(m));
    m[0][0] = cotangent / aspect;
    m[1][1] = cotangent;
    m[2][2] = -(zFar + zNear) / deltaZ;
    m[2][3] = -1.0f;
    m[3][2] = -2.0f * zNear * zFar / deltaZ;
    glMultMatrixf(&m[0][0]);
}

// 0.8.1 GUI 移植：panorama 全景背景（6 面立方体 3D 旋转背景）

static char* panorama_images[] = {
	"gui/background/panorama_0.png",
	"gui/background/panorama_1.png",
	"gui/background/panorama_2.png",
	"gui/background/panorama_3.png",
	"gui/background/panorama_4.png",
	"gui/background/panorama_5.png"
};
static float dword_D6E05C20 = 0;

void Screen::renderMenuBackground(float a) {
	dword_D6E05C20 += 0.016f * 30.0f; // 0.6.1 无 field_D34，按帧估算
	if (minecraft->options.classicBackground) {
		for (int32_t i = 0; i < 6; ++i) {
			minecraft->textures->loadTexture(panorama_images[i]);
		}
		{
			DisableState be2(0xBE2);
			DisableState b44(0xB44);
			DisableState b71(0xB71);
			glMatrixMode(0x1701);
			glPushMatrix();
			int32_t v8 = 0;
			glLoadIdentity();
			menuPerspective(120.0, 1.0, 0.05, 10.0);
			glMatrixMode(0x1700u);
			glPushMatrix();
			glLoadIdentity();
			glColor4f(1.0, 1.0, 1.0, 1.0);
			glRotatef(180.0, 1.0, 0.0, 0.0);
			glRotatef(Mth::sin((float)(a + dword_D6E05C20) / 400.0f) + 20.0f, 1, 0, 0);
			glRotatef(-(float)((float)(a + dword_D6E05C20) * 0.1f), 0.0, 1.0, 0.0);
			do {
				float v9, v10, v11;
				glPushMatrix();
				switch (v8) {
					case 1: v9 = 90;  v10 = 0; v11 = 1; break;
					case 2: v9 = 180; v10 = 0; v11 = 1; break;
					case 3: v9 = -90; v10 = 0; v11 = 1; break;
					case 4: v9 = 90;  v11 = 0; v10 = 1; break;
					case 5: v9 = -90; v11 = 0; v10 = 1; break;
					default: v9 = 0; v10 = 0; v11 = 0; break;
				}
				if (v8 != 0)
					glRotatef(v9, v10, v11, 0.0);
				char* texture = panorama_images[v8++];
				minecraft->textures->loadAndBindTexture(texture);
				Tesselator::instance.begin(GL_QUADS);
				Tesselator::instance.vertexUV(-1, -1, 1, 0, 0);
				Tesselator::instance.vertexUV(1, -1, 1, 1, 0);
				Tesselator::instance.vertexUV(1, 1, 1, 1, 1);
				Tesselator::instance.vertexUV(-1, 1, 1, 0, 1);
				Tesselator::instance.draw();
				glPopMatrix();
			} while (v8 != 6);
			glMatrixMode(0x1701u);
			glPopMatrix();
			glMatrixMode(0x1700u);
			glPopMatrix();
		}
		Color4 v21(1, 1, 1, 0.35f);
		Color4 v22(0, 0, 0, 0.35f);
		this->fillGradient(0, 0, this->width, this->height, v21.toARGB(), v22.toARGB());
	} else {
		// 霓虹网格背景（classicBackground=false 时）
		int32_t colorA, colorB;
		minecraft->options.getNeonColors(colorA, colorB);
		this->fill(0, 0, this->width, this->height, 0xFF0D0214);
		int32_t cellSize = 24;
		for (int32_t x = 0; x < this->width; x += cellSize)
			this->fill(x, 0, x + 1, this->height, 0xFF220033);
		for (int32_t y = 0; y < this->height; y += cellSize)
			this->fill(0, y, this->width, y + 1, 0xFF220033);
	}
}
