#include <Minecraft.hpp>
#include <I18n.hpp>
#include <cpputils.hpp>
#include <gui/NinePatchFactory.hpp>
#include <gui/buttons/ImageButton.hpp>
#include <gui/buttons/Touch_TButton.hpp>
#include <gui/elements/Label.hpp>
#include <gui/elements/TextBox081.hpp>
#include <gui/screens/CreateWorldScreen.hpp>
#include <gui/screens/PlayScreen.hpp>
#include <gui/screens/ProgressScreen.hpp>
#include <gui/screens/WorldRulesWidgets.hpp>
#include <level/LevelSettings.hpp>
#include <level/storage/LevelStorageSource.hpp>
#include <rendering/Tesselator.hpp>
#include <util/IntRectangle.hpp>
#include <util/ParameterStringify.hpp>
#include <util/Util.hpp>
#include <utils.h>
#include <stdio.h>

#include "../../../client/gui/components/OptionsPane.h"
#include "../../../client/gui/components/OptionsItem.h"
#include "../../../client/renderer/gles.h"

// 往滚动列表里加一行（左侧文字 + 右侧控件）。
// 0.8.1 的 OptionsItem 行高在 setupPositions 里算，必须先调一次，
// OptionsPane 才能排出行距（语言界面同款做法）。
static void addOptionRow(OptionsPane* pane, const std::string& label, GuiElement* element) {
  if (!pane || !element) return;
  OptionsItem* item = new OptionsItem(label, element);
  item->setupPositions();
  pane->addChild(item);
}

static std::string difficultyLabel(int d) {
  switch (d) {
  case 0: return I18n::get("options.difficulty.peaceful");
  case 1: return I18n::get("options.difficulty.easy");
  case 3: return I18n::get("options.difficulty.hard");
  default: return I18n::get("options.difficulty.normal");
  }
}

CreateWorldScreen::CreateWorldScreen(CreateWorldScreenType a2,
                                     const MCOServerListItem &a3)
    : SelectWorldScreen(), field_16C(a2), field_170(a3) {
  this->bHeader = 0;
  this->bBack = 0;
  this->bCreate = 0;
  this->pane = 0;
  this->field_144 = 0;
  this->field_148 = 0;
  this->bGameMode = 0;
  this->bWorldType = 0;
  this->bDifficulty = 0;
  for (int i = 0; i < RULE_COUNT; ++i) this->toggles[i] = 0;
  this->creative = true;
  this->worldType = WorldType::Old;
}

void CreateWorldScreen::closeScreen() {
  CreateWorldScreenType v2 = this->field_16C;
  Minecraft *minecraft = this->minecraft;
  if (v2 == WST_MCOGAME_RECREATE) {
    // MCO(Realms) 未移植
    printf("CreateWorldScreen::waitForMCO - MCO not implemented\n");
    minecraft->setScreen(new PlayScreen(0));
  } else {
    minecraft->setScreen(new PlayScreen(v2 == WST_LOCALGAME));
  }
}

std::string CreateWorldScreen::getLevelName() {
  std::string t = *this->field_144->getText();
  return t.empty() ? I18n::get("createWorld.defaultName") : t;
}

int32_t CreateWorldScreen::getSeed() {
  std::string text = *this->field_148->getText();
  if (text.size() <= 1) {
    return getEpochTimeS();
  }
  std::string s = Util081::stringTrim(text);
  if (s.size() == 0) {
    return getEpochTimeS();
  }
  int32_t v7;
  if (sscanf(s.c_str(), "%d", &v7) <= 0) {
    return Util081::hashCode(s);
  }
  return v7;
}

bool CreateWorldScreen::isCreative() {
  return this->creative;
}

void CreateWorldScreen::refreshButtonTexts() {
  if (this->bGameMode)
    this->bGameMode->setMsg(this->creative ? I18n::get("createWorld.mode.creative")
                                           : I18n::get("createWorld.mode.survival"));
  if (this->bWorldType)
    this->bWorldType->setMsg(this->worldType == WorldType::Infinite
                                 ? I18n::get("createWorld.type.infinite")
                                 : I18n::get("createWorld.type.classic"));
  if (this->bDifficulty)
    this->bDifficulty->setMsg(difficultyLabel(this->rules.difficulty));
}

