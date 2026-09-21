#pragma once
#include <gui/Screen.hpp>
#include <gui/pane/Touch_IInventoryPaneCallback.hpp>
#include <memory>

struct ImageWithBackground;
struct NinePatchLayer;
class Item;
class Tile;
struct ImageButton;
struct CategoryButton;
namespace Touch {
	struct InventoryPane;
}
struct CreativeInventoryScreen: Screen, Touch::IInventoryPaneCallback
{
	struct TabButtonWithMeta
	{
		int32_t field_0;
		std::shared_ptr<ImageButton> field_4;
		TabButtonWithMeta(int f0, std::shared_ptr<ImageButton> f4);
		TabButtonWithMeta(const CreativeInventoryScreen::TabButtonWithMeta&);
		TabButtonWithMeta(CreativeInventoryScreen::TabButtonWithMeta&&);
		~TabButtonWithMeta();
	};
	static std::vector<ItemInstance> filteredItems[6];
	static std::vector<ItemInstance> items;
	// 上次关闭背包时选中的 tab 按钮 id（6..11，无 mod 时无 6）；
	// 重新打开背包时恢复到上次页面而不是每次重置到第一个 tab。
	static int lastTabButtonId;

	int32_t field_58, field_5C;
	std::shared_ptr<ImageWithBackground> field_60;
	std::shared_ptr<ImageWithBackground> armorButton;
	std::shared_ptr<NinePatchLayer> field_68;
	std::shared_ptr<NinePatchLayer> field_70;
	std::shared_ptr<Touch::InventoryPane> field_78[6];
	std::vector<CreativeInventoryScreen::TabButtonWithMeta> field_98;
	Button* field_A4;
	int32_t field_A8, field_AC, field_B0;
	int32_t currentPaneMaybe;
	int32_t field_B8;
	int8_t field_BC, field_BD, field_BE, field_BF;

	CreativeInventoryScreen();
	void _putItemInToolbar(const ItemInstance*);
	void closeWindow();
	std::shared_ptr<ImageButton> createInventoryTabButton(int32_t, int32_t);
	void drawIcon(int, std::shared_ptr<ImageButton>, bool_t, bool_t);
	int32_t getCategoryFromPanel(const Touch::InventoryPane*);
	ItemInstance getItemFromType(int32_t);
	static void populateFilteredItems();
	static void populateItem(Item*, int32_t, int32_t);
	static void populateItem(Tile*, int32_t, int32_t);
	static void populateItem(const Tile*, int32_t, int32_t);
	static void populateItems();

	virtual ~CreativeInventoryScreen();
	virtual void render(int32_t, int32_t, float);
	virtual void init();
	virtual void setupPositions();
	virtual bool_t handleBackEvent(bool_t);
	virtual void tick();
	virtual bool renderGameBehind();
	virtual void buttonClicked(Button*);
	virtual void mouseClicked(int32_t, int32_t, int32_t);
	virtual void mouseReleased(int32_t, int32_t, int32_t);
	virtual void onMouseWheel(int dy);
	virtual bool addItem(const Touch::InventoryPane*, int32_t);
	virtual bool isAllowed(int32_t);
	virtual std::vector<const ItemInstance*> getItems(const Touch::InventoryPane*);
};
