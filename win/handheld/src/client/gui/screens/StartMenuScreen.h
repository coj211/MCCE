#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS__StartMenuScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS__StartMenuScreen_H__

#include "../Screen.h"
#include "../components/Button.h"

// 0.8.1 GUI 移植：0.8.1 风格主菜单（替换 1.7.10 还原版）
class StartMenuScreen: public Screen
{
public:
	StartMenuScreen();
	virtual ~StartMenuScreen();

	void init();
	void setupPositions();

	void tick();
	void render(int xm, int ym, float a);

	void buttonClicked(Button* button);
	bool handleBackEvent(bool isDown);
	bool isInGameScreen();
private:
	void _updateLicense();

	Button startGameButton;
	Button joinGameButton;
	Button optionsButton;
	Button createButton;
	Button buyButton;

	std::string mojangABMaybe;
	int copyrightPosX;
	std::string gameVersion;
	int versionPosX;
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS__StartMenuScreen_H__*/
