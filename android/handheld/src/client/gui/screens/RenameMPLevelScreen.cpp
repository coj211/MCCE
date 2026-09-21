#include "RenameMPLevelScreen.h"
#include "SelectWorldScreen.h"
#include "StartMenuScreen.h"
#include "DialogDefinitions.h"
#include "../Gui.h"
#include "../../Minecraft.h"
#include "../../../AppPlatform.h"
#include "../../../platform/log.h"
#include "../../../platform/input/Keyboard.h"
#include "../../../locale/I18n.h"
#include "../../../world/level/storage/LevelStorageSource.h"
#include "../../../util/StringUtils.h"
#include "../Font.h"


static char ILLEGAL_FILE_CHARACTERS[] = {
	'/', '\n', '\r', '\t', '\0', '\f', '`', '?', '*', '\\', '<', '>', '|', '\"', ':'
};

RenameMPLevelScreen::RenameMPLevelScreen( const std::string& levelId, const std::string& levelName )
:	_levelId(levelId),
	_name(levelName),
	_cursorTick(0),
	bDone(1, I18n::get("gui.ok")),
	bCancel(2, I18n::get("gui.cancel"))
{
}

void RenameMPLevelScreen::init() {
	buttons.push_back(&bDone);
	buttons.push_back(&bCancel);
	tabButtons.push_back(&bDone);
	tabButtons.push_back(&bCancel);
}

void RenameMPLevelScreen::setupPositions() {
	const int fieldW = 240;
	const int btnW = 120;
	const int bh = 20;

	bDone.width = bCancel.width = btnW;
	bDone.height = bCancel.height = bh;

	bDone.x = width / 2 - 4 - btnW;
	bCancel.x = width / 2 + 4;
	bDone.y = bCancel.y = height - 30;
}

void RenameMPLevelScreen::tick() {
	_cursorTick++;
}

void RenameMPLevelScreen::render(int xm, int ym, float a)
{
	renderBackground();

	const int cx = width / 2;

	drawCenteredString(minecraft->font, I18n::get("selectWorld.renameTitle"), cx, 30, 0xffffffff);
	drawCenteredString(minecraft->font, I18n::get("selectWorld.enterName"), cx, 64, 0xffaaaaaa);

	drawTextField();

	super::render(xm, ym, a);
}

void RenameMPLevelScreen::drawTextField()
{
	const int cx = width / 2;
	const int fieldW = 240;
	const int fieldH = 22;
	const int x0 = cx - fieldW / 2;
	const int y0 = 78;

	fillGradient(x0, y0, x0 + fieldW, y0 + fieldH, 0x80000000, 0x80000000);

	std::string shown = _name;
	if ((_cursorTick / 20) % 2 == 0)
		shown += "_";
	minecraft->font->draw(shown, x0 + 6, y0 + 6, 0xffffffff);
}

void RenameMPLevelScreen::doRename()
{
	std::string newId = Util::stringTrim(_name);
	for (int i = 0; i < sizeof(ILLEGAL_FILE_CHARACTERS) / sizeof(char); ++i)
		newId = Util::stringReplace(newId, std::string(1, ILLEGAL_FILE_CHARACTERS[i]), "");
	if ((int)newId.length() == 0)
		newId = "saved_world";

	LOGI("Renaming level '%s' -> '%s'\n", _levelId.c_str(), newId.c_str());
	minecraft->getLevelSource()->renameLevel(_levelId, newId);
	minecraft->screenChooser.setScreen(SCREEN_SELECTWORLD);
}

void RenameMPLevelScreen::buttonClicked(Button* button)
{
	if (button->id == bDone.id)
		doRename();
	if (button->id == bCancel.id)
		minecraft->screenChooser.setScreen(SCREEN_SELECTWORLD);
}

bool RenameMPLevelScreen::handleBackEvent(bool isDown)
{
	if (!isDown)
		minecraft->screenChooser.setScreen(SCREEN_SELECTWORLD);
	return true;
}

void RenameMPLevelScreen::keyPressed(int eventKey)
{
	if (eventKey == Keyboard::KEY_BACKSPACE) {
		if (!_name.empty()) _name.erase(_name.size() - 1, 1);
	} else if (eventKey == Keyboard::KEY_RETURN) {
		doRename();
	} else {
		super::keyPressed(eventKey);
	}
}

void RenameMPLevelScreen::keyboardNewChar(char inputChar)
{
	if (inputChar < 32) return;
	if (_name.size() < 64) _name += inputChar;
}

void RenameMPLevelScreen::keyboardText(const std::string& text)
{
	// UTF-8 aware: append whole string (may be multi-byte CJK).
	for (size_t i = 0; i < text.size(); ++i)
		keyboardNewChar(text[i]);
}
