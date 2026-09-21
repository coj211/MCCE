#include <gui/screens/DeleteWorldScreen.hpp>
#include <Minecraft.hpp>
#include <I18n.hpp>
#include "../../../world/level/storage/LevelStorageSource.h"

DeleteWorldScreen::DeleteWorldScreen(const LevelSummary& a2)
	: ConfirmScreen(0, I18n::get("deleteWorld.question"), std::string() + "'" + a2.name + "' " + I18n::get("deleteWorld.lost"), I18n::get("deleteWorld.delete"), I18n::get("gui.cancel"), 0), levelSummary(a2) {
}

DeleteWorldScreen::~DeleteWorldScreen() {
}
void DeleteWorldScreen::postResult(bool a2) {
	if(a2) {
		this->minecraft->getLevelSource()->deleteLevel(this->levelSummary.id);
	}
	this->minecraft->screenChooser.setScreen(SCREEN_SELECTWORLD);
}
