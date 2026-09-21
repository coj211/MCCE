#include <gui/screens/ArmorScreen.hpp>
#include <gui/screens/CreativeInventoryScreen.hpp>
#include <Minecraft.hpp>
#include <entity/player/gamemode/GameMode.hpp>
#include <entity/LocalPlayer.hpp>
#include <gui/NinePatchLayer.hpp>
#include <gui/pane/Touch_InventoryPane.hpp>
#include <inventory/Inventory.hpp>
#include <item/ArmorItem.hpp>
#include <item/Item.hpp>
#include <math.h>
#include <math/Mth.hpp>
#include <rendering/EntityRenderDispatcher.hpp>
#include <rendering/Tesselator.hpp>
#include <rendering/Textures.hpp>
#include <rendering/entity/ItemRenderer.hpp>
#include <rendering/entity/MobRenderer.hpp>
#include <utils.h>
#include <gui/NinePatchFactory.hpp>

ArmorScreen::ArmorScreen()
	: Screen()
	, backButton(4, "Back", 0)
	, field_B8(0)
	, field_E8(1)
	, field_118(2)
	, field_148(3)
	, header(5, "Armor") {
	this->field_58 = 0;
	this->field_5C = 0;
	this->field_60 = 0;
	this->field_64 = "";
	this->field_7C = 90.0;
	this->field_1BC = 0;
	this->field_1C0.x = 0;
	this->field_1C0.y = 0;
	this->field_1C0.w = 1;
	this->field_1C0.h = 1;
	this->field_1D0 = 0;
	this->field_1D4 = 0;
	this->field_1D8 = 1;
	this->field_1DC = 1;
	this->performUpdate = 0;
	this->field_1F0 = 0;
	this->field_1F4 = 0;
	this->field_1F8 = 0;
	this->field_1FC = 0;
}
bool_t ArmorScreen::canMoveToSlot(int32_t a2, const ItemInstance* a3) {
	if(ItemInstance::isArmorItem(a3)) {
		ArmorItem* it = (ArmorItem*)a3->itemClass; //TODO uses a3->field_48
		return it->slot == a2;
	}
	return 0;
}
void ArmorScreen::closeScreen() {
	if (this->minecraft->gameMode && this->minecraft->gameMode->isCreativeType()) {
		this->minecraft->setScreen(new CreativeInventoryScreen());
	} else {
		// 0.8.1 回到 INVENTORY_SCREEN（背包选择）；目标项目 ArmorScreen 即生存背包，直接关闭
		this->minecraft->setScreen(NULL);
	}
}
void ArmorScreen::drawSlotItemAt(Tesselator& a2, int32_t a3, const ItemInstance* a4, int32_t a5, int32_t a6) {
	float v6;			   // s17
	float v10;			   // s16
	Textures* textures; // r4

	v6 = (float)a5;
	v10 = (float)a6;
	this->field_1F4->draw(a2, (float)a5, (float)a6);
	if(a4 && !a4->isNull()) {
		ItemRenderer::renderGuiItemNew(this->minecraft->textures, a4, 0, v6 + 3.0, v10, 1.0, 1.0, 1.0);
		glDisable(0xDE1u);
		ItemRenderer::renderGuiItemDecorations(a4, v6 + 2.0, v10 + 3.0);
		glEnable(0xDE1u);
	} else {
		// 0.8.1 用 TextureAtlasTextureItem 画空槽图标；目标项目无 TextureAtlas，
		// 空槽只显示槽位背景（field_1F4 已画）
	}
}
void ArmorScreen::handleRenderPane(Touch::InventoryPane* a2, Tesselator& a3, int32_t a4, int32_t a5, float a6) {
	if(a2) {
		a2->render(a4, a5, a6);
		this->field_1F8->draw(a3, (float)(a2->rect.x - 1), (float)(a2->rect.y - 1));
	}
}
void ArmorScreen::renderPlayer(float a2, float a3) {
	// 0.8.1 的 3D 玩家预览依赖 LocalPlayer 的 field_124/168/16C/170 等字段与
	// MobRenderer::menuMode；目标项目无这些，简化不渲染（保留槽位背景即可）
}

