#include "SimpleChooseLevelScreen.h"
#include "ProgressScreen.h"
#include "ScreenChooser.h"
#include "../../Minecraft.h"
#include "../../../world/level/LevelSettings.h"
#include "../../../platform/time.h"
#include "../../../platform/input/Keyboard.h"
#include "../../../locale/I18n.h"
#include "../../../util/StringUtils.h"
#include "../Font.h"
#include <stdio.h>

static char ILLEGAL_FILE_CHARACTERS[] = {
	'/', '\n', '\r', '\t', '\0', '\f', '`', '?', '*', '\\', '<', '>', '|', '\"', ':'
};

SimpleChooseLevelScreen::SimpleChooseLevelScreen(const std::string& levelName)
:	bDone(1, I18n::get("gui.ok")),
	bCancel(2, I18n::get("gui.cancel")),
	bMode(3, ""),
	bWorldType(4, ""),
	_name(levelName),
	_seed(""),
	_focus(0),
	_gameType(GameType::Creative),
	_worldType(WorldType::Old),
	_cursorTick(0)
{
}

SimpleChooseLevelScreen::~SimpleChooseLevelScreen()
{
}

void SimpleChooseLevelScreen::init()
{
	super::init(); // loads the level list for getUniqueLevelName()

	buttons.push_back(&bDone);
	buttons.push_back(&bCancel);
	buttons.push_back(&bMode);
	buttons.push_back(&bWorldType);

	tabButtons.push_back(&bDone);
	tabButtons.push_back(&bCancel);
	tabButtons.push_back(&bMode);
	tabButtons.push_back(&bWorldType);

	// Toggle labels follow the current state
	bMode.msg = I18n::get("selectWorld.gameMode.creative");
	bWorldType.msg = I18n::get("createWorld.oldWorld");
}

void SimpleChooseLevelScreen::setupPositions()
{
	const int cx = width / 2;
	const int bh = 20;
	const int btnW = 150;

	bDone.width = bCancel.width = 120;
	bMode.width = bWorldType.width = btnW;
	bDone.height = bCancel.height = bMode.height = bWorldType.height = bh;

	bDone.x = cx - 4 - 120;
	bCancel.x = cx + 4;
	bDone.y = bCancel.y = height - 30;

	bMode.x = width / 4 - btnW / 2;
	bMode.y = 124;

	bWorldType.x = width * 3 / 4 - btnW / 2;
	bWorldType.y = 124;
}

void SimpleChooseLevelScreen::tick()
{
	_cursorTick++;
}

void SimpleChooseLevelScreen::render( int xm, int ym, float a )
{
	renderBackground();
	glEnable2(GL_BLEND);

	const int cx = width / 2;

	drawCenteredString(minecraft->font, I18n::get("selectWorld.create"), cx, 20, 0xffffffff);
	drawCenteredString(minecraft->font, I18n::get("selectWorld.enterName"), cx, 52, 0xffaaaaaa);
	drawTextField();

	// Game mode & world options sit side by side (left/right columns)
	drawCenteredString(minecraft->font, I18n::get("selectWorld.gameMode"), width / 4, 108, 0xffaaaaaa);
	drawCenteredString(minecraft->font, I18n::get("createWorld.worldOptions"), width * 3 / 4, 108, 0xffaaaaaa);
	drawModeDescription();

	// Seed now takes the row where world options used to be
	drawCenteredString(minecraft->font, I18n::get("selectWorld.enterSeed"), cx, 192, 0xffaaaaaa);
	drawSeedField();
	drawCenteredString(minecraft->font, I18n::get("selectWorld.seedInfo"), cx, 252, 0xff666666);

	super::render(xm, ym, a);
	glDisable2(GL_BLEND);
}

void SimpleChooseLevelScreen::drawTextField()
{
	const int cx = width / 2;
	const int fieldW = 240;
	const int fieldH = 22;
	const int x0 = cx - fieldW / 2;
	const int y0 = 64;

	fillGradient(x0, y0, x0 + fieldW, y0 + fieldH, 0x80000000, 0x80000000);

	std::string shown = _name;
	if (_focus == 0 && (_cursorTick / 20) % 2 == 0)
		shown += "_";
	minecraft->font->draw(shown, x0 + 6, y0 + 6, 0xffffffff);
}

void SimpleChooseLevelScreen::drawSeedField()
{
	const int cx = width / 2;
	const int fieldW = 240;
	const int fieldH = 22;
	const int x0 = cx - fieldW / 2;
	const int y0 = 204;

	fillGradient(x0, y0, x0 + fieldW, y0 + fieldH, 0x80000000, 0x80000000);

	std::string shown = _seed;
	if (_focus == 1 && (_cursorTick / 20) % 2 == 0)
		shown += "_";
	minecraft->font->draw(shown, x0 + 6, y0 + 6, 0xffffffff);
}