void CreateWorldScreen::cycleGameMode() {
  this->creative = !this->creative;
  this->refreshButtonTexts();
}

void CreateWorldScreen::cycleWorldType() {
  this->worldType = (this->worldType == WorldType::Old) ? WorldType::Infinite : WorldType::Old;
  this->refreshButtonTexts();
}

void CreateWorldScreen::cycleDifficulty() {
  int d = this->rules.difficulty;
  if (d < 0 || d > 3) d = 1;
  this->rules.difficulty = (d + 1) % 4;
  this->refreshButtonTexts();
}

bool CreateWorldScreen::getRuleValue(int index) {
  switch (index) {
  case RULE_DAYLIGHT: return this->rules.daylightCycle;
  case RULE_KEEP_INV: return this->rules.keepInventory;
  case RULE_RESPAWN: return this->rules.immediateRespawn;
  case RULE_WAKE_UP: return this->rules.instantWakeUp;
  case RULE_SPAWN_MOBS: return this->rules.spawnMobs;
  case RULE_SPAWN_ENEMIES: return this->rules.spawnEnemies;
  case RULE_AUTO_OP: return this->rules.autoOp;
  }
  return false;
}

void CreateWorldScreen::toggleRule(int index) {
  switch (index) {
  case RULE_DAYLIGHT: this->rules.daylightCycle = !this->rules.daylightCycle; break;
  case RULE_KEEP_INV: this->rules.keepInventory = !this->rules.keepInventory; break;
  case RULE_RESPAWN: this->rules.immediateRespawn = !this->rules.immediateRespawn; break;
  case RULE_WAKE_UP: this->rules.instantWakeUp = !this->rules.instantWakeUp; break;
  case RULE_SPAWN_MOBS: this->rules.spawnMobs = !this->rules.spawnMobs; break;
  case RULE_SPAWN_ENEMIES: this->rules.spawnEnemies = !this->rules.spawnEnemies; break;
  case RULE_AUTO_OP: this->rules.autoOp = !this->rules.autoOp; break;
  default: break;
  }
}

void CreateWorldScreen::generateLocalGame() {
  this->minecraft->getLevelSource()->getLevelList(this->field_50);
  std::string text(*this->field_144->getText());
  if (text == "")
    text = I18n::get("createWorld.defaultName");
  std::string ret = this->getUniqueLevelName(text);

  int32_t genType = (this->worldType == WorldType::Infinite) ? WorldType::Infinite : WorldType::Old;

  WorldRules r = this->rules;
  if (r.difficulty < 0) r.difficulty = this->minecraft->options.difficulty;
  // 难度同时写进玩家全局选项（创建后游戏内难度 = 这里选的）
  this->minecraft->options.difficulty = r.difficulty;

  this->minecraft->selectLevel(
      ret, text,
      LevelSettings{this->getSeed(),
                    this->creative ? GameType::Creative : GameType::Survival,
                    genType, r});
  this->minecraft->hostMultiplayer(19132);

  std::string v16;
  if (this->field_148->text == "") {
    std::string v21 = "{\"%\": \"%\", \"%\": \"%\"}";
    const char *v11 = this->creative ? "creative" : "survival";
    std::string v18 = this->field_148->text;
    std::vector<std::string> v24;
    v24.push_back("game_type");
    v24.push_back(v11);
    v24.push_back("seed");
    v24.push_back(v18);
    v16 = Util081::simpleFormat(v21, v24);
  } else {
    std::string v21 = "{\"%\": \"%\"}";
    const char *v11 = this->creative ? "creative" : "survival";
    std::vector<std::string> v24;
    v24.push_back("game_type");
    v24.push_back(v11);
    v16 = Util081::simpleFormat(v21, v24);
  }
  this->minecraft->platform()->statsTrackData("create_world", v16);
  // setScreen 会 delete 当前屏，必须放在最后（之后不能再访问 this）
  this->minecraft->setScreen(new ProgressScreen());
}

