#include <gui/pane/Touch_InventoryPane.hpp>
#include <Minecraft.hpp>
#include <gui/Gui.hpp>
#include <gui/pane/Touch_IInventoryPaneCallback.hpp>
#include <item/ItemInstance.hpp>
#include <rendering/Tesselator.hpp>
#include <rendering/Textures.hpp>
#include <rendering/TileRenderer.hpp>
#include <rendering/entity/ItemRenderer.hpp>
#include <rendering/states/DisableState.hpp>
#include <rendering/states/EnableClientState.hpp>
#include <string.h>
#include <tile/Tile.hpp>

Touch::InventoryPane::InventoryPane(Touch::IInventoryPaneCallback* a2, Minecraft* a3, const IntRectangle& a4, int32_t a5, float a6, int32_t a7, int32_t a8, int32_t a9, bool_t a10)
	: ScrollingPane(1537, a4, {0, 0, a8, a8}, 0, a7, Gui::GuiScale, {0, 0, 0, 0}) {
	this->field_228 = a4;
	this->callback = a2;
	this->minecraft = a3;
	this->field_238 = a5;
	this->field_248 = 2;
	this->field_24C = 4;
	this->field_254 = -1;
	this->field_25C = -1;
	this->invTicker = -1;
	this->field_250 = 1;
	this->field_258 = 0.0;
	this->field_264 = a9;
	this->renderDecorations = 1;
	this->field_2E4 = 0;
	this->field_2E8 = 0;
	this->field_2F4 = a10;
	this->field_2EC = 1;
	this->field_2F0 = 1;
	this->field_23C = NULL;  // 0.8.1 的触摸区，client 版由 ScrollingPane::area 承担
	// 触摸区直接使用 ScrollingPane 构造时建的 area（client 版无 0.8.1 的 field_5C 扩边）

	AppPlatform::_singleton->addListener(this, 1.0);
}

