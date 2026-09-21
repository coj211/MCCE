#include <gui/screens/PauseScreen.hpp>
#include <cpputils.hpp>
#include <Minecraft.hpp>
#include <I18n.hpp>
#include <level/Level.hpp>
#include <gui/buttons/Touch_TButton.hpp>
#include <gui/elements/Label.hpp>
#include <gui/screens/OptionsScreen.hpp>
#include <stdio.h>

// 0.8.1 暂停菜单移植（0.6.1 引擎裁剪：无玩家名概念，移除玩家列表；leaveGame 为单参）
PauseScreen081::PauseScreen081(bool a2)
	: Screen() {
	this->field_5C = a2;
	this->field_54 = 0;
	this->tickCounter = 0;
	this->backToGameButton = 0;
	this->quitToTitleButton = 0;
	this->quitAndCopyMapButton = 0;
	this->optionsButton = 0;
	this->field_74 = 0;
}
PauseScreen081::~PauseScreen081() {
	safeRemove<Button>(this->backToGameButton);
	safeRemove<Button>(this->quitToTitleButton);
	safeRemove<Button>(this->quitAndCopyMapButton);
	safeRemove<Button>(this->optionsButton);
	safeRemove<Label>(this->gameMenuLabel);
}
void PauseScreen081::render(int32_t a2, int32_t a3, float a4) {
	this->renderBackground(0);
	Screen::render(a2, a3, a4);
}
void PauseScreen081::init() {
	this->backToGameButton = new Touch::TButton(1, I18n::get("pause.returnToGame"), 0);
	this->quitToTitleButton = new Touch::TButton(2, I18n::get("pause.quitToTitle"), 0);
	this->quitAndCopyMapButton = new Touch::TButton(3, I18n::get("pause.quitAndCopyMap"), 0);
	this->optionsButton = new Touch::TButton(4, I18n::get("pause.options"), 0);
	((Touch::TButton*)this->backToGameButton)->init(this->minecraft);
	((Touch::TButton*)this->quitToTitleButton)->init(this->minecraft);
	((Touch::TButton*)this->quitAndCopyMapButton)->init(this->minecraft);
	((Touch::TButton*)this->optionsButton)->init(this->minecraft);
	this->gameMenuLabel = new Label(I18n::get("pause.title"), this->minecraft, -1, 0, 0, 0, 1);
	this->buttons.push_back(this->backToGameButton);
	this->buttons.push_back(this->optionsButton);
	this->buttons.push_back(this->quitToTitleButton);
	this->elements.emplace_back(this->gameMenuLabel);
}
void PauseScreen081::setupPositions() {
	int32_t v2;
	int32_t v3;
	Button* backToGameButton;
	Button* quitToTitleButton;
	Button* quitAndCopyMapButton;
	int32_t width;

	v2 = this->width / 20;
	v3 = this->height / 10;
	backToGameButton = this->backToGameButton;
	quitToTitleButton = this->quitToTitleButton;
	this->field_54 = 0;
	quitToTitleButton->width = 8 * v2;
	backToGameButton->width = 8 * v2;
	quitAndCopyMapButton = this->quitAndCopyMapButton;
	width = this->backToGameButton->width;
	this->optionsButton->width = width;
	quitAndCopyMapButton->width = width;
	this->backToGameButton->x = v2;
	this->backToGameButton->y = 48;
	this->optionsButton->x = v2;
	this->optionsButton->y = 80;
	this->quitToTitleButton->x = v2;
	this->quitToTitleButton->y = 112;
	this->gameMenuLabel->x = this->backToGameButton->x + this->backToGameButton->width / 2 - this->gameMenuLabel->width / 2;
	this->gameMenuLabel->y = this->backToGameButton->y - 17;
	this->quitAndCopyMapButton->x = (this->width - this->quitAndCopyMapButton->width) / 2;
	this->quitAndCopyMapButton->y = 144;
}
void PauseScreen081::tick() {
	++this->tickCounter;
}
bool PauseScreen081::renderGameBehind() {
	return 1;
}
void PauseScreen081::buttonClicked(Button* a2) {
	if(a2->id == this->backToGameButton->id) {
		this->minecraft->setScreen(0);
		return;
	}
	if(a2->id == this->quitToTitleButton->id) {
		this->minecraft->leaveGame(0);
		return;
	}
	if(a2->id == this->quitAndCopyMapButton->id) {
		this->minecraft->leaveGame(1);
		return;
	}
	if(a2->id == this->optionsButton->id) {
		this->minecraft->setScreen(new OptionsScreen(1));
	}

}
