#include "ItemRenderer.h"
#include "EntityRenderDispatcher.h"
#include "../Tesselator.h"
#include "../TileRenderer.h"
#include "../Textures.h"
#include "../../gui/Font.h"
#include "../../../world/entity/item/ItemEntity.h"
#include "../../../world/item/ItemInstance.h"
#include "../../../world/level/material/Material.h"
#include "../../../world/Facing.h"
#include "../../../world/item/TileItem.h"
#include "../../../world/item/BucketItem.h"
#include "../../../world/level/tile/Tile.h"
#include "../../../util/Mth.h"
#include "../../../util/Random.h"
#include "EntityRenderer.h"
#include "../ItemInHandRenderer.h"
#include "../../gui/Gui.h"
#include "../../../world/item/Item.h"
#if defined(_WIN32) || defined(__ANDROID__)
// 模组物品的自定义 3D 模型（Item.defineItem 的 model / modelTexture）：
// 手持和掉落物早就走这条了，物品栏现在也走（见 renderGuiModItemModel）。
#include "../../mod/ModEngine.h"
#endif

/*static*/
TileRenderer* ItemRenderer::tileRenderer = new TileRenderer();

ItemRenderer::ItemRenderer()
{
	shadowRadius = 0.15f;
	shadowStrength = 0.75f;
}

void ItemRenderer::teardown_static() {
	if (tileRenderer) {
		delete tileRenderer;
		tileRenderer = NULL;
	}
}

void ItemRenderer::render(Entity* itemEntity_, float x, float y, float z, float rot, float a) {
	ItemEntity* itemEntity = (ItemEntity*) itemEntity_;
	random.setSeed(187);
	ItemInstance* item = &itemEntity->item;

	glPushMatrix2();
	float bob = Mth::sin((itemEntity->age + a) / 10.0f + itemEntity->bobOffs) * 0.1f + 0.1f;
	float spin = ((itemEntity->age + a) / 20.0f + itemEntity->bobOffs) * Mth::RADDEG;

	int count = 1;
	if (item->count > 20) count = 4;
	else if (item->count > 5) count = 3;
	else if (item->count > 1) count = 2;

	glTranslatef2((float) x, (float) y + bob, (float) z);
	//glEnable2(GL_RESCALE_NORMAL);
	// 模组物品（id>=256）带自定义 3D 模型时，和方块一样走 3D（renderItem 内部
	// 自己就会选模组模型那条路）；没有模型的模组物品仍是下面那张 2D 图标。
	bool modDropModel = false;
#if defined(_WIN32) || defined(__ANDROID__)
	if (item->id >= 256 && ModEngine::instance) {
		const std::vector<ModBlockPart>* mp = ModEngine::instance->itemModelParts(item->id);
		modDropModel = (mp != NULL && !mp->empty());
	}
#endif
	if ((item->id < 256 && Tile::tiles[item->id] != NULL && TileRenderer::canRender(Tile::tiles[item->id]->getRenderShape())) || modDropModel) {
		glRotatef2(spin, 0, 1, 0);

		float br = itemEntity->getBrightness(a);
		if (item->id == Tile::sand->id || item->id == Tile::sandStone->id) br *= 0.8f;
		glColor4f2(br, br, br, 1.0f);

		bindTexture("terrain.png");
		float s = 1 / 4.0f;
		//if (!Tile::tiles[item->id]->isCubeShaped() && item->id != Tile::stoneSlabHalf->id) {
		if (item->id < 256) {
			const int shape = Tile::tiles[item->id]->getRenderShape();
			if (shape == Tile::SHAPE_CROSS_TEXTURE || shape == Tile::SHAPE_TORCH)
				s = 0.5f;
		}
		glScalef2(s, s, s);
		for (int i = 0; i < count; i++) {
			if (i > 0) {
				glPushMatrix2();
				float xo = (random.nextFloat() * 2 - 1) * 0.2f / s;
				float yo = (random.nextFloat() * 2 - 1) * 0.2f / s;
				float zo = (random.nextFloat() * 2 - 1) * 0.2f / s;
				glTranslatef2(xo, yo, zo);
			}
			//static Stopwatch w;
			//w.start();
			entityRenderDispatcher->itemInHandRenderer->renderItem(NULL, item);
			//tileRenderer->renderTile(Tile::tiles[item->id], item->getAuxValue());
			//w.stop();
			//w.printEvery(100, "render-item");
			if (i > 0) glPopMatrix2();
		}
	} else {
		glScalef2(1 / 2.0f, 1 / 2.0f, 1 / 2.0f);
		int icon = item->getIcon();
		if (item->id < 256) {
			bindTexture("terrain.png");
		} else {
			bindTexture("gui/items.png");
		}
		Tesselator& t = Tesselator::instance;

		float u0 = ((icon % 16) * 16 + 0) / 256.0f;
		float u1 = ((icon % 16) * 16 + 16) / 256.0f;
		// terrain.png is 256x512 (16 cols x 32 rows); gui/items.png stays 256x256.
		float vScale = (item->id < 256) ? 512.0f : 256.0f;
		float v0 = ((icon / 16) * 16 + 0) / vScale;
		float v1 = ((icon / 16) * 16 + 16) / vScale;

		float r = 1.0f;
		float xo = 0.5f;
		float yo = 0.25f;

		// glRotatef2(-playerRotX, 1, 0, 0);
		for (int i = 0; i < count; i++) {
			glPushMatrix2();
			if (i > 0) {
				float _xo = (random.nextFloat() * 2 - 1) * 0.3f;
				float _yo = (random.nextFloat() * 2 - 1) * 0.3f;
				float _zo = (random.nextFloat() * 2 - 1) * 0.3f;
				glTranslatef2(_xo, _yo, _zo);
			}
			glRotatef2(180 - entityRenderDispatcher->playerRotY, 0, 1, 0);
			t.begin();
			//t.normal(0, 1, 0);
			t.vertexUV(0 - xo, 0 - yo, 0, u0, v1);
			t.vertexUV(r - xo, 0 - yo, 0, u1, v1);
			t.vertexUV(r - xo, 1 - yo, 0, u1, v0);
			t.vertexUV(0 - xo, 1 - yo, 0, u0, v0);
			//t.end();
			t.draw();

			glPopMatrix2();
		}
	}
	//glDisable2(GL_RESCALE_NORMAL);
	glPopMatrix2();
}


