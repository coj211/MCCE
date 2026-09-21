#pragma once
// 世界规则行（开关 + "点一下切一档"按钮）的公用控件。
// 新世界界面（CreateWorldScreen）与世界设置界面（RenameMPLevelScreen）共用。
#include "IWorldRulesHost.hpp"
#include <_types.h>
#include <gui/buttons/ImageButton.hpp>
#include <gui/buttons/Touch_TButton.hpp>
#include <Minecraft.hpp>
#include <util/IntRectangle.hpp>

// 开关索引（IWorldRulesHost::getRuleValue / toggleRule）
enum {
	RULE_DAYLIGHT = 0,   // 昼夜更替
	RULE_KEEP_INV,       // 死亡不掉落
	RULE_RESPAWN,        // 死亡立刻重生
	RULE_WAKE_UP,        // 睡觉立刻苏醒
	RULE_SPAWN_MOBS,     // 生成生物
	RULE_SPAWN_ENEMIES,  // 生成敌对生物
	RULE_AUTO_OP,        // 创建者（房主）获得 op
	RULE_COUNT
};

// 按钮动作（WorldRuleActionButton::_action）
enum {
	ACTION_GAME_MODE = 0,   // 创造 / 生存
	ACTION_WORLD_TYPE,      // 经典 / 无限
	ACTION_DIFFICULTY,      // 和平 / 简单 / 普通 / 困难
};

// on/off 开关：外观与设置页、语言界面的开关完全一致
// （gui/touchgui.png 的两帧开关图，开启时画第二帧）。
struct WorldRuleToggleButton : ImageButton {
	typedef ImageButton super;
	WorldRuleToggleButton(int id, IWorldRulesHost* host, int index)
		: ImageButton(id, ""), _host(host), _index(index) {
		ImageDef def;
		def.setSrc(IntRectangle(160, 206, 39, 20));
		def.name = "gui/touchgui.png";
		def.width = 39 * 0.7f;
		def.height = 20 * 0.7f;
		this->setImageDef(def, true);
	}
	// 点击在 OptionsPane 里分发（坐标已按滚动偏移校正），自己处理即可
	virtual void mouseClicked(Minecraft* mc, int x, int y, int buttonNum) {
		if (buttonNum == 1 && _host && this->clicked(mc, x, y))
			_host->toggleRule(_index);
	}
protected:
	virtual bool isSecondImage(bool hovered) {
		return _host && _host->getRuleValue(_index);
	}
private:
	IWorldRulesHost* _host;
	int _index;
};

// 普通的"游戏内按钮"（主菜单那种），点一下切一档。
struct WorldRuleActionButton : Touch::TButton {
	typedef Touch::TButton super;
	WorldRuleActionButton(int id, const std::string& msg, Minecraft* mc, IWorldRulesHost* host, int action)
		: Touch::TButton(id, msg, mc), _host(host), _action(action) {}
	virtual void mouseClicked(Minecraft* mc, int x, int y, int buttonNum) {
		if (buttonNum != 1 || !_host || !this->clicked(mc, x, y)) return;
		this->setPressed();
		switch (_action) {
		case ACTION_GAME_MODE:  _host->cycleGameMode();  break;
		case ACTION_WORLD_TYPE: _host->cycleWorldType(); break;
		case ACTION_DIFFICULTY: _host->cycleDifficulty(); break;
		}
	}
	virtual void mouseReleased(Minecraft* mc, int x, int y, int buttonNum) {
		this->released(x, y);
	}
private:
	IWorldRulesHost* _host;
	int _action;
};
