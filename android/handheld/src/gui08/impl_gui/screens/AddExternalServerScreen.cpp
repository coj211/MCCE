#include <gui/screens/AddExternalServerScreen.hpp>
#include <ExternalServerFile.hpp>
#include <Minecraft.hpp>
#include <I18n.hpp>
#include <gui/NinePatchFactory.hpp>
#include <gui/buttons/ImageButton.hpp>
#include <gui/buttons/Touch_TButton.hpp>
#include <gui/elements/TextBox081.hpp>
#include <gui/screens/PlayScreen.hpp>
#include <rendering/Tesselator.hpp>
#include <util/IntRectangle.hpp>
#include <utils.h>
#include <stdlib.h>
#include <string.h>

#include "../../../client/gui/components/OptionsPane.h"
#include "../../../client/gui/components/OptionsItem.h"
#include "../../../client/renderer/gles.h"

// Java 版服务器开关：外观与语言界面的开关一致（gui/touchgui.png 两帧）
struct JavaToggleButton : ImageButton {
	typedef ImageButton super;
	JavaToggleButton(int id, AddExternalServerScreen* owner)
		: ImageButton(id, ""), _owner(owner) {
		ImageDef def;
		def.setSrc(IntRectangle(160, 206, 39, 20));
		def.name = "gui/touchgui.png";
		def.width = 39 * 0.7f;
		def.height = 20 * 0.7f;
		this->setImageDef(def, true);
	}
	virtual void mouseClicked(Minecraft* mc, int x, int y, int buttonNum) {
		if (buttonNum == 1 && _owner && this->clicked(mc, x, y))
			_owner->toggleJava();
	}
protected:
	virtual bool isSecondImage(bool hovered) {
		return _owner && _owner->isJavaServer;
	}
private:
	AddExternalServerScreen* _owner;
};

static void addOptionRow(OptionsPane* pane, const std::string& label, GuiElement* element) {
	if (!pane || !element) return;
	OptionsItem* item = new OptionsItem(label, element);
	item->setupPositions();
	pane->addChild(item);
}

AddExternalServerScreen::AddExternalServerScreen() {
	this->bHeader = 0;
	this->bBack = 0;
	this->bAdd = 0;
	this->pane = 0;
	this->nameBox = 0;
	this->addressBox = 0;
	this->portBox = 0;
	this->javaToggle = 0;
	this->isJavaServer = 0;
}

void AddExternalServerScreen::closeScreen() {
	this->minecraft->setScreen(new PlayScreen(1));
}

void AddExternalServerScreen::toggleJava() {
	this->isJavaServer = !this->isJavaServer;
	// Java 版默认端口是 25565，基岩版是 19132；只在端口还是默认值时跟着换
	if (this->portBox) {
		if (this->isJavaServer) {
			if (this->portBox->text == "19132")
				this->portBox->setText("25565");
		} else if (this->portBox->text == "25565") {
			this->portBox->setText("19132");
		}
	}
}

void AddExternalServerScreen::addServer() {
	if (!this->nameBox || !this->addressBox || !this->portBox)
		return;
	long port = strtol(this->portBox->text.c_str(), 0, 0);
	if (port <= 0)
		return;
	if (this->nameBox->text.empty())
		return;
	if (this->addressBox->text.empty())
		return;
	this->minecraft->externalServerFile->addServer(this->nameBox->text,
	                                               this->addressBox->text,
	                                               (int32_t)port, this->isJavaServer);
	this->minecraft->setScreen(new PlayScreen(1));
}

AddExternalServerScreen::~AddExternalServerScreen() {
	if (this->bHeader) { delete this->bHeader; this->bHeader = 0; }
	if (this->bBack) { delete this->bBack; this->bBack = 0; }
	if (this->bAdd) { delete this->bAdd; this->bAdd = 0; }
	if (this->pane) {
		delete this->pane;
		this->pane = 0;
	}
}