// @note: _18 -> a,b,c,-1,   a,b,c-1, ...
static const signed short _6[] = {139, 140, 141, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
static const signed short _17[] = {16, 17, 18, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
static const signed short _18[] = {79, 80, 81, -1, 79, 80, 81, -1, 79, 80, 81, -1, 79, 80, 81, -1};
static const signed short _24[] = {11, 12, 13, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
static const signed short _35[] = {52, 59, 58, 57, 56, 55, 54, 53, 67, 66, 65, 64, 63, 62, 61, 60};
static const signed short _44[] = {28, 32, 30, 29, 31, 33, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
static const signed short _98[] = {1, 2, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
static const signed short _155[] = {34, 36, 35, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
static const signed short _263[] = {230, 151, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
static const signed short _351[] = {-1, 152, 154, -1, 193, 215, 216, -1, -1, 217, 218, 219, 220, 221, 222, 144};

static const signed short _mapper[] = {-1, 7, 9, 8, 0, 5, -2, -1, -1, -1, -1, -1, 14, 15, 39, 38, 37, -2, -2, -1, 49, 41, 46, -1, -2, -1, -1, -1, -1, -1, 235, -1, -1, -1, -1, -2, -1, 134, 135, 136, 137, 43, 44, -1, -2, 6, 76, 71, 4, 47, 129, -1, -1, 22, 74, -1, 40, 45, 72, -1, -1, 75, -1, -1, -1, 128, -1, 21, -1, -1, -1, -1, -1, 42, -1, -1, -1, -1, -1, -1, 48, 77, 10, 236, -1, 69, -1, 20, -1, 50, -1, -1, -1, -1, -1, -1, 68, -1, -2, -1, -1, -1, 130, 78, -1, -1, -1, 70, 23, 25, -1, -1, 19, -1, 26, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 24, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -2, 27, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 73, -1, 51, -1, -1, -1, -1, -1, 82, -1, -1, 174, 173, 175, 231, 234, 147, 190, -2, 153, 150, 149, 146, 185, 166, 164, 167, 186, 170, 169, 171, 187, 177, 176, 178, 165, 195, 194, 188, 181, 180, 182, 189, 191, 228, 168, 172, 145, 179, 183, 142, 233, 232, 198, 200, 201, 202, -1, -1, -1, -1, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 192, 156, 157, 133, -1, 148, 131, -1, -1, -1, -1, -1, -1, -1, 226, -1, 199, -1, 159, 158, 138, 224, 225, -1, -1, -1, -1, -1, -1, -1, 227, -1, -1, -2, 223, 229, -1, 132, -1, -1, -1, 184, 196, -1, 143, 160, 161, 162, 163, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 155, 197};

#define IRMAPCASE(x) case x: return  _##x [item->getAuxValue() & 15]

int ItemRenderer::getGuiBlocksSlot(int itemId) {
	if (itemId < 0 || itemId >= (int)(sizeof(_mapper) / sizeof(const signed short)))
		return -1;
	int texId = _mapper[itemId];
	if (texId != -2 && texId >= 0 && texId < 128)
		return texId;
	return -1;
}

int ItemRenderer::getAtlasPos(const ItemInstance* item) {
	int id = item->id;
	if (id < 0 || id >= sizeof(_mapper) / sizeof(const signed short))
		return -1;

	int texId = _mapper[id];
	if (texId != -2) {
		// Vanilla items with no terrain mapping (e.g. buckets) must not be
		// rendered via gui_blocks (their icon coords are a 16px grid on
		// gui/items.png, which gui_blocks' 48px slots mis-map to other
		// blocks). Fall through to renderGuiItemCorrect's 2D path instead.
		if (texId == -1 && Item::items[id] && !dynamic_cast<TileItem*>(Item::items[id])) {
			if (dynamic_cast<BucketItem*>(Item::items[id])) return -1; // buckets
			int icon = Item::items[id]->getIcon(0);
			if (icon >= 0 && icon < 128) return icon;
		}
		return texId;
	}

	switch(id) {
		IRMAPCASE(6);
		IRMAPCASE(17);
		IRMAPCASE(18);
		IRMAPCASE(24);
		IRMAPCASE(35);
		IRMAPCASE(44);
		IRMAPCASE(98);
		IRMAPCASE(155);
		IRMAPCASE(263);
		IRMAPCASE(351);
	default:
		break;
	}
	return -1;
}

/*static*/
bool ItemRenderer::renderGuiModItemModel(Textures* textures, const ItemInstance* item, float x, float y, bool cpuTransform) {
#if defined(_WIN32) || defined(__ANDROID__)
	if (!item || !textures)
		return false;
	ModEngine* me = ModEngine::instance;
	if (!me)
		return false;
	const std::vector<ModBlockPart>* parts = me->itemModelParts(item->id);
	if (!parts || parts->empty())
		return false;

	// 模型自带的大贴图（Item.defineItem 的 modelTexture）不在引擎纹理库里，
	// 按 GL 纹理 id 直接绑（与 ItemInHandRenderer 的模组模型那条路一致）；
	// 没给就还走 terrain 图集（老模组行为不变）。
	const std::string* texPath = me->itemModelTexture(item->id);
	unsigned int modTex = texPath ? me->getModTexture(*texPath) : 0;
	if (modTex != 0)
		glBindTexture(GL_TEXTURE_2D, (GLuint)modTex);
	else
		textures->loadAndBindTexture("terrain.png");

	// 缩放与视角都照抄上面方块图标的写法（1 格 = 10px、等距斜视），
	// 这样模组物品和原版方块物品在一个格子里看起来一样大、一个角度。
	const float S = 10.0f;
	const float cx = x + 8.0f;   // 16x16 格子的中心
	const float cy = y + 8.0f;

	Tesselator& t = Tesselator::instance;
	if (cpuTransform) {
		// 0.8.1 背包走顶点收集（MeshBuffer）：glPushMatrix 是 GPU 变换，对收集进去
		// 的顶点无效，必须用 CPU 变换（offset/scale3d/tilt，与 renderGuiItemNew
		// 的方块支同款）。
		t.offset(cx, cy, -10.0f);
		t.scale3d(S, S, S);
		t.tilt();
	} else {
		glPushMatrix2();
		glTranslatef2(cx, cy, -8.0f);
		glScalef2(S, S, S);
		glRotatef2(180.0f + 30.0f, 1, 0, 0);
		glRotatef2(45.0f, 0, 1, 0);
	}

	// 模型部件本身以原点为中心（与手持/掉落物一致），所以不需要再居中。
	t.begin();
	t.color(0xff, 0xff, 0xff);
	for (size_t i = 0; i < parts->size(); ++i)
		tileRenderer->renderModelBox((*parts)[i], NULL, 0.0f, 0.0f, 0.0f, 1.0f);
	t.draw();

	if (cpuTransform) {
		t.resetScale();
		t.resetTilt();
		t.offset(0.0f, 0.0f, 0.0f);
	} else {
		glPopMatrix2();
	}
	return true;
#else
	(void)textures; (void)item; (void)x; (void)y; (void)cpuTransform;
	return false;
#endif
}

/*static*/
void ItemRenderer::renderGuiItem(Font* font, Textures* textures, const ItemInstance* item, float x, float y, bool fancy) {
	renderGuiItem(font, textures, item, x, y, 16, 16, fancy);
}
void ItemRenderer::renderGuiItem(Font* font, Textures* textures, const ItemInstance* item, float x, float y, float w, float h, bool fancy) {
	if (item == NULL) {
		//LOGW("item is NULL @ ItemRenderer::renderGuiItem\n");
		return;
	}
	const int id = item->id;
	if (!Item::items[id])
		return;

	int i = getAtlasPos(item);

	// 09c · 方块（id<256 可渲染 3D）强制走 renderGuiItemCorrect：
	// 让背包/物品栏图标也显示成 terrain.png 的真实 3D 方块（与原版 0.8.1
	// 创造背包一致），从而吃 Assets.replaceImage("terrain.png",...) 的
	// 材质覆盖；而不是 gui_blocks.png 里那张固定的 2D 预渲染图标。
	// 3D 渲染用 GPU 变换(glPushMatrix)，在 Tesselator override(收集)模式下
	// 无效，故此处打断收集即时绘制（与 mod 物品的既有处理一致）。
	// 模组物品（id>=256）带自定义 3D 模型（Item.defineItem 的 model）时走同一条路。
	bool modHasModel = false;
#if defined(_WIN32) || defined(__ANDROID__)
	if (ModEngine::instance) {
		const std::vector<ModBlockPart>* mp = ModEngine::instance->itemModelParts(id);
		modHasModel = (mp != NULL && !mp->empty());
	}
#endif
	if ((id < 256 && Tile::tiles[id] != NULL &&
		TileRenderer::canRender(Tile::tiles[id]->getRenderShape())) || modHasModel) {
		Tesselator& t = Tesselator::instance;
		bool overridden = t.isOverridden();
		if (overridden)
			t.endOverrideAndDraw();
		renderGuiItemCorrect(font, textures, item, int(x), int(y));
		if (overridden)
			t.beginOverride();
		return;
	}

	if (i < 0) {
		Tesselator& t = Tesselator::instance;
		if (!t.isOverridden())
			renderGuiItemCorrect(font, textures, item, int(x), int(y));
		else {
			// @huge @attn @todo @fix:	This is just guess-works..
			//							it we're batching for saving the
			//							buffer, this will fail miserably
			t.endOverrideAndDraw();
			// No backdrop fill: the vanilla red marker (and its black
			// equivalent when blending is off) doesn't belong on mod-defined
			// items. renderGuiItemCorrect binds terrain.png itself.
			renderGuiItemCorrect(font, textures, item, int(x), int(y));
			t.beginOverride();
		}
		return;
	}

	textures->loadAndBindTexture("gui/gui_blocks.png");
	float u0, u1, v0, v1;
	if (i < 128) {
		const float P = 48.0f / 512.0f;
		u0 = (float)(i%10) * P;
		v0 = (float)(i/10) * P;
		u1 = u0 + P;
		v1 = v0 + P;
	} else {
		i -= 128;
		const float P = 16.0f / 512.0f;
		u0 = float(i & 31) * P;
		v0 = 27 * P + float(i >> 5) * P; // 27 "icon" rows down
		u1 = u0 + P;
		v1 = v0 + P;
	}

	const float blitOffset = 0;
	Tesselator& t = Tesselator::instance;
	t.begin();
	t.colorABGR( item->count>0? 0xffffffff : 0x60ffffff);
	t.vertexUV(x,     y + h, blitOffset, u0, v1);
	t.vertexUV(x + w, y + h, blitOffset, u1, v1);
	t.vertexUV(x + w, y,     blitOffset, u1, v0);
	t.vertexUV(x,     y,     blitOffset, u0, v0);
	t.draw();
}

void ItemRenderer::renderGuiItemDecorations(const ItemInstance* item, float x, float y) {
	if (!item) return;
	if (item->count > 0 && item->isDamaged()) {
		float p = std::floor(13.5f - (float) item->getDamageValue() * 13.0f / (float) item->getMaxDamage());
		int cc = (int) std::floor(255.5f - (float) item->getDamageValue() * 255.0f / (float) item->getMaxDamage());
		//glDisable(GL_LIGHTING);
		//glDisable(GL_DEPTH_TEST);
		//glDisable(GL_TEXTURE_2D);

		Tesselator& t = Tesselator::instance;

		int ca = (255 - cc) << 16 | (cc) << 8;
		int cb = ((255 - cc) / 4) << 16 | (255 / 4) << 8;
		fillRect(t, x + 2, y + 13, 13, 1, 0x000000);
		fillRect(t, x + 2, y + 13, 12, 1, cb);
		fillRect(t, x + 2, y + 13, p, 1, ca);

		//glEnable(GL_TEXTURE_2D);
		//glEnable(GL_LIGHTING);
		//glEnable(GL_DEPTH_TEST);
		glColor4f2(1, 1, 1, 1);
	}
}

void ItemRenderer::fillRect(Tesselator& t, float x, float y, float w, float h, int c) {
	t.begin();
	t.color(c);
	t.vertex(x + 0, y + 0, 0);
	t.vertex(x + 0, y + h, 0);
	t.vertex(x + w, y + h, 0);
	t.vertex(x + w, y + 0, 0);
	t.draw();
}


void ItemRenderer::renderGuiItemCorrect(Font* font, Textures* textures, const ItemInstance* item, int x, int y) {
	if (item == NULL)
		return;

	// 模组物品的 3D 模型优先（物品栏/背包里也显示模型，而不是那张 2D 图标）。
	if (renderGuiModItemModel(textures, item, (float)x, (float)y, false))
		return;

	//glDisable(GL_CULL_FACE);
	if (item->id < 256 && Tile::tiles[item->id] != NULL && TileRenderer::canRender(Tile::tiles[item->id]->getRenderShape()) && !(item->id >= 69 && item->id <= 71))
	{
		int paint = item->id;
		textures->loadAndBindTexture("terrain.png");

		static float ff = 0;// ff += 0.005f;
		static float gg = 0;// gg += 0.01f;

		Tile* tile = Tile::tiles[paint];
		glPushMatrix2();
		glTranslatef2((GLfloat)(x - 2), (GLfloat)(y + 3), -8);
		glScalef2(10.0f, 10.0f, 10.0f);
		glTranslatef2(1.0f, 0.5f, 0.0f);
		glRotatef2(ff + 180.0f + 30.0f, 1, 0, 0);
		glRotatef2(gg + 45.0f, 0, 1, 0);

		//glColor4f2(1, 1, 1, 1);
		glScalef2(1, 1, 1);
		tileRenderer->renderGuiTile(tile, item->getAuxValue());
		glPopMatrix2();
	}
	else if (item->getIcon() >= 0)
	{
		//if (item->id == Item::camera->id) {
		//	printf("item->id: %d, %d\n", item->id, item->getIcon());
		//}
		if (item->id < 256) {
			textures->loadAndBindTexture("terrain.png");
		} else {
			textures->loadAndBindTexture("gui/items.png");
		}
		//Tesselator& t = Tesselator::instance;
		//t.scale2d(Gui::InvGuiScale, Gui::InvGuiScale);
		blit((float)x, (float)y, (float)(item->getIcon() % 16 * 16), (float)(item->getIcon() / 16 * 16), 16, 16, item->id < 256 ? 512.0f : 256.0f);
		//t.resetScale();
	}
	//glEnable(GL_CULL_FACE);
}

/*static*/
void ItemRenderer::blit(float x, float y, float sx, float sy, float w, float h, float texHeight) {
	float blitOffset = 0;
	const float us = 1 / 256.0f;
	const float vs = 1 / texHeight;
	Tesselator& t = Tesselator::instance;
	t.begin();
	t.vertexUV(x, y + h, blitOffset, sx * us, (sy + h) * vs);
	t.vertexUV(x + w, y + h, blitOffset, (sx + w) * us, (sy + h) * vs);
	t.vertexUV(x + w, y, blitOffset, (sx + w) * us, sy * vs);
	t.vertexUV(x, y, blitOffset, sx * us, sy * vs);
	//t.end();
	t.draw();
}

/* ------------------------------------------------------------------ */
// 0.8.1 背包渲染管线（Touch::InventoryPane / CreativeInventoryScreen / ArmorScreen）
//
// 目标项目 Item 用 int 图标索引（getIcon() -> icon），图标在 terrain.png /
// gui/items.png 中按 16x16 网格排列；0.8.1 用 TextureUVCoordinateSet（UV 坐标）。
// 这里移植 0.8.1 的调用面（iconBlit / renderGuiItemNew / renderGuiItemInChunk），
// 内部映射到目标项目的 int 图标系统，渲染效果一致。

/*static*/
void ItemRenderer::iconBlit(float x, float y, float sx, float sy, float w, float h, float alpha, int color, float scale, float texHeight) {
	// 与 0.8.1 相同的缩放语义：scale 从中心缩放（0.8.1 的 a9 参数）
	float v19 = (w * 0.5f) * (scale - 1.0f);
	float v20 = (h * 0.5f) * (scale - 1.0f);
	float x0 = x - v19;
	float y0 = y - v20;
	Tesselator& t = Tesselator::instance;
	t.begin(GL_QUADS);
	if (color == -1) {
		t.color(alpha, alpha, alpha, 1.0f);
	} else {
		float r = (float)((color >> 16) & 0xff) / 255.0f * alpha;
		float g = (float)((color >> 8) & 0xff) / 255.0f * alpha;
		float b = (float)((color) & 0xff) / 255.0f * alpha;
		if (r > 1.0f) r = 1.0f; else if (r < 0.0f) r = 0.0f;
		if (g > 1.0f) g = 1.0f; else if (g < 0.0f) g = 0.0f;
		if (b > 1.0f) b = 1.0f; else if (b < 0.0f) b = 0.0f;
		t.color(r, g, b, 1.0f);
	}
	const float us = 1 / 256.0f;
	// terrain.png 是 256x512（32 行 16px），items.png 是 256x256（16 行）：
	// 高度必须按贴图实际高度算，否则 tile 图标 UV 错位
	const float vs = 1 / texHeight;
	t.vertexUV(x0,         y0 + h + 2*v20, 0, sx * us,     (sy + h) * vs);
	t.vertexUV(x0 + w + 2*v19, y0 + h + 2*v20, 0, (sx + w) * us, (sy + h) * vs);
	t.vertexUV(x0 + w + 2*v19, y0,         0, (sx + w) * us, sy * vs);
	t.vertexUV(x0,         y0,         0, sx * us,     sy * vs);
	t.draw();
}

/*static*/
void ItemRenderer::renderGuiItemNew(Textures* textures, const ItemInstance* item, int a3, float x, float y, float alpha, float brightness, float scale) {
	if (!item) return;
	// 模组物品的 3D 模型（0.8.1 背包是顶点收集模式 → 用 CPU 变换）。
	if (renderGuiModItemModel(textures, item, x, y, true))
		return;
	Tile* tileClass = (item->id < 256) ? Tile::tiles[item->id] : NULL;
	if (tileClass && TileRenderer::canRender(tileClass->getRenderShape())) {
		// 3D 方块（背包 chunk 收集路径）：必须用 CPU 顶点变换
		// （offset + scale3d + tilt，0.8.1 renderGuiItemInChunk 同款）。
		// glPushMatrix 是 GPU 变换，对 override 模式收集进 MeshBuffer 的
		// 顶点无效——叶子/冰等 3D 方块因此在背包里不渲染（物品栏即时渲染正常）。
		float v13 = x + (float)((scale - 1.0f) * -6.0f) + 1.0f;
		float v14 = y + (float)((scale - 1.0f) * 4.0f) + 13.0f;
		// 09c · 3D 方块必须显式绑 terrain.png：0.8.1 背包调用方绑的
		// terrain-atlas.tga 在 win32 不存在(InvalidId)，不显式绑定会用残留
		// 纹理渲染 → mod 的 terrain 槽位覆盖(replaceImage)不生效。
		textures->loadAndBindTexture("terrain.png");
		Tesselator::instance.offset(v13, v14, -10.0f);
		Tesselator::instance.scale3d(scale * 9.0f, scale * 9.0f, scale * 9.0f);
		Tesselator::instance.tilt();
		tileRenderer->renderGuiTile(tileClass, item->getAuxValue());
		Tesselator::instance.resetScale();
		Tesselator::instance.resetTilt();
		Tesselator::instance.offset(0.0f, 0.0f, 0.0f);
	} else if (item->getIcon() >= 0) {
		if (item->id < 256) {
			textures->loadAndBindTexture("terrain.png");
			// terrain.png 256x512：32 行 16px 图标
			iconBlit(x, y, (float)(item->getIcon() % 16 * 16), (float)(item->getIcon() / 16 * 16), 16, 16, alpha, -1, scale, 512.0f);
		} else {
			// 与 hotbar/renderGuiItem 一致：id>=256 的物品图标在 gui_blocks.png
			// 的 48px 槽（getAtlasPos 的 icon 值），而非 items.png 的 16px 槽。
			// 否则鸡蛋(icon=12)会采到 items.png 的沙色槽，变成沙子。
			int atlas = getAtlasPos(item);
			if (atlas >= 0 && atlas < 90) {
				// 48px 槽区仅 10 列×9 行（icon 0..89）；icon≥90 会落进
				// 16px 图标区或越出贴图，退回 items.png 渲染
				textures->loadAndBindTexture("gui/gui_blocks.png");
				float u0, u1, v0, v1;
				if (atlas < 128) {
					const float P = 48.0f / 512.0f;
					u0 = (float)(atlas % 10) * P;
					v0 = (float)(atlas / 10) * P;
					u1 = u0 + P;
					v1 = v0 + P;
				} else {
					atlas -= 128;
					const float P = 16.0f / 512.0f;
					u0 = float(atlas & 31) * P;
					v0 = 27 * P + float(atlas >> 5) * P;
					u1 = u0 + P;
					v1 = v0 + P;
				}
				// 同 renderGuiItem 的四顶点绘制（支持 alpha 淡入淡出）
				Tesselator& t = Tesselator::instance;
				t.begin(GL_QUADS);
				t.color(alpha, alpha, alpha, 1.0f);
				t.vertexUV(x,          y + 16.0f, 0, u0, v1);
				t.vertexUV(x + 16.0f,  y + 16.0f, 0, u1, v1);
				t.vertexUV(x + 16.0f,  y,         0, u1, v0);
				t.vertexUV(x,          y,         0, u0, v0);
				t.draw();
			} else {
				textures->loadAndBindTexture("gui/items.png");
				iconBlit(x, y, (float)(item->getIcon() % 16 * 16), (float)(item->getIcon() / 16 * 16), 16, 16, alpha, -1, scale, 256.0f);
			}
		}
	}
}

/*static*/
void ItemRenderer::renderGuiItemInChunk(int chunkType, Textures* textures, const ItemInstance* item, float x, float y, float alpha, float brightness, float scale) {
	// 0.8.1 中 IRCT_NULL/ONE/TWO 区分 batch 类型（方块/物品），由 Tesselator
	// override 模式收集到 MeshBuffer。目标项目无该 batch 管线，直接走
	// renderGuiItemNew 的即时渲染路径（Tesselator 仍处于 override 模式，
	// 顶点会被收集到当前 buffer）。
	renderGuiItemNew(textures, item, 0, x, y, alpha, brightness, scale);
}
