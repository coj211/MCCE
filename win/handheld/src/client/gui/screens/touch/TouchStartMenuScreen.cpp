#include "TouchStartMenuScreen.h"
#include "TouchSelectWorldScreen.h"
#include "../InvalidLicenseScreen.h"
#include "../OptionsScreen.h"
#include "../LanguageScreen.h"
#include "../JoinGameScreen.h"
#include "../ModsScreen.h"
// 0.8.1 GUI 移植：主菜单 Play → 0.8.1 游戏大厅（顶部 Back/New/External/Edit）
#include "../../../../gui08/gui/screens/PlayScreen.hpp"
#include "../../../Minecraft.h"
#include "../../Font.h"
#include "../../../renderer/Tesselator.h"
#include "../../../renderer/Textures.h"
#include "../../../renderer/TextureData.h"
#include "../../../../locale/I18n.h"
#include "../../../../SharedConstants.h"
// 09 · UI 覆盖系统：主菜单标题可被 mod 替换
#include "../../../../mod/ModEngine.h"
#include "../../../../util/Random.h"
#include <gui/buttons/ImageWithBackground.hpp>
#include <math.h>
#include <cstdint>
// 皮肤纸娃娃无实体渲染
#include "../../../model/HumanoidModel.h"
#include "../../../renderer/entity/EntityRenderDispatcher.h"
#include "../../../renderer/entity/HumanoidMobRenderer.h"
#include "../../../../util/Mth.h"
#include "../../../../platform/time.h"

// 0.8.1 GUI 移植：0.8.1 风格触摸主菜单（替换 1.7.10 还原版）
namespace Touch {
#include "gSplashes.inc"
}

int32_t Touch::StartMenuScreen::currentSplash = -1;

Touch::StartMenuScreen::StartMenuScreen()
    : playButton(2, "游戏", 0), playOnRealmsButton(3, "Play on Realms", 0),
      marketplaceButton(6, "Texture Packs", 0), buyButton(5),
      modButton(7, "模组", 0) {
  this->settingsButtonMaybe = 0;
  this->languageButton = 0;
  this->field_140 = 0;
  this->field_148 = 0;
  this->field_14C = 1;
  this->field_144 = 0;
  this->field_150 = 0;
  // 皮肤纸娃娃: 无实体渲染(主菜单无 world/player)
  this->_dollCenterX = 0;
  this->_dollCenterY = 0;
  this->_dollW = 96;
  this->_dollH = 170;
  this->_dollYaw = 0;
  this->_dollPitch = 0;
  this->_dollLastTime = 0;
  this->_dollHasPointer = false;
}

void Touch::StartMenuScreen::_updateLicense() {
  bool v4;
  int32_t licenseId = this->minecraft->getLicenseId();
  if (licenseId < 0)
    v4 = 0;
  else {
    if ((uint32_t)licenseId > 1) {
      this->minecraft->setScreen(new InvalidLicenseScreen(
          licenseId,
          this->minecraft->platform()->hasBuyButtonWhenInvalidLicense()));
      return;
    }
    v4 = 1;
  }
  this->playOnRealmsButton.active = v4;
  this->playButton.active = v4;
}

void Touch::StartMenuScreen::chooseRandomSplash() {
  Random rng;
  int32_t splashcnt = 0;
  while (gSplashes[splashcnt])
    ++splashcnt;
  Touch::StartMenuScreen::currentSplash = rng.nextInt(splashcnt);
}
void Touch::StartMenuScreen::setupPlayButtons(bool a2) {
  int32_t width, height;
  int32_t v4, v5, v6;
  width = this->width;
  this->playButton.width = 100;
  this->playButton.height = 30;
  height = this->height;
  v4 = width / 2 - 50;
  this->playButton.x = v4;
  v5 = height / 2;
  if (a2)
    v6 = v5 - 15;
  else
    v6 = v5 + 10;

  this->playButton.y = v6;
  this->playOnRealmsButton.width = 100;
  this->playOnRealmsButton.x = v4;
  this->playOnRealmsButton.height = 30;
  this->playOnRealmsButton.y = v6 + 40;
}

Touch::StartMenuScreen::~StartMenuScreen() {
  if (this->settingsButtonMaybe) {
    delete this->settingsButtonMaybe;
    this->settingsButtonMaybe = 0;
  }
  if (this->languageButton) {
    delete this->languageButton;
    this->languageButton = 0;
  }
}

