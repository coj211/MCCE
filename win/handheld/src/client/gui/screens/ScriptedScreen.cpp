#include "ScriptedScreen.h"
#include "../../Minecraft.h"
#include "../../../mod/ModEngine.h"
#include "../../../locale/I18n.h"
#include "../../renderer/gles.h"
#include "../../renderer/Tesselator.h"

ScriptedScreen::ScriptedScreen()
:	_activeInput(-1),
	wantKeyboard(false)
{
}

ScriptedScreen::~ScriptedScreen() {
	for (size_t i = 0; i < _btns.size(); ++i) delete _btns[i];
	_btns.clear();
	for (size_t i = 0; i < _boxes.size(); ++i) delete _boxes[i];
	_boxes.clear();
}

void ScriptedScreen::init() {
	buttons.clear();
	_btns.clear();
	_boxes.clear();
	for (size_t i = 0; i < btnDefs.size(); ++i) {
		const Btn& b = btnDefs[i];
		Button* btn = new Button(b.id, b.x, b.y, b.w, b.h, b.text);
		_btns.push_back(btn);
		buttons.push_back(btn);
	}
	for (size_t i = 0; i < inputs.size(); ++i) {
		Inp& in = inputs[i];
		TextBox* tb = new TextBox(in.id, in.x, in.y, in.w, in.h, in.text);
		tb->focused = false;
		_boxes.push_back(tb);
	}
	if (wantKeyboard && minecraft)
		minecraft->platform()->showKeyboard();
}

void ScriptedScreen::setupPositions() {
}

void ScriptedScreen::tick() {
}

void ScriptedScreen::buttonClicked(Button* button) {
	if (ModEngine::instance)
		ModEngine::instance->notifyScreenButton(button->id);
}

void ScriptedScreen::keyPressed(int eventKey) {
	if (eventKey == Keyboard::KEY_BACKSPACE) {
		for (size_t i = 0; i < inputs.size(); ++i) {
			if ((int)i == _activeInput && !inputs[i].text.empty()) {
				inputs[i].text.erase(inputs[i].text.size() - 1, 1);
				if (ModEngine::instance)
					ModEngine::instance->notifyScreenText(inputs[i].id, inputs[i].text);
				return;
			}
		}
		return;
	}
	if (eventKey == Keyboard::KEY_RETURN || eventKey == Keyboard::KEY_ESCAPE) {
		// Enter/ESC closes the screen (common dialog behaviour).
		if (ModEngine::instance)
			ModEngine::instance->notifyScreenClose();
		minecraft->setScreen(NULL);
		return;
	}
	super::keyPressed(eventKey);
}

void ScriptedScreen::keyboardNewChar(char inputChar) {
	if (_activeInput < 0 || _activeInput >= (int)inputs.size())
		return;
	inputs[_activeInput].text += inputChar;
	if (ModEngine::instance)
		ModEngine::instance->notifyScreenText(inputs[_activeInput].id, inputs[_activeInput].text);
}

void ScriptedScreen::keyboardText(const std::string& text) {
	if (_activeInput < 0 || _activeInput >= (int)inputs.size())
		return;
	inputs[_activeInput].text += text;
	if (ModEngine::instance)
		ModEngine::instance->notifyScreenText(inputs[_activeInput].id, inputs[_activeInput].text);
}

bool ScriptedScreen::handleBackEvent(bool isDown) {
	if (ModEngine::instance && isDown)
		ModEngine::instance->notifyScreenClose();
	minecraft->setScreen(NULL);
	return true;
}

void ScriptedScreen::mouseClicked(int x, int y, int buttonNum) {
	// Focus the clicked input box.
	_activeInput = -1;
	for (size_t i = 0; i < inputs.size(); ++i) {
		const Inp& in = inputs[i];
		if (x >= in.x && x <= in.x + in.w && y >= in.y && y <= in.y + in.h) {
			_activeInput = (int)i;
			break;
		}
	}
	// Grid cells: clicked cell fires the onCell callback (container-like).
	for (size_t i = 0; i < cells.size(); ++i) {
		const Cell& c = cells[i];
		if (x >= c.x && x <= c.x + c.w && y >= c.y && y <= c.y + c.h) {
			if (ModEngine::instance)
				ModEngine::instance->notifyScreenCell(c.id);
			break;
		}
	}
	super::mouseClicked(x, y, buttonNum);
}

void ScriptedScreen::render(int xm, int ym, float a) {
	renderDirtBackground(0);

	// Title
	if (!title.empty() && font)
		drawCenteredString(font, title, width / 2, 14, 0xffffffff);

	// Declared images (zip textures)
	for (size_t i = 0; i < images.size(); ++i) {
		const Img& im = images[i];
		unsigned int tex = ModEngine::instance ? ModEngine::instance->getModTexture(im.path) : 0;
		if (!tex) continue;
		glEnable2(GL_TEXTURE_2D);
		glEnable2(GL_BLEND);
		glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glBindTexture(GL_TEXTURE_2D, tex);
		Tesselator& t = Tesselator::instance;
		t.begin();
		t.color(255, 255, 255, 255);
		t.vertexUV(im.x, im.y + im.h, 0, 0, 1);
		t.vertexUV(im.x + im.w, im.y + im.h, 0, 1, 1);
		t.vertexUV(im.x + im.w, im.y, 0, 1, 0);
		t.vertexUV(im.x, im.y, 0, 0, 0);
		t.draw();
		glDisable2(GL_TEXTURE_2D);
		glDisable2(GL_BLEND);
	}

	// Declared static texts
	if (font) {
		for (size_t i = 0; i < texts.size(); ++i) {
			const Txt& tx = texts[i];
			drawString(font, tx.text, tx.x, tx.y, tx.color);
		}
	}

	// Input boxes
	for (size_t i = 0; i < inputs.size(); ++i) {
		const Inp& in = inputs[i];
		fill(in.x - 2, in.y - 2, in.x + in.w + 2, in.y + in.h + 2, 0xff000000);
		fill(in.x, in.y, in.x + in.w, in.y + in.h, 0xffffffff);
		if (font && !in.text.empty())
			drawString(font, in.text, in.x + 2, in.y + (in.h - 8) / 2, 0xff000000);
	}

	// Grid cells (container-like slots): dark bg + border, centered text.
	for (size_t i = 0; i < cells.size(); ++i) {
		const Cell& c = cells[i];
		fill(c.x, c.y, c.x + c.w, c.y + c.h, 0xff555555);
		fill(c.x + 1, c.y + 1, c.x + c.w - 1, c.y + c.h - 1, 0xff222222);
		if (font && !c.text.empty()) {
			int tw = font->width(c.text);
			drawString(font, c.text, c.x + (c.w - tw) / 2, c.y + (c.h - 8) / 2, c.color);
		}
	}

	// Buttons (through the base class render so hover/click work)
	super::render(xm, ym, a);

	// Per-frame JS callback (custom drawing)
	if (ModEngine::instance)
		ModEngine::instance->notifyScreenRender();
}
