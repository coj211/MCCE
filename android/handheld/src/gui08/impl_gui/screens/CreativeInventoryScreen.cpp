#include <gui/screens/CreativeInventoryScreen.hpp>
#include <gui/screens/ArmorScreen.hpp>
#include <Minecraft.hpp>
#include <entity/LocalPlayer.hpp>
#include <gui/buttons/CategoryButton.hpp>
#include <gui/buttons/ImageWithBackground.hpp>
#include <gui/pane/Touch_InventoryPane.hpp>
#include <input/Mouse.hpp>
#include <inventory/Inventory.hpp>
#include <item/Item.hpp>
#include <item/ItemInstance.hpp>
#include <rendering/entity/ItemRenderer.hpp>
#include <tile/Tile.hpp>
#include <rendering/Tesselator.hpp>
#include <gui/NinePatchFactory.hpp>
#include <util/Color4.hpp>
#include <initializer_list>
#include <cstdio>
#include "../../../mod/ModEngine.h"

std::vector<ItemInstance> CreativeInventoryScreen::items;
std::vector<ItemInstance> CreativeInventoryScreen::filteredItems[6];
int CreativeInventoryScreen::lastTabButtonId = 0;

CreativeInventoryScreen::TabButtonWithMeta::TabButtonWithMeta(int f0, std::shared_ptr<ImageButton> f4) {
	this->field_0 = f0;
	this->field_4 = f4;
}
CreativeInventoryScreen::TabButtonWithMeta::TabButtonWithMeta(const CreativeInventoryScreen::TabButtonWithMeta& a2)
	: field_4(a2.field_4) {
	this->field_0 = a2.field_0;
}
CreativeInventoryScreen::TabButtonWithMeta::TabButtonWithMeta(CreativeInventoryScreen::TabButtonWithMeta&& a2) {
	this->field_0 = a2.field_0;
	this->field_4 = a2.field_4;
	a2.field_4 = 0;
}
CreativeInventoryScreen::TabButtonWithMeta::~TabButtonWithMeta() {
}
CreativeInventoryScreen::CreativeInventoryScreen() {
	this->field_58 = 24;
	this->field_5C = 2;
	this->field_A8 = 0;
	this->field_AC = 0;
	this->field_B0 = 0;
	this->currentPaneMaybe = 0;
	this->field_B8 = 0;
	this->field_BC = 0;
}
void CreativeInventoryScreen::_putItemInToolbar(const ItemInstance*) {
}
void CreativeInventoryScreen::closeWindow() {
	this->minecraft->setScreen(0);
}
std::shared_ptr<ImageButton> CreativeInventoryScreen::createInventoryTabButton(int32_t a3, int32_t a4) {
	std::shared_ptr<ImageButton> res(new CategoryButton(a3, this->field_70.get(), this->field_70.get(), &this->field_A4));
	res->height = this->field_58;
	res->width = this->field_58;
	res->setOverrideScreenRendering(1);
	return res;
}
void CreativeInventoryScreen::drawIcon(int a2, std::shared_ptr<ImageButton> a3, bool_t a4, bool_t a5) {
	// 0.8.1 移植：tab 图标暂用数字 1-6（从上往下：顶部=1 原材料 … 底部=6 mod）
	// a2 = field_0 = 数字（init 里按 12-id 赋值）
	char buf[4];
	snprintf(buf, sizeof(buf), "%d", a2);
	float cx = (float)(a3->x + this->field_58 / 2) - 4.0f;
	float cy = (float)(a3->y + this->field_58 / 2) - 4.0f;
	if (this->minecraft->font) {
		this->minecraft->font->drawShadow(buf, cx, cy, 0xffffffff);
	}
}
int32_t CreativeInventoryScreen::getCategoryFromPanel(const Touch::InventoryPane* a2) {
	int32_t v2 = 0;
	int32_t max_panes = 6; // 0.8.1 移植：固定 6 分类（无 mod 时 pane 0 无按钮但数组仍在）
	while(this->field_78[v2].get() != a2) {
		if(++v2 == max_panes) return 0;
	}
	return v2;
}
ItemInstance CreativeInventoryScreen::getItemFromType(int32_t a3) {
	switch(a3) {
		case 2:
			return Tile::bookshelf ? ItemInstance(Tile::bookshelf) : ItemInstance();
		case 3:
			return Item::sword_iron ? ItemInstance(Item::sword_iron) : ItemInstance();
		case 4:
			return Item::seeds_wheat ? ItemInstance(Item::seeds_wheat) : ItemInstance();
		case 5:
			return Item::bed ? ItemInstance(Item::bed, 1, 0) : ItemInstance();
		case 6:
			return Tile::coloredPlanks ? ItemInstance(Tile::coloredPlanks, 1, 6) : ItemInstance();
		case 7:
			return Item::chestplate_iron ? ItemInstance(Item::chestplate_iron) : ItemInstance();
		default:
			return Tile::redBrick ? ItemInstance(Tile::redBrick) : ItemInstance();
	}
}
// 手动分类辅助：判断 item 是否属于列表
static bool cis_itemIn(Item* i, std::initializer_list<Item*> list) {
	for (auto* x : list) if (x == i) return true;
	return false;
}
// 0.8.1 创造背包六分类手动归类（不依赖本项目位标志 ItemCategory，版本物品 id 可能不同）：
//   pane 0 = mod 物品（tab6 底部）   pane 1 = 全物品（tab5）
//   pane 2 = 食物/可消耗（tab4）      pane 3 = 方块（tab3）
//   pane 4 = 盔甲/武器（tab2）        pane 5 = 原材料/杂项（tab1 顶部）
// 分类优先级：方块 > 盔甲武器 > 食物可消耗 > 原材料
static int32_t cis_manualCategory(const ItemInstance& it) {
	Tile* t = it.tileClass;
	Item* i = it.itemClass;
	// —— 方块（tileClass 非空即为可放置方块；含植物/花/玻璃/门等可放置物）——
	if (t) {
		return 3;
	}
	// —— 盔甲 / 武器 / 工具 ——
	if (cis_itemIn(i, {Item::bucket, Item::bucket_empty, Item::bucket_water, Item::bucket_lava,
	                   Item::bow, Item::arrow, Item::flintAndSteel,
	                   Item::shears, Item::clock, Item::compass, Item::minecart,
	                   Item::mobPlacer, Item::camera,
	                   Item::sword_wood, Item::sword_stone, Item::sword_iron, Item::sword_gold, Item::sword_emerald,
	                   Item::pickAxe_wood, Item::pickAxe_stone, Item::pickAxe_iron, Item::pickAxe_gold, Item::pickAxe_emerald,
	                   Item::hatchet_wood, Item::hatchet_stone, Item::hatchet_iron, Item::hatchet_gold, Item::hatchet_emerald,
	                   Item::shovel_wood, Item::shovel_stone, Item::shovel_iron, Item::shovel_gold, Item::shovel_emerald,
	                   Item::hoe_wood, Item::hoe_stone, Item::hoe_iron, Item::hoe_gold, Item::hoe_emerald,
	                   Item::helmet_cloth, Item::chestplate_cloth, Item::leggings_cloth, Item::boots_cloth,
	                   Item::helmet_chain, Item::chestplate_chain, Item::leggings_chain, Item::boots_chain,
	                   Item::helmet_iron, Item::chestplate_iron, Item::leggings_iron, Item::boots_iron,
	                   Item::helmet_gold, Item::chestplate_gold, Item::leggings_gold, Item::boots_gold,
	                   Item::helmet_diamond, Item::chestplate_diamond, Item::leggings_diamond, Item::boots_diamond}))
		return 4;
	// —— 可放置物品（床/门/告示牌/画/花盆等）→ 方块 tab ——
	if (cis_itemIn(i, {Item::bed,
	                   Item::door_wood, Item::door_iron, Item::door_spruce, Item::door_birch,
	                   Item::sign, Item::painting, Item::flowerPot}))
		return 3;
	// —— 食物 / 可消耗（蛋、雪球、种子也算）——
	if (cis_itemIn(i, {Item::apple, Item::bread, Item::cake, Item::melon,
	                   Item::porkChop_raw, Item::porkChop_cooked,
	                   Item::beef_raw, Item::beef_cooked,
	                   Item::chicken_raw, Item::chicken_cooked,
	                   Item::carrot, Item::potato, Item::mushroomStew,
	                   Item::egg, Item::snowBall,
	                   Item::seeds_wheat, Item::seeds_pumpkin,
	                   Item::seeds_melon, Item::seeds_beetroot}))
		return 2;
	// —— 原材料 / 杂项（无法放置的）——
	return 5;
}
void CreativeInventoryScreen::populateFilteredItems() {
	// 先清空所有分类
	for (int i = 0; i < 6; ++i) {
		CreativeInventoryScreen::filteredItems[i].clear();
	}
	for(auto&& it: CreativeInventoryScreen::items) {
		int32_t cat = cis_manualCategory(it);
		if (cat >= 2 && cat <= 5) {
			CreativeInventoryScreen::filteredItems[cat].emplace_back(it);
		}
		// tab5 全物品：原版全部物品都放一份
		CreativeInventoryScreen::filteredItems[1].emplace_back(it);
	}
	// tab6 mod 物品（ModEngine defineItem 注册的 id，400-511 段）
	if (ModEngine::instance) {
		const std::vector<int>& modIds = ModEngine::instance->creativeItems();
		for (size_t i = 0; i < modIds.size(); ++i) {
			int id = modIds[i];
			if (id > 0 && id < Item::MAX_ITEMS && Item::items[id]) {
				CreativeInventoryScreen::filteredItems[0].emplace_back(ItemInstance(id, 1, 0));
				// 全物品也包含 mod 物品
				CreativeInventoryScreen::filteredItems[1].emplace_back(ItemInstance(id, 1, 0));
			}
		}
	}
}
void CreativeInventoryScreen::populateItem(Item* a1, int32_t a2, int32_t a3) {
	if(a1) {
		CreativeInventoryScreen::items.emplace_back(ItemInstance(a1, a2, a3));
	}
}
void CreativeInventoryScreen::populateItem(Tile* a1, int32_t a2, int32_t a3) {
	if(a1) {
		CreativeInventoryScreen::items.emplace_back(ItemInstance(a1, a2, a3));
	}
}
void CreativeInventoryScreen::populateItem(const Tile* a1, int32_t a2, int32_t a3) {
	if(a1) {
		CreativeInventoryScreen::items.emplace_back(ItemInstance(a1, a2, a3));
	}
}
void CreativeInventoryScreen::populateItems() {
	CreativeInventoryScreen::items.clear();
	CreativeInventoryScreen::populateItem(Tile::rail, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::goldenRail, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::stoneBrick, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::stoneBrickSmooth, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::stoneBrickSmooth, 1, 1);
	CreativeInventoryScreen::populateItem(Tile::stoneBrickSmooth, 1, 2);
	CreativeInventoryScreen::populateItem(Tile::mossStone, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::wood, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::redBrick, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::rock, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::dirt, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::grass, 1, 0);
	if (Options::instance && Options::instance->newAdditions) {
		CreativeInventoryScreen::populateItem(Tile::grassPath, 1, 0);
	}
	CreativeInventoryScreen::populateItem(Tile::clay, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::sandStone, 5, 0);
	CreativeInventoryScreen::populateItem(Tile::sandStone, 5, 1);
	CreativeInventoryScreen::populateItem(Tile::sandStone, 5, 2);
	CreativeInventoryScreen::populateItem(Tile::sand, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::gravel, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::cobbleWall, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::cobbleWall, 5, 1);
	for(int32_t i = 0; i != 4; ++i) {
		CreativeInventoryScreen::populateItem(Tile::treeTrunk, 5, i);
	}
	CreativeInventoryScreen::populateItem(Tile::netherBrick, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::netherrack, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::unbreakable, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::stairs_stone, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::stairs_wood, 1, 0);
	if (Options::instance && Options::instance->newAdditions) {
		CreativeInventoryScreen::populateItem(Tile::woodStairsDark, 1, 0);
		CreativeInventoryScreen::populateItem(Tile::woodStairsBirch, 1, 0);
		CreativeInventoryScreen::populateItem(Tile::woodStairsJungle, 1, 0);
	}
	CreativeInventoryScreen::populateItem(Tile::stairs_brick, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::stairs_sandStone, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::stairs_stoneBrickSmooth, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::stairs_netherBricks, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::stairs_quartz, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::stoneSlabHalf, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::stoneSlabHalf, 1, 3);
	for(int32_t v2 = 0; v2 != ((Options::instance && Options::instance->newAdditions) ? 4 : 1); ++v2) {
		CreativeInventoryScreen::populateItem(Tile::woodSlabHalf, 1, v2);
	}
	CreativeInventoryScreen::populateItem(Tile::stoneSlabHalf, 1, 4);
	CreativeInventoryScreen::populateItem(Tile::stoneSlabHalf, 1, 1);
	CreativeInventoryScreen::populateItem(Tile::stoneSlabHalf, 1, 5);
	CreativeInventoryScreen::populateItem(Tile::stoneSlabHalf, 1, 6);
	if (Tile::rockSlabHalf) CreativeInventoryScreen::populateItem(Tile::rockSlabHalf, 1, 0);
	if (Tile::dirtSlabHalf) CreativeInventoryScreen::populateItem(Tile::dirtSlabHalf, 1, 0);
	if (Tile::grassSlabHalf) CreativeInventoryScreen::populateItem(Tile::grassSlabHalf, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::quartzBlock, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::quartzBlock, 1, 2);
	CreativeInventoryScreen::populateItem(Tile::quartzBlock, 1, 1);
	CreativeInventoryScreen::populateItem(Tile::coalOre, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::ironOre, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::goldOre, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::emeraldOre, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::lapisOre, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::redStoneOre, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::netherQuartz, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::goldBlock, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::ironBlock, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::emeraldBlock, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::lapisBlock, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::coalBlock, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::obsidian, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::soulSand, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::ice, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::snow, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::topSnow, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::glass, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::lightGem, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::netherReactor, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::ladder, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::sponge, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::torch, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::thinGlass, 1, 0);
	CreativeInventoryScreen::populateItem(Item::bucket_empty, 1, 0);
	CreativeInventoryScreen::populateItem(Item::bucket_water, 1, 0);
	CreativeInventoryScreen::populateItem(Item::bucket_lava, 1, 0);
	CreativeInventoryScreen::populateItem(Item::door_wood, 1, 0);
	CreativeInventoryScreen::populateItem(Item::door_iron, 1, 0);
	if (Options::instance && Options::instance->newAdditions) {
		CreativeInventoryScreen::populateItem(Item::door_spruce, 1, 0);
		CreativeInventoryScreen::populateItem(Item::door_birch, 1, 0);
	}
	CreativeInventoryScreen::populateItem(Tile::trapdoor, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::fence, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::fenceGate, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::ironFence, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::bookshelf, 1, 0);
	CreativeInventoryScreen::populateItem(Item::sign, 1, 0);
	CreativeInventoryScreen::populateItem(Item::feather, 1, 0);
	for(int i = 0; i < 16; ++i) {
		CreativeInventoryScreen::populateItem(Item::dye_powder, 1, i);
	}
	CreativeInventoryScreen::populateItem(Item::coal, 1, 0);
	CreativeInventoryScreen::populateItem(Item::coal, 1, 1);
	CreativeInventoryScreen::populateItem(Item::ironIngot, 1, 0);
	CreativeInventoryScreen::populateItem(Item::goldIngot, 1, 0);
	CreativeInventoryScreen::populateItem(Item::emerald, 1, 0);
	CreativeInventoryScreen::populateItem(Item::netherQuartz, 1, 0);
	CreativeInventoryScreen::populateItem(Item::brick, 1, 0);
	CreativeInventoryScreen::populateItem(Item::netherbrick, 1, 0);
	CreativeInventoryScreen::populateItem(Item::painting, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::workBench, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::stonecutterBench, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::chest, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::furnace, 1, 0);
	CreativeInventoryScreen::populateItem(Item::camera, 1, 0);
	if (!(Options::instance && Options::instance->newAdditions)) {
		CreativeInventoryScreen::populateItem(Item::bed, 1, 14);
	}
	CreativeInventoryScreen::populateItem(Tile::tnt, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::redstoneLampOff, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::flower, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::rose, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::flowerRose, 1, 0);
	if (Options::instance && Options::instance->newAdditions) {
		CreativeInventoryScreen::populateItem(Tile::flowerPaeonia, 1, 0);
		CreativeInventoryScreen::populateItem(Tile::flowerDaisy, 1, 0);
		CreativeInventoryScreen::populateItem(Tile::flowerHoustonia, 1, 0);
		CreativeInventoryScreen::populateItem(Tile::flowerOrchid, 1, 0);
		CreativeInventoryScreen::populateItem(Tile::flowerAllium, 1, 0);
		CreativeInventoryScreen::populateItem(Tile::doublePlant, 1, 0);
		CreativeInventoryScreen::populateItem(Tile::doublePlant, 1, 1);
		CreativeInventoryScreen::populateItem(Tile::doublePlant, 1, 2);
		CreativeInventoryScreen::populateItem(Tile::doublePlant, 1, 3);
		CreativeInventoryScreen::populateItem(Tile::vine, 1, 0);
		CreativeInventoryScreen::populateItem(Tile::waterLily, 1, 0);
		CreativeInventoryScreen::populateItem(Tile::seagrass, 1, 0);
	}
	CreativeInventoryScreen::populateItem(Tile::mushroom1, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::mushroom2, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::cactus, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::melon, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::pumpkin, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::litPumpkin, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::web, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::hayBlock, 1, 0);
	CreativeInventoryScreen::populateItem(Item::reeds, 1, 0);
	CreativeInventoryScreen::populateItem(Item::wheat, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::tallgrass, 5, 1);
	CreativeInventoryScreen::populateItem(Tile::tallgrass, 5, 2);
	CreativeInventoryScreen::populateItem(Tile::deadBush, 1, 0);
	for(int32_t j = 0; j != 4; ++j) {
		CreativeInventoryScreen::populateItem(Tile::sapling, 1, j);
		CreativeInventoryScreen::populateItem(Tile::leaves, 1, j);
	}
	CreativeInventoryScreen::populateItem(Item::seeds_wheat, 1, 0);
	CreativeInventoryScreen::populateItem(Item::seeds_pumpkin, 1, 0);
	CreativeInventoryScreen::populateItem(Item::seeds_melon, 1, 0);
	CreativeInventoryScreen::populateItem(Item::seeds_beetroot, 1, 0);
	CreativeInventoryScreen::populateItem(Item::carrot, 1, 0);
	CreativeInventoryScreen::populateItem(Item::potato, 1, 0);
	CreativeInventoryScreen::populateItem(Item::paper, 1, 0);
	CreativeInventoryScreen::populateItem(Item::hoe_wood, 1, 0);
	CreativeInventoryScreen::populateItem(Item::hoe_stone, 1, 0);
	CreativeInventoryScreen::populateItem(Item::hoe_iron, 1, 0);
	CreativeInventoryScreen::populateItem(Item::hoe_gold, 1, 0);
	CreativeInventoryScreen::populateItem(Item::hoe_emerald, 1, 0);
	CreativeInventoryScreen::populateItem(Item::cake, 1, 0);
	CreativeInventoryScreen::populateItem(Item::egg, 1, 0);
	CreativeInventoryScreen::populateItem(Item::sword_wood, 1, 0);
	CreativeInventoryScreen::populateItem(Item::sword_stone, 1, 0);
	CreativeInventoryScreen::populateItem(Item::sword_iron, 1, 0);
	CreativeInventoryScreen::populateItem(Item::sword_gold, 1, 0);
	CreativeInventoryScreen::populateItem(Item::sword_emerald, 1, 0);
	
	CreativeInventoryScreen::populateItem(Item::pickAxe_wood, 1, 0);
	CreativeInventoryScreen::populateItem(Item::pickAxe_stone, 1, 0);
	CreativeInventoryScreen::populateItem(Item::pickAxe_iron, 1, 0);
	CreativeInventoryScreen::populateItem(Item::pickAxe_gold, 1, 0);
	CreativeInventoryScreen::populateItem(Item::pickAxe_emerald, 1, 0);

	CreativeInventoryScreen::populateItem(Item::hatchet_wood, 1, 0);
	CreativeInventoryScreen::populateItem(Item::hatchet_stone, 1, 0);
	CreativeInventoryScreen::populateItem(Item::hatchet_iron, 1, 0);
	CreativeInventoryScreen::populateItem(Item::hatchet_gold, 1, 0);
	CreativeInventoryScreen::populateItem(Item::hatchet_emerald, 1, 0);

	CreativeInventoryScreen::populateItem(Item::shovel_wood, 1, 0);
	CreativeInventoryScreen::populateItem(Item::shovel_stone, 1, 0);
	CreativeInventoryScreen::populateItem(Item::shovel_iron, 1, 0);
	CreativeInventoryScreen::populateItem(Item::shovel_gold, 1, 0);
	CreativeInventoryScreen::populateItem(Item::shovel_emerald, 1, 0);
	
	CreativeInventoryScreen::populateItem(Item::helmet_cloth, 1, 0);
	CreativeInventoryScreen::populateItem(Item::chestplate_cloth, 1, 0);
	CreativeInventoryScreen::populateItem(Item::leggings_cloth, 1, 0);
	CreativeInventoryScreen::populateItem(Item::boots_cloth, 1, 0);

	CreativeInventoryScreen::populateItem(Item::helmet_chain, 1, 0);
	CreativeInventoryScreen::populateItem(Item::chestplate_chain, 1, 0);
	CreativeInventoryScreen::populateItem(Item::leggings_chain, 1, 0);
	CreativeInventoryScreen::populateItem(Item::boots_chain, 1, 0);

	CreativeInventoryScreen::populateItem(Item::helmet_iron, 1, 0);
	CreativeInventoryScreen::populateItem(Item::chestplate_iron, 1, 0);
	CreativeInventoryScreen::populateItem(Item::leggings_iron, 1, 0);
	CreativeInventoryScreen::populateItem(Item::boots_iron, 1, 0);

	CreativeInventoryScreen::populateItem(Item::helmet_gold, 1, 0);
	CreativeInventoryScreen::populateItem(Item::chestplate_gold, 1, 0);
	CreativeInventoryScreen::populateItem(Item::leggings_gold, 1, 0);
	CreativeInventoryScreen::populateItem(Item::boots_gold, 1, 0);

	CreativeInventoryScreen::populateItem(Item::helmet_diamond, 1, 0);
	CreativeInventoryScreen::populateItem(Item::chestplate_diamond, 1, 0);
	CreativeInventoryScreen::populateItem(Item::leggings_diamond, 1, 0);
	CreativeInventoryScreen::populateItem(Item::boots_diamond, 1, 0);
	CreativeInventoryScreen::populateItem(Item::bow, 1, 0);
	CreativeInventoryScreen::populateItem(Item::flintAndSteel, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::lever, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::daylightDetector, 1, 0);
	CreativeInventoryScreen::populateItem(Item::flowerPot, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::pressurePlateStone, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::pressurePlate_cobblestone, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::pressurePlatePlanks, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::pressurePlate_spruce, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::pressurePlate_birch, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::pressurePlate_jungle, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::pressurePlate_gold, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::pressurePlate_iron, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::button_stone, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::button_cobblestone, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::button_wood, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::button_spruce, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::button_birch, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::button_jungle, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::button_gold, 1, 0);
	CreativeInventoryScreen::populateItem(Tile::button_iron, 1, 0);
	CreativeInventoryScreen::populateItem(Item::shears, 1, 0);
	CreativeInventoryScreen::populateItem(Item::clock, 1, 0);
	CreativeInventoryScreen::populateItem(Item::compass, 1, 0);
	CreativeInventoryScreen::populateItem(Item::minecart, 1, 0);
	CreativeInventoryScreen::populateItem(Item::mobPlacer, 1, 10);
	CreativeInventoryScreen::populateItem(Item::mobPlacer, 1, 11);
	CreativeInventoryScreen::populateItem(Item::mobPlacer, 1, 12);
	CreativeInventoryScreen::populateItem(Item::mobPlacer, 1, 13);
	CreativeInventoryScreen::populateItem(Item::mobPlacer, 1, 32);
	CreativeInventoryScreen::populateItem(Item::mobPlacer, 1, 33);
	CreativeInventoryScreen::populateItem(Item::mobPlacer, 1, 34);
	CreativeInventoryScreen::populateItem(Item::mobPlacer, 1, 35);
	CreativeInventoryScreen::populateItem(Item::mobPlacer, 1, 36);
	for(int i: {0, 8, 7, 0xF, 0xC, 0xE, 1, 4, 5, 0xD, 9, 3, 0xB, 0xA, 2, 6}) {
		CreativeInventoryScreen::populateItem(Tile::cloth, 1, i);
	}
	for(int i: {0, 8, 7, 0xF, 0xC, 0xE, 1, 4, 5, 0xD, 9, 3, 0xB, 0xA, 2, 6}) {
		CreativeInventoryScreen::populateItem(Tile::woolCarpet, 1, i);
	}
	if (Options::instance && Options::instance->newAdditions) {
		for(int i: {0, 8, 7, 0xF, 0xC, 0xE, 1, 4, 5, 0xD, 9, 3, 0xB, 0xA, 2, 6}) {
			CreativeInventoryScreen::populateItem(Tile::stainedGlass, 1, i);
		}
		for(int i: {0, 8, 7, 0xF, 0xC, 0xE, 1, 4, 5, 0xD, 9, 3, 0xB, 0xA, 2, 6}) {
			CreativeInventoryScreen::populateItem(Tile::stainedGlassPane, 1, i);
		}
		
		CreativeInventoryScreen::populateItem(Tile::fence_spruce, 1, 0);
		CreativeInventoryScreen::populateItem(Tile::fence_birch, 1, 0);
		CreativeInventoryScreen::populateItem(Tile::trapdoor_spruce, 1, 0);
		CreativeInventoryScreen::populateItem(Tile::trapdoor_birch, 1, 0);
		
		// 床只保留一个（红色 aux 14），不按 16 色循环铺开——
		// 否则方块 tab 里出现 16 个床，用户反馈是 bug
		CreativeInventoryScreen::populateItem(Item::bed, 1, 14);
	}
}

