#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS_SCRIPTEDSCREEN_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS_SCRIPTEDSCREEN_H__

#include "../Screen.h"
#include "../components/Button.h"
#include "../components/TextBox.h"
#include <string>
#include <vector>

// A screen built entirely from a JS mod (stage 5). The mod calls
// UI.openScreen({...}) with button/text/image/input definitions and JS
// callbacks (onButton/onText/onClose/onRender); this C++ screen renders
// them and forwards input back to the JS callbacks via ModEngine.
//
// Everything the mod declared is snapshotted into plain C++ vectors here
// (no duktape access outside ModEngine), so the screen owns no JS state
// and survives mod reloads safely.
class ScriptedScreen: public Screen {
	typedef Screen super;
public:
	struct Btn { int id; std::string text; int x, y, w, h; };
	struct Txt { int x, y; std::string text; int color; };
	struct Img { int x, y, w, h; std::string path; };
	struct Inp { int id; int x, y, w, h; std::string text; bool focused; };
	struct Cell { int id; int x, y, w, h; std::string text; int color; };  // grid cell

	ScriptedScreen();
	~ScriptedScreen();

	void init();
	void setupPositions();
	void render(int xm, int ym, float a);
	void tick();
	void buttonClicked(Button* button);
	virtual void keyPressed(int eventKey);
	virtual void keyboardNewChar(char inputChar);
	virtual void keyboardText(const std::string& text);
	virtual void mouseClicked(int x, int y, int buttonNum);
	bool handleBackEvent(bool isDown);

	// Populated by ModEngine before setScreen(this).
	std::string title;
	std::vector<Btn> btnDefs;
	std::vector<Txt> texts;
	std::vector<Img> images;
	std::vector<Inp> inputs;
	std::vector<Cell> cells;      // clickable grid cells (container-like)
	bool wantKeyboard;      // show OS keyboard on init
	int _activeInput;       // id of focused input box, -1 = none

private:
	std::vector<Button*> _btns;
	std::vector<TextBox*> _boxes;
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS_SCRIPTEDSCREEN_H__*/
