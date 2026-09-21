#include <gui/screens/Touch_DeleteWorldScreen.hpp>
#include <Minecraft.hpp>
#include <I18n.hpp>
#include <gui/screens/PlayScreen.hpp>
#include "../../../world/level/storage/LevelStorageSource.h"


Touch::DeleteWorldScreen::DeleteWorldScreen(const LevelSummary& a2)	: ConfirmScreen(0, I18n::get("deleteWorld.question"), std::string() + "'" + a2.name + "' " + I18n::get("deleteWorld.lost"), I18n::get("deleteWorld.delete"), I18n::get("gui.cancel"), 0), levelSummary(a2) {
}

Touch::DeleteWorldScreen::~DeleteWorldScreen() {
}
void Touch::DeleteWorldScreen::postResult(bool a2) {
	if(a2) {
		this->minecraft->getLevelSource()->deleteLevel(this->levelSummary.id);
	}
	// 0.8.1 GUI 移植：删除后回新大厅（PlayScreen），而不是老版 SCREEN_SELECTWORLD
	this->minecraft->setScreen(new PlayScreen(1));
}
