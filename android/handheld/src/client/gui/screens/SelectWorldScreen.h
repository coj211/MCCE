#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS__SelectWorldScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS__SelectWorldScreen_H__

#include "../Screen.h"
#include "../components/Button.h"
#include <string>
#include <vector>
#include "../../../world/level/storage/LevelStorageSource.h"

// 0.8.1 GUI 移植：0.8.1 风格选世界（替换 0.6.1 版）
struct WorldSelectionList;

class SelectWorldScreenLegacy: public Screen
{
public:
	std::vector<LevelSummary> field_50;
	Button deleteButton;
	Button createNewButton, backButton, field_EC;
	WorldSelectionList* selectionList;
	int8_t field_120, field_121, field_122, field_123;
	int32_t field_124;

	SelectWorldScreenLegacy();
	std::string getUniqueLevelName(const std::string&);
	void loadLevelSource();

	virtual ~SelectWorldScreenLegacy();
	virtual void render(int32_t, int32_t, float);
	virtual void init();
	virtual void setupPositions();
	virtual bool handleBackEvent(bool);
	virtual void tick();
	virtual bool isInGameScreen();
	virtual void buttonClicked(Button*);
	virtual void keyPressed(int32_t);
	virtual bool isIndexValid(int32_t);
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS__SelectWorldScreen_H__*/
