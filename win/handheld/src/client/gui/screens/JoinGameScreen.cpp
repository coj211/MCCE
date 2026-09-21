#include "JoinGameScreen.h"
#include "AddServerScreen.h"
#include "StartMenuScreen.h"
#include "ProgressScreen.h"
#include "../Font.h"
#include "../../../network/RakNetInstance.h"
#include "../../../locale/I18n.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include <stdio.h>

JoinGameScreen::JoinGameScreen()
:	bJoin(  2, "Join Game"),
	bBack(  3, "Back"),
	bAddServer(4, I18n::get("addserver.add")),
	gamesList(NULL)
{
	bJoin.active = false;
	//gamesList->yInertia = 0.5f;
}

JoinGameScreen::~JoinGameScreen()
{
	delete gamesList;
}

void JoinGameScreen::buttonClicked(Button* button)
{
	if (button->id == bJoin.id)
	{
		if (isIndexValid(gamesList->selectedItem))
		{
			PingedCompatibleServer selectedServer = gamesList->copiedServerList[gamesList->selectedItem];
			minecraft->joinMultiplayer(selectedServer);
			{
				bJoin.active = false;
				bBack.active = false;
				minecraft->setScreen(new ProgressScreen());
			}
		}
		//minecraft->locateMultiplayer();
		//minecraft->setScreen(new JoinGameScreen());
	}
	if (button->id == bBack.id)
	{
		minecraft->cancelLocateMultiplayer();
		minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
	}
	if (button->id == bAddServer.id)
	{
		minecraft->cancelLocateMultiplayer();
		minecraft->setScreen(new AddServerScreen());
	}
}

bool JoinGameScreen::handleBackEvent(bool isDown)
{
	if (!isDown)
	{
		minecraft->cancelLocateMultiplayer();
		minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
	}
	return true;
}


bool JoinGameScreen::isIndexValid( int index )
{
	return gamesList && index >= 0 && index < gamesList->getNumberOfItems();
}

void JoinGameScreen::tick()
{
	const ServerList& orgServerList = minecraft->raknetInstance->getServerList();
	ServerList serverList;
	for (unsigned int i = 0; i < orgServerList.size(); ++i)
		if (orgServerList[i].name.GetLength() > 0)
			serverList.push_back(orgServerList[i]);

	// Prepend saved custom servers (marked special) above the LAN scan.
	{
		const std::vector<CustomServer>& cs = customServers();
		std::vector<PingedCompatibleServer> custom;
		for (size_t i = 0; i < cs.size(); ++i) {
			PingedCompatibleServer s;
			s.name = cs[i].name.c_str();
			s.address.FromStringExplicitPort(cs[i].ip.c_str(), (unsigned short)cs[i].port, 0);
			s.pingTime = 0;
			s.isSpecial = true;
			custom.push_back(s);
		}
		serverList.insert(serverList.begin(), custom.begin(), custom.end());
	}

	if (serverList.size() != gamesList->copiedServerList.size())
	{
		// copy the currently selected item
		PingedCompatibleServer selectedServer;
		bool hasSelection = false;
		if (isIndexValid(gamesList->selectedItem))
		{
			selectedServer = gamesList->copiedServerList[gamesList->selectedItem];
			hasSelection = true;
		}

		gamesList->copiedServerList = serverList;
		gamesList->selectItem(-1, false);

		// re-select previous item if it still exists
		if (hasSelection)
		{
			for (unsigned int i = 0; i < gamesList->copiedServerList.size(); i++)
			{
				if (gamesList->copiedServerList[i].address == selectedServer.address)
				{
					gamesList->selectItem(i, false);
					break;
				}
			}
		}
	} else {
		for (int i = (int)gamesList->copiedServerList.size()-1; i >= 0 ; --i) {
			for (int j = 0; j < (int) serverList.size(); ++j)
				if (serverList[j].address == gamesList->copiedServerList[i].address)
					gamesList->copiedServerList[i].name = serverList[j].name;
		}
	}

	bJoin.active = isIndexValid(gamesList->selectedItem);
}

void JoinGameScreen::init()
{
	buttons.push_back(&bJoin);
	buttons.push_back(&bBack);
	buttons.push_back(&bAddServer);

	loadCustomServers();
	minecraft->raknetInstance->clearServerList();
	gamesList = new AvailableGamesList(minecraft, width, height);

#ifdef ANDROID
	tabButtons.push_back(&bJoin);
	tabButtons.push_back(&bBack);
#endif
}

