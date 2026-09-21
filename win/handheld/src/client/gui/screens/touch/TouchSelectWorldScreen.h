#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS_TOUCH__TouchSelectWorldScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS_TOUCH__TouchSelectWorldScreen_H__

#include "../../Screen.h"
#include "../../components/Button.h"
#include "../../components/ImageButton.h"
#include <string>
#include <vector>
#include "../../../../world/level/storage/LevelStorageSource.h"

// 0.8.1 GUI 移植：0.8.1 触摸版选世界（顶部 Back/标题/Create new + 横排世界卡片 + 底部垃圾桶删除）
namespace Touch {
	struct TouchWorldSelectionList;

	class SelectWorldScreen: public Screen
	{
	public:
		std::vector<LevelSummary> field_19C;
		ImageButton field_54;
		// 0.8.1 真机布局：右上 Edit + New 两个按钮
		Touch::TButton editButton;
		Touch::TButton createNewButton;
		Touch::TButton backButton;
		Button field_168;
		Touch::THeader selectWorldHeader;
		TouchWorldSelectionList* selectionList;
		int8_t field_1A8, field_1A9, field_1AA, field_1AB;
		int32_t field_1AC;

		SelectWorldScreen();
		std::string getUniqueLevelName(const std::string&);
		void loadLevelSource();

		virtual ~SelectWorldScreen();
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
}

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS_TOUCH__TouchSelectWorldScreen_H__*/