void Touch::InventoryPane::buildInventoryItemsChunk(ItemRenderChunkType a2) {
	std::vector<const ItemInstance*> v10 = this->callback->getItems(this);
	for(uint32_t i = 0; i < v10.size(); ++i) {
		const ItemInstance* v5 = v10[i];
		if(v5) {
			float v6 = (float)this->itemBbox.h * (float)(i / this->columns);
			float v7 = Gui::floorAlignToScreenPixel((float)((float)((float)this->field_264 + (float)((float)this->itemBbox.w * (float)(i % this->columns))) + 4.0) + (float)this->field_228.x);
			float v8 = Gui::floorAlignToScreenPixel((float)((float)(v6 + (float)this->field_264) + 4.0) + (float)this->field_228.y);
			float v9;
			if(this->callback->isAllowed(i)) {
				v9 = 1.0;
			} else {
				v9 = 0.25;
			}
			ItemRenderer::renderGuiItemInChunk(a2, this->minecraft->textures, v5, v7, v8, v9, 1.0, 1.0);
			;
		}
	}
}
void Touch::InventoryPane::drawScrollBar(ScrollBar& a2) {
	float color; // s13

	color = a2.alpha;
	if(color > 0.0) {
		this->fill((float)(this->bbox.x + this->bbox.w) - a2.w, a2.y, (float)(this->bbox.x + this->bbox.w), a2.y + a2.h, ((int32_t)(float)(color * 255.0) << 24) | 0xAAAAAA);
	}
}
void Touch::InventoryPane::renderSelectedItem(std::vector<ScrollingPane::GridItem>& a2, std::vector<const ItemInstance*> a3, Tesselator& a4, ScrollingPane::GridItem*& a5, float& a6, float& a7) {
	int32_t v11;			 // r1
	int32_t v12;			 // r0
	int32_t v13;			 // r5
	const ItemInstance* v14; // r7
	float v15;				 // s18
	int32_t v16;			 // s17
	float v17;				 // s15
	Tile* tileClass;		 // r0
	float v19;				 // s18
	int32_t v20;			 // r0
	float v21;				 // s17
	float v22;				 // s16

	v11 = 0;
	v12 = a2.size();
	while(v11 != v12) {
		v13 = a2[v11].id;
		if(a2[v11].id == this->field_25C && this->invTicker > 0) {
			v14 = a3[v13];
			if(v14) {
				a4.beginOverride();
				v15 = (float)this->itemBbox.h * (float)(v13 / this->columns);
				v16 = (int32_t)Gui::floorAlignToScreenPixel((float)((float)((float)this->field_264 + (float)((float)this->itemBbox.w * (float)(v13 % this->columns))) + 4.0) + (float)this->field_228.x);
				v17 = Gui::floorAlignToScreenPixel((float)((float)(v15 + (float)this->field_264) + 4.0) + (float)this->field_228.y);
				tileClass = (v14->id < 256) ? Tile::tiles[v14->id] : NULL;
				v19 = (float)(-15 * this->invTicker + 255) / 255.0;
				if(tileClass && TileRenderer::canRender(tileClass->getRenderShape())) {
					this->minecraft->textures->loadAndBindTexture("terrain-atlas.tga");
				} else {
					tileClass = NULL;
					this->minecraft->textures->loadAndBindTexture("items-opaque.png");
				}
				v21 = (float)v16;
				v22 = (float)(int32_t)v17;
				ItemRenderer::renderGuiItemInChunk(IRCT_THREE, this->minecraft->textures, v14, v21, v22, v19, 1.0, 1.0);
				if(v13 == this->field_254 && this->field_258 >= 0.0) {
					a5 = &a2[v11];
					a6 = v21;
					a7 = v22;
				}
				a4.endOverrideAndDraw();
				return;
			}
		}
		++v11;
	}
}
void Touch::InventoryPane::setRenderDecorations(bool_t a2) {
	this->renderDecorations = a2;
}
void Touch::InventoryPane::tick() {
	--this->invTicker;
	ScrollingPane::tick();
}
Touch::InventoryPane::~InventoryPane() {
	AppPlatform::_singleton->removeListener(this);
	if(this->field_23C) delete this->field_23C;
}
void Touch::InventoryPane::renderBatch(std::vector<ScrollingPane::GridItem>& a2, float a3) {
	// 0.8.1 的 renderBatch(items, alpha, xOffset, yOffset) 是 4 参；client 版只有
	// (items, alpha)，滚动偏移在 client 里通过 _contentOffset 维护，且 items 的
	// xf/yf 已经包含偏移，所以这里无需额外平移。
	this->fill((float)(this->bbox.x - this->field_248 - 1), (float)(this->bbox.y - this->field_24C), (float)(this->bbox.x + this->bbox.w + this->field_248 + 1), (float)(this->field_24C + this->bbox.y + this->bbox.h), 0xFF333333);
	std::vector<const ItemInstance*> v40 = this->callback->getItems(this);
	glEnable(0xC11u);
	float v9 = this->screenScale;
	glScissor((unsigned int)(float)(v9 * (float)this->bbox.x), this->minecraft->height - (unsigned int)(float)(v9 * (float)(this->bbox.h + this->bbox.y)), (unsigned int)(float)(v9 * (float)this->bbox.w), (unsigned int)(float)(v9 * (float)this->bbox.h));
	Tesselator::instance.beginOverride();
	Tesselator::instance.colorABGR(0xFFFFFFFF);
	for(int v8 = 0; v8 < a2.size(); ++v8) {
		ScrollingPane::GridItem* v10 = &a2[v8];
		this->blit(v10->xf, v10->yf, 200, 46, (float)this->itemBbox.w, (float)this->itemBbox.h, 16, 16);
	}
	this->minecraft->textures->loadAndBindTexture("gui/gui.png");
	Tesselator::instance.endOverrideAndDraw();

	ScrollingPane::GridItem* v35 = 0;
	float v36 = 0, v37 = 0;
	for(int v12 = 0; v12 < a2.size(); ++v12) {
		ScrollingPane::GridItem* v13 = &a2[v12];
		if(v13->id >= 0 && v13->id < (int32_t)v40.size() && v40[v13->id]) {
			int v34 = v13->id;
			float v32 = Gui::floorAlignToScreenPixel((float)((float)this->field_264 + v13->xf) + 4.0);
			float v14 = Gui::floorAlignToScreenPixel((float)((float)this->field_264 + v13->yf) + 4.0);
			if(v34 == this->field_254 && this->field_258 >= 0.0) {
				v35 = v13;
				v36 = v32;
				v37 = v14;
			}
		}
	}

	{
		EnableClientState v38(32888);
		EnableClientState v39(32886);
		glPushMatrix();
		// client 的 GridItem.xf/yf 已含滚动偏移（见 ScrollingPane::render 的 bxx/byy），
		// 无需 0.8.1 的 glTranslatef(-a4,-a5) 平移。
		// 0.8.1 原代码绑 terrain-atlas.tga / items-opaque.png —— win32 无这两个文件
		// (InvalidId 不换绑定)，field_26C/294 的 3D 方块顶点(terrain UV) 与
		// field_2BC 的物品顶点(items UV) 会用残留纹理采样 → 方块显示旧贴图/错乱。
		// 改为绑定真实图集：方块=terrain.png（mod replaceImage 覆盖也作用于它）。
		this->minecraft->textures->loadAndBindTexture("terrain.png");
		this->field_26C.render();
		this->field_294.render();
		this->minecraft->textures->loadAndBindTexture("gui/items.png");
		this->field_2BC.render();
		this->renderSelectedItem(a2, v40, Tesselator::instance, v35, v36, v37);
		glPopMatrix();
	}

	if(v35) {
		{
			DisableState v41(0xDE1); //useless statement?
		}
		float v19 = v37 - 5.0;
		float v20 = (float)(v37 - 5.0) + 2.0;
		this->fill(v36, v37 - 5.0, v36 + 16.0, v20, -10461088);
		int v21;
		if(this->field_250 == 1) {
			v21 = 0xFF00FF00;
		} else {
			v21 = 0xFF476543;
		}
		this->fill(v36, v19, v36 + (float)(this->field_258 * 16.0), v20, v21);
	}
	if(!this->minecraft->isCreativeMode()) {
		float v23 = Gui::GuiScale * 0.5;
		Tesselator::instance.scale2d(Gui::InvGuiScale + Gui::InvGuiScale, Gui::InvGuiScale + Gui::InvGuiScale);
		for(int v22 = 0; v22 < a2.size(); ++v22) { //should be uint??
			ScrollingPane::GridItem* v24 = &a2[v22];
			if(v24->id >= 0 && v24->id < (int32_t)v40.size()) {
				const ItemInstance* v25 = v40[v24->id];
				if(v25) {
					char v42[64];
					memset(v42, 0, sizeof(v42));
					Gui::itemCountItoa(v42, v25->count);
					float v33 = Gui::floorAlignToScreenPixel(v23 * (float)((float)((float)this->field_264 + v24->xf) + 3.0));
					float v26 = Gui::floorAlignToScreenPixel(v23 * (float)((float)((float)this->field_264 + v24->yf) + 3.0));
					this->minecraft->gui.renderSlotText(v25, v33, v26, 1, 1);
				}
			}
		}
		Tesselator::instance.resetScale();
	}

	if(this->renderDecorations) {
		DisableState v38(3553);
		EnableClientState v39(32886);
		for(int v27 = 0; v27 < a2.size(); ++v27) {
			ScrollingPane::GridItem* v28 = &a2[v27];
			if(v28->id >= 0 && v28->id < (int32_t)v40.size()) {
				const ItemInstance* v29 = v40[v28->id];
				if(v29) {
					if(!v29->isNull() && v29->isDamaged()) {
						ItemRenderer::renderGuiItemDecorations(v29, v28->xf + 7.0, v28->yf + 11.0);
					}
				}
			}
		}
		DisableState v41(3553);
	}
	glDisable(0xC11u);
	if(this->field_2F4) {
		this->fillGradient(this->field_2E4 - this->field_248, this->bbox.y, this->field_248 + this->field_2EC, this->bbox.y + 20, 0x99000000, 0);
		int v31 = this->bbox.y + this->bbox.h;
		this->fillGradient(this->field_2E4 - this->field_248, v31 - 20, this->field_248 + this->field_2EC, v31, 0, 0x99000000);
	}
	this->drawScrollBar(this->vScroll);
	this->drawScrollBar(this->hScroll);
}
bool_t Touch::InventoryPane::onSelect(int32_t a2, bool_t a3) {
	if(this->callback->isAllowed(a2) && this->callback->addItem(this, a2)) {
		this->field_25C = a2;
		this->invTicker = 7;
	}
	return 0;
}
void Touch::InventoryPane::refreshItems() {
	Tesselator::instance.beginOverride();
	this->buildInventoryItemsChunk(IRCT_NULL);
	Tesselator::instance.voidBeginAndEndCalls(0);
	this->field_26C = Tesselator::instance.end();
	Tesselator::instance.beginOverride();
	this->buildInventoryItemsChunk(IRCT_ONE);
	Tesselator::instance.voidBeginAndEndCalls(0);
	this->field_294 = Tesselator::instance.end();

	Tesselator::instance.beginOverride();
	this->buildInventoryItemsChunk(IRCT_TWO);
	Tesselator::instance.voidBeginAndEndCalls(0);
	this->field_2BC = Tesselator::instance.end();
}
void Touch::InventoryPane::onAppSuspended() {
	this->field_26C.reset();
	this->field_294.reset();
	this->field_2BC.reset();
}
void Touch::InventoryPane::onAppResumed() {
	this->refreshItems();
}