CreativeInventoryScreen::~CreativeInventoryScreen() {
}

void CreativeInventoryScreen::render(int32_t a2, int32_t a3, float a4) {
	this->renderBackground(0);
	this->minecraft->gui.renderToolBar(a4, 1);
	for(auto v27: this->field_98) {
		if(v27.field_4.get() != this->field_A4) {
			// 注意：不能 setActive(false) —— Button::clicked 检查 active，
			// 禁用会导致分类点不了。选中态由 CategoryButton 的 field_70 判断
			v27.field_4->render(this->minecraft, a2, a3);
			this->drawIcon(v27.field_0, v27.field_4, 1, v27.field_4->pressed);
		}
	}
	glColor4f(1.0, 1.0, 1.0, 1.0);
	this->field_68->draw(Tesselator::instance, this->field_A8, this->field_AC);
	std::shared_ptr<Touch::InventoryPane> v25 = this->field_78[this->currentPaneMaybe];
	// client InventoryPane 的 renderBatch 已画面板背景（0.8.1 的 field_228/248/24C 边框填充由 client 版内部处理）
	v25->render(a2, a3, a4);
	Screen::render(a2, a3, a4);
	if (this->armorButton) {
		float v8 = 0.0f;
		int v9 = this->field_58;
		float v10 = (float)v9;
		if (this->armorButton->pressed) {
			v8 = 2.0f;
		}
		float v13 = (float)(v10 - v8) / 25.0f;
		float v11 = (float)((float)this->armorButton->y + (float)(v10 * 0.5f)) - 8.0f;
		ItemInstance chestplate(Item::chestplate_iron);
		ItemRenderer::renderGuiItemNew(this->minecraft->textures, &chestplate, 0, (float)(this->armorButton->x + v9 / 2 - 8) + 1.0f, v11, 1.0f, 1.0f, v13);
	}
	for(auto v27: this->field_98) {
		if(v27.field_4.get() == this->field_A4) {
			v27.field_4->render(this->minecraft, a2, a3);
			this->drawIcon(v27.field_0, v27.field_4, 0, v27.field_4->pressed);
		}
	}
	this->minecraft->gui.renderOnSelectItemNameText(this->width, this->minecraft->font, this->height - 19);
}
void CreativeInventoryScreen::init()
{
	CreativeInventoryScreen::items.clear();
	for (int i = 0; i < 6; i++) {
		this->field_78[i].reset();
		CreativeInventoryScreen::filteredItems[i].clear();
	}
	if (Item::bed) {
		Item::bed->setCategory((Options::instance && Options::instance->newAdditions) ? 5 : 2);
	}
	CreativeInventoryScreen::populateItems();
	if (Options::instance && Options::instance->newAdditions) {
		for(int i: {0, 8, 7, 0xF, 0xC, 0xE, 1, 4, 5, 0xD, 9, 3, 0xB, 0xA, 2, 6}) {
			if (Tile::coloredPlanks) {
				CreativeInventoryScreen::populateItem(Tile::coloredPlanks, 1, i);
			}
		}
		if (Tile::coloredLogs) {
				CreativeInventoryScreen::populateItem(Tile::coloredLogs, 1, 0);
			}
		if (Tile::coloredStairs) {
				CreativeInventoryScreen::populateItem(Tile::coloredStairs, 1, 0);
			}
		if (Tile::coloredBrickStairs) {
				CreativeInventoryScreen::populateItem(Tile::coloredBrickStairs, 1, 0);
			}
		if (Tile::coloredFences) {
				CreativeInventoryScreen::populateItem(Tile::coloredFences, 1, 0);
			}
		for(int i: {0, 8, 7, 0xF, 0xC, 0xE, 1, 4, 5, 0xD, 9, 3, 0xB, 0xA, 2, 6}) {
			if (i < 8) {
				if (Tile::coloredSlabHalf1) {
					CreativeInventoryScreen::populateItem(Tile::coloredSlabHalf1, 1, i);
				}
				if (Tile::coloredBrickSlabHalf1) {
					CreativeInventoryScreen::populateItem(Tile::coloredBrickSlabHalf1, 1, i);
				}
			} else {
				if (Tile::coloredSlabHalf2) {
					CreativeInventoryScreen::populateItem(Tile::coloredSlabHalf2, 1, i - 8);
				}
				if (Tile::coloredBrickSlabHalf2) {
					CreativeInventoryScreen::populateItem(Tile::coloredBrickSlabHalf2, 1, i - 8);
				}
			}
		}
		for(int i: {0, 8, 7, 0xF, 0xC, 0xE, 1, 4, 5, 0xD, 9, 3, 0xB, 0xA, 2, 6}) {
			if (Tile::coloredBricks) {
				CreativeInventoryScreen::populateItem(Tile::coloredBricks, 1, i);
			}
		}
	}
	CreativeInventoryScreen::populateFilteredItems();

	NinePatchFactory v16(this->minecraft->textures, "gui/spritesheet.png");
	this->field_68 = std::shared_ptr<NinePatchLayer>(v16.createSymmetrical(IntRectangle{34, 43, 14, 14}, 3, 3, 14, 14));
	// 0.8.1 移植：tab 分类固定 6 类（无 mod 物品时 tab6 消失，只建 5 个按钮）
	bool hasModItems = ModEngine::instance && !ModEngine::instance->creativeItems().empty();
	int num_tabs = hasModItems ? 6 : 5;
	int v4 = (height - 25) / num_tabs - this->field_5C;
	if(v4 >= 30) {
		v4 = 30;
	}
	this->field_58 = v4;
	IntRectangle a5 = {this->minecraft->options.leftHanded ? 65 : 49, this->minecraft->options.leftHanded ? 55 : 43, 14, 14};
	this->field_70 = std::shared_ptr<NinePatchLayer>(v16.createSymmetrical(a5, 3, 3, v4, v4));
	this->field_98.clear();
	// 按钮从下往上排：底部 id6=tab6 mod（无 mod 不创建）→ 顶部 id11=tab1 原材料。
	// 按钮数字 = 12-id（id11→"1" 顶部 … id6→"6" 底部）。
	if (hasModItems) {
		this->field_98.emplace_back(CreativeInventoryScreen::TabButtonWithMeta(6, this->createInventoryTabButton(6, 6)));
	}
	this->field_98.emplace_back(CreativeInventoryScreen::TabButtonWithMeta(5, this->createInventoryTabButton(7, 5)));
	this->field_98.emplace_back(CreativeInventoryScreen::TabButtonWithMeta(4, this->createInventoryTabButton(8, 4)));
	this->field_98.emplace_back(CreativeInventoryScreen::TabButtonWithMeta(3, this->createInventoryTabButton(9, 3)));
	this->field_98.emplace_back(CreativeInventoryScreen::TabButtonWithMeta(2, this->createInventoryTabButton(10, 2)));
	this->field_98.emplace_back(CreativeInventoryScreen::TabButtonWithMeta(1, this->createInventoryTabButton(11, 1)));

	ImageDef v18;
	v18.name = "gui/spritesheet.png";
	v18.x = 0;
	v18.y = 1;
	v18.height = 18.0;
	v18.width = 18.0;
	v18.hasSubImage = 1;
	v18.u = 60;
	v18.v = 0;
	v18.subW = 18;
	v18.subH = 18;

	ImageWithBackground* v5 = new ImageWithBackground(5);
	v5->init(this->minecraft->textures, this->field_58, this->field_58, a5, a5, 2, 2, "gui/spritesheet.png");
	v5->width = this->field_58;
	v5->height = this->field_58 - 1;
	v5->setImageDef(v18, 0);
	this->field_60 = std::shared_ptr<ImageWithBackground>(v5);

	ImageWithBackground* vArmor = new ImageWithBackground(12);
	vArmor->init(this->minecraft->textures, this->field_58, this->field_58, a5, a5, 2, 2, "gui/spritesheet.png");
	vArmor->width = this->field_58;
	vArmor->height = this->field_58 - 1;
	this->armorButton = std::shared_ptr<ImageWithBackground>(vArmor);

	this->field_A4 = this->field_98[0].field_4.get();
	// 初始选中底部第一个 tab（有 mod=mod tab id6→pane0；无 mod=全物品 id7→pane1），
	// currentPaneMaybe 必须与 field_A4 一致，否则进入时显示空面板。
	// 若上次关闭时停在某个 tab（lastTabButtonId），且本次该 tab 按钮存在
	// （无 mod 时 id6 不会创建），则恢复上次页面。
	if (CreativeInventoryScreen::lastTabButtonId >= 6) {
		for (auto& tb : this->field_98) {
			if (tb.field_4->id == CreativeInventoryScreen::lastTabButtonId) {
				this->field_A4 = tb.field_4.get();
				break;
			}
		}
	}
	this->currentPaneMaybe = this->field_A4->id - 6;
	this->buttons.clear();
	for(auto p: this->field_98) {
		this->buttons.emplace_back(p.field_4.get());
	}
	this->buttons.emplace_back(this->field_60.get());
	this->buttons.emplace_back(this->armorButton.get());
	this->field_BC = 1;
}
void CreativeInventoryScreen::setupPositions()
{
	this->field_68->setSize((float)((float)this->width - 4.0) - (float)this->field_58 * 2.0f, (float)this->height - 25.0);
	int v3 = this->field_58 + 2;
	this->field_AC = 2;
	this->field_A8 = v3;
	this->field_B0 = 0;
	int v6 = ~this->field_60->height + (int)this->field_68->getHeight();
	float v7;
	if(this->minecraft->options.leftHanded) {
		v7 = (float)((float)v3 + this->field_68->getWidth()) + 2.0;
	} else {
		v7 = (float)(v3 - this->field_58 + 3);
	}
	this->field_60->x = (int)v7;
	this->field_60->y = this->field_AC;

	float rightX = this->minecraft->options.leftHanded ? 2.0f : (float)((float)v3 + this->field_68->getWidth()) + 2.0f;
	if (this->armorButton) {
		this->armorButton->x = (int)rightX;
		this->armorButton->y = this->field_AC;
		this->armorButton->width = this->field_58;
		this->armorButton->height = this->field_58 - 1;
	}

	for(auto&& p: this->field_98) {
		p.field_4->x = (int)v7;
		p.field_4->y = v6;
		p.field_4->width = this->field_58;
		p.field_4->height = this->field_58;
		v6 -= this->field_58 + this->field_5C;
	}

	if(!this->field_78[0].get()) {
		int v14 = (int)this->field_68->getWidth() - 14;
		int v15 = this->field_A8;
		IntRectangle r3_0;
		r3_0.y = this->field_AC + 8;
		r3_0.w = 26 * (v14 / 26);
		this->field_B8 = v14 % 26 / 2;
		int height2 = (int)this->field_68->getHeight();
		r3_0.x = v14 % 26 / 2 + v15 + 7;
		r3_0.h = height2 - 16;
		int v17 = CreativeInventoryScreen::filteredItems[0].size();

		// 0.8.1 移植：始终创建 6 个 pane（pane 0 = mod，无 mod 时为空也不碍事）
		for (int pi = 0; pi < 6; ++pi) {
			int cnt = (int)CreativeInventoryScreen::filteredItems[pi].size();
			this->field_78[pi] = std::shared_ptr<Touch::InventoryPane>(new Touch::InventoryPane(this, this->minecraft, r3_0, r3_0.w, 1, cnt, 26, 1));
		}
	}
}
bool_t CreativeInventoryScreen::handleBackEvent(bool_t a2) {
	if(!a2) {
		this->closeWindow();
	}
	return 1;
}
void CreativeInventoryScreen::tick() {
	this->field_78[this->currentPaneMaybe]->tick();
}
bool CreativeInventoryScreen::renderGameBehind() {
	// 0.8.1 创造背包默认透出游戏画面：世界 + 底部快捷栏在背包后渲染，
	// 背包自身背景只是半透明遮罩（Screen::renderBackground 的 renderGameBehind
	// 分支），不再平铺泥土背景挡住世界。
	return true;
}
void CreativeInventoryScreen::buttonClicked(Button* a2) {
	if(a2 == this->field_60.get()) {
		this->closeWindow();
	} else if(a2 == this->armorButton.get() || a2->id == 12) {
		this->minecraft->setScreen(new ArmorScreen());
	} else {
		if(a2->id >= 6 && a2->id <= 11) {
			this->field_A4 = a2;
			this->currentPaneMaybe = a2->id - 6;
			CreativeInventoryScreen::lastTabButtonId = a2->id;
			this->setupPositions();
		}
	}
}
void CreativeInventoryScreen::mouseClicked(int32_t a2, int32_t a3, int32_t a4) {
	// 点底部快捷栏槽 = 切换选中槽（类似手机版点底部物品栏），
	// 方便把背包/分类格子里的物品直接放进指定快捷槽（addItem 用 selectedSlot）。
	if (a4 == 1 /* MouseAction::ACTION_LEFT */ && this->minecraft->player && this->minecraft->player->inventory) {
		Gui& gui = this->minecraft->gui;
		int num = gui.getNumSlots() - 1;
		if (num > 0) {
			int slotX = 0, slotY = 0;
			gui.getSlotPos(0, slotX, slotY);
			// 快捷栏框高 22（Gui::renderToolBar 的槽位），命中行放宽 2px
			if (a3 >= slotY - 2 && a3 <= slotY + 22) {
				for (int i = 0; i < num; ++i) {
					int px = 0, py = 0;
					gui.getSlotPos(i, px, py);
					if (a2 >= px && a2 < px + 20) {
						this->minecraft->player->inventory->selectSlot(i);
						return; // 命中快捷栏，不再向下分发（避免同时触发格子拿取）
					}
				}
			}
		}
	}
	Screen::mouseClicked(a2, a3, a4);
	// 注意：不能调 gui.handleClick —— 其内置的"聊天按钮"区域（右上角 60x20）
	// 与盔甲按钮位置重叠，会导致点盔甲时误开聊天
}

