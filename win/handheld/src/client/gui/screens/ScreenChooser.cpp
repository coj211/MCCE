#include "ScreenChooser.h"
#include "StartMenuScreen.h"
#include "SelectWorldScreen.h"
#include "JoinGameScreen.h"
#include "PauseScreen.h"
#include "RenameMPLevelScreen.h"
#include "IngameBlockSelectionScreen.h"
#include "ChatInputScreen.h"
#include "touch/TouchStartMenuScreen.h"
#include "touch/TouchSelectWorldScreen.h"
#include "touch/TouchJoinGameScreen.h"
#include "touch/TouchIngameBlockSelectionScreen.h"
#include <gui/screens/PauseScreen.hpp> // 0.8.1 暂停菜单移植
#include <gui/screens/CreativeInventoryScreen.hpp> // 0.8.1 背包移植：创造背包

#include "../../Minecraft.h"

Screen* ScreenChooser::createScreen( ScreenId id )
{
	Screen* screen = NULL;

	// On macOS always use the touchscreen UI variant for screens (nicer look),
	// while keeping useTouchscreen() = false so the renderer uses crosshair picking.
#if defined(MACOS) || defined(LINUX) || defined(WIN32)
	if (true) {
#else
	if (_mc->useTouchscreen()) {
#endif
		switch (id) {
		case SCREEN_STARTMENU:	screen = new Touch::StartMenuScreen();	break;
		case SCREEN_SELECTWORLD:screen = new Touch::SelectWorldScreen();break;
		case SCREEN_JOINGAME:	screen = new Touch::JoinGameScreen();	break;
		case SCREEN_PAUSE:	    screen = new PauseScreen081(false); break;
		case SCREEN_PAUSEPREV:	screen = new PauseScreen081(true);	 break;
		case SCREEN_BLOCKSELECTION:	screen = new Touch::IngameBlockSelectionScreen();	break;
		case SCREEN_CREATIVE_INVENTORY: screen = new CreativeInventoryScreen();	break;
		case SCREEN_CHAT:	        screen = new ChatInputScreen();	break;

		case SCREEN_NONE:
		default:
			// Do nothing
			break;
		}
	} else {
		switch (id) {
		case SCREEN_STARTMENU:	screen = new StartMenuScreen();  break;
		case SCREEN_SELECTWORLD:screen = new SelectWorldScreenLegacy();break;
		case SCREEN_JOINGAME:	screen = new JoinGameScreen();   break;
		case SCREEN_PAUSE:	    screen = new PauseScreen081(false); break;
		case SCREEN_PAUSEPREV:	screen = new PauseScreen081(true);	 break;
		case SCREEN_BLOCKSELECTION:	screen = new IngameBlockSelectionScreen();	break;
		case SCREEN_CHAT:	        screen = new ChatInputScreen();	break;

		case SCREEN_NONE:
		default:
			// Do nothing
			break;
		}
	}
	return screen;
}

Screen* ScreenChooser::setScreen(ScreenId id)
{
	Screen* screen = createScreen(id);
	_mc->setScreen(screen);
	return screen;
}