void CreateWorldScreen::generateMCOGame(bool_t) {
  printf("CreateWorldScreen::generateMCOGame - not implemented\n");
}

CreateWorldScreen::~CreateWorldScreen() {
  if (this->bHeader) { delete this->bHeader; this->bHeader = 0; }
  if (this->bBack) { delete this->bBack; this->bBack = 0; }
  if (this->bCreate) { delete this->bCreate; this->bCreate = 0; }
  // 列表里的控件（输入框/按钮）归 pane 所有，pane 析构会一并删除
  if (this->pane) {
    delete this->pane;
    this->pane = 0;
  }
}

void CreateWorldScreen::init() {
  if (this->rules.difficulty < 0)
    this->rules.difficulty = this->minecraft->options.difficulty;
  if (this->rules.difficulty < 0 || this->rules.difficulty > 3)
    this->rules.difficulty = 1;

  this->bHeader = new Touch::THeader(0, I18n::get("createWorld.title"));
  this->bBack = new Touch::TButton(1, I18n::get("gui.back"), this->minecraft);
  this->bBack->width = 38;
  this->bBack->height = 18;
  this->bCreate = new Touch::TButton(2, I18n::get("createWorld.create"), this->minecraft);
  this->bCreate->width = 76;
  this->bCreate->height = 18;

  this->buttons.push_back(this->bHeader);
  this->buttons.push_back(this->bBack);
  this->buttons.push_back(this->bCreate);

  this->pane = new OptionsPane();

  const char *extAscii = TextBox081::extendedAcsii
                             ? TextBox081::extendedAcsii
                             : " !\"#$%&\'()*+,-./"
                               "0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]"
                               "^_`abcdefghijklmnopqrstuvwxyz{|}~";

  // 第 1 行：世界名
  this->field_144 = new TextBox081(this->minecraft, I18n::get("createWorld.name"),
                                   16, extAscii, strlen(extAscii), 0, 0, 0, 0);
  this->field_144->text = this->field_170.worldName;
  this->field_144->width = 120;
  addOptionRow(this->pane, I18n::get("createWorld.name"), this->field_144);

  // 第 2 行：种子
  this->field_148 = new TextBox081(this->minecraft, I18n::get("createWorld.seed"),
                                   32, extAscii, strlen(extAscii), 0, 0, 0, 0);
  this->field_148->width = 120;
  addOptionRow(this->pane, I18n::get("createWorld.seed"), this->field_148);

  // 第 3 行：世界模式（创造/生存，点一下切换）
  this->bGameMode = new WorldRuleActionButton(10, "", this->minecraft, this, ACTION_GAME_MODE);
  this->bGameMode->width = 66;
  addOptionRow(this->pane, I18n::get("createWorld.gameMode"), this->bGameMode);

  // 第 4 行：世界类型（经典/无限）
  this->bWorldType = new WorldRuleActionButton(11, "", this->minecraft, this, ACTION_WORLD_TYPE);
  this->bWorldType->width = 66;
  addOptionRow(this->pane, I18n::get("createWorld.worldType"), this->bWorldType);

  // 第 5 行：难度（和平/简单/普通/困难）
  this->bDifficulty = new WorldRuleActionButton(12, "", this->minecraft, this, ACTION_DIFFICULTY);
  this->bDifficulty->width = 66;   // 与其他切档按钮同宽（对齐）
  addOptionRow(this->pane, I18n::get("createWorld.difficulty"), this->bDifficulty);

  // 后面的开关行
  static const char *ruleLabelKeys[RULE_COUNT] = {
      "createWorld.rule.daylight",
      "createWorld.rule.keepInventory",
      "createWorld.rule.immediateRespawn",
      "createWorld.rule.instantWakeUp",
      "createWorld.rule.spawnMobs",
      "createWorld.rule.spawnEnemies",
      "createWorld.rule.autoOp",
  };
  for (int i = 0; i < RULE_COUNT; ++i) {
    this->toggles[i] = new WorldRuleToggleButton(20 + i, this, i);
    addOptionRow(this->pane, I18n::get(ruleLabelKeys[i]), this->toggles[i]);
  }

  this->refreshButtonTexts();
}