bool_t ArmorScreen::takeAndClearSlot(int32_t a2) {
	ItemInstance* item = this->player->getArmor(a2);
	if(item) {
		if (this->minecraft->gameMode && this->minecraft->gameMode->isCreativeType()) {
			this->player->setArmor(a2, 0);
			return 1;
		}
		int32_t empty = this->minecraft->player->inventory->getNumEmptySlots();
		if(!this->minecraft->player->inventory->add(item)) {
			this->minecraft->player->drop(item, 0);
		}
		this->player->setArmor(a2, 0);
		int32_t nempty = this->minecraft->player->inventory->getNumEmptySlots();
		if(this->performUpdate) {
			this->performUpdate = 1;
			return 1;
		} else {
			this->performUpdate = nempty != empty;
			return this->performUpdate;
		}
	}
	return 0;
}
void ArmorScreen::updateItems() {
	this->field_1E0.clear();
	if (this->minecraft->gameMode && this->minecraft->gameMode->isCreativeType()) {
		static const Item* creativeArmors[] = {
			Item::helmet_cloth, Item::chestplate_cloth, Item::leggings_cloth, Item::boots_cloth,
			Item::helmet_chain, Item::chestplate_chain, Item::leggings_chain, Item::boots_chain,
			Item::helmet_iron, Item::chestplate_iron, Item::leggings_iron, Item::boots_iron,
			Item::helmet_diamond, Item::chestplate_diamond, Item::leggings_diamond, Item::boots_diamond,
			Item::helmet_gold, Item::chestplate_gold, Item::leggings_gold, Item::boots_gold
		};
		static std::vector<ItemInstance> creativeArmorInstances;
		creativeArmorInstances.clear();
		for (const Item* it : creativeArmors) {
			if (it) {
				creativeArmorInstances.emplace_back((Item*)it, 1, 0);
			}
		}
		for (size_t i = 0; i < creativeArmorInstances.size(); ++i) {
			this->field_1E0.push_back(&creativeArmorInstances[i]);
		}
	} else {
		for(int32_t i = 9; i < this->minecraft->player->inventory->getContainerSize(); ++i) {
			ItemInstance* v5 = this->minecraft->player->inventory->getItem(i);
			if(ItemInstance::isArmorItem(v5)) {
				this->field_1E0.emplace_back(v5);
			}
		}
	}
}