void JoinGameScreen::setupPositions() {
	int yBase = height - 26;

	bJoin.y =	yBase;
	bBack.y =   yBase;
	bAddServer.y = yBase;

	bBack.width = bJoin.width = bAddServer.width = 120;

	// Center buttons: Join | Back on the bottom, Add Server top-right.
	bJoin.x = width / 2 - 4 - bJoin.width;
	bBack.x = width / 2 + 4;
	bAddServer.x = width - bAddServer.width - 10;
	bAddServer.y = 40;
}

void JoinGameScreen::render( int xm, int ym, float a )
{
	bool hasNetwork = minecraft->platform()->isNetworkEnabled(true);
#ifdef WIN32
	hasNetwork = hasNetwork && !GetAsyncKeyState(VK_TAB);
#endif

	renderBackground();
	if (hasNetwork) gamesList->render(xm, ym, a);
	Screen::render(xm, ym, a);

	if (hasNetwork) {
#ifdef RPI
		std::string s = "Scanning for Local Network Games...";
#else
		std::string s = "Scanning for WiFi Games...";
#endif
		drawCenteredString(minecraft->font, s, width / 2, 8, 0xffffffff);

		const int textWidth = minecraft->font->width(s);
		const int spinnerX = width/2 + textWidth / 2 + 6;

		static const char* spinnerTexts[] = {"-", "\\", "|", "/"};
		int n = ((int)(5.5f * getTimeS()) % 4);
		drawCenteredString(minecraft->font, spinnerTexts[n], spinnerX, 8, 0xffffffff);
	} else {
		std::string s = "WiFi is disabled";
		const int yy = height / 2 - 8;
		drawCenteredString(minecraft->font, s, width / 2, yy, 0xffffffff);
	}
}

bool JoinGameScreen::isInGameScreen() { return false; }

// ---------------------------------------------------------------------------
// Persistent custom server list (games/custom_servers.txt next to the exe).
// Format: one server per line, "name|ip|port" (name may contain spaces).
// ---------------------------------------------------------------------------

static std::string customServersPath() {
#ifdef _WIN32
	char buf[MAX_PATH];
	GetModuleFileNameA(NULL, buf, sizeof(buf));
	std::string p(buf);
	size_t slash = p.find_last_of("\\/");
	std::string dir = (slash == std::string::npos) ? "." : p.substr(0, slash);
	// Ensure the games dir exists (same place world saves live).
	CreateDirectoryA((dir + "\\games").c_str(), NULL);
	return dir + "\\games\\custom_servers.txt";
#else
	return "custom_servers.txt";
#endif
}

std::vector<JoinGameScreen::CustomServer>& JoinGameScreen::customServers()
{
	static std::vector<CustomServer> list;
	return list;
}

void JoinGameScreen::loadCustomServers()
{
	std::vector<CustomServer>& list = customServers();
	list.clear();
	FILE* f = fopen(customServersPath().c_str(), "r");
	if (!f) return;
	char line[512];
	while (fgets(line, sizeof(line), f)) {
		// strip trailing newline
		size_t len = strlen(line);
		while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
			line[--len] = 0;
		if (len == 0) continue;
		std::string s(line);
		size_t p1 = s.find('|');
		if (p1 == std::string::npos) continue;
		size_t p2 = s.find('|', p1 + 1);
		if (p2 == std::string::npos) continue;
		CustomServer cs;
		cs.name = s.substr(0, p1);
		cs.ip   = s.substr(p1 + 1, p2 - p1 - 1);
		cs.port = atoi(s.substr(p2 + 1).c_str());
		if (cs.port <= 0 || cs.port > 65535) cs.port = 19132;
		list.push_back(cs);
	}
	fclose(f);
}

void JoinGameScreen::saveCustomServers()
{
	const std::vector<CustomServer>& list = customServers();
	FILE* f = fopen(customServersPath().c_str(), "w");
	if (!f) return;
	for (size_t i = 0; i < list.size(); ++i)
		fprintf(f, "%s|%s|%d\n", list[i].name.c_str(), list[i].ip.c_str(), list[i].port);
	fclose(f);
}

void JoinGameScreen::addCustomServer(const std::string& ip, int port)
{
	std::vector<CustomServer>& list = customServers();
	// Avoid duplicates.
	for (size_t i = 0; i < list.size(); ++i) {
		if (list[i].ip == ip && list[i].port == port)
			return;
	}
	CustomServer cs;
	cs.name = ip + ":" + (port ? std::to_string(port) : std::string("19132"));
	cs.ip = ip;
	cs.port = port ? port : 19132;
	list.push_back(cs);
	saveCustomServers();
}
