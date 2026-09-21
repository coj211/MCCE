#pragma once
#include <gui/Screen.hpp>

struct Label;
struct PackedScrollContainer;

// 0.8.1 暂停菜单（gui08 移植），类名带 081 避免与 client 0.6.1 版 PauseScreen 冲突
struct PauseScreen081: Screen
{
	int32_t field_54;
	int32_t tickCounter;
	bool field_5C;
	int8_t field_5D, field_5E, field_5F;
	Button* backToGameButton;
	Button* quitToTitleButton;
	Button* quitAndCopyMapButton;
	Button* optionsButton;
	Label* gameMenuLabel;
	PackedScrollContainer* field_74;

	PauseScreen081(bool);

	virtual ~PauseScreen081();
	virtual void render(int32_t, int32_t, float);
	virtual void init();
	virtual void setupPositions();
	virtual void tick();
	virtual bool renderGameBehind();
	virtual void buttonClicked(Button*);
	// 09 · UI 覆盖系统：本屏逻辑名（mod 用 "pause.<element>" 覆盖）
	const char* uiScreenId() const { return "pause"; }
};
