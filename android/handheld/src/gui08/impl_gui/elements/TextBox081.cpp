#include <gui/elements/TextBox081.hpp>
#include <Minecraft.hpp>
#include <I18n.hpp>
#include <gui/buttons/Touch_TButton.hpp>
#include <rendering/Font.hpp>
#include <util/Util.hpp>
#include <utils.h>
#include <gui/Screen.hpp>
#include <input/Keyboard.hpp>
#include <string.h>

char* TextBox081::numberChars = "0123456789";
char* TextBox081::extendedAcsii = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890?^" "~'-_.,;<>!\"#&()=` []{}";
TextBox081* TextBox081::s_focused = 0;

TextBox081::TextBox081(Minecraft* a2, const Options::Option* a3, const std::string& a4)
	: GuiElement(1, 1, 0, 0, 24, 24) {
	this->key = -1;
	this->field_30 = 0;
	this->option = a3;
	this->field_40 = 0;
	this->field_58 = 0;
	this->field_5C = 1;
	if(a3) this->text = a2->options.getStringValue(a3);
	this->doneButton = new Touch::TButton(1, 0, 0, I18n::get("gui.done"), 0);
	this->doneButton->init(a2);
	this->doneButton->x = (a2->screen ? a2->screen->width / 2 - this->doneButton->width / 2 : 0);
	this->field_44 = 16;
	// 0.8.1 GUI 移植修复：NAME 等 option 输入框必须能输入任意字符。
	// 原版这里是 validChars="/\n\r\t" + field_4C=15，白名单只有 4 个字符，
	// 导致 NAME 无法输入字母（ASCII 全被 keyboardNewChar 拒收）。
	this->validChars = TextBox081::extendedAcsii;
	this->field_4C = TextBox081::extendedAcsii ? (uint32_t)strlen(TextBox081::extendedAcsii) : 15;
	this->height = 18;
	this->field_60 = 0;
}
TextBox081::TextBox081(Minecraft* a2, const std::string& a3, int32_t a4, const char* a5, int32_t a6, Screen* screen, void (Screen::*a8)(int32_t), int32_t a9, int32_t a10)
	: GuiElement(1, 1, 0, 0, 24, 24)
	, field_3C(a3) { //TODO seems to have one less argument according to demangled function name
	this->field_2C = 0;
	this->option = 0;
	this->key = a10;
	this->field_44 = a4;
	this->field_40 = 0;
	this->validChars = a5;
	this->field_5C = 1;
	this->field_4C = a6;
	this->field_5D = 0;
	this->field_58 = screen;
	this->field_50 = a8;
	this->field_54 = a9;
	this->doneButton = new Touch::TButton(1, 0, 0, I18n::get("gui.done"), 0);
	this->doneButton->init(a2);
	this->doneButton->x = (a2->screen ? a2->screen->width / 2 - this->doneButton->width / 2 : 0);
	this->doneButton->height = 38;
	this->height = 18;
	this->field_60 = 0;
}
int32_t TextBox081::getKey() {
	return this->key;
}
std::string* TextBox081::getText() {
	return &this->text;
}
void TextBox081::setText(const std::string& a2) {
	this->text = a2;
}
void TextBox081::setValidChars(const char* a2, uint32_t a3) {
	this->validChars = a2;
	this->field_4C = a3;
}
void TextBox081::updateText(Minecraft* a2) {
	if(this->option) {
		a2->options.set(this->option, this->text);
		this->text = a2->options.getStringValue(this->option);
	}
}

