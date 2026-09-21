#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS__AddServerScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS__AddServerScreen_H__

#include "../Screen.h"
#include "../components/Button.h"
#include "../components/SmallButton.h"
#include <string>

// Enter a remote server address (IPv4/IPv6/hostname) + port and connect
// directly, bypassing the LAN scan. Used from JoinGameScreen ("Add Server").
class AddServerScreen: public Screen
{
	typedef Screen super;
public:
	AddServerScreen();
	virtual ~AddServerScreen();

	void init();
	void setupPositions();

	virtual bool handleBackEvent(bool isDown);
	virtual void tick();

	void render(int xm, int ym, float a);

	void buttonClicked(Button* button);

	virtual void keyPressed(int eventKey);
	virtual void keyboardNewChar(char inputChar);
	virtual void keyboardText(const std::string& text);

	bool isInGameScreen();

private:
	void connectNow();
	bool isIPValid(const std::string& ip);
	int parsePort(const std::string& s);

	Button bConnect;
	Button bCancel;
	bool _connected;
	std::string _ip;
	std::string _port;
	// which field has focus: 0 = IP, 1 = port
	int _focus;
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS__AddServerScreen_H__*/
