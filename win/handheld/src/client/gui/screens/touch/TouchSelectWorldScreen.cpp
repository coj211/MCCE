#include "TouchSelectWorldScreen.h"
#include <gui/elements/Touch_TouchWorldSelectionList.hpp>
#include <gui/screens/Touch_DeleteWorldScreen.hpp>
#include "../../../Minecraft.h"
#include "../../../../platform/input/Mouse.h"
#include <util/Util.hpp>
#include <set>
#include "../../../../world/level/storage/LevelStorageSource.h"
#include <algorithm>
#include "../../../../world/level/LevelSettings.h"
#include "../ProgressScreen.h"

Touch::SelectWorldScreen::SelectWorldScreen()
	: field_54(1, "")
	, editButton(5, I18n::get("play.edit"), 0)
	, createNewButton(2, I18n::get("selectWorld.create"), 0)
	, selectWorldHeader(0, I18n::get("selectWorld.title"))
	, backButton(3, I18n::get("gui.back"), 0)
	, field_168(4, "") {
	this->selectionList = 0;
	this->field_1A9 = 0;
	this->field_1AC = 0;
	this->field_54.active = 0;
	{
		ImageDef a3;
		a3.name = "gui/touchgui.png";
		a3.hasSubImage = 1;
		a3.width = 34;
		a3.v = 0;
		a3.height = 26;
		a3.u = 150;
		a3.subW = 34;
		a3.subH = 26;
		this->field_54.setImageDef(a3, 1);
	}
}
std::string Touch::SelectWorldScreen::getUniqueLevelName(const std::string& a3) {
	std::set<std::string> v14;
	for(int v6 = 0; v6 < this->field_19C.size(); ++v6) {
		v14.insert(this->field_19C[v6].id);
	}
	std::string ret = a3;
	while(1) {
		if(v14.find(ret) == v14.end()) {
			break;
		}
		ret += '-';
	}
	return ret;
}
void Touch::SelectWorldScreen::loadLevelSource(){
	this->minecraft->getLevelSource()->getLevelList(this->field_19C);
	std::sort(this->field_19C.begin(), this->field_19C.end());
	for(int i = 0; i < this->field_19C.size(); ++i) {
		LevelSummary* v9 = &this->field_19C[i];
		if(v9->id != LevelStorageSource::TempLevelId) {
			this->selectionList->field_78.emplace_back(LevelSummary(*v9));
		}
	}
}

Touch::SelectWorldScreen::~SelectWorldScreen() {
	if(this->selectionList) delete this->selectionList;
}
void Touch::SelectWorldScreen::render(int32_t mx, int32_t my, float pt) {
	this->renderBackground(0);
	if(this->field_1A8) {
		this->selectionList->render(mx, my, pt);
	} else {
		this->selectionList->render((uint8_t)this->field_1A8, (uint8_t)this->field_1A8, pt);
		this->field_1A8 = Mouse::getButtonState(1) == 0;
	}
	Screen::render(mx, my, pt);
}
void Touch::SelectWorldScreen::init() {
	this->selectionList = new Touch::TouchWorldSelectionList(this->minecraft, this->width, this->height);
	this->loadLevelSource();
	this->selectionList->commit();
	this->backButton.init(this->minecraft);
	this->editButton.init(this->minecraft);
	this->createNewButton.init(this->minecraft);
	this->buttons.emplace_back(&this->field_54);
	this->buttons.emplace_back(&this->editButton);
	this->buttons.emplace_back(&this->createNewButton);
	this->buttons.emplace_back(&this->backButton);
	this->buttons.emplace_back(&this->selectWorldHeader);
	this->field_1A8 = Mouse::getButtonState(1) == 0;
	this->tabButtons.emplace_back(&this->field_168);
	this->tabButtons.emplace_back(&this->field_54);
	this->tabButtons.emplace_back(&this->editButton);
	this->tabButtons.emplace_back(&this->createNewButton);
	this->tabButtons.emplace_back(&this->backButton);
}
void Touch::SelectWorldScreen::setupPositions(){
	int32_t width, v2, v3, v4, v5, v6, height;
	width = this->field_54.width;
	this->field_54.y = this->height - 30;
	v2 = this->width;
	this->createNewButton.y = 0;
	this->editButton.y = 0;
	this->backButton.y = 0;
	this->selectWorldHeader.y = 0;
	this->backButton.x = 0;
	v3 = this->backButton.width;
	this->field_54.x = (v2 - width) / 2;
	v4 = this->createNewButton.width;
	v5 = v2 - v4;
	this->editButton.x = v5 - this->editButton.width - 4;
	this->selectWorldHeader.x = v3;
	v6 = v3 + v4 + this->editButton.width + 4;
	height = this->createNewButton.height;
	this->createNewButton.x = v5;
	this->selectWorldHeader.width = v2 - v6;
	this->selectWorldHeader.height = height;
}
bool Touch::SelectWorldScreen::handleBackEvent(bool a2) {
	if(!a2) {
		this->minecraft->cancelLocateMultiplayer();
		this->minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
	}
	return 1;
}

void Touch::SelectWorldScreen::tick(){
	if(this->selectionList) {
		this->selectionList->tick();
	}
}
bool Touch::SelectWorldScreen::isInGameScreen() {
	return 1;
}
void Touch::SelectWorldScreen::buttonClicked(Button* a2) {
	if(a2->id == this->createNewButton.id && !this->field_1AC && !this->field_1A9) {
		// 0.8.1 GUI 移植：创建世界（0.6.1 方式，跳过系统输入框）
		std::string name = this->getUniqueLevelName("New World");
		LevelSettings settings(getEpochTimeS(), GameType::Creative);
		this->minecraft->selectLevel(name, name, settings);
		this->minecraft->hostMultiplayer();
		this->minecraft->setScreen(new ProgressScreen());
	}
	if(a2->id == this->editButton.id) {
		// 编辑世界：功能后续处理（0.8.1 的 Edit 改世界名）
		return;
	}
	if(a2->id == this->field_54.id) {
		if(this->isIndexValid(this->selectionList->selectedItem)) {
			this->minecraft->setScreen(new Touch::DeleteWorldScreen(this->selectionList->field_78[this->selectionList->selectedItem]));
		}
	}
	if(a2->id == this->backButton.id) {
		this->minecraft->cancelLocateMultiplayer();
		this->minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
	}
	if(a2->id == this->field_168.id) {
		int32_t v9 = this->selectionList->getItemAtPosition(this->width / 2, this->height / 2);
		this->selectionList->selectItem(v9, 0);
	}
}
void Touch::SelectWorldScreen::keyPressed(int32_t a2) {
	if(this->field_168.text) {
		if(a2 == this->minecraft->options.keyLeft.key) {
			this->selectionList->stepLeft();
		}
		if(a2 == this->minecraft->options.keyRight.key) {
			this->selectionList->stepRight();
		}
	}
	Screen::keyPressed(a2);
}
bool Touch::SelectWorldScreen::isIndexValid(int32_t a2) {
	if(this->selectionList) {
		return a2 >= 0 && a2 < this->selectionList->getNumberOfItems() - 1;
	}
	return 0;
}
