#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS__RenameMPLevelScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS__RenameMPLevelScreen_H__

#include "../Screen.h"
#include "../components/Button.h"

// Java Edition style rename dialog: title + "world name" label + text field
// (same look as the Add Server screen) + Done / Cancel.
class RenameMPLevelScreen: public Screen
{
	typedef Screen super;
public:
    RenameMPLevelScreen(const std::string& levelId, const std::string& levelName);

    virtual void init();
    virtual void setupPositions();
    virtual void tick();
	virtual void render(int xm, int ym, float a);

	virtual void buttonClicked(Button* button);
	virtual bool handleBackEvent(bool isDown);
	virtual void keyPressed(int eventKey);
	// Text input field -> enable the IME so CJK names can be typed.
	virtual bool hasTextInput() const { return true; }
protected:
	virtual void keyboardNewChar(char inputChar);
	virtual void keyboardText(const std::string& text);

private:
    void doRename();
    void drawTextField();

    Button bDone;
    Button bCancel;

    std::string _levelId;
    std::string _name;      // current text field content
    int _cursorTick;
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS__RenameMPLevelScreen_H__*/