void SimpleChooseLevelScreen::drawModeDescription()
{
	// Centered under the game mode column (left half)
	const int cxl = width / 4;
	if (_gameType == GameType::Creative) {
		drawCenteredString(minecraft->font, I18n::get("selectWorld.gameMode.creative.line1"), cxl, 156, 0xff888888);
		drawCenteredString(minecraft->font, I18n::get("selectWorld.gameMode.creative.line2"), cxl, 168, 0xff888888);
	} else {
		drawCenteredString(minecraft->font, I18n::get("selectWorld.gameMode.survival.line1"), cxl, 156, 0xff888888);
		drawCenteredString(minecraft->font, I18n::get("selectWorld.gameMode.survival.line2"), cxl, 168, 0xff888888);
	}
}

void SimpleChooseLevelScreen::buttonClicked( Button* button )
{
	if (button->id == bDone.id) {
		doCreate();
		return;
	}
	if (button->id == bCancel.id) {
		minecraft->screenChooser.setScreen(SCREEN_SELECTWORLD);
		return;
	}
	if (button->id == bMode.id) {
		// Toggle creative <-> survival
		_gameType = (_gameType == GameType::Creative)? GameType::Survival : GameType::Creative;
		bMode.msg = (_gameType == GameType::Creative)?
			I18n::get("selectWorld.gameMode.creative") : I18n::get("selectWorld.gameMode.survival");
		return;
	}
	if (button->id == bWorldType.id) {
		// Toggle infinite <-> finite(old)
		_worldType = (_worldType == WorldType::Old)? WorldType::Infinite : WorldType::Old;
		bWorldType.msg = (_worldType == WorldType::Old)?
			I18n::get("createWorld.oldWorld") : I18n::get("createWorld.infinite");
		return;
	}
}

void SimpleChooseLevelScreen::mouseClicked(int x, int y, int buttonNum)
{
	super::mouseClicked(x, y, buttonNum);

	// Clicking a text field moves the input focus there.
	const int cx = width / 2;
	const int fieldW = 240;
	const int fieldH = 22;
	if (x >= cx - fieldW / 2 && x < cx + fieldW / 2) {
		if (y >= 64 && y < 64 + fieldH)
			_focus = 0;
		else if (y >= 204 && y < 204 + fieldH)
			_focus = 1;
	}
}

void SimpleChooseLevelScreen::doCreate()
{
	std::string name = Util::stringTrim(_name);
	std::string levelId = name;
	for (int i = 0; i < sizeof(ILLEGAL_FILE_CHARACTERS) / sizeof(char); ++i)
		levelId = Util::stringReplace(levelId, std::string(1, ILLEGAL_FILE_CHARACTERS[i]), "");
	if ((int)levelId.length() == 0)
		levelId = "world";
	levelId = getUniqueLevelName(levelId);

	// Parse the seed: numeric -> direct, otherwise hash the string.
	int seed = getEpochTimeS();
	{
		std::string seedString = Util::stringTrim(_seed);
		if (seedString.length() > 0) {
			int tmpSeed;
			if (sscanf(seedString.c_str(), "%d", &tmpSeed) > 0)
				seed = tmpSeed;
			else
				seed = Util::hashCode(seedString);
		}
	}

	LevelSettings settings(seed, _gameType, _worldType);
	LOGI("Creating a level with id '%s', name '%s', seed '%d', gameType %d, worldType %d\n",
		levelId.c_str(), name.c_str(), seed, _gameType, _worldType);
	minecraft->selectLevel(levelId, name, settings);
	minecraft->hostMultiplayer();
	minecraft->setScreen(new ProgressScreen());
}

bool SimpleChooseLevelScreen::handleBackEvent(bool isDown) {
	if (!isDown)
		minecraft->screenChooser.setScreen(SCREEN_SELECTWORLD);
	return true;
}

void SimpleChooseLevelScreen::keyPressed(int eventKey)
{
	if (eventKey == Keyboard::KEY_BACKSPACE) {
		if (_focus == 0) {
			if (!_name.empty()) _name.erase(_name.size() - 1, 1);
		} else {
			if (!_seed.empty()) _seed.erase(_seed.size() - 1, 1);
		}
	} else if (eventKey == Keyboard::KEY_RETURN) {
		doCreate();
	} else {
		super::keyPressed(eventKey);
	}
}

void SimpleChooseLevelScreen::keyboardNewChar(char inputChar)
{
	if (inputChar < 32) return;
	if (_focus == 0) {
		if (_name.size() < 64) _name += inputChar;
	} else {
		if (_seed.size() < 32) _seed += inputChar;
	}
}

void SimpleChooseLevelScreen::keyboardText(const std::string& text)
{
	// UTF-8 aware: append whole string (may be multi-byte CJK).
	for (size_t i = 0; i < text.size(); ++i)
		keyboardNewChar(text[i]);
}
