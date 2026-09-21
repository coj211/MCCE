#include "OptionsScreen.h"
#include "ScreenChooser.h"
#include "../components/OptionsGroup.h"
#include "../components/ScrollingPane.h"
#include "../../Minecraft.h"
#include "../../renderer/Tesselator.h"
#include "../../../locale/I18n.h"
#include <cstdint>

#include "OptionsScreen.h"
#include "ScreenChooser.h"
#include "../components/ScrollingPane.h"
#include "../../Minecraft.h"
#include "../components/OptionsGroup.h"
#include "../components/OptionsPane.h"
#include <gui/buttons/CategoryButton.hpp>
#include "PauseScreen.h"
#include <gui/screens/PauseScreen.hpp>   // 0.8.1 暂停菜单移植(PauseScreen081)
#include "../components/Button.h"
#include "../components/Button.h"
#include "../components/NinePatch.h"
#include "../components/ImageButton.h"
#include <cstdint>

OptionsScreen::OptionsScreen(bool a2) {
	this->isInWorld = a2;
	this->headerOptions = 0;
	this->buttonBack = 0;
	this->field_5C = 0;
	this->field_7C = 0;
	this->field_80 = 0;
	this->field_84 = 0;
}

void OptionsScreen::closeScreen() {
	this->minecraft->options.save();
	if(this->isInWorld) {
		// 0.8.1 GUI 移植：从 0.8.1 暂停菜单进来的选项页，返回也应回 0.8.1
		// 暂停菜单（PauseScreen081）。原代码回 0.6.1 PauseScreen —— 那个
		// 老暂停页在现代触摸界面没有对应按钮布局，表现为“卡出老 061 页面”。
		this->minecraft->setScreen(new PauseScreen081(false));
	} else {
		this->minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
	}
}
int32_t OptionsScreen::createCategoryButton(int32_t a2, int32_t a3, struct ImageDef& a4, int32_t a5, int32_t a6, int32_t a7, int32_t a8) {
	CategoryButton* v12 = new CategoryButton(a2, this->field_7C, this->field_80, (Button**)&this->field_5C);
	a4.hasSubImage = 1;
	a4.u = a5;
	a4.v = a6;
	a4.subW = 28;
	a4.subH = 28;
	v12->x = a7;
	v12->y = a8;
	v12->setImageDef(a4, 1);
	v12->height = a3;
	v12->width = a3;
	this->field_60.emplace_back(v12);
	return a2 + 1;
}
void OptionsScreen::createCategoryButtons(void) {
	NinePatchFactory a1(this->minecraft->textures, "gui/spritesheet.png");
	this->field_7C = a1.createSymmetrical({8, 32, 8, 8}, 2, 2, this->field_88, this->field_88);
	this->field_80 = a1.createSymmetrical({0, 32, 8, 8}, 2, 2, this->field_88, this->field_88);
	ImageDef v11;
	v11.name = "gui/touchgui2.png";
	v11.width = v11.height = this->field_88;
	int32_t v7 = this->createCategoryButton(2, this->field_88, v11, 134, 0, 0, 0);
	int32_t v8 = this->createCategoryButton(v7, this->field_88, v11, 106, 0, 0, 0);
	int32_t v9 = this->createCategoryButton(v8, this->field_88, v11, 134, 28, 0, 0);
	int32_t v10 = this->createCategoryButton(v9, this->field_88, v11, 106, 28, 0, 0);
	this->createCategoryButton(v10, this->field_88, v11, 134, 56, 0, 0);
}
void OptionsScreen::generateOptionScreens(void) {
	for (auto p : this->optionPanes) delete p;
	this->optionPanes.clear();

	this->optionPanes.emplace_back(new OptionsPane());
	this->optionPanes.emplace_back(new OptionsPane());
	this->optionPanes.emplace_back(new OptionsPane());
	this->optionPanes.emplace_back(new OptionsPane());
	this->optionPanes.emplace_back(new OptionsPane());

	// 难度不在这里改：难度按世界保存，见 Play 列表里的「世界设置」
	this->optionPanes[0]->createOptionsGroup("options.group.game").addOptionItem(&Options::Option::NAME, this->minecraft).addOptionItem(&Options::Option::THIRD_PERSON, this->minecraft).addOptionItem(&Options::Option::SERVER_VISIBLE, this->minecraft).addOptionItem(&Options::Option::SHOW_COORDINATES, this->minecraft).addOptionItem(&Options::Option::DEBUG_SCREEN, this->minecraft).addOptionItem(&Options::Option::HUD_CAMERA_BUTTON, this->minecraft);
	this->optionPanes[1]->createOptionsGroup("options.group.input").addOptionItem(&Options::Option::SENSITIVITY, this->minecraft).addOptionItem(&Options::Option::INVERT_MOUSE, this->minecraft).addOptionItem(&Options::Option::LEFT_HANDED, this->minecraft).addOptionItem(&Options::Option::USE_TOUCHSCREEN, this->minecraft).addOptionItem(&Options::Option::USE_TOUCH_JOYPAD, this->minecraft).addOptionItem(&Options::Option::PIXELS_PER_MILLIMETER, this->minecraft).addOptionItem(&Options::Option::SPRINT, this->minecraft).addOptionItem(&Options::Option::AUTO_JUMP, this->minecraft).addOptionItem(&Options::Option::SWAP_JUMP_AND_SNEAK, this->minecraft);
	this->optionPanes[2]->createOptionsGroup("options.group.graphics").addOptionItem(&Options::Option::RENDER_DISTANCE, this->minecraft).addOptionItem(&Options::Option::BRIGHTNESS, this->minecraft).addOptionItem(&Options::Option::FOV, this->minecraft).addOptionItem(&Options::Option::FOG_ENABLED, this->minecraft).addOptionItem(&Options::Option::GRAPHICS, this->minecraft).addOptionItem(&Options::Option::FANCY_SKIES, this->minecraft).addOptionItem(&Options::Option::CLASSIC_TEXTURES, this->minecraft).addOptionItem(&Options::Option::ANIMATE_TEXTURES, this->minecraft).addOptionItem(&Options::Option::ANIMATE_WATER, this->minecraft).addOptionItem(&Options::Option::ANIMATE_LAVA, this->minecraft).addOptionItem(&Options::Option::ANIMATE_FIRE, this->minecraft).addOptionItem(&Options::Option::SMOOTH_CHUNKS, this->minecraft);
	this->optionPanes[2]->createOptionsGroup("options.group.graphics.experimental").addOptionItem(&Options::Option::HIDE_GUI, this->minecraft).addOptionItem(&Options::Option::LOD_CHUNKS, this->minecraft);
	this->optionPanes[3]->createOptionsGroup("options.group.audio").addOptionItem(&Options::Option::SOUND, this->minecraft).addOptionItem(&Options::Option::MUSIC, this->minecraft);
	
	auto grp = this->optionPanes[4]->createOptionsGroup("options.newadditions");
#if !defined(ANDROID) && !defined(MCPE_IOS) && !defined(__APPLE__)
	grp.addOptionItem(&Options::Option::MARKETPLACE, this->minecraft);
#endif
	grp.addOptionItem(&Options::Option::SHOW_FPS, this->minecraft);
	grp.addOptionItem(&Options::Option::DISCORD_RPC, this->minecraft);
	grp.addOptionItem(&Options::Option::PANORAMA_ANGLE, this->minecraft);

	if (this->minecraft->options.newAdditions == 1) {
		grp.addOptionItem(&Options::Option::CHAT_COLOR, this->minecraft);
		grp.addOptionItem(&Options::Option::CHAT_BG_COLOR, this->minecraft);
		grp.addOptionItem(&Options::Option::CLASSIC_BACKGROUND, this->minecraft);
		grp.addOptionItem(&Options::Option::CLASSIC_GUI, this->minecraft);
		grp.addOptionItem(&Options::Option::NEON_COLOR_THEME, this->minecraft);
	}
}
void OptionsScreen::selectCategory(int32_t a2) {
	int32_t v2 = 0;
	for(auto&& p: this->field_60) {
		bool v5 = a2 == v2++;
		if(v5) {
			this->field_5C = p;
		}
		p->text = v5;
	}
	if(a2 < this->optionPanes.size()) {
		this->selectedCategory = this->optionPanes[a2];
	}
}