void CreateWorldScreen::setupPositions() {
  this->bHeader->x = 0;
  this->bHeader->y = 0;
  this->bHeader->width = this->width;
  this->bHeader->height = this->bBack->height + 8;
  this->bBack->x = 4;
  this->bBack->y = 4;
  this->bCreate->x = this->width - this->bCreate->width - 4;
  this->bCreate->y = 4;

  if (this->pane) {
    this->pane->x = 8;
    this->pane->y = this->bHeader->height + 3;
    this->pane->width = this->width - 16;
    this->pane->setupPositions();
  }
}

void CreateWorldScreen::render(int32_t xm, int32_t ym, float a) {
  // 与语言界面一致：背景 = 主界面那个全景（renderMenuBackground 画），
  // 这里先把混合/贴图状态摆好，否则全景画不出来
  glEnable2(GL_BLEND);
  glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable2(GL_TEXTURE_2D);

  // 输入框聚焦时只画输入框（全屏，其余交给 TextBox081::topRender）
  if (this->pane && this->pane->suppressOtherGUI()) {
    this->renderBackground(0);
    this->pane->topRender(this->minecraft, xm, ym);
    return;
  }

  this->renderMenuBackground(a);
  this->renderBackground(0);
  Screen::render(xm, ym, a);
  if (this->pane)
    this->pane->render(this->minecraft, xm, ym - 1);
}

void CreateWorldScreen::tick() {
  if (this->pane)
    this->pane->tick(this->minecraft);
}

void CreateWorldScreen::feedMCOEvent(MCOEvent) {}

void CreateWorldScreen::setTextboxText(const std::string &a2) {
  if (this->pane)
    this->pane->setTextboxText(a2);
}

bool CreateWorldScreen::handleBackEvent(bool a2) {
  if (!a2) {
    if (this->pane && this->pane->suppressOtherGUI()) {
      this->pane->backPressed(this->minecraft, 0);
      return 1;
    }
    this->closeScreen();
  }
  return 1;
}

void CreateWorldScreen::buttonClicked(Button *a2) {
  if (a2 == this->bBack) {
    this->closeScreen();
    return;
  }
  if (a2 == this->bCreate) {
    switch (this->field_16C) {
    case WST_LOCALGAME:
      this->generateLocalGame();
      return;
    case WST_MCOGAME_NEW:
      this->generateMCOGame(0);
      return;
    case WST_MCOGAME_RECREATE:
      this->generateMCOGame(1);
      return;
    default:
      return;
    }
  }
}

void CreateWorldScreen::mouseClicked(int32_t a2, int32_t a3, int32_t a4) {
  if (this->pane && this->pane->suppressOtherGUI()) {
    this->pane->focusuedMouseClicked(this->minecraft, a2, a3, a4);
    return;
  }
  if (this->pane)
    this->pane->mouseClicked(this->minecraft, a2, a3, a4);
  Screen::mouseClicked(a2, a3, a4);
}

void CreateWorldScreen::mouseReleased(int32_t a2, int32_t a3, int32_t a4) {
  if (this->pane && this->pane->suppressOtherGUI()) {
    this->pane->focusuedMouseReleased(this->minecraft, a2, a3, a4);
    return;
  }
  if (this->pane)
    this->pane->mouseReleased(this->minecraft, a2, a3, a4);
  Screen::mouseReleased(a2, a3, a4);
}

void CreateWorldScreen::keyPressed(int32_t a2) {
  if (this->pane)
    this->pane->keyPressed(this->minecraft, a2);
  Screen::keyPressed(a2);
}

void CreateWorldScreen::keyboardNewChar(const std::string &a2, bool_t a3) {
  if (this->pane)
    this->pane->keyboardNewChar(this->minecraft, a2, a3);
}

// 0.8.1 GUI 移植：基类 keyboardText 逐字符调用 char 版本（utf8 逐字节），
// 转发给 3 参版本（TextBox081 支持 UTF-8 逐字节拼接，中文可正常输入）
void CreateWorldScreen::keyboardNewChar(char inputChar) {
  std::string s(1, inputChar);
  if (this->pane)
    this->pane->keyboardNewChar(this->minecraft, s, false);
}