void Touch::StartMenuScreen::render(int32_t a2, int32_t a3, float a4) {
  // 换肤延迟应用: 鼠标点击只记录路径; 在这里(渲染准备阶段、任何 GUI 绘制
  // 之前)应用 —— 避免在 mouseClicked 里做 GL 纹理替换破坏渲染状态
  // (症状: 存档列表消失/顶栏错/mod 开关贴图错)。
  if (!_pendingSkinPath.empty() && this->minecraft && this->minecraft->modEngine) {
    std::string p = _pendingSkinPath;
    _pendingSkinPath.clear();
    this->minecraft->modEngine->applyPlayerSkinFromFile(p);
  }
  // 0.6.1 无 Realms（mojangConnector）——恒隐藏 Realms 按钮
  bool mco = false;
  this->playOnRealmsButton.setActiveAndVisibility(mco);
  // 位置统一由 setupPositions() 设置（2x2 按钮组整体居中，resize 时重调）
  // 09b · 主菜单背景可被 UI.setImage("mainmenu.background", png) 换成静态整图
  // （铺满全屏，替代旋转全景背景）。mod 也可用 Assets.replaceImage 换
  // gui/background/panorama_0..5.png 六面保持旋转。
  {
    unsigned int bgTex = 0;
    if (this->minecraft->modEngine) {
      ModEngine::UiElementOverride bgO;
      if (this->minecraft->modEngine->uiQuery("mainmenu", "background", bgO) &&
          bgO.hasImage && !bgO.imagePath.empty())
        bgTex = this->minecraft->modEngine->getModTexture(bgO.imagePath);
    }
    if (bgTex) {
      glEnable2(GL_TEXTURE_2D);
      glEnable2(GL_BLEND);
      glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glBindTexture(GL_TEXTURE_2D, bgTex);
      glColor4f(1, 1, 1, 1);
      Tesselator::instance.begin(GL_QUADS);
      Tesselator::instance.color(0xffffffff);
      Tesselator::instance.vertexUV(0, (float)this->height, 0, 0, 1);
      Tesselator::instance.vertexUV((float)this->width, (float)this->height, 0, 1, 1);
      Tesselator::instance.vertexUV((float)this->width, 0, 0, 1, 0);
      Tesselator::instance.vertexUV(0, 0, 0, 0, 0);
      Tesselator::instance.draw();
    } else {
      this->renderMenuBackground(a4);
    }
  }
  // 09 · UI 覆盖：mainmenu.title 可被 UI.setImage 换成 mod 整图
  unsigned int modTitleTex = 0;
  if (this->minecraft->modEngine) {
    ModEngine::UiElementOverride titleO;
    if (this->minecraft->modEngine->uiQuery("mainmenu", "title", titleO) &&
        titleO.hasImage && !titleO.imagePath.empty())
      modTitleTex = this->minecraft->modEngine->getModTexture(titleO.imagePath);
  }
  if (modTitleTex)
    glBindTexture2(GL_TEXTURE_2D, modTitleTex);
  else
    this->minecraft->textures->loadAndBindTexture("gui/title.png");
  glColor4f(1, 1, 1, 1);
  Tesselator::instance.begin(GL_QUADS);

  int32_t v12 = this->field_140;
  int32_t v13 = this->field_144;
  float v14 = v12;
  float v15 = v12 + this->field_148;
  float v16 = v13 + this->field_14C;
  Tesselator::instance.vertexUV(v12, v16, this->blitOffset, 0, 1);
  Tesselator::instance.vertexUV(v15, v16, this->blitOffset, 1, 1);
  Tesselator::instance.vertexUV(v15, v13, this->blitOffset, 1, 0);
  Tesselator::instance.vertexUV(v14, v13, this->blitOffset, 0, 0);
  Tesselator::instance.draw();
  this->drawString(this->font, this->field_138, 1, this->height - 10, 0xffffff);
  this->drawString(this->font, this->field_13C, 1, this->height - 20, 0xffffff);
  if (Touch::StartMenuScreen::currentSplash == -1) {
    Touch::StartMenuScreen::chooseRandomSplash();
  }
  // 0.6.1 无 field_D34（引擎时间）——用 tick 计数折算（20 tick/s）
  // 0.6.1 无 field_D34——按帧增量模拟（约 60fps）
  double v17 = this->field_150 + 0.016;
  this->field_150 = v17;

  this->font->drawTransformed(
      gSplashes[Touch::StartMenuScreen::currentSplash], v15 * 0.71, v16 - 15,
      0xFFFFFF00, -20, (float)(powf(sin(v17 * 3.14 * 2.3), 4.0) * 0.06) + 1.3,
      1, (float)this->width * 0.3125);
  Screen::render(a2, a3, a4);
  // 皮肤纸娃娃：绘制在四按钮组右侧, 跟随指针(鼠标/触摸)看向指向处
  this->renderSkinDoll((float)a2, (float)a3);
}
void Touch::StartMenuScreen::init() {
  // 语言感知：主界面按钮文案随当前语言（0.8.1 界面在语言切换后重建进入，
  // 这里在 init 时从 I18n 取词即可生效）。
  this->playButton.setMsg(I18n::get("mainmenu.game"));
  this->modButton.setMsg(I18n::get("mods.title"));
  this->playButton.init(this->minecraft);
  this->playOnRealmsButton.init(this->minecraft);
  this->settingsButtonMaybe = new ImageWithBackground(4);
  IntRectangle v8, v12;
  ImageDef v13;
  v12.w = 8;
  v12.h = 0x43;
  v12.x = 112;
  v12.y = 0;
  v8.x = 0x78;
  v8.y = 0;
  v8.w = 8;
  v8.h = 0x43;
  this->settingsButtonMaybe->init(this->minecraft->textures, 32, 32, v12, v8,
                                  2, 2, "gui/spritesheet.png");
  // 0.6.1 的 touchgui.png 图集与 0.8.1 不同（218,0 处不是扳手），
  // 用从 0.8.1 touchgui.png 提取的独立扳手图
  // 图标绘制区域 = 按钮 32x32（settings_wrench.png 全图，扳手位于画布中心）
  v13.hasSubImage = 1;
  v13.name = "gui/settings_wrench.png";
  v13.x = 0;
  v13.y = 0;
  v13.width = 32;
  v13.v = 0;
  v13.height = 32;
  v13.u = 0;
  v13.subW = 32;
  v13.subH = 32;
  this->settingsButtonMaybe->setImageDef(v13, 0);
  this->settingsButtonMaybe->width = 32;
  this->settingsButtonMaybe->height = 32;
  // Mod 按钮：与 Play 同尺寸（用户定制）
  this->modButton.init(this->minecraft);
  this->modButton.width = this->playButton.width;
  this->modButton.height = this->playButton.height;
  // 语言按钮：与设置键同尺寸 32x32，图标用 language_icon.png（用户 Selection.png 转制）
  this->languageButton = new ImageWithBackground(8);
  this->languageButton->init(this->minecraft->textures, 32, 32, v12, v8, 2, 2, "gui/spritesheet.png");
  {
    ImageDef langDef;
    langDef.hasSubImage = 1;
    langDef.name = "gui/language_icon.png";
    langDef.x = 0;
    langDef.y = 0;
    langDef.width = 32;
    langDef.height = 32;
    langDef.v = 0;
    langDef.u = 0;
    langDef.subW = 32;
    langDef.subH = 32;
    this->languageButton->setImageDef(langDef, 0);
  }
  this->languageButton->width = 32;
  this->languageButton->height = 32;
  this->marketplaceButton.init(this->minecraft);
  this->marketplaceButton.setMsg("Texture Packs");
  // 0.6.1 无 marketplace——隐藏
  this->marketplaceButton.active = false;
  this->marketplaceButton.visible = false;
  this->buttons.emplace_back(&this->marketplaceButton);
  this->tabButtons.emplace_back(&this->marketplaceButton);
  this->buttons.emplace_back(&this->playButton);
  this->buttons.emplace_back(&this->playOnRealmsButton);
  this->buttons.emplace_back(this->settingsButtonMaybe);
  this->buttons.emplace_back(&this->modButton);
  this->buttons.emplace_back(this->languageButton);
  this->tabButtons.emplace_back(&this->playButton);
  this->tabButtons.emplace_back(&this->playOnRealmsButton);
  this->tabButtons.emplace_back(this->settingsButtonMaybe);
  this->tabButtons.emplace_back(&this->modButton);
  this->tabButtons.emplace_back(this->languageButton);
  this->field_138 = "\x0fMojang AB";
  this->field_13C = Common::getGameVersionString();
  this->playOnRealmsButton.active = 0;
  this->playButton.active = 0;
}

