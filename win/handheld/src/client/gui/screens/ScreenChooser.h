#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS__ScreenChooser_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS__ScreenChooser_H__

enum ScreenId {
	SCREEN_NONE,
	SCREEN_STARTMENU,
	SCREEN_JOINGAME,
	SCREEN_PAUSE,
	SCREEN_PAUSEPREV,
	SCREEN_SELECTWORLD,
	SCREEN_BLOCKSELECTION,
	SCREEN_CHAT,
	SCREEN_CREATIVE_INVENTORY  // 0.8.1 背包移植：创造背包
};

class Screen;
class Minecraft;

class ScreenChooser
{
public:
	ScreenChooser(Minecraft* mc)
	:	_mc(mc)
	{}

	Screen* createScreen(ScreenId id);
	Screen* setScreen(ScreenId id);
private:
	Minecraft* _mc;
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS__ScreenChooser_H__*/