OptionsScreen::~OptionsScreen() {
	if(this->buttonBack) {
		delete this->buttonBack;
		this->buttonBack = 0;
	}
	if(this->headerOptions) {
		delete this->headerOptions;
		this->headerOptions = 0;
	}
	if (this->field_7C) { delete this->field_7C; this->field_7C = 0; }
	if (this->field_80) { delete this->field_80; this->field_80 = 0; }
	for(auto&& p: this->field_60) {
		if(p) {
			delete p;
			p = 0;
		}
	}
	for(auto&& p: this->optionPanes) {
		if(p) {
			delete p;
			p = 0;
		}
	}
	this->field_60.clear();
	this->optionPanes.clear();
}
void OptionsScreen::render(int32_t a2, int32_t a3, float a4) {
	OptionsPane* selectedCategory; // r0
	Minecraft* minecraft; // r7
	int width; // r0
	int height; // r10
	OptionsPane* v12; // r4

	selectedCategory = this->selectedCategory;
	if(selectedCategory && selectedCategory->suppressOtherGUI()) {
		if(this->selectedCategory) {
			this->selectedCategory->topRender(this->minecraft, a2, a3);
		}
	} else {
		if(!this->isInWorld) {
			if(this->renderGameBehind()) {
				this->renderMenuBackground(a4);
			}
		}
		this->renderBackground(0);
		this->fill(0, 0, this->field_88 + 10, this->height, 0xFF958782);
		Screen::render(a2, a3, a4);
		minecraft = this->minecraft;
		width = this->width;
		height = this->height;
		v12 = this->selectedCategory;
		if(v12) {
			v12->render(minecraft, a2, a3 - 1);
		}

		static bool prevClassicTextures = this->minecraft->options.classicTextures;
		static bool showRestartNotice = false;
		static int  restartNoticeTimer = 0;
		if (this->minecraft->options.classicTextures != prevClassicTextures) {
			showRestartNotice = true;
			restartNoticeTimer = 180; // ~3 seconds at 60fps
			prevClassicTextures = this->minecraft->options.classicTextures;
		}
		if (showRestartNotice) {
			this->drawCenteredString(this->font, I18n::get("options.restartRequired"), this->width / 2, this->height - 15, 0xFFFFAA44);
			if (restartNoticeTimer > 0) {
				--restartNoticeTimer;
			} else {
				showRestartNotice = false;
			}
		}
	}
}
void OptionsScreen::init() {
	this->field_88 = 28;
	this->headerOptions = new Touch::THeader(0, I18n::get("mainmenu.settings"));
	this->buttonBack = new Touch::TButton(1, I18n::get("gui.back"), 0);
	this->buttonBack->width = 38;
	this->buttonBack->height = 18;
	this->buttonBack->init(this->minecraft);
	this->buttons.emplace_back(this->headerOptions);
	this->buttons.emplace_back(this->buttonBack);
	this->createCategoryButtons();
	for(auto&& i = this->field_60.begin(); i != this->field_60.end(); ++i) {
		this->buttons.emplace_back(*i);
		this->tabButtons.emplace_back(*i);
	}
	this->generateOptionScreens();
}
void OptionsScreen::setupPositions() {
	int32_t v2 = this->field_60.size();
	// 0.8.1 GUI 移植 + 用户定制：分类按钮从顶部（header 下）开始竖排，不做垂直居中
	int32_t v3 = 0;

	for(int32_t i = 0; i < this->field_60.size(); ++i) {
		this->field_60[i]->y = this->headerOptions->height + 3 + v3 + 29 * i;
		this->field_60[i]->x = 5;
	}
	this->buttonBack->x = 4;
	this->buttonBack->y = 4;
	this->headerOptions->x = 0;
	this->headerOptions->y = 0;
	this->headerOptions->width = this->width;
	this->headerOptions->height = this->buttonBack->height + 8;
	for(auto&& p: this->optionPanes) {
		if(this->field_60.size()) {
			p->x = this->field_60[0]->width + 20;
			p->y = this->headerOptions->height + 3;
			p->width = this->width - this->field_60[0]->width - 20;
			p->height = this->height - this->headerOptions->height - 3;
			p->setupPositions();
		}
	}
	this->selectCategory(0);
}
bool OptionsScreen::handleBackEvent(bool a2) {
	if(!a2) {
		if(!this->selectedCategory || !this->selectedCategory->suppressOtherGUI() || !this->selectedCategory->backPressed(this->minecraft, 0)) {
			this->closeScreen();
		}
	}
	return 1;
}
void OptionsScreen::tick() {
	static int lastNewAdditions = this->minecraft->options.newAdditions;
	if (this->minecraft->options.newAdditions != lastNewAdditions) {
		lastNewAdditions = this->minecraft->options.newAdditions;
		int selectedIdx = 0;
		for(int i = 0; i < this->optionPanes.size(); i++) {
			if (this->optionPanes[i] == this->selectedCategory) {
				selectedIdx = i;
				break;
			}
		}
		this->generateOptionScreens();
		this->setupPositions();
		this->selectCategory(selectedIdx);
	}

	if(this->selectedCategory) {
		this->selectedCategory->tick(this->minecraft);
	}
	Screen::tick();
}
void OptionsScreen::removed() {
}
bool OptionsScreen::renderGameBehind() {
	return 1;
}
void OptionsScreen::setTextboxText(const std::string& a2) {
	if(this->selectedCategory) {
		this->selectedCategory->setTextboxText(a2);
	}
}
void OptionsScreen::buttonClicked(Button* a2) {
	if(a2 == this->buttonBack) {
		this->closeScreen();
	} else {
		int32_t id = a2->id;
		if((uint32_t)(id - 2) <= 5) {
			this->selectCategory(id - this->field_60[0]->id);
		}
	}
}
void OptionsScreen::mouseClicked(int32_t a2, int32_t a3, int32_t a4) {
	if (a4 == 4 || a4 == 5) {
		// 0.6.1 OptionsPane 无 scrollingPane：滚轮暂不处理
		return;
	}
	bool v8; // r0
	OptionsPane* selectedCategory; // r4

	v8 = this->selectedCategory->suppressOtherGUI();
	selectedCategory = this->selectedCategory;
	if(v8) {
		if(selectedCategory) {
			selectedCategory->focusuedMouseClicked(this->minecraft, a2, a3, a4);
		}
	} else {
		if(selectedCategory) {
			selectedCategory->mouseClicked(this->minecraft, a2, a3, a4);
		}
		Screen::mouseClicked(a2, a3, a4);
	}
}
void OptionsScreen::mouseReleased(int32_t a2, int32_t a3, int32_t a4) {
	bool v8; // r0
	OptionsPane* selectedCategory; // r4

	v8 = this->selectedCategory->suppressOtherGUI();
	selectedCategory = this->selectedCategory;
	if(v8) {
		if(selectedCategory) {
			selectedCategory->focusuedMouseReleased(this->minecraft, a2, a3, a4);
		}
	} else {
		if(selectedCategory) {
			selectedCategory->mouseReleased(this->minecraft, a2, a3, a4);
		}
		Screen::mouseReleased(a2, a3, a4);
	}
}
void OptionsScreen::keyPressed(int32_t a2) {
	if(this->selectedCategory) {
		this->selectedCategory->keyPressed(this->minecraft, a2);
	}
}
void OptionsScreen::keyboardNewChar(const std::string& a2, bool a3) {
	if(this->selectedCategory) {
		this->selectedCategory->keyboardNewChar(this->minecraft, a2, a3);
	}
}
// 0.8.1 GUI 移植：基类 keyboardText 逐字符调用 char 版本（utf8 逐字节），
// 转发给 3 参版本（TextBox081 支持 UTF-8 逐字节拼接，中文可正常输入）
void OptionsScreen::keyboardNewChar(char inputChar) {
	if(!this->selectedCategory) {
		return;
	}
	std::string s(1, inputChar);
	this->selectedCategory->keyboardNewChar(this->minecraft, s, false);
}
