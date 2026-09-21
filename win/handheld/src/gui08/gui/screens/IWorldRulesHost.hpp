#pragma once
// 世界规则界面（新世界 / 世界设置）共用的回调接口。
// 开关按钮与"点一下切一档"的游戏内按钮通过它把改动交回宿主界面，
// 这样两个界面可以共用同一套控件代码。
struct IWorldRulesHost {
	virtual ~IWorldRulesHost() {}
	// 开关（索引见 WorldRulesWidgets.hpp 的 RULE_*）
	virtual bool getRuleValue(int index) = 0;
	virtual void toggleRule(int index) = 0;
	// 点一下切一档
	virtual void cycleGameMode() = 0;
	// 世界类型只在创建新世界时可选（世界一旦生成，地形生成方式就改不了了），
	// 所以给一个默认空实现 —— 世界设置界面不需要它。
	virtual void cycleWorldType() {}
	virtual void cycleDifficulty() = 0;
};
