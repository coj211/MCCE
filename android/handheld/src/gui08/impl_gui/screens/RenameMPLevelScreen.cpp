#include <gui/screens/RenameMPLevelScreen.hpp>
#include <Minecraft.hpp>
#include <I18n.hpp>
#include <cpputils.hpp>
#include <gui/NinePatchFactory.hpp>
#include <gui/buttons/ImageButton.hpp>
#include <gui/buttons/Touch_TButton.hpp>
#include <gui/elements/TextBox081.hpp>
#include <gui/screens/PlayScreen.hpp>
#include <gui/screens/WorldRulesWidgets.hpp>
#include <level/LevelSettings.hpp>
#include <level/storage/ExternalFileLevelStorage.hpp>
#include <level/storage/ExternalFileLevelStorageSource.hpp>
#include <level/storage/LevelData.hpp>
#include <level/storage/LevelStorageSource.hpp>
#include <util/IntRectangle.hpp>
#include <util/Util.hpp>
#include <utils.h>
#include <stdio.h>

#include "../../../client/gui/components/OptionsPane.h"
#include "../../../client/gui/components/OptionsItem.h"
#include "../../../client/renderer/gles.h"

static void addOptionRow(OptionsPane* pane, const std::string& label, GuiElement* element) {
	if (!pane || !element) return;
	OptionsItem* item = new OptionsItem(label, element);
	item->setupPositions();
	pane->addChild(item);
}

static std::string wsDifficultyLabel(int d) {
	switch (d) {
	case 0: return I18n::get("options.difficulty.peaceful");
	case 1: return I18n::get("options.difficulty.easy");
	case 3: return I18n::get("options.difficulty.hard");
	default: return I18n::get("options.difficulty.normal");
	}
}

// 文件名里不允许出现的字符（原版 rename 逻辑）
static char_t kWsBadChars[] = {0x2F, 0xA, 0xD, 9, 0, 0xC, 0x60, 0x3F,
                               0x2A, 0x5C, 0x3C, 0x3E, 0x7C, 0x22, 0x3A};

RenameMPLevelScreen::RenameMPLevelScreen(const std::string& folderName, const std::string& displayName)
	: name(folderName), displayName(displayName) {
	this->bHeader = 0;
	this->bBack = 0;
	this->bDone = 0;
	this->pane = 0;
	this->nameBox = 0;
	this->bGameMode = 0;
	this->bDifficulty = 0;
	for (int i = 0; i < 7; ++i) this->toggles[i] = 0;
	this->creative = false;
	this->originalGeneratorVersion = 0;
}

void RenameMPLevelScreen::closeScreen() {
	this->minecraft->setScreen(new PlayScreen(1));
}

RenameMPLevelScreen::~RenameMPLevelScreen() {
	if (this->bHeader) { delete this->bHeader; this->bHeader = 0; }
	if (this->bBack) { delete this->bBack; this->bBack = 0; }
	if (this->bDone) { delete this->bDone; this->bDone = 0; }
	if (this->pane) {
		delete this->pane;
		this->pane = 0;
	}
}

bool RenameMPLevelScreen::loadWorldData() {
	LevelStorageSource* ls = this->minecraft->getLevelSource();
	ExternalFileLevelStorageSource* els = dynamic_cast<ExternalFileLevelStorageSource*>(ls);
	if (!els) return false;
	std::string levelPath = els->getFullPath(this->name);
	LevelData data;
	if (!ExternalFileLevelStorage::readLevelData(levelPath, data))
		return false;

	this->originalGeneratorVersion = data.getGeneratorVersion();
	this->creative = (data.getGameType() == GameType::Creative);
	this->rules.difficulty = data.getDifficulty();
	if (this->rules.difficulty < 0) this->rules.difficulty = this->minecraft->options.difficulty;
	if (this->rules.difficulty < 0 || this->rules.difficulty > 3) this->rules.difficulty = 1;
	this->rules.daylightCycle = data.getDaylightCycle();
	this->rules.keepInventory = data.getKeepInventory();
	this->rules.immediateRespawn = data.getImmediateRespawn();
	this->rules.instantWakeUp = data.getInstantWakeUp();
	this->rules.spawnMobs = data.getSpawnMobs();
	this->rules.spawnEnemies = data.getSpawnEnemies();

	std::string n = data.getLevelName();
	if (!n.empty()) this->displayName = n;
	return true;
}

