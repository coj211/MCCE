#pragma once
#include <gui/screens/CreateWorldScreenType.hpp>
#include <gui/screens/SelectWorldScreen.hpp>
#include <memory>
#include <network/mco/MCOServerListItem.hpp>
#include <level/LevelSettings.hpp>
#include <gui/screens/IWorldRulesHost.hpp>

struct RestRequestJob;
struct ImageButton;
struct TextBox081;
struct Label;
struct NinePatchLayer;
struct OptionsPane;
struct WorldRuleToggleButton;

// 创建世界界面（0.8.1 风格重做）
//
//   顶栏：左「返回」、右「创建世界」(ok)
//   下面是一个可滚动的选项列表（和语言界面共用 OptionsPane / OptionsItem）：
//     世界名、种子            —— 输入框
//     世界模式（创造/生存）   —— 游戏内按钮，点一下切一档
//     世界类型（经典/无限）   —— 游戏内按钮
//     难度（和平/简单/普通/困难）—— 游戏内按钮
//     昼夜更替 / 死亡不掉落 / 死亡立刻重生 / 睡觉立刻苏醒 /
//     生成生物 / 生成敌对生物 / 创建者获得 op —— on/off 开关
//
//   这些规则会随世界存档保存（LevelData），进世界后真正生效。
struct CreateWorldScreen : SelectWorldScreen, IWorldRulesHost {
  Touch::THeader *bHeader;
  Touch::TButton *bBack;         // 左上：返回
  Touch::TButton *bCreate;       // 右上：创建世界（ok）
  OptionsPane *pane;             // 可滚动列表
  TextBox081 *field_144;         // 世界名
  TextBox081 *field_148;         // 种子
  Touch::TButton *bGameMode;     // 创造 / 生存
  Touch::TButton *bWorldType;    // 经典 / 无限
  Touch::TButton *bDifficulty;   // 和平 / 简单 / 普通 / 困难
  WorldRuleToggleButton *toggles[7];
  CreateWorldScreenType field_16C;
  MCOServerListItem field_170;
  std::shared_ptr<RestRequestJob> field_1B8;

  bool creative;      // 当前选的游戏模式
  int  worldType;     // WorldType::*
  WorldRules rules;   // 当前世界规则

  CreateWorldScreen(CreateWorldScreenType, const MCOServerListItem &);
  void closeScreen();
  void generateLocalGame();
  void generateMCOGame(bool_t);
  std::string getLevelName();
  int32_t getSeed();
  bool isCreative();

  // 点一下切一档
  virtual void cycleGameMode();
  virtual void cycleWorldType();
  virtual void cycleDifficulty();
  // 开关（索引见 CreateWorldScreen.cpp 的 RULE_* 常量）
  virtual bool getRuleValue(int index);
  virtual void toggleRule(int index);
  void refreshButtonTexts();

  virtual ~CreateWorldScreen();
  virtual void render(int32_t, int32_t, float);
  virtual void init();
  virtual void setupPositions();
  virtual bool handleBackEvent(bool);
  virtual void tick();
  virtual void feedMCOEvent(MCOEvent);
  virtual void setTextboxText(const std::string &);
  virtual void buttonClicked(Button *);
  virtual void mouseClicked(int32_t, int32_t, int32_t);
  virtual void mouseReleased(int32_t, int32_t, int32_t);
  virtual void keyPressed(int32_t);
  virtual void keyboardNewChar(const std::string &, bool_t);
  // 0.8.1 GUI 移植：基类 keyboardText 逐字符调用 char 版本，
  // 必须覆盖它字符才能进创建向导（否则名称/种子输入框收不到输入）
  virtual void keyboardNewChar(char inputChar);
  // 名称/种子输入框在列表里（不在 textBoxes），必须声明有文本输入，
  // 否则 Win32 层不激活 IME、不生成字符 → 无法输入
  virtual bool hasTextInput() const { return true; }
  // 与语言界面/设置页一致：背景是主界面那个 6 图全景（renderMenuBackground 画的），
  // 覆盖成 true 才不会用泥土背景把全景盖掉
  virtual bool renderGameBehind() { return true; }
};
