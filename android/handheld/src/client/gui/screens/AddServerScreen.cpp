#include "AddServerScreen.h"
#include "JoinGameScreen.h"
#include "../../Minecraft.h"
#include "../../../network/RakNetInstance.h"
#include "../../../AppConstants.h"
#include "../../../locale/I18n.h"
#include <string.h>
#include <ctype.h>

AddServerScreen::AddServerScreen()
:	bConnect(  2, I18n::get("addserver.add")),
	bCancel(   3, I18n::get("gui.cancel")),
	_connected(false),
	_focus(0)
{
}

AddServerScreen::~AddServerScreen()
{
}

void AddServerScreen::init()
{
	buttons.push_back(&bConnect);
	buttons.push_back(&bCancel);
}

void AddServerScreen::setupPositions()
{
	int yBase = height - 26;

	bCancel.width = bConnect.width = 120;
	bConnect.x = width / 2 - 4 - bConnect.width;
	bCancel.x = width / 2 + 4;
	bConnect.y = yBase;
	bCancel.y = yBase;
}

bool AddServerScreen::handleBackEvent(bool isDown)
{
	if (!isDown)
		minecraft->screenChooser.setScreen(SCREEN_JOINGAME);
	return true;
}

void AddServerScreen::tick()
{
}

bool AddServerScreen::isIPValid(const std::string& ip)
{
	if (ip.empty()) return false;
	// IPv6 literal or hostname contains ':'; IPv4 dotted; hostnames with dots/dashes.
	for (size_t i = 0; i < ip.size(); ++i) {
		char c = ip[i];
		if (!(isalnum((unsigned char)c) || c == '.' || c == ':' || c == '-' || c == '_'))
			return false;
	}
	return true;
}

int AddServerScreen::parsePort(const std::string& s)
{
	if (s.empty()) return 19132;
	int p = 0;
	for (size_t i = 0; i < s.size(); ++i) {
		if (!isdigit((unsigned char)s[i])) return -1;
		p = p * 10 + (s[i] - '0');
		if (p > 65535) return -1;
	}
	return p;
}

void AddServerScreen::connectNow()
{
	if (_connected) return;
	if (!isIPValid(_ip)) return;
	int port = parsePort(_port);
	if (port <= 0) return;

	// Save the server into the persistent custom list; the Join screen shows
	// it at the top of the list where the player selects and joins it.
	JoinGameScreen::addCustomServer(_ip, port);

	_connected = true;
	minecraft->screenChooser.setScreen(SCREEN_JOINGAME);
}

void AddServerScreen::buttonClicked(Button* button)
{
	if (button->id == bConnect.id) {
		connectNow();
	}
	if (button->id == bCancel.id) {
		minecraft->screenChooser.setScreen(SCREEN_JOINGAME);
	}
}

void AddServerScreen::keyPressed(int eventKey)
{
	if (eventKey == Keyboard::KEY_BACKSPACE) {
		if (_focus == 0) {
			if (!_ip.empty()) _ip.erase(_ip.size() - 1, 1);
		} else {
			if (!_port.empty()) _port.erase(_port.size() - 1, 1);
		}
	} else if (eventKey == Keyboard::KEY_RETURN) {
		if (_focus == 0) {
			_focus = 1;
		} else {
			connectNow();
		}
	} else {
		super::keyPressed(eventKey);
	}
}

void AddServerScreen::keyboardNewChar(char inputChar)
{
	if (inputChar < 32) return;
	if (_focus == 0) {
		if (_ip.size() < 64) _ip += inputChar;
	} else {
		if (_port.size() < 6) _port += inputChar;
	}
}

void AddServerScreen::keyboardText(const std::string& text)
{
	// UTF-8 aware: append whole string (may be multi-byte).
	for (size_t i = 0; i < text.size(); ++i)
		keyboardNewChar(text[i]);
}

void AddServerScreen::render(int xm, int ym, float a)
{
	renderBackground();

	const int cx = width / 2;
	const int fieldW = 240;
	const int fieldH = 22;

	drawCenteredString(minecraft->font, I18n::get("addserver.title"), cx, 30, 0xffffffff);
	drawCenteredString(minecraft->font, I18n::get("addserver.address"), cx, 60, 0xffaaaaaa);

	// IP field
	{
		int x0 = cx - fieldW / 2;
		int y0 = 76;
		fillGradient(x0, y0, x0 + fieldW, y0 + fieldH, 0x80000000, 0x80000000);
		std::string shown = _ip;
		if (_focus == 0)
			shown += "_";
		minecraft->font->draw(shown, x0 + 6, y0 + 6, 0xffffffff);
	}

	drawCenteredString(minecraft->font, I18n::get("addserver.port"), cx, 110, 0xffaaaaaa);

	// Port field
	{
		int x0 = cx - fieldW / 2;
		int y0 = 126;
		fillGradient(x0, y0, x0 + fieldW, y0 + fieldH, 0x80000000, 0x80000000);
		std::string shown = _port.empty()? "" : _port;
		if (_focus == 1)
			shown += "_";
		minecraft->font->draw(shown, x0 + 6, y0 + 6, 0xffffffff);
	}

	drawCenteredString(minecraft->font, I18n::get("addserver.hint"), cx, height - 44, 0xff888888);

	super::render(xm, ym, a);
}

bool AddServerScreen::isInGameScreen() { return false; }
