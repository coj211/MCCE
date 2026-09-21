#ifndef NET_MINECRAFT_CLIENT_RENDERER_ENTITY__ItemRenderer_H__
#define NET_MINECRAFT_CLIENT_RENDERER_ENTITY__ItemRenderer_H__

//package net.minecraft.client.renderer.entity;

#include "EntityRenderer.h"
#include "../../../util/Random.h"

class Font;
class Entity;
class ItemInstance;
class Textures;
class Tesselator;
class TileRenderer;

class ItemRenderer: public EntityRenderer
{
public:
    ItemRenderer();

    void render(Entity* itemEntity_, float x, float y, float z, float rot, float a);
    static void renderGuiItem(Font* font, Textures* textures, const ItemInstance* item, float x, float y, bool fancy);
	static void renderGuiItem(Font* font, Textures* textures, const ItemInstance* item, float x, float y, float w, float h, bool fancy);
	static void renderGuiItemCorrect(Font* font, Textures* textures, const ItemInstance* item, int x, int y);
	//void renderGuiItemDecorations(Font* font, Textures* textures, ItemInstance* item, int x, int y);
	static void renderGuiItemDecorations(const ItemInstance* item, float x, float y);

	// 0.8.1 背包渲染管线（Touch::InventoryPane / CreativeInventoryScreen 用）
	static void iconBlit(float x, float y, float sx, float sy, float w, float h, float alpha, int color, float scale, float texHeight);
	static void renderGuiItemNew(Textures* textures, const ItemInstance* item, int a3, float x, float y, float alpha, float brightness, float scale);
	static void renderGuiItemInChunk(int chunkType, Textures* textures, const ItemInstance* item, float x, float y, float alpha, float brightness, float scale);

	// 物品栏/背包里的模组物品 3D 模型（Item.defineItem 的 model）：画了返回 true
	// （调用方就当这次渲染已完成），没模型返回 false（调用方走原来的 2D 图标）。
	// cpuTransform = 顶点收集模式（0.8.1 背包）下必须用 Tesselator 的 CPU 变换；
	// glPushMatrix 在收集模式里是无效的（顶点绕开矩阵直接进 buffer）。
	static bool renderGuiModItemModel(Textures* textures, const ItemInstance* item, float x, float y, bool cpuTransform);

	static void blit(float x, float y, float sx, float sy, float w, float h, float texHeight);
	static int  getAtlasPos(const ItemInstance* item);
	// 09c · 背包方块图标：返回方块/物品 id 在 gui_blocks.png 里的槽位
	// （<0 = 无图集图标，走 terrain/items 渲染）。供 mod replaceBlockIcon 用。
	static int  getGuiBlocksSlot(int itemId);

	static void teardown_static();
private:
	static void fillRect(Tesselator& t, float x, float y, float w, float h, int c);
	static TileRenderer* tileRenderer;
	Random random;
};

#endif /*NET_MINECRAFT_CLIENT_RENDERER_ENTITY__ItemRenderer_H__*/