void ArmorScreen::setupInventoryPane() {
	if(this->field_1BC) delete this->field_1BC;
	this->field_1BC = new Touch::InventoryPane(this, this->minecraft, this->field_1C0, this->field_1C0.w, 4.0f, this->field_1E0.size(), 32, 4);
}
ArmorScreen::~ArmorScreen() {
	if(this->field_1BC) delete this->field_1BC;
	if(this->field_1F0) delete this->field_1F0;
	if(this->field_1F4) delete this->field_1F4;
	if(this->field_1F8) delete this->field_1F8;
	if(this->field_1FC) delete this->field_1FC;

	if(this->field_58) delete this->field_58;
}
void ArmorScreen::render(int32_t a2, int32_t a3, float a4) {
	int32_t v8;			 // r3
	int32_t v9;			 // r6
	ItemInstance* armor; // r0
	BlankButton* v11;	 // r2
	int32_t v12;		 // r2
	int32_t x;		 // [sp+0h] [bp-20h]
	int32_t y;		 // [sp+4h] [bp-1Ch]

	Tesselator::instance.addOffset(0.0, 0.0, -500.0);
	this->field_1F0->draw(Tesselator::instance, 0.0, 0.0);
	Tesselator::instance.addOffset(0.0, 0.0, 500.0);
	Screen::render(a2, a3, a4);
	v8 = a2;
	v9 = 0;
	this->handleRenderPane(this->field_1BC, Tesselator::instance, v8, a3, a4);
	Tesselator::instance.colorABGR(-1);
	glColor4f(1.0, 1.0, 1.0, 1.0);
	Tesselator::instance.addOffset(0.0, 0.0, -490.0);
	this->field_1FC->draw(Tesselator::instance, (float)this->field_1D0, (float)this->field_1D4);
	Tesselator::instance.addOffset(0.0, 0.0, 490.0);
	glClear(0x100u);
	this->renderPlayer((float)(this->field_1D0 + this->field_1D8 / 2), (float)this->height * 0.85);
	do {
		armor = this->player->getArmor(v9);
		v11 = this->field_178[v9];
		x = v11->x;
		y = v11->y;
		v12 = v9++;
		this->drawSlotItemAt(Tesselator::instance, v12, armor, x, y);
	} while(v9 != 4);
}
void ArmorScreen::init() {
	Screen::init();
	this->player = this->minecraft->player;
	this->backButton.width = 38;
	this->backButton.height = 18;
	this->backButton.init(this->minecraft);
	this->buttons.emplace_back(&this->header);
	this->buttons.emplace_back(&this->backButton);
	this->field_178[0] = &this->field_B8;
	this->field_178[1] = &this->field_E8;
	this->field_178[2] = &this->field_118;
	this->field_178[3] = &this->field_148;
	for(int32_t i = 0; i != 4; ++i) {
		this->buttons.emplace_back(this->field_178[i]);
	}
	NinePatchFactory npf(this->minecraft->textures, "gui/spritesheet.png");
	this->field_1F0 = npf.createSymmetrical({0, 0, 16, 16}, 4, 4, 32, 32);
	this->field_1F4 = npf.createSymmetrical({0, 32, 8, 8}, 3, 3, 20, 20);
	this->field_1F8 = npf.createSymmetrical({28, 42, 4, 4}, 1, 1, 32, 32)->setExcluded(16);
	this->field_1FC = npf.createSymmetrical({0, 20, 8, 8}, 3, 3, 32, 32);
	this->updateItems();
}
void ArmorScreen::setupPositions() {
	float width;	 // s14
	int v3;			 // r3
	int v4;			 // r2
	int v5;			 // r6
	int i;			 // r3
	BlankButton* v7; // r2
	int minY;		 // r0
	int v9;			 // r0
	int v10;		 // r5
	int v11;		 // r6
	int v12;		 // r5
	int height;		 // r3
	int v14;		 // s12
	int v15;		 // [sp+Ch] [bp-4h]

	width = (float)this->width;
	this->header.y = 0;
	this->header.x = 0;
	this->header.width = this->width;
	this->backButton.x = 4;
	this->backButton.y = 4;
	v3 = (int)(float)(width / 1.8);
	v4 = v3 - 20;
	if(v3 - 20 < 0) {
		v4 = v3 + 11;
	}
	v5 = (v4 & 0xFFFFFFE0) + 20;
	v15 = this->height - 40;
	this->field_1C0.x = 10;
	this->field_1C0.y = 32;
	this->field_1C0.w = v4 & 0xFFFFFFE0;
	this->field_1C0.h = v15;
	for(i = 0; i != 16; i += 4) {
		v7 = this->field_178[i / 4u];
		v7->x = v5;
		minY = this->field_1C0.y;
		v7->width = 20;
		v7->height = 20;
		v9 = minY + 6 * i;
		v7->y = v9;
	}
	v10 = this->width;
	this->field_1D4 = this->field_1C0.y;
	v11 = this->field_178[0]->x + this->field_178[0]->width;
	v12 = v10 - v11;
	this->field_1D0 = v11 + v12 / 10;
	height = this->field_1C0.h;
	v14 = this->field_1C0.w;
	this->field_1DC = height;
	this->field_1D8 = v12 + 2 * (v12 / -10);
	this->field_1F8->setSize((float)v14 + 2.0, (float)height + 2.0);
	this->field_1FC->setSize((float)this->field_1D8, (float)this->field_1DC);
	this->field_1F0->setSize((float)this->width, (float)this->height);
	this->updateItems();
	this->setupInventoryPane();
}
bool_t ArmorScreen::handleBackEvent(bool_t a2) {
	if(!a2) {
		this->closeScreen();
	}
	return 1;
}
void ArmorScreen::tick() {
	if(this->field_1BC) {
		this->field_1BC->tick();
	}
	if(this->performUpdate) {
		this->updateItems();
		this->setupInventoryPane();
		this->performUpdate = 0;
	}
}
bool ArmorScreen::renderGameBehind() {
	return 0;
}

void ArmorScreen::buttonClicked(Button* a2) {
	if(a2 == &this->backButton) {
		this->closeScreen();
	}
	uint32_t bid = a2->id;
	if(bid <= 3) {
		this->takeAndClearSlot(bid);
	}
}
bool ArmorScreen::addItem(const Touch::InventoryPane* a2, int32_t a3) {
	const ItemInstance* v5 = this->field_1E0[a3];
	if(ItemInstance::isArmorItem(v5)) {
		if (this->minecraft->gameMode && this->minecraft->gameMode->isCreativeType()) {
			this->player->setArmor(((ArmorItem*)v5->itemClass)->slot, v5);
			return 1;
		}
		ItemInstance* armor = this->player->getArmor(((ArmorItem*)v5->itemClass)->slot);
		ItemInstance v15;
		if(ItemInstance::isArmorItem(armor)) {
			v15 = *armor;
		}
		this->player->setArmor(((ArmorItem*)v5->itemClass)->slot, v5);
		this->player->inventory->removeItem(v5);
		this->field_1E0[a3] = 0;
		if(!v15.isNull() && !this->player->inventory->add(&v15)) {
			this->player->drop(&v15, 0);
		}
		this->performUpdate = 1;
		return 1;
	}
	return 0;
}
bool ArmorScreen::isAllowed(int32_t) {
	return 1;
}
std::vector<const ItemInstance*> ArmorScreen::getItems(const Touch::InventoryPane* a3) {
	return std::vector<const ItemInstance*>(this->field_1E0);
}