void RenameMPLevelScreen::saveWorld() {
	LevelStorageSource* ls = this->minecraft->getLevelSource();
	ExternalFileLevelStorageSource* els = dynamic_cast<ExternalFileLevelStorageSource*>(ls);
	if (els) {
		std::string levelPath = els->getFullPath(this->name);
		LevelData data;
		if (ExternalFileLevelStorage::readLevelData(levelPath, data)) {
			data.setGameType(this->creative ? GameType::Creative : GameType::Survival);
			data.setDifficulty(this->rules.difficulty);
			data.setDaylightCycle(this->rules.daylightCycle);
			data.setKeepInventory(this->rules.keepInventory);
			data.setImmediateRespawn(this->rules.immediateRespawn);
			data.setInstantWakeUp(this->rules.instantWakeUp);
			data.setSpawnMobs(this->rules.spawnMobs);
			data.setSpawnEnemies(this->rules.spawnEnemies);
			// players 传 NULL：writeLevelData 会用存档里读出的玩家数据写回
			ExternalFileLevelStorage::saveLevelData(levelPath, data, nullptr);
		}
	}

	if (this->nameBox) {
		std::string dest = this->nameBox->text;
		if (!dest.empty()) {
			for (int i = 0; i < 15; ++i) {
				std::string bad(1, kWsBadChars[i]);
				dest = *Util081::stringReplace(dest, bad, "", -1);
			}
			if (dest.empty()) dest = "saved_world";
			if (dest != this->name) ls->renameLevel(this->name, dest);
		}
	}
	this->closeScreen();
}

void RenameMPLevelScreen::refreshButtonTexts() {
	if (this->bGameMode)
		this->bGameMode->setMsg(this->creative ? I18n::get("createWorld.mode.creative")
		                                       : I18n::get("createWorld.mode.survival"));
	if (this->bDifficulty)
		this->bDifficulty->setMsg(wsDifficultyLabel(this->rules.difficulty));
}

void RenameMPLevelScreen::cycleGameMode() {
	this->creative = !this->creative;
	this->refreshButtonTexts();
}

void RenameMPLevelScreen::cycleDifficulty() {
	int d = this->rules.difficulty;
	if (d < 0 || d > 3) d = 1;
	this->rules.difficulty = (d + 1) % 4;
	this->refreshButtonTexts();
}

bool RenameMPLevelScreen::getRuleValue(int index) {
	switch (index) {
	case RULE_DAYLIGHT: return this->rules.daylightCycle;
	case RULE_KEEP_INV: return this->rules.keepInventory;
	case RULE_RESPAWN: return this->rules.immediateRespawn;
	case RULE_WAKE_UP: return this->rules.instantWakeUp;
	case RULE_SPAWN_MOBS: return this->rules.spawnMobs;
	case RULE_SPAWN_ENEMIES: return this->rules.spawnEnemies;
	case RULE_AUTO_OP: return this->rules.autoOp;
	}
	return false;
}

void RenameMPLevelScreen::toggleRule(int index) {
	switch (index) {
	case RULE_DAYLIGHT: this->rules.daylightCycle = !this->rules.daylightCycle; break;
	case RULE_KEEP_INV: this->rules.keepInventory = !this->rules.keepInventory; break;
	case RULE_RESPAWN: this->rules.immediateRespawn = !this->rules.immediateRespawn; break;
	case RULE_WAKE_UP: this->rules.instantWakeUp = !this->rules.instantWakeUp; break;
	case RULE_SPAWN_MOBS: this->rules.spawnMobs = !this->rules.spawnMobs; break;
	case RULE_SPAWN_ENEMIES: this->rules.spawnEnemies = !this->rules.spawnEnemies; break;
	case RULE_AUTO_OP: this->rules.autoOp = !this->rules.autoOp; break;
	default: break;
	}
}