void AddExternalServerScreen::init() {
	this->bHeader = new Touch::THeader(0, I18n::get("addServer.addTitle"));
	this->bBack = new Touch::TButton(1, I18n::get("gui.back"), this->minecraft);
	this->bBack->width = 38;
	this->bBack->height = 18;
	this->bAdd = new Touch::TButton(2, I18n::get("addServer.add"), this->minecraft);
	this->bAdd->width = 38;
	this->bAdd->height = 18;

	this->buttons.push_back(this->bHeader);
	this->buttons.push_back(this->bBack);
	this->buttons.push_back(this->bAdd);

	this->pane = new OptionsPane();

	const char* extAscii = TextBox081::extendedAcsii
	                           ? TextBox081::extendedAcsii
	                           : " !\"#$%&\'()*+,-./"
	                             "0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]"
	                             "^_`abcdefghijklmnopqrstuvwxyz{|}~";
	const char* numChars = TextBox081::numberChars ? TextBox081::numberChars : "0123456789";

	this->nameBox = new TextBox081(this->minecraft, I18n::get("addServer.enterName"),
	                               16, extAscii, strlen(extAscii), 0, 0, 0, 0);
	this->nameBox->width = 120;
	addOptionRow(this->pane, I18n::get("addServer.enterName"), this->nameBox);

	this->addressBox = new TextBox081(this->minecraft, I18n::get("addServer.enterIp"),
	                                  256, extAscii, strlen(extAscii), 0, 0, 0, 0);
	this->addressBox->setText("127.0.0.1");
	this->addressBox->width = 120;
	addOptionRow(this->pane, I18n::get("addServer.enterIp"), this->addressBox);

	this->portBox = new TextBox081(this->minecraft, I18n::get("addServer.enterPort"),
	                               6, numChars, strlen(numChars), 0, 0, 0, 0);
	this->portBox->setText("19132");
	this->portBox->width = 120;
	addOptionRow(this->pane, I18n::get("addServer.enterPort"), this->portBox);

	this->javaToggle = new JavaToggleButton(10, this);
	addOptionRow(this->pane, I18n::get("addServer.java"), this->javaToggle);
}

void AddExternalServerScreen::setupPositions() {
	this->bHeader->x = 0;
	this->bHeader->y = 0;
	this->bHeader->width = this->width;
	this->bHeader->height = this->bBack->height + 8;
	this->bBack->x = 4;
	this->bBack->y = 4;
	this->bAdd->x = this->width - this->bAdd->width - 4;
	this->bAdd->y = 4;

	if (this->pane) {
		this->pane->x = 8;
		this->pane->y = this->bHeader->height + 3;
		this->pane->width = this->width - 16;
		this->pane->setupPositions();
	}
}

void AddExternalServerScreen::render(int32_t xm, int32_t ym, float a) {
	// 与语言界面一致：背景 = 主界面那个全景（renderMenuBackground 画）
	glEnable2(GL_BLEND);
	glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable2(GL_TEXTURE_2D);

	if (this->pane && this->pane->suppressOtherGUI()) {
		this->renderBackground(0);
		this->pane->topRender(this->minecraft, xm, ym);
		return;
	}

	this->renderMenuBackground(a);
	this->renderBackground(0);
	Screen::render(xm, ym, a);
	if (this->pane)
		this->pane->render(this->minecraft, xm, ym - 1);
}

void AddExternalServerScreen::tick() {
	if (this->pane)
		this->pane->tick(this->minecraft);
}

void AddExternalServerScreen::setTextboxText(const std::string& a2) {
	if (this->pane)
		this->pane->setTextboxText(a2);
}

bool_t AddExternalServerScreen::handleBackEvent(bool_t a2) {
	if (!a2) {
		if (this->pane && this->pane->suppressOtherGUI()) {
			this->pane->backPressed(this->minecraft, 0);
			return 1;
		}
		this->closeScreen();
	}
	return 1;
}

void AddExternalServerScreen::buttonClicked(Button* a2) {
	if (a2 == this->bBack) {
		this->closeScreen();
	} else if (a2 == this->bAdd) {
		this->addServer();
	}
}

void AddExternalServerScreen::mouseClicked(int32_t a2, int32_t a3, int32_t a4) {
	if (this->pane && this->pane->suppressOtherGUI()) {
		this->pane->focusuedMouseClicked(this->minecraft, a2, a3, a4);
		return;
	}
	if (this->pane)
		this->pane->mouseClicked(this->minecraft, a2, a3, a4);
	Screen::mouseClicked(a2, a3, a4);
}

void AddExternalServerScreen::mouseReleased(int32_t a2, int32_t a3, int32_t a4) {
	if (this->pane && this->pane->suppressOtherGUI()) {
		this->pane->focusuedMouseReleased(this->minecraft, a2, a3, a4);
		return;
	}
	if (this->pane)
		this->pane->mouseReleased(this->minecraft, a2, a3, a4);
	Screen::mouseReleased(a2, a3, a4);
}

void AddExternalServerScreen::keyPressed(int32_t a2) {
	if (this->pane)
		this->pane->keyPressed(this->minecraft, a2);
	Screen::keyPressed(a2);
}

void AddExternalServerScreen::keyboardNewChar(const std::string& a2, bool_t a3) {
	if (this->pane)
		this->pane->keyboardNewChar(this->minecraft, a2, a3);
}

void AddExternalServerScreen::keyboardNewChar(char inputChar) {
	std::string s(1, inputChar);
	if (this->pane)
		this->pane->keyboardNewChar(this->minecraft, s, false);
}