void CreativeInventoryScreen::onMouseWheel(int dy) {
	// 背包打开时保留游戏内的滚轮习惯：切换快捷栏选中槽。
	// 无 screen 时该逻辑在 Minecraft::pollMouse 里做，有 screen 则分发到这里。
	Inventory* inv = this->minecraft->player ? this->minecraft->player->inventory : NULL;
	if (!inv) return;
	int num = this->minecraft->gui.getNumSlots() - 1;
	if (num <= 0) return;
	int slot = (inv->selected - dy + num) % num;
	if (slot < 0) slot += num;
	inv->selectSlot(slot);
}
void CreativeInventoryScreen::mouseReleased(int32_t a2, int32_t a3, int32_t a4) {
	Screen::mouseReleased(a2, a3, a4);
	for(auto&& p: this->field_98) {
		p.field_4->released(a2, a3);
	}
}
bool CreativeInventoryScreen::addItem(const Touch::InventoryPane* a2, int32_t a3) {
	std::vector<ItemInstance>* v7 = &filteredItems[this->getCategoryFromPanel(a2)];
	if(a3 >= v7->size()) {
		return 0;
	}
	const ItemInstance* v8 = &v7->at(a3);
	int32_t id = v8->getId();
	int32_t auxv = v8->getAuxValue();
	int32_t slot = this->minecraft->player->inventory->getLinkedSlotForItemAndAux(id, auxv);
	if(slot < 0 || slot >= this->minecraft->gui.getNumSlots() - 1) {
		// 创造拿取：放入选中快捷栏格（0.8.1 link 机制：物品放主物品栏
		// 槽，linkSlot 后快捷栏 getItem→getLinked 读它）。
		// 注意：不能盲写 selectedSlot()+9 —— setupDefault 里 Sel 布局
		// 并非"第 i 格固定链 i+9"，盲写会覆盖其它快捷格正链着的主槽，
		// 造成两个快捷格读到同一 items 槽（拿一次物品第 1、2 格都出现）。
		// 正确做法：优先复用选中格当前链的主槽（若空）；否则找第一个
		// 真正空闲且未被任何快捷格引用的主槽放入并 link。
		Inventory* inv = this->minecraft->player->inventory;
		int sel = inv->selectedSlot();
		ItemInstance v15(*v8);
		int mainSlot = -1;

		// 判断主槽是否被"其它"快捷格引用
		auto slotReferencedByOther = [&](int s) -> bool {
			for (int k = 0; k < inv->numLinkedSlots; ++k) {
				if (k != sel && inv->linkedSlots[k].inventorySlot == s) return true;
			}
			return false;
		};

		// 1) 选中格当前链的主槽若空闲且未被其它格引用 → 直接放那里
		int curLinked = inv->linkedSlots[sel].inventorySlot;
		if (curLinked >= inv->numLinkedSlots && curLinked < inv->numTotalSlots) {
			ItemInstance* existing = inv->getItem(curLinked);
			bool curEmpty = (!existing || existing->isNull()) && !slotReferencedByOther(curLinked);
			if (curEmpty)
				mainSlot = curLinked;
		}

		// 2) 否则扫描主物品栏找空闲且未被任何快捷格引用的槽
		if (mainSlot < 0) {
			for (int i = inv->numLinkedSlots; i < inv->numTotalSlots; ++i) {
				ItemInstance* it = inv->getItem(i);
				if (it && !it->isNull()) continue;
				bool referenced = false;
				for (int k = 0; k < inv->numLinkedSlots; ++k) {
					if (inv->linkedSlots[k].inventorySlot == i) { referenced = true; break; }
				}
				if (!referenced) { mainSlot = i; break; }
			}
		}

		// 3) 兜底：全满或全被引用时，找第一个空闲主槽（允许引用，
		//    把该引用格一并切走即可避免双显）；再无则覆盖选中格链槽
		if (mainSlot < 0) {
			for (int i = inv->numLinkedSlots; i < inv->numTotalSlots; ++i) {
				ItemInstance* it = inv->getItem(i);
				if (!it || it->isNull()) { mainSlot = i; break; }
			}
		}
		if (mainSlot < 0) {
			mainSlot = (curLinked >= inv->numLinkedSlots && curLinked < inv->numTotalSlots)
				? curLinked : (sel + Inventory::MAX_SELECTION_SIZE);
		}

		inv->setItem(mainSlot, &v15);
		if (mainSlot != curLinked)
			inv->linkSlot(sel, mainSlot);
	} else {
		this->minecraft->player->inventory->selectedSlot() = slot;
	}
	this->minecraft->gui.resetItemNameOverlay();
	this->minecraft->gui.flashSlot(this->minecraft->player->inventory->selectedSlot());
	return 1;
}
bool CreativeInventoryScreen::isAllowed(int32_t) {
	return 1;
}
std::vector<const ItemInstance*> CreativeInventoryScreen::getItems(const Touch::InventoryPane* a3) {
	int32_t cat = this->getCategoryFromPanel(a3);
	int32_t v5 = CreativeInventoryScreen::filteredItems[cat].size();
	std::vector<const ItemInstance*> vec(v5);
	for(int32_t i = 0; i < v5; ++i) {
		vec[i] = &CreativeInventoryScreen::filteredItems[cat][i];
	}
	return vec;
}