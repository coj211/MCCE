#pragma once
#include <gui/Screen.hpp>
#include <gui/screens/IWorldRulesHost.hpp>
#include <memory>
#include <string>
#include <level/LevelSettings.hpp>

namespace Touch { class TButton; class THeader; }
struct TextBox081;
struct OptionsPane;
struct WorldRuleToggleButton;

// 世界设置界面（Play 列表里每个存档右边的铅笔按钮进入）
//
// 布局与新世界界面（CreateWorldScreen）一致：顶栏左「返回」右「完成」，
// 下面一个可滚动列表：世界名（改名）、世界模式、世界类型、难度 +
// 昼夜更替/死亡不掉落/死亡立刻重生/睡觉立刻苏醒/生成生物/生成敌对生物/op。
//
// 这些设置是**每个世界各自保存**的（写进该世界的 level.dat）。
struct RenameMPLevelScreen : Screen, IWorldRulesHost {
	std::string name;         // 世界文件夹名（levelId）
	std::string displayName;  // 显示名

	Touch::THeader* bHeader;
	Touch::TButton* bBack;
	Touch::TButton* bDone;
	OptionsPane* pane;
	TextBox081* nameBox;
	Touch::TButton* bGameMode;
	Touch::TButton* bDifficulty;
	WorldRuleToggleButton* toggles[7];

	bool creative;
	int  originalGeneratorVersion;
	WorldRules rules;

	RenameMPLevelScreen(const std::string& folderName, const std::string& displayName = "");
	void closeScreen();
	bool loadWorldData();   // 读该世界存档（成功返回 true）
	void saveWorld();       // 写回存档 + 改名

	void refreshButtonTexts();
	virtual bool getRuleValue(int index);
	virtual void toggleRule(int index);
	virtual void cycleGameMode();
	// 注意：世界类型（经典/无限）不在这里改 —— 地形已经生成，改不了
	virtual void cycleDifficulty();

	virtual ~RenameMPLevelScreen();
	virtual void render(int32_t, int32_t, float);
	virtual void init();
	virtual void setupPositions();
	virtual void tick();
	virtual void mouseClicked(int32_t, int32_t, int32_t);
	virtual void mouseReleased(int32_t, int32_t, int32_t);
	virtual void keyPressed(int32_t);
	virtual void keyboardNewChar(const std::string&, bool_t);
	virtual void keyboardNewChar(char inputChar);
	virtual bool handleBackEvent(bool);
	virtual void buttonClicked(Button*);
	virtual void setTextboxText(const std::string&);
	// 世界名输入框不在 textBoxes 里，必须声明有文本输入，
	// 否则 Win32 层不激活 IME、不生成字符 → 无法输入
	virtual bool hasTextInput() const { return true; }
	// 与语言界面/设置页一致：背景是主界面那个 6 图全景（renderMenuBackground 画的），
	// 覆盖成 true 才不会用泥土背景把全景盖掉
	virtual bool renderGameBehind() { return true; }
};
