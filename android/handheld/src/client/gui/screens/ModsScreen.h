#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS_MODSSCREEN_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS_MODSSCREEN_H__

#include "../Screen.h"
#include "../components/Button.h"

#include <vector>
#include <string>

struct ModInfo;
struct NinePatchLayer;
struct PackedScrollContainer;
struct ModListItemElement;
namespace Touch { struct TButton; struct THeader; }

// Mod manager screen —— 0.8.1 游戏页面（PlayScreen）同款结构：
// 顶栏(返回 / 标题"模组" / 加载模组) + 右侧 PackedScrollContainer 竖排模组列表
// (ModListItemElement：九宫格背景 + 模组名/作者/版本 + 右侧 on/off 开关，
//   结构照抄世界列表项 LocalServerListItemElement) + 左侧封面区 + 全景背景。
class ModsScreen: public Screen {
	typedef Screen super;
public:
	ModsScreen();
	~ModsScreen();
	void init();
	void setupPositions();
	void buttonClicked(Button* button);
	void render(int xm, int ym, float a);
	void tick();
	void removed();
	virtual void mouseClicked(int x, int y, int buttonNum);
	virtual void mouseReleased(int x, int y, int buttonNum);
	virtual void onMouseWheel(int dy);
	virtual void renderBackground(int vo);
	// 列表项整行点击 → 选中（左侧封面区联动）
	void selectMod(const ModInfo* info);
	bool isSelected(const ModInfo* info) const;
	// 列表项开关切换后调用 → 下帧刷新列表（重新 scanMods 同步启停状态）
	void onModToggled();
private:
	void refreshMods();
	void pickAndInstall();
	void drawCover(int x, int y, int w, int h, const ModInfo& m);
	int descMinScroll() const;   // 简介可滚下限（负值），0 = 无需滚动

	Touch::THeader* bHeader;
	Touch::TButton* bBack;
	Touch::TButton* bLoad;
	NinePatchLayer* frame;                 // 列表九宫格边框
	PackedScrollContainer* list;           // 右侧模组列表（elements 树；children = ModListItemElement）
	std::vector<ModListItemElement*> items;// 列表项（本类拥有：重建时 delete，再 clearAll）
	std::vector<ModInfo> mods;             // scanMods 结果（值拷贝；列表项持有各自 new 的副本）
	int _selected;                         // mods[] 索引，-1 = 无
	int _listLeft, _listTop, _listW, _listH;
	bool _dirty;
	// 左侧简介区滚动（描述文本超出可视高度时可拖动/滚轮查看）
	int _descScroll;        // 像素偏移（0 = 顶部，负值向下滚）
	bool _descDragging;     // 正在拖动简介
	bool _descDragged;      // 本次按下期间拖动过（拖动与点击区分）
	int _descDragStartY;    // 拖起点的鼠标 GUI y
	int _descScrollAtStart; // 拖起点时的滚动偏移
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS_MODSSCREEN_H__*/