void Touch::StartMenuScreen::setupPositions() {
  TextureData *data =
      this->minecraft->textures->loadAndGetTextureData("gui/title.png");
  int32_t tw = data ? data->w : 0;
  int32_t th = data ? data->h : 0;
  // 09 · UI 覆盖：标题被 mod 整图替换时按 mod 图尺寸布局（不会变形）
  if (this->minecraft->modEngine) {
    ModEngine::UiElementOverride titleO;
    if (this->minecraft->modEngine->uiQuery("mainmenu", "title", titleO) &&
        titleO.hasImage && !titleO.imagePath.empty()) {
      int mw = 0, mh = 0;
      if (this->minecraft->modEngine->getModTextureSize(titleO.imagePath, mw, mh) && mw > 0 && mh > 0) {
        tw = mw;
        th = mh;
      }
    }
  }
  if (tw > 0 && th > 0) {
    int32_t width = this->width;
    float v8 = (float)tw;
    int32_t td_height = th;
    this->field_144 = 12;
    float v10 = v8 * 0.5f;
    if (width * 0.5 <= v8 * 0.5) {
      v10 = width * 0.5f;
    }
    this->field_148 = (int)(v10 + v10);
    this->field_140 = (int)((float)(width / 2) - v10);
    this->field_14C = (int)(((float)(v10 + v10) / v8) * (float)td_height);
  }

  // 0.6.1 无 mojangConnector——Realms 恒隐藏
  bool mco = false;
  this->setupPlayButtons(mco);

  this->marketplaceButton.width = this->playButton.width;
  this->marketplaceButton.height = this->playButton.height;
  this->marketplaceButton.x = this->playButton.x;
  // 0.6.1 无 marketplace（Play 保持 setupPlayButtons 的屏幕中部位置，其他按钮以它为基准）
  this->marketplaceButton.y = -1000;
  this->marketplaceButton.visible = 0;
  this->marketplaceButton.active = 0;
  this->settingsButtonMaybe->height = 32;
  this->settingsButtonMaybe->width = 32;
  this->settingsButtonMaybe->x =
      this->width - this->settingsButtonMaybe->width - 2;
  this->settingsButtonMaybe->y =
      this->height - this->settingsButtonMaybe->height - 2;
  int32_t v16b = this->width;
  this->buyButton.y = this->height - this->buyButton.height - 3;
  this->buyButton.x = (v16b - this->buyButton.width) / 2;
  // 用户定制：2x2 按钮组整体居中（设置/游戏 / 语言/模组），间距紧凑
  const int gap = 3;   // 按钮间距（水平/垂直）
  const int iconW = 32, iconH = 32;
  int32_t groupW = iconW + gap + this->playButton.width;
  int32_t groupH = this->playButton.height + gap + this->playButton.height;
  int32_t gx = (this->width - groupW) / 2;
  int32_t gy = (this->height - groupH) / 2 + 20;  // 整体垂直居中后再向下 20px
  // 游戏（右上）
  this->playButton.x = gx + iconW + gap;
  this->playButton.y = gy;
  // 设置（左上，与游戏同行垂直居中）
  this->settingsButtonMaybe->width = iconW;
  this->settingsButtonMaybe->height = iconH;
  this->settingsButtonMaybe->x = gx;
  this->settingsButtonMaybe->y = gy + (this->playButton.height - iconH) / 2;
  // 模组（右下，紧挨游戏正下方）
  this->modButton.width = this->playButton.width;
  this->modButton.height = this->playButton.height;
  this->modButton.x = this->playButton.x;
  this->modButton.y = gy + this->playButton.height + gap;
  // 语言（左下，紧挨模组左边）
  this->languageButton->width = iconW;
  this->languageButton->height = iconH;
  this->languageButton->x = gx;
  this->languageButton->y =
      this->modButton.y + (this->modButton.height - iconH) / 2;

  // 皮肤纸娃娃：四按钮组右侧（视觉中心 x/y、交互判定区）
  this->_dollW = groupW + 120;  // 判定区宽(横跨按钮组右段至纸娃娃)
  this->_dollH = groupH + 140;  // 判定区高(上下外扩, 方便悬停)
  this->_dollCenterX = gx + groupW + (int)(iconW * 1.0f) + 58;
  this->_dollCenterY = gy + groupH / 2;
}
bool Touch::StartMenuScreen::handleBackEvent(bool) {
  this->minecraft->quit();
  return true;
}
void Touch::StartMenuScreen::tick() { this->_updateLicense(); }
bool Touch::StartMenuScreen::isInGameScreen() { return false; }

