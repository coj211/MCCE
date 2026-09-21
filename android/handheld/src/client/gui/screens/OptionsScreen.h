#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS__OptionsScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS__OptionsScreen_H__

#include "../Screen.h"
#include <string>
#include <vector>

// 0.8.1 GUI 移植：0.8.1 风格设置页（替换 0.6.1 版）
struct OptionsPane;
struct ImageButton;
struct NinePatchLayer;
struct ImageDef;
namespace Touch { struct THeader; struct TButton; }

class OptionsScreen: public Screen
{
public:
	OptionsScreen(bool isInWorld = false);
	virtual ~OptionsScreen();

	void closeScreen();
	int32_t createCategoryButton(int32_t a2, int32_t a3, struct ImageDef& a4, int32_t a5, int32_t a6, int32_t a7, int32_t a8);
	void createCategoryButtons(void);
	void generateOptionScreens(void);
	void selectCategory(int32_t a2);

	virtual void render(int32_t a2, int32_t a3, float a4);
	virtual void init();
	virtual void setupPositions();
	virtual bool handleBackEvent(bool a2);
	virtual void tick();
	virtual void removed();
	virtual bool renderGameBehind();
	virtual void setTextboxText(const std::string& a2);
	virtual void buttonClicked(Button* a2);
	virtual void mouseClicked(int32_t a2, int32_t a3, int32_t a4);
	virtual void mouseReleased(int32_t a2, int32_t a3, int32_t a4);
	virtual void keyPressed(int32_t a2);
	virtual void keyboardNewChar(const std::string& a2, bool a3);
	// 0.8.1 GUI 移植：基类 keyboardText 逐字符调用 char 版本，
	// 必须覆盖它字符才能进设置页（否则 NAME 输入框收不到任何输入）
	virtual void keyboardNewChar(char inputChar);
	// NAME 输入框在 optionPanes 里（不在 textBoxes），必须声明有文本输入，
	// 否则 Win32 层不激活 IME、不生成字符 → 无法输入
	virtual bool hasTextInput() const { return true; }
	// 09 · UI 覆盖系统：本屏逻辑名（mod 用 "options.<element>" 覆盖）
	const char* uiScreenId() const { return "options"; }

	Touch::THeader* headerOptions;
	Touch::TButton* buttonBack;
	ImageButton* field_5C;
	std::vector<ImageButton*> field_60;
	std::vector<struct OptionsPane*> optionPanes;
	struct OptionsPane* selectedCategory;
	NinePatchLayer* field_7C;
	NinePatchLayer* field_80;
	int32_t field_84, field_88;
	bool isInWorld;
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS__OptionsScreen_H__*/
