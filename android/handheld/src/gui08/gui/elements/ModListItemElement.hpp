#pragma once
#include <gui/GuiElement.hpp>

struct ModsScreen;
struct Minecraft;
struct ModInfo;
struct ImageWithBackground;
struct Button;

// 模组列表项 —— 完全照抄世界列表项 LocalServerListItemElement 的结构：
// 整行一个 TButton 九宫格背景（field_2C），右侧一个 on/off 开关按钮
// （贴图 gui/mods/on.png / off.png）。点击整行 = 选中，点击开关 = 启停。
struct ModListItemElement: GuiElement
{
	float field_24, field_28;
	Button* field_2C;              // 九宫格背景（整行可点击）
	Button* field_30;              // 按下的按钮
	ImageWithBackground* toggleButton; // on/off 开关
	ModInfo* info;                 // 模组数据（由 ModsScreen 分配，此项析构时删除）
	ModsScreen* field_54;          // 所属 ModsScreen

	ModListItemElement(Minecraft*, ModInfo*, ModsScreen*);
	void init(Minecraft*);
	void updateToggleIcon(Minecraft*);

	virtual ~ModListItemElement();
	virtual void tick(Minecraft*);
	virtual void render(Minecraft*, int32_t, int32_t);
	virtual void mouseClicked(Minecraft*, int32_t, int32_t, int32_t);
	virtual void mouseReleased(Minecraft*, int32_t, int32_t, int32_t);
};
