#include "StartMenuScreen.h"
#include "InvalidLicenseScreen.h"
#include "OptionsScreen.h"
#include "JoinGameScreen.h"
#include "ScreenChooser.h"
#include "../Font.h"
#include "../../../client/renderer/Tesselator.h"
#include "../../../client/renderer/Textures.h"
#include "../../SharedConstants.h"
#include <cstdint>

// 0.8.1 GUI 移植：0.8.1 风格主菜单（替换 1.7.10 还原版）

StartMenuScreen::StartMenuScreen()
    : Screen(), startGameButton(2, 0, 0, 60, 24, "Start Game"),
      joinGameButton(3, 0, 0, 160, 24, "Join Game"),
      optionsButton(4, 0, 0, 78, 22, "Options"),
      createButton(999, 0, 0, 78, 22, "Create"),
      buyButton(5, 0, 0, 78, 22, "Buy") {
  this->mojangABMaybe = "";
  this->gameVersion = "";
}

void StartMenuScreen::_updateLicense() {
  int32_t lide;         // r0
  int32_t v3;           // r6
  bool v4;              // r3
  int8_t v6;            // r0
  Minecraft *minecraft; // r7
  int8_t v8;            // r8

  lide = this->minecraft->getLicenseId();
  v3 = lide;
  if (lide < 0) {
    v4 = 0;
  } else {
    if ((uint32_t)lide > 1) {
      v6 = this->minecraft->platform()->hasBuyButtonWhenInvalidLicense();
      minecraft = this->minecraft;
      v8 = v6;
      minecraft->setScreen(new InvalidLicenseScreen(v3, v8));
      return;
    }
    v4 = 1;
  }
  this->optionsButton.active = v4;
  this->startGameButton.active = v4;
  this->joinGameButton.active = v4;
}

StartMenuScreen::~StartMenuScreen() {}
void StartMenuScreen::render(int32_t a2, int32_t a3, float a4) {
  TextureData *td; // r5
  float width;     // s15
  float v12;       // s17
  float v13;       // s16
  float v14;       // s15
  float height;    // s14
  float v16;       // s19
  float v17;       // s16

  // 0.8.1 用 renderMenuBackground（panorama/霓虹背景）；0.6.1 先走经典背景
  this->renderMenuBackground(a4);
  td = this->minecraft->textures->loadAndGetTextureData("gui/title.png");
  if (td) {
    this->minecraft->textures->loadAndBindTexture("gui/title.png");
    width = (float)td->w;
    v12 = (float)this->width * 0.5;
    v13 = width * 0.5;
    if (v12 <= (float)(width * 0.5)) {
      v13 = (float)this->width * 0.5;
    }
    v14 = (float)(v13 + v13) / width;
    height = (float)td->h;
    glColor4f(1.0, 1.0, 1.0, 1.0);
    Tesselator::instance.begin(GL_QUADS);
    v16 = v12 - v13;
    Tesselator::instance.vertexUV(v12 - v13, (float)(v14 * height) + 4.0,
                                  this->blitOffset, 0.0, 1.0);
    v17 = v12 + v13;
    Tesselator::instance.vertexUV(v17, (float)(v14 * height) + 4.0,
                                  this->blitOffset, 1.0, 1.0);
    Tesselator::instance.vertexUV(v17, 4.0, this->blitOffset, 1.0, 0.0);
    Tesselator::instance.vertexUV(v16, 4.0, this->blitOffset, 0.0, 0.0);
    Tesselator::instance.draw();
  }
  this->drawString(this->font, this->gameVersion, this->versionPosX, 62,
                   0xFFCCCCCC);
  this->drawString(this->font, this->mojangABMaybe, this->copyrightPosX,
                   this->height - 10, 0xFFFFFF);
  Screen::render(a2, a3, a4);
}
void StartMenuScreen::init() {
  this->buttons.emplace_back(&this->startGameButton);
  this->buttons.emplace_back(&this->joinGameButton);
  this->tabButtons.emplace_back(&this->startGameButton);
  this->tabButtons.emplace_back(&this->joinGameButton);
  this->buttons.emplace_back(&this->optionsButton);
  this->tabButtons.emplace_back(&this->optionsButton);
  this->mojangABMaybe = "\xFFMojang AB";
  this->gameVersion = Common::getGameVersionString();
  this->optionsButton.active = 0;
  this->startGameButton.active = 0;
  this->joinGameButton.active = 0;
}
void StartMenuScreen::setupPositions() {
  int32_t width;        // r5
  int32_t v3;           // r3
  int32_t v4;           // r2
  int32_t v5;           // r3
  int32_t v6;           // r2
  int32_t v7;           // r3
  int32_t v8;           // r3
  int32_t v9;           // r0
  Minecraft *minecraft; // r3
  int32_t v11;          // r5

  width = this->width;
  v3 = this->height / 2;
  this->startGameButton.y = v3 - 3;
  v4 = v3 + 25;
  v3 += 55;
  this->optionsButton.y = v3;
  this->buyButton.y = v3;
  this->createButton.y = v3;
  v5 = this->startGameButton.width;
  this->joinGameButton.y = v4;
  v6 = this->optionsButton.width;
  this->startGameButton.x = (width - v5) / 2;
  v7 = (width - this->joinGameButton.width) / 2;
  this->joinGameButton.x = v7;
  this->optionsButton.x = v7;
  v8 = v7 + v6 + 4;
  this->buyButton.x = v8;
  this->createButton.x = v8;
  v9 = this->minecraft->font->width(this->mojangABMaybe);
  minecraft = this->minecraft;
  this->copyrightPosX = width - v9 - 1;
  v11 = this->width;
  this->versionPosX = (v11 - minecraft->font->width(this->gameVersion)) / 2;
}
bool StartMenuScreen::handleBackEvent(bool a2) {
  (void)a2;
  this->minecraft->quit();
  return true;
}
void StartMenuScreen::tick() { this->_updateLicense(); }
bool StartMenuScreen::isInGameScreen() { return false; }
void StartMenuScreen::buttonClicked(Button *a2) {
  if (a2->id == this->startGameButton.id) {
    this->minecraft->screenChooser.setScreen(SCREEN_SELECTWORLD);
  }
  if (a2->id == this->joinGameButton.id) {
    this->minecraft->locateMultiplayer();
    this->minecraft->setScreen(new JoinGameScreen());
  }
  if (a2->id == this->optionsButton.id) {
    this->minecraft->setScreen(new OptionsScreen());
  }
  if (a2->id == this->buyButton.id) {
    this->minecraft->platform()->buyGame();
  }
}
