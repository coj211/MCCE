#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS__DemoChooseLevelScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS__DemoChooseLevelScreen_H__

#include "ChooseLevelScreen.h"
#include "../components/Button.h"

// Java Edition style single-page world creation:
//   world name field / game mode toggle (+description) / world options
//   toggle / Done-Cancel.
class SimpleChooseLevelScreen: public ChooseLevelScreen
{
	typedef ChooseLevelScreen super;
public:
	SimpleChooseLevelScreen(const std::string& levelName);

	virtual ~SimpleChooseLevelScreen();

	void init();
	void setupPositions();
	void tick();
	void render(int xm, int ym, float a);
	void buttonClicked(Button* button);
	bool handleBackEvent(bool isDown);
	void keyPressed(int eventKey);
	// Text input field -> enable the IME so CJK names can be typed.
	virtual bool hasTextInput() const { return true; }
protected:
	virtual void mouseClicked(int x, int y, int buttonNum);
	virtual void keyboardNewChar(char inputChar);
	virtual void keyboardText(const std::string& text);

private:
	void doCreate();
	void drawTextField();
	void drawSeedField();
	void drawModeDescription();

	Button bDone;      // 确定 (create)
	Button bCancel;    // 取消
	Button bMode;      // game mode toggle: 创造 <-> 生存
	Button bWorldType; // world options toggle: 无限 <-> 有限

	std::string _name;
	std::string _seed;
	int _focus;      // 0 = name field, 1 = seed field
	int _gameType;   // GameType::Creative / Survival
	int _worldType;  // WorldType::Infinite / Old
	int _cursorTick;
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS__DemoChooseLevelScreen_H__*/
