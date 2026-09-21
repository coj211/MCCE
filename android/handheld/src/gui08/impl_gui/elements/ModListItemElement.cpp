#include <gui/elements/ModListItemElement.hpp>
#include <Minecraft.hpp>
#include <gui/buttons/ImageWithBackground.hpp>
#include <gui/buttons/Touch_TButton.hpp>
#include "../../../mod/ModEngine.h"
#include "../../../client/gui/screens/ModsScreen.h"
#include <rendering/Font.hpp>
#include <input/Mouse.hpp>
#include <gui/Gui.hpp>

// 模组列表项 —— 代码结构完全照抄世界列表项 LocalServerListItemElement：
// init 建 TButton 九宫格背景（field_2C）+ 右侧开关按钮；render 画背景 +
// 模组名 + 开关；mouseClicked/mouseReleased 区分"点整行"和"点开关"。

ModListItemElement::ModListItemElement(Minecraft* a2, ModInfo* a3, ModsScreen* owner)
	: GuiElement(1, 1, 0, 0, 24, 24) {
	this->field_24 = this->field_28 = 0;
	this->field_2C = 0;
	this->field_30 = 0;
	this->toggleButton = 0;
	this->info = a3;
	this->field_54 = owner;
	this->height = 32;
}

void ModListItemElement::init(Minecraft* a2) {
	Touch::TButton* v4 = new Touch::TButton(1, "", 0);
	v4->init(a2, "gui/spritesheet.png", {8, 32, 8, 8}, {0, 32, 8, 8}, 2, 2, 120, 32);
	this->field_2C = v4;

	this->toggleButton = new ImageWithBackground(-1);
	this->toggleButton->init(a2->textures, 32, 32, {112, 0, 8, 67}, {120, 0, 8, 67}, 2, 2, "gui/spritesheet.png");
	this->toggleButton->width = 32;
	this->toggleButton->height = 32;
	this->toggleButton->setupPositions();
	// 开关显示贴图原色（ImageWithBackground 默认 inactive 会整体叠灰，on/off 无法区分）
	this->toggleButton->active = 1;
	this->updateToggleIcon(a2);
}

void ModListItemElement::updateToggleIcon(Minecraft* a2) {
	if (!this->toggleButton || !this->info)
		return;
	ImageDef def;
	def.name = this->info->enabled ? "gui/mods/on.png" : "gui/mods/off.png";
	def.hasSubImage = 0;
	def.width = 18.0f;
	def.height = 18.0f;
	this->toggleButton->setImageDef(def, 0);
}

ModListItemElement::~ModListItemElement() {
	if (this->toggleButton) {
		delete this->toggleButton;
		this->toggleButton = 0;
	}
	if (this->field_2C) {
		delete this->field_2C;
		this->field_2C = 0;
	}
	if (this->info) {
		delete this->info;
		this->info = 0;
	}
}

void ModListItemElement::tick(Minecraft* a2) {
	// 无周期刷新：开关图标在 mouseReleased 切换后即时更新
}

void ModListItemElement::render(Minecraft* a2, int32_t a3, int32_t a4) {
	int32_t width = this->width - 40; // 右侧留给开关
	this->field_2C->x = this->x;
	this->field_2C->y = this->y;
	this->field_2C->width = width;
	this->field_2C->render(a2, a3, a4);

	this->updateToggleIcon(a2);   // 每次渲染前按当前启停状态刷图标
	this->toggleButton->x = this->x + this->width - 36;
	this->toggleButton->y = this->y + (this->height - 32) / 2;
	this->toggleButton->render(a2, a3, a4);

	if (this->info) {
		// 选中态高亮（同世界列表项：已选中 0xFFFFA0 / 普通白色；按下时也高亮）
		int32_t v12 = this->field_2C->isPressed(a3, a4) ? 0xFFFFA0 : 0xFFFFFFFF;
		if (this->field_54 && this->field_54->isSelected(this->info))
			v12 = 0xFFFFA0;
		a2->font->drawShadow(this->info->name, (float)this->x + 5.0, (float)this->y + 5.0, v12);
		std::string meta;
		if (!this->info->author.empty()) meta += this->info->author;
		if (!this->info->version.empty()) {
			if (!meta.empty()) meta += " ";
			meta += "v" + this->info->version;
		}
		if (!meta.empty())
			a2->font->drawShadow(meta, (float)this->x + 5.0, (float)this->y + 16.0, 0xFFBBBBBB);
	}
}

void ModListItemElement::mouseClicked(Minecraft* a2, int32_t a3, int32_t a4, int32_t a5) {
	float v9 = (float)Mouse::getX() * Gui::InvGuiScale;
	float v12 = (float)Mouse::getY() * Gui::InvGuiScale;
	if (this->toggleButton && this->toggleButton->clicked(a2, a3, a4)) {
		this->field_30 = this->toggleButton;
		this->field_24 = v9;
		this->field_28 = v12;
	} else {
		if (!this->field_2C->clicked(a2, a3, a4))
			return;
		this->field_24 = v9;
		this->field_28 = v12;
		this->field_30 = this->field_2C;
	}
	this->field_30->setPressed();
}

void ModListItemElement::mouseReleased(Minecraft* a2, int32_t a3, int32_t a4, int32_t a5) {
	if (this->field_30 != this->toggleButton) {
		// 整行点击：选中（供左侧封面区联动）
		if (this->field_30 == this->field_2C) {
			if (this->field_2C->clicked(a2, a3, a4)) {
				if (this->field_54 && this->info)
					this->field_54->selectMod(this->info);
			}
			this->field_2C->released(a3, a4);
		}
		this->field_30 = 0;
		return;
	}
	// 开关点击：切换启停（点一下从启用变到不启用）
	if (this->toggleButton->clicked(a2, a3, a4)) {
		if (this->info) {
			this->info->enabled = !this->info->enabled;
			a2->modEngine->setEnabled(this->info->file, this->info->enabled);
		}
		this->updateToggleIcon(a2);
		if (this->field_54)
			this->field_54->onModToggled();
	}
	this->toggleButton->released(a3, a4);
	this->field_30 = 0;
}