void RenameMPLevelScreen::init() {
	// 读出这个世界的存档（游戏模式/难度/各项规则）
	this->loadWorldData();

	this->bHeader = new Touch::THeader(0, I18n::get("rename.title"));
	this->bBack = new Touch::TButton(1, I18n::get("gui.back"), this->minecraft);
	this->bBack->width = 38;
	this->bBack->height = 18;
	this->bDone = new Touch::TButton(2, I18n::get("gui.done"), this->minecraft);
	this->bDone->width = 38;
	this->bDone->height = 18;

	this->buttons.push_back(this->bHeader);
	this->buttons.push_back(this->bBack);
	this->buttons.push_back(this->bDone);

	this->pane = new OptionsPane();

	const char* extAscii = TextBox081::extendedAcsii
	                           ? TextBox081::extendedAcsii
	                           : " !\"#$%&\'()*+,-./"
	                             "0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]"
	                             "^_`abcdefghijklmnopqrstuvwxyz{|}~";

	this->nameBox = new TextBox081(this->minecraft, I18n::get("rename.worldName"),
	                               16, extAscii, strlen(extAscii), 0, 0, 0, 0);
	this->nameBox->setText(this->displayName);
	this->nameBox->width = 120;
	addOptionRow(this->pane, I18n::get("rename.worldName"), this->nameBox);

	this->bGameMode = new WorldRuleActionButton(10, "", this->minecraft, this, ACTION_GAME_MODE);
	this->bGameMode->width = 66;
	addOptionRow(this->pane, I18n::get("createWorld.gameMode"), this->bGameMode);

	this->bDifficulty = new WorldRuleActionButton(12, "", this->minecraft, this, ACTION_DIFFICULTY);
	this->bDifficulty->width = 66;   // 与其他切档按钮同宽（对齐）
	addOptionRow(this->pane, I18n::get("createWorld.difficulty"), this->bDifficulty);

	static const char* ruleLabelKeys[RULE_COUNT] = {
	    "createWorld.rule.daylight",
	    "createWorld.rule.keepInventory",
	    "createWorld.rule.immediateRespawn",
	    "createWorld.rule.instantWakeUp",
	    "createWorld.rule.spawnMobs",
	    "createWorld.rule.spawnEnemies",
	    "createWorld.rule.autoOp",
	};
	for (int i = 0; i < RULE_COUNT; ++i) {
		// 「创建者获得 op」只在新建世界时选：世界建好后 op 名单已经定了，
		// 在设置里改它没有意义，所以这一行不在这里出现。
		if (i == RULE_AUTO_OP) continue;
		this->toggles[i] = new WorldRuleToggleButton(20 + i, this, i);
		addOptionRow(this->pane, I18n::get(ruleLabelKeys[i]), this->toggles[i]);
	}

	this->refreshButtonTexts();
}

void RenameMPLevelScreen::setupPositions() {
	this->bHeader->x = 0;
	this->bHeader->y = 0;
	this->bHeader->width = this->width;
	this->bHeader->height = this->bBack->height + 8;
	this->bBack->x = 4;
	this->bBack->y = 4;
	this->bDone->x = this->width - this->bDone->width - 4;
	this->bDone->y = 4;

	if (this->pane) {
		this->pane->x = 8;
		this->pane->y = this->bHeader->height + 3;
		this->pane->width = this->width - 16;
		this->pane->setupPositions();
	}
}

void RenameMPLevelScreen::render(int32_t xm, int32_t ym, float a) {
	// 与语言界面一致：背景 = 主界面那个全景（renderMenuBackground 画），
	// 这里先把混合/贴图状态摆好，否则全景画不出来
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

void RenameMPLevelScreen::tick() {
	if (this->pane)
		this->pane->tick(this->minecraft);
}

void RenameMPLevelScreen::setTextboxText(const std::string& a2) {
	if (this->pane)
		this->pane->setTextboxText(a2);
}

bool RenameMPLevelScreen::handleBackEvent(bool a2) {
	if (!a2) {
		if (this->pane && this->pane->suppressOtherGUI()) {
			this->pane->backPressed(this->minecraft, 0);
			return 1;
		}
		this->closeScreen();
	}
	return 1;
}

void RenameMPLevelScreen::buttonClicked(Button* a2) {
	if (a2 == this->bBack) {
		this->closeScreen();
	} else if (a2 == this->bDone) {
		this->saveWorld();
	}
}

void RenameMPLevelScreen::mouseClicked(int32_t a2, int32_t a3, int32_t a4) {
	if (this->pane && this->pane->suppressOtherGUI()) {
		this->pane->focusuedMouseClicked(this->minecraft, a2, a3, a4);
		return;
	}
	if (this->pane)
		this->pane->mouseClicked(this->minecraft, a2, a3, a4);
	Screen::mouseClicked(a2, a3, a4);
}

void RenameMPLevelScreen::mouseReleased(int32_t a2, int32_t a3, int32_t a4) {
	if (this->pane && this->pane->suppressOtherGUI()) {
		this->pane->focusuedMouseReleased(this->minecraft, a2, a3, a4);
		return;
	}
	if (this->pane)
		this->pane->mouseReleased(this->minecraft, a2, a3, a4);
	Screen::mouseReleased(a2, a3, a4);
}

void RenameMPLevelScreen::keyPressed(int32_t a2) {
	if (this->pane)
		this->pane->keyPressed(this->minecraft, a2);
	Screen::keyPressed(a2);
}

void RenameMPLevelScreen::keyboardNewChar(const std::string& a2, bool_t a3) {
	if (this->pane)
		this->pane->keyboardNewChar(this->minecraft, a2, a3);
}

void RenameMPLevelScreen::keyboardNewChar(char inputChar) {
	std::string s(1, inputChar);
	if (this->pane)
		this->pane->keyboardNewChar(this->minecraft, s, false);
}