TextBox081::~TextBox081() {
	if(this->doneButton) {
		delete this->doneButton;
		this->doneButton = 0;
	}
	this->field_2C = 0;
	if (TextBox081::s_focused == this)
		TextBox081::s_focused = 0;
}
void TextBox081::tick(Minecraft* a2) {
	Options::Option* option; // r1

	if(this->field_40 && !a2->options.canModify(this->option)) {
		this->field_40 = 0;
		a2->platform()->hideKeyboard();
	}

	if(this->option) {
		this->field_5C = a2->options.canModify(this->option);
	}
}
void TextBox081::render(Minecraft* a2, int32_t a3, int32_t a4) {
	int v6; // r3

	if(this->field_5C) {
		this->fill(this->x, this->y, this->x + this->width, this->y + this->height, 0xFF373535);
		this->drawRect(this->x, this->y, this->x + this->width, this->y + this->height, 0xFF676060, 1);
	}
	v6 = 0xFF808080;
	if(this->field_5C) {
		v6 = 0xFFFFFFFF;
	}
	a2->font->draw(this->text, (float)this->x + 5.0, (float)this->y + 5.0, v6, 0);
}
void TextBox081::topRender(Minecraft* a2, int32_t a3, int32_t a4) {
	if(this->field_40) {
		this->fill(0, 0, a2->screen->width, a2->screen->height, 0xFF000000);
		a2->font->draw(this->field_3C, 5.0, 5.0, -3355444, 0);
		this->drawRect(5, 20, a2->screen->width - 5, 37, -1, 1);
		std::string v10 = (getEpochTimeS() & 1) != 0 ? this->text + '_' : this->text;
		a2->font->draw(v10, 10.0, 25.0, -1, 0);
		this->doneButton->x = a2->screen->width / 2 - this->doneButton->width / 2;
		this->doneButton->y = 45;
		this->doneButton->render(a2, a3, a4);
	}
}
void TextBox081::mouseClicked(Minecraft* a2, int32_t a3, int32_t a4, int32_t a5) {
	if(this->field_40) {
		this->field_30 = 0;
		return;
	}
	if(this->pointInside(a3, a4)) {
		this->field_30 = 1;
		return;
	}
	this->field_30 = 0;
}
void TextBox081::mouseReleased(Minecraft* a2, int32_t a3, int32_t a4, int32_t a5) {
	if(this->field_30) {
		if(this->pointInside(a3, a4)) {
			this->setFocus(a2);
		}
	}
	this->field_30 = 0;
}
void TextBox081::focusuedMouseClicked(Minecraft* a2, int32_t a3, int32_t a4, int32_t a5) {
	if(this->field_40) {
		if(this->doneButton->clicked(a2, a3, a4)) {
			this->doneButton->setPressed();
			this->field_2C = (Button*)this->doneButton;
		} else {
			this->field_2C = 0;
		}
	}
}
void TextBox081::focusuedMouseReleased(Minecraft* a2, int32_t a3, int32_t a4, int32_t a5) {
	if(this->field_40) {
		if(this->doneButton == (Button*)this->field_2C && this->doneButton->clicked(a2, a3, a4)) {
			this->doneButton->released(a3, a4);
			this->loseFocus(a2);
			this->updateText(a2);
		}
	}
	this->field_2C = 0;
}
void TextBox081::keyPressed(Minecraft* a2, int32_t a3) {
	if(this->field_40) {
		if(a3 == 8) {
			if(this->text.size() != 0) {
				this->text = Util081::utf8substring(this->text, 0, Util081::utf8len(this->text) - 1);
				this->updateText(a2);
			}
		} else if(a3 == 13) {
			this->loseFocus(a2);
			this->updateText(a2);
		}
	}
}
void TextBox081::keyboardNewChar(Minecraft* a2, const std::string& a3, bool a4) {
	if(this->field_40) {
		int32_t v7 = 0;
		if(this->validChars) {
			// 0.8.1 GUI 移植：UTF-8 多字节字符(中文等)直接放行，ASCII 才查白名单
			if((unsigned char)a3[0] > 127) goto LABEL_8;
			while(v7 < this->field_4C) {
				if(a3[0] == this->validChars[v7]) {
					goto LABEL_8;
				}
				++v7;
			}
		} else {
LABEL_8:
			std::string v9(this->text);
			if(this->field_5D) {
				v9 = Util081::utf8substring(v9, 0, Util081::utf8len(v9) - 1);
			}
			v9 += a3;
			this->field_5D = a4;
			if(Util081::utf8len(v9) <= this->field_44) {
				this->text = v9;
				this->updateText(a2);
			}
		}
	}
}
bool TextBox081::backPressed(Minecraft* a2, bool a3) {
	if(!this->field_40) {
		return this->field_40;
	}
	this->loseFocus(a2);
	this->updateText(a2);
	return 1;
}
bool TextBox081::suppressOtherGUI() {
	return this->field_40;
}
void TextBox081::setTextboxText(const std::string& a2) {
	if(this->field_40) {
		this->text = a2;
	}
}
void TextBox081::setFocus(Minecraft* a2) {
	if(!this->field_40) {
		// 互斥聚焦：同一时间只允许一个输入框聚焦（多输入框页面切换时，
		// 旧框必须先失焦，否则字符会同时进两个框）
		if (TextBox081::s_focused && TextBox081::s_focused != this)
			TextBox081::s_focused->loseFocus(a2);
		TextBox081::s_focused = this;
		a2->platform()->showKeyboard(&this->text, this->field_44, this->validChars != 0);
		this->field_40 = 1;
	}
}
bool TextBox081::loseFocus(Minecraft* a2) {
	if(this->field_40) {
		if(this->field_58) {
			(this->field_58->*this->field_50)(this->key); //TODO check
		}
		a2->platform()->hideKeyboard();
		this->field_40 = 0;
		if (TextBox081::s_focused == this)
			TextBox081::s_focused = 0;
		return 1;
	}
	return 0;
}