// 皮肤纸娃娃: 无实体渲染(主菜单无 world/player)。
// 视觉与 ArmorScreen::renderPlayer 一致: 同一 HumanoidModel + 同一 GL 序列,
// 仅把"玩家实体"替换为常量; 指针(鼠标/触摸)悬停跟随转向。
void Touch::StartMenuScreen::renderSkinDoll(float mouseX, float mouseY) {
  if (!this->minecraft) return;

  // 复用全局 PlayerRenderer 的人形模型(受 graphics reset 管理, 与盔甲纸娃娃同源)
  EntityRenderDispatcher* rd = EntityRenderDispatcher::getInstance();
  EntityRenderer* er = rd->getRenderer(ER_PLAYER_RENDERER);
  if (!er) return;
  HumanoidMobRenderer* hmr = (HumanoidMobRenderer*)er;
  HumanoidModel* model = hmr->getHumanoidModel();
  if (!model) return;

  float xo = (float)this->_dollCenterX;
  float yo = (float)this->_dollCenterY;

  // 平滑朝向(与 ArmorScreen 相同的交互逻辑)
  float t = getTimeS();
  if (this->_dollLastTime <= 0.0f) this->_dollLastTime = t;
  float dt = t - this->_dollLastTime;
  if (dt < 0.0f) dt = 0.0f;
  if (dt > 0.1f) dt = 0.1f;
  this->_dollLastTime = t;

  // 全屏跟随: 只要指针(鼠标/触摸)在主菜单屏幕内就持续看向指针方向,
  // 移出屏幕/无指针时才回正面微摆。避免"鼠标一出纸娃娃小区域就弹回"。
  bool over = (mouseX > 0.0f && mouseY > 0.0f) &&
              (mouseX < (float)this->width && mouseY < (float)this->height);
  if (mouseX <= 0.0f && mouseY <= 0.0f) over = false;

  float targetYaw, targetPitch;
  if (over) {
    // 偏移以"屏幕半宽/半高"归一化 → 鼠标到屏边即角度上限。
    // 横向 ±1 → ±60° 转身(不转到背后), 纵向 ±1 → ±20° 俯仰。
    float nx = (mouseX - xo) / ((float)this->width * 0.5f);
    float ny = (mouseY - yo) / ((float)this->height * 0.5f);
    if (nx < -1.0f) nx = -1.0f; else if (nx > 1.0f) nx = 1.0f;
    if (ny < -1.0f) ny = -1.0f; else if (ny > 1.0f) ny = 1.0f;
    targetYaw = -nx * 60.0f;	// 左右: 鼠标左→向左看 实测方向沿用
    targetPitch = ny * 20.0f;	// 上下: 鼠标下→向下看
  } else {
    targetYaw = 5.0f * Mth::sin(t);
    targetPitch = 5.0f * Mth::cos(t * 0.05f);
  }
  this->_dollHasPointer = over;

  float k = Mth::clamp(dt * 10.0f, 0.0f, 1.0f);
  this->_dollYaw += (targetYaw - this->_dollYaw) * k;
  this->_dollPitch += (targetPitch - this->_dollPitch) * k;

  // ---- GL 序列(ArmorScreen::renderPlayer 无实体版) ----
  float ss = 56.0f;  // 缩放: 比上一版(40)大 40%, 主菜单按钮组右侧展示
  glPushMatrix();
  // GUI 阶段(正交, 无深度)先画了卡片/按钮; 模型需要"自身部件正常前后遮挡",
  // 因此开启深度测试但先清空深度缓冲: 模型只跟自身比深度, 不被 GUI 深度淘汰,
  // 模型内部(head/body/arm 等)遮挡关系正确。
  GLint depthWasOn = glIsEnabled(GL_DEPTH_TEST);
  glClear(GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  glTranslatef(xo, yo + 0.9f * ss, -200);
  glScalef(-ss, ss, ss);
  glRotatef(180, 0, 0, 1);

  const float xtan = this->_dollYaw;
  const float ytan = this->_dollPitch;
  glRotatef(ytan, 1, 0, 0);

  // 复刻 MobRenderer::render(玩家, 站立): 常量代替实体字段
  glPushMatrix2();
  glDisable2(GL_CULL_FACE);
  model->attackTime = 0;
  model->riding = false;
  model->young = false;
  // 64x64 双层皮肤: 显示 outer 第二层(帽子/外套等); 老式 64x32 隐藏。
  model->setOuterVisible(ModEngine::instance != NULL && ModEngine::isPlayerSkin64());

  // setupPosition(mob, 0, 0 - heightOffset, 0) —— ArmorScreen 外层 +1.62 已抵消
  float bodyRot = xtan;
  float headRot = xtan + xtan;
  float headRotx = ytan;
  // setupRotations: 站立无死亡
  glRotatef2(180 - bodyRot, 0, 1, 0);
  float ascale = 1 / 16.0f;
  glScalef2(-1, -1, 1);
  glTranslatef2(0, -24 * ascale - 0.125f / 16.0f, 0);

  // walk 动画(同 ArmorScreen: 微弱呼吸)
  float ws = 0.25f;
  float wp = t * ws * (float)SharedConstants::TicksPerSecond;
  float bob = t * 20.0f;

  // 绑定皮肤纹理(默认玩家贴图; 皮肤系统就绪后可替换)
  this->minecraft->textures->loadAndBindTexture("mob/char.png");

  model->render(NULL, wp, ws, bob, headRot - bodyRot, headRotx, ascale);
  glEnable2(GL_CULL_FACE);
  glPopMatrix2();
  if (!depthWasOn) glDisable(GL_DEPTH_TEST);
  glPopMatrix();
}

// 点击纸娃娃可视区域(四按钮右侧) → 选皮肤。仅左键按下; 命中小人所在
// 的包围盒(比 _dollW/_dollH 判定区略收窄, 避免误触右侧空白)。
void Touch::StartMenuScreen::mouseClicked(int x, int y, int buttonNum) {
  if (buttonNum != 1 /*ACTION_LEFT*/) {  // MouseAction::ACTION_LEFT == 1
    Screen::mouseClicked(x, y, buttonNum);
    return;
  }
  // 命中纸娃娃才拦截; 其余事件交给按钮/基类。
  int hw = 46, hh = 62;  // 半宽/半高(小人可视范围)
  if (x >= _dollCenterX - hw && x <= _dollCenterX + hw &&
      y >= _dollCenterY - hh && y <= _dollCenterY + hh) {
    pickSkinFile();
    return;
  }
  Screen::mouseClicked(x, y, buttonNum);
}

void Touch::StartMenuScreen::pickSkinFile() {
#ifdef _WIN32
  char file[MAX_PATH] = {0};
  OPENFILENAMEA ofn;
  memset(&ofn, 0, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = NULL;   // 主菜单屏; ModsScreen 用的是 g_win32Hwnd
  ofn.lpstrFilter = "Images (*.png;*.jpg;*.jpeg)\0*.png;*.jpg;*.jpeg\0PNG (*.png)\0*.png\0JPG (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0All Files (*.*)\0*.*\0\0";
  ofn.lpstrFile = file;
  ofn.nMaxFile = MAX_PATH;
  // OFN_NOCHANGEDIR: 防止浏览文件时 Windows 改掉进程 CWD —— 游戏 world/data
  // 都是相对 exe 目录的路径, CWD 一改存档列表就空、创建世界崩(见 crash log)。
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
  if (GetOpenFileNameA(&ofn)) {
    _pendingSkinPath = file;  // 延迟到下一帧 render 开头应用
  }
#endif
}

void Touch::StartMenuScreen::buttonClicked(struct Button *button) {
  if (button->id == this->playButton.id) {
    // 0.8.1 的 PlayScreen(1)：本地世界+服务器大厅（顶部 Back/New/External/Edit）
    this->minecraft->setScreen(new PlayScreen(1));
    return;
  }
  if (button->id == this->playOnRealmsButton.id) {
    // 0.6.1 无 Realms——进多人服务器列表
    this->minecraft->setScreen(new JoinGameScreen());
    return;
  }
  if (button->id == this->marketplaceButton.id) {
    // 0.6.1 无纹理包市场（按钮隐藏，不触发）
    return;
  }
  if (button->id == this->settingsButtonMaybe->id) {
    this->minecraft->setScreen(new OptionsScreen(false));
    return;
  }
  if (button->id == this->buyButton.id) {
    this->minecraft->platform()->buyGame();
  }
  if (button->id == this->modButton.id) {
    // 用户定制：Mod 按钮 → 模组管理界面（左侧封面 + 右侧列表 + 开关）
    this->minecraft->setScreen(new ModsScreen());
    return;
  }
  if (button->id == this->languageButton->id) {
    // 用户定制：语言按钮 → 语言选择界面（顶栏 + 返回 + 语言行，选中即切）
    this->minecraft->setScreen(new LanguageScreen());
    return;
  }
}
