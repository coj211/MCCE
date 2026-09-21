#include "ModEngine.h"
#include "../platform/log.h"    // LOGI：服务器下 mod 日志要同时进控制台
#include "ModZip.h"
#include "GlJsBindings.h"
#include "stb_vorbis.h"
#include <math.h>   // 骨骼动画的欧拉角/矩阵（sinf/cosf/atan2f/asinf）
// 皮肤等运行时图片解码(png/jpg/bmp): 实现仅本编译单元生效
#define STB_IMAGE_IMPLEMENTATION
#include "../../thirdparty/stb/stb_image.h"
#include "../world/item/Item.h"
#include "../world/item/ItemInstance.h"
#include "../world/entity/player/Player.h"
#include "../world/level/Level.h"
#include "../world/level/tile/entity/ModTileEntity.h"
#include "../world/level/material/Material.h"
#include "../world/level/biome/Biome.h"
#include "../world/entity/MobCategory.h"
#include "../world/level/levelgen/feature/TreeFeature.h"
#include "../world/level/levelgen/feature/BirchFeature.h"
#include "../world/level/levelgen/feature/PineFeature.h"
#include "../world/level/levelgen/feature/SpruceFeature.h"

// 模组定义的群系（JS 侧 Biome.define）。它是真正的 Biome 子类实例，所以
// 地表方块（buildSurfaces 读 topMaterial/material）、天空色（Level::getSkyColor
// → biome->getSkyColor）、刷怪表（biome->getMobs）、地形起伏（getHeights 调
// adjustScale/adjustDepth）全都自动跟着走，不用在渲染/地形里加分支。
class ScriptedBiome : public Biome {
	typedef Biome super;
public:
	// 定义里搬过来的字段：刷怪表会被 MobSpawner 反复拿引用，存实例里最稳。
	int scriptedId;
	float heightScale, heightBias;
	int skyColor;            // <0 = 按温度算（原版行为）
	bool hasDecorate, hasHeight;
	std::string treeKind;    // "" = 原版树；none/oak/birch/pine/spruce

	ScriptedBiome(int biomeId, const ModEngine::ScriptedBiomeDef& d);

	virtual bool isScriptedBiome() const { return true; }
	virtual float getTemperature(void) { return temperature; }
	virtual int getSkyColor(float temp) {
		return skyColor >= 0 ? skyColor : super::getSkyColor(temp);
	}
	// 地形起伏（对应 1.6.4 的 Biome::adjustScale/adjustDepth）。
	// 1 / 0 = 原版；上限 1 与原版 getHeights 里的 clamp 一致。
	virtual float adjustScale(float scale) {
		float s = scale * heightScale;
		return s > 1.0f ? 1.0f : s;
	}
	virtual float adjustDepth(float depth) {
		return depth + heightBias;
	}
	virtual MobList& getMobs(const MobCategory& category) {
		if (&category == &MobCategory::monster) return _modEnemies;
		if (&category == &MobCategory::creature) return _friendlies;
		if (&category == &MobCategory::waterCreature) return _waterFriendlies;
		return super::getMobs(category);
	}
	// 有 decorate 回调 = 这个群系的植被由模组自己管：不返回原版树
	// （RandomLevelSource::postProcess 对 NULL 已有判空，不会崩）。
	virtual Feature* getTreeFeature(Random* random) {
		if (hasDecorate || treeKind == "none") return NULL;
		if (treeKind.empty()) return super::getTreeFeature(random);
		if (treeKind == "birch")  return new BirchFeature(false);
		if (treeKind == "pine")   return new PineFeature(false);
		if (treeKind == "spruce") return new SpruceFeature(false);
		return new TreeFeature(false);   // oak / 其它未识别值
	}
	virtual Feature* getGrassFeature(Random* random) {
		return hasDecorate ? NULL : super::getGrassFeature(random);
	}
	// 区块生成时刷动物的概率（字段默认 = 原版 0.08）。
	virtual float getCreatureProbability() { return creatureProbability; }
private:
	MobList _modEnemies;   // 模组自己给的怪表（基类那份 _enemies 被它替换）
};

ScriptedBiome::ScriptedBiome(int biomeId, const ModEngine::ScriptedBiomeDef& d)
:	scriptedId(biomeId),
	heightScale(d.heightScale), heightBias(d.heightBias),
	skyColor(d.skyColor), hasDecorate(d.hasDecorate), hasHeight(d.hasHeight)
{
	id = biomeId;
	if (!d.name.empty()) name = d.name;
	temperature = d.temperature;
	downfall = d.downfall;
	treeKind = d.treeKind;
	// 颜色：只在模组明确给了才覆盖（原版那套值保持不变）。
	if (d.grassColor >= 0)   grassColor = d.grassColor;
	if (d.foliageColor >= 0) foliageColor = d.foliageColor;
	if (d.fogColor >= 0)     fogColor = d.fogColor;
	if (d.snowOverride >= 0) snowOverride = d.snowOverride;
	if (d.waterFogColor >= 0) waterFogColor = d.waterFogColor;
	// 植被：模组群系默认不长原版树/花/蘑菇/甘蔗/仙人掌（想要就自己给数量），
	// 想全权接管就用 decorate。
	treeCount      = (d.treeCount >= 0) ? d.treeCount : ((!d.treeKind.empty() && d.treeKind != "none") ? 1 : 0);
	grassCount     = (d.grassCount >= 0) ? d.grassCount : 0;
	flowerCount    = (d.flowerCount >= 0) ? d.flowerCount : 0;
	mushroomChance = (d.mushroomChance >= 0) ? d.mushroomChance : 0;
	reedsCount     = (d.reedsCount >= 0) ? d.reedsCount : 0;
	cactusCount    = (d.cactusCount >= 0) ? d.cactusCount : 0;
	// 刷怪：只在模组给了才覆盖。
	if (d.creatureProbability >= 0.0f) creatureProbability = d.creatureProbability;
	if (d.spawnYMin >= 0) spawnYMin = d.spawnYMin;
	if (d.spawnYMax >= 0) spawnYMax = d.spawnYMax;
	if (d.monsterLightMax >= 0) monsterLightMax = d.monsterLightMax;
	// 方块：没指定就沿用原版平地（草/泥土）。
	topMaterial = (char)(d.topMaterial >= 0 ? d.topMaterial : (int)Biome::plains->topMaterial);
	material = (char)(d.material >= 0 ? d.material : (int)Biome::plains->material);
	// 刷怪：给了 spawns 就完全按模组的（三张表都清空重填），
	// 一张都没给 = 保留基类那套原版默认怪。
	bool hasSpawns = !d.monsters.empty() || !d.creatures.empty() || !d.water.empty();
	if (hasSpawns) {
		_modEnemies.clear();
		_friendlies.clear();
		_waterFriendlies.clear();
		for (size_t i = 0; i < d.monsters.size(); ++i)
			_modEnemies.insert(_modEnemies.end(), MobSpawnerData(d.monsters[i].mob, d.monsters[i].weight, d.monsters[i].minCount, d.monsters[i].maxCount));
		for (size_t i = 0; i < d.creatures.size(); ++i)
			_friendlies.insert(_friendlies.end(), MobSpawnerData(d.creatures[i].mob, d.creatures[i].weight, d.creatures[i].minCount, d.creatures[i].maxCount));
		for (size_t i = 0; i < d.water.size(); ++i)
			_waterFriendlies.insert(_waterFriendlies.end(), MobSpawnerData(d.water[i].mob, d.water[i].weight, d.water[i].minCount, d.water[i].maxCount));
	}
}

// Custom item that forwards right-click (useOn) to a JS callback registered
// via Item.defineItem(id, { onUse: function(x, y, z, face) {...} }).
class ModItem : public Item {
	typedef Item super;
	int _modId;
	int _attackDamage;    // stage 4: custom attack damage (default 1)
	float _miningSpeed;   // stage 4: getDestroySpeed (default 1)
	int _nutrition;       // stage 4: >0 = food item
	bool _isFood;
	// 3D 模型部件（复用方块那套 ModBlockPart；空 = 按 2D 图标画）
	std::vector<ModBlockPart> _parts;
	// 动画回调 key（空 = 静态）。和方块动画一样存成全局函数名。
	std::string _animKey;	// 模型级独立大贴图（modelTexture；空 = 走 terrain 图集）
	std::string _modelTexture;
	// 模型骨架（Item.defineItem 的 bones 选项）：骨骼名 -> pivot/parent。
	// 部件上的 bone 字段指向这里；渲染前会把骨骼这一帧的动画沿 parent 链累积，
	// 再整体套到部件上（基岩 .animation.json 的语义）。空 = 不做骨骼动画。
	std::vector<ModBoneDef> _bones;

public:
	ModItem(int id, int modId) : super(id), _modId(modId),
		_attackDamage(1), _miningSpeed(1.0f), _nutrition(0), _isFood(false) {}
	void setModAttackDamage(int d) { _attackDamage = d; }
	void setModMiningSpeed(float s) { _miningSpeed = s; }
	void setModFood(int nutrition) { _nutrition = nutrition; _isFood = nutrition > 0; }
	// Stage 4: durability (protected setters in Item, expose here).
	void setModDurability(int d) { setMaxDamage(d); setStackedByData(true); setMaxStackSize(1); }
	// 3D 模型 / 动画（对应 Item.defineItem 的 model / anim 选项）
	void setModModel(const std::vector<ModBlockPart>& p) { _parts = p; }
	bool hasModModel() const { return !_parts.empty(); }
	const std::vector<ModBlockPart>& modModelParts() const { return _parts; }
	void setModModelTexture(const std::string& p) { _modelTexture = p; }
	const std::string& modModelTexture() const { return _modelTexture; }

	void setModAnimKey(const std::string& k) { _animKey = k; }
	bool isModAnimated() const { return !_animKey.empty(); }
	const std::string& modAnimKey() const { return _animKey; }
	void setModBones(const std::vector<ModBoneDef>& b) { _bones = b; }
	const std::vector<ModBoneDef>& modBones() const { return _bones; }

	bool useOn(ItemInstance* instance, Player* player, Level* level, int x, int y, int z, int face, float clickX, float clickY, float clickZ) {
		if (ModEngine::instance && ModEngine::instance->callItemUseOn(_modId, x, y, z, face))
			return true;
		return false;
	}

	float getDestroySpeed(ItemInstance* itemInstance, Tile* tile) {
		return _miningSpeed;
	}

	// A mod tool with a custom mining speed can mine stone/metal blocks
	// (vanilla tools gate these behind canDestroySpecial; without this the
	// block uses the slow "can't destroy" path — the mod pickaxe appeared
	// fast on dirt but slow on stone).
	bool canDestroySpecial(const Tile* tile) const {
		if (tile == NULL)
			return false;
		if (tile->material == Material::stone || tile->material == Material::metal)
			return _miningSpeed > 1.0f;
		return _miningSpeed > 1.0f;
	}

	int getAttackDamage(Entity* entity) {
		return _attackDamage;
	}

	bool isHandEquipped() const {
		return _attackDamage > 1 || _miningSpeed > 1.0f;
	}

	void hurtEnemy(ItemInstance* itemInstance, Mob* mob) {
		if (getMaxDamage() > 0)
			itemInstance->hurt(1);
	}

	bool mineBlock(ItemInstance* itemInstance, int tile, int x, int y, int z) {
		if (getMaxDamage() > 0)
			itemInstance->hurt(2);
		return true;
	}

	bool isFood() const {
		return _isFood;
	}

	int getNutrition() {
		return _nutrition;
	}

	int getUseDuration(ItemInstance* itemInstance) {
		return _isFood ? (int)(20 * 1.6) : 0;
	}

	// Eating animation (held-item pose + chew particles in HumanoidModel /
	// ItemInHandRenderer rely on this).
	UseAnim::UseAnimation getUseAnimation() {
		return _isFood ? UseAnim::eat : UseAnim::none;
	}

	ItemInstance* use(ItemInstance* instance, Level* level, Player* player) {
		if (_isFood) {
#if defined(_WIN32) || defined(__ANDROID__)
			if (ModEngine::instance && !ModEngine::instance->canEatFood(id))
				return instance;   // mod says "no"
			player->startUsingItem(*instance, getUseDuration(instance));
#endif
			return instance;
		}
		return super::use(instance, level, player);
	}

	ItemInstance useTimeDepleted(ItemInstance* instance, Level* level, Player* player) {
		if (_isFood) {
			// Guard against negative counts: eating the last item must not
			// underflow (negative counts break count==0 checks elsewhere).
			if (instance->count > 0)
				instance->count--;
			player->foodData.eat(_nutrition);
			level->playSound(player, "random.burp", 0.5f, level->random.nextFloat() * 0.1f + 0.9f);
			return *instance;
		}
		return super::useTimeDepleted(instance, level, player);
	}
};

#include <ctype.h>

#include <png.h>
#include "../client/renderer/gles.h"
#include "../client/renderer/Textures.h"
#include "../client/renderer/LevelRenderer.h"
#include "../client/renderer/GameRenderer.h"   // player.setZoom：转 GameRenderer::zoomRegion（开镜变焦）
#include "../client/gui/screens/ProgressScreen.h"
#include "../client/gui/screens/ScriptedScreen.h"
#include "../network/ServerSideNetworkHandler.h"
#include "../network/packet/PlaceBlockPacket.h"   // 客户端 mod 改方块要同步给服务器
#include "../network/packet/PlayerPosePacket.h"    // 玩家动作同步（客户端上报 / 服务器广播）
// 09c · replaceBlockIcon：查方块在 gui_blocks.png 的图标槽
#include "../client/renderer/entity/ItemRenderer.h"
#include "../world/level/LevelSettings.h"
#include "../world/item/WeaponItem.h"
#include "../world/item/PickaxeItem.h"
#include "../world/item/HatchetItem.h"
#include "../world/item/ShovelItem.h"
#include "../world/item/ArmorItem.h"
#include "../world/entity/item/ItemEntity.h"
#include "../world/entity/projectile/Arrow.h"
#include "../world/entity/Painting.h"

#include "../../thirdparty/duktape/duktape.h"
#if defined(__ANDROID__)
#include <android/log.h>
#include <dirent.h>
#include <sys/stat.h>
#endif
#if defined(_WIN32)
#include <windows.h>
#endif
#if defined(_WIN32)
#include <signal.h>
#include <exception>
#endif
#if defined(_WIN32)
#include <crtdbg.h>
#endif

#include <cstdio>
#include <fstream>    // Android 上安装模组：按文件名把选中的 zip 拷进 mods/
#include <iterator>   // std::istreambuf_iterator
#include <cstring>
#include <cctype>
#include <vector>

#include "../client/Minecraft.h"
#include "../world/level/storage/LevelStorageSource.h"            // 世界目录（op 名单存世界目录里）
#include "../world/level/storage/ExternalFileLevelStorageSource.h"
#include "../client/player/LocalPlayer.h"
#include "../platform/input/Keyboard.h"
#include "../platform/input/Mouse.h"
#include "../client/Options.h"
#include "../world/entity/player/Player.h"
#include "../world/entity/player/Inventory.h"
#include "../world/entity/Entity.h"
#include "../world/item/ItemInstance.h"
#include "../world/item/Item.h"
#include "../world/level/tile/Tile.h"
#include "../world/level/tile/ModTile.h"
#include "../world/level/material/Material.h"
#include "../world/Facing.h"
#include "../world/item/TileItem.h"
#include "../world/item/crafting/Recipes.h"
#include "../locale/I18n.h"
#include "../client/gui/Gui.h"
#include "../world/level/Level.h"
#include "../client/sound/SoundEngine.h"
#include "../platform/time.h"
#include "../client/model/ScriptedModel.h"
#include "../world/entity/monster/ScriptedMob.h"
#include "../world/entity/Mob.h"
#include "../client/particle/ScriptedParticle.h"
#include "../client/particle/ParticleEngine.h"
#include "../client/gamemode/GameMode.h"
#include "../world/level/chunk/ChunkSource.h"
#include "../world/level/dimension/ScriptedDimension.h"
#include "../world/level/MobSpawner.h"
#include "../world/entity/monster/Zombie.h"
#include "../world/entity/monster/Creeper.h"
#include "../world/entity/monster/Skeleton.h"
#include "../world/entity/monster/PigZombie.h"
#include "../world/entity/animal/Cow.h"
#include "../world/entity/animal/Pig.h"
#include "../world/entity/animal/Sheep.h"
#include "../world/entity/animal/Chicken.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <shlwapi.h>
#endif

// Shortcut used by the JS bindings below.
#define MCMOD() (ModEngine::instance ? ModEngine::instance->minecraft() : NULL)

// ---------------------------------------------------------------------------
// JS player API
// ---------------------------------------------------------------------------

// 脚本里 player.* 取哪个玩家：优先本次事件的上下文玩家（服务器上 mc->player
// 恒为空），没有上下文时退回本机玩家（单机 / 房主模式）。
static Player* modPlayerForScript() {
	ModEngine* me = ModEngine::instance;
	if (!me) return NULL;
	if (me->eventPlayer()) return me->eventPlayer();
	Minecraft* m = me->minecraft();
	return m ? m->player : NULL;
}

static duk_ret_t jsPlayerGetX(duk_context* ctx) {
	Player* p = modPlayerForScript();
	duk_push_number(ctx, p ? p->x : 0);
	return 1;
}
static duk_ret_t jsPlayerGetY(duk_context* ctx) {
	Player* p = modPlayerForScript();
	duk_push_number(ctx, p ? p->y : 0);
	return 1;
}
// JS player.getYRot() / player.getXRot() - facing angles in degrees.
static duk_ret_t jsPlayerGetYRot(duk_context* ctx) {
	Player* p = modPlayerForScript();
	if (!p)
		return 0;
	duk_push_number(ctx, (double)p->yRot);
	return 1;
}
static duk_ret_t jsPlayerGetXRot(duk_context* ctx) {
	Player* p = modPlayerForScript();
	if (!p)
		return 0;
	duk_push_number(ctx, (double)p->xRot);
	return 1;
}

static duk_ret_t jsPlayerGetZ(duk_context* ctx) {
	Player* p = modPlayerForScript();
	duk_push_number(ctx, p ? p->z : 0);
	return 1;
}
static duk_ret_t jsPlayerSetPos(duk_context* ctx) {
	Player* p = modPlayerForScript();
	if (p) {
		double x = duk_get_number(ctx, 0);
		double y = duk_get_number(ctx, 1);
		double z = duk_get_number(ctx, 2);
		p->moveTo((float)x, (float)y, (float)z, p->yRot, p->xRot);
	}
	return 0;
}
static duk_ret_t jsPlayerGetHealth(duk_context* ctx) {
	Minecraft* m = MCMOD();
	duk_push_int(ctx, (m && m->player) ? m->player->health : 0);
	return 1;
}
// JS player.damage(amount) - deals damage through the normal hurt system
// (screen flash + hurt animation), unlike setHealth which edits the field.
static duk_ret_t jsPlayerDamage(duk_context* ctx) {
	Minecraft* m = MCMOD();
	if (m && m->player)
		m->player->hurt(NULL, duk_get_int(ctx, 0));
	return 0;
}

static duk_ret_t jsPlayerSetHealth(duk_context* ctx) {
	Minecraft* m = MCMOD();
	if (m && m->player)
		m->player->health = duk_get_int(ctx, 0);
	return 0;
}
static duk_ret_t jsPlayerAddItem(duk_context* ctx) {
	Minecraft* m = MCMOD();
	if (m && m->player) {
		int id = duk_get_int(ctx, 0);
		int count = duk_get_int(ctx, 1);
		if (count <= 0) count = 1;
		ItemInstance inst(id, count, 0);
		m->player->inventory->add(&inst);
	}
	return 0;
}

// player.countItem(id) -> 背包里这种物品的总数（快捷栏 + 背包）。
// 给"枪械从背包扣子弹"这类玩法用：模组要能知道玩家手里到底还有多少发。
static duk_ret_t jsPlayerCountItem(duk_context* ctx) {
	Minecraft* m = MCMOD();
	int id = duk_get_int(ctx, 0), total = 0;
	if (m && m->player && m->player->inventory) {
		Container* inv = m->player->inventory;
		int n = inv->getContainerSize();
		for (int i = 0; i < n; ++i) {
			ItemInstance* it = inv->getItem(i);
			if (it && it->id == id && it->count > 0)
				total += it->count;
		}
	}
	duk_push_int(ctx, total);
	return 1;
}

// player.removeItem(id, count) -> 实际扣掉的数量（可能少于请求：背包不够）。
// 从最后一格往前找，尽量不动快捷栏里的东西；扣空的槽写成空物品。
static duk_ret_t jsPlayerRemoveItem(duk_context* ctx) {
	Minecraft* m = MCMOD();
	int id = duk_get_int(ctx, 0), want = duk_get_int(ctx, 1), got = 0;
	if (want > 0 && m && m->player && m->player->inventory) {
		Container* inv = m->player->inventory;
		for (int i = inv->getContainerSize() - 1; i >= 0 && got < want; --i) {
			ItemInstance* it = inv->getItem(i);
			if (!it || it->id != id || it->count <= 0)
				continue;
			int take = (it->count <= (want - got)) ? it->count : (want - got);
			got += take;
			if (it->count - take <= 0) {
				ItemInstance empty(0, 0, 0);
				inv->setItem(i, &empty);
			} else {
				ItemInstance left(it->id, it->count - take, it->getAuxValue());
				inv->setItem(i, &left);
			}
		}
	}
	duk_push_int(ctx, got);
	return 1;
}

// Stage 4: held-item durability for the JS side.
//   player.getHeldItemDamage()    -> current durability damage (0 = fresh)
//   player.getHeldItemMaxDamage() -> max durability (0 = not damageable)
static duk_ret_t jsPlayerGetHeldItemDamage(duk_context* ctx) {
	Minecraft* m = MCMOD();
	ItemInstance* inst = (m && m->player) ? m->player->getCarriedItem() : NULL;
	duk_push_int(ctx, inst ? inst->getDamageValue() : 0);
	return 1;
}
static duk_ret_t jsPlayerGetHeldItemMaxDamage(duk_context* ctx) {
	Minecraft* m = MCMOD();
	ItemInstance* inst = (m && m->player) ? m->player->getCarriedItem() : NULL;
	duk_push_int(ctx, inst ? inst->getMaxDamage() : 0);
	return 1;
}
static duk_ret_t jsPlayerGetHeldItemId(duk_context* ctx) {
	Minecraft* m = MCMOD();
	ItemInstance* inst = (m && m->player) ? m->player->getCarriedItem() : NULL;
	duk_push_int(ctx, inst ? inst->id : 0);
	return 1;
}
// JS player.setHeldItemDamage(d) - set durability damage of the held item
// (0 = pristine, maxDamage = broken). 0.6.1 swords already break on use;
// blocking mods push this value up to convert hits into durability loss.
static duk_ret_t jsPlayerSetHeldItemDamage(duk_context* ctx) {
	Minecraft* m = MCMOD();
	ItemInstance* inst = (m && m->player) ? m->player->getCarriedItem() : NULL;
	if (inst)
		inst->setAuxValue(duk_get_int(ctx, 0));
	return 0;
}
// JS player.isUseHeld() - is the use/build key currently held down?
// win32: right mouse button (WM_RBUTTONDOWN feeds Mouse::ACTION_RIGHT);
// keyboard fallback: the "use" key (U). Used for a blocking mod.
static duk_ret_t jsPlayerIsUseHeld(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	Minecraft* mc = me ? me->minecraft() : NULL;
	if (!mc || !mc->player) { duk_push_boolean(ctx, 0); return 1; }
	bool use = Mouse::isButtonDown(MouseAction::ACTION_RIGHT);
	if (!use)
		use = Keyboard::isKeyDown(mc->options.keyUse.key);
	duk_push_boolean(ctx, use ? 1 : 0);
	return 1;
}
// JS player.setItemPose(0|1) - first-person item pose: 0 normal, 1 blocking
// (item rendered up in front-left like a raised shield/sword block).
static duk_ret_t jsPlayerSetItemPose(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	if (me)
		me->setItemPose(duk_get_int(ctx, 0) != 0 ? 1 : 0);
	return 0;
}

// ModEngine::setCameraZoom —— JS player.setZoom(z) 的实现体。
// 客户端把状态转给 GameRenderer 里那套现成但从来没人调过的 zoomRegion：
//   zoom > 1 → zoomRegion(z, 0, 0)（setupCamera 里 glScalef2 + gluPerspective，
//              世界按收窄后的投影【重新渲染】—— 真变焦，放大后不糊）；
//   zoom <= 1 → unZoomRegion()（回到 1，正常视野）。
// 服务端没有 GameRenderer，只记状态（setZoom 在服务端是空操作）。
void ModEngine::setCameraZoom(float z) {
	if (z != z || z < 1.0f) z = 1.0f;   // NaN / 小于 1 都当"正常"
	_cameraZoom = z;
#ifndef STANDALONE_SERVER
	Minecraft* m = minecraft();
	if (m && m->gameRenderer) {
		// 用 setModZoom（只收窄世界 FOV）而不是 zoomRegion —— 后者会连第一人称的枪一起
		// 跳过渲染（开镜就看不到枪了）；顺手把老的 zoom 状态复位，免得两套叠加。
		m->gameRenderer->setModZoom(z);
		m->gameRenderer->unZoomRegion();
	}
#endif
}

// JS player.setZoom(z) - camera zoom for scopes/ADS: z<=1 normal, z>1 zoomed in.
static duk_ret_t jsPlayerSetZoom(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	if (me)
		me->setCameraZoom((float)duk_get_number(ctx, 0));
	return 0;
}
static duk_ret_t jsPlayerSendMessage(duk_context* ctx) {
#ifndef STANDALONE_SERVER
	Minecraft* m = MCMOD();
	if (m)
		m->gui.addMessage(duk_safe_to_string(ctx, 0));
	return 0;
#else
	(void)ctx; return 0;   // 服务器：聊天广播见 ServerSideNetworkHandler（待接入）
#endif
}

// ---------------------------------------------------------------------------
// JS level API
// ---------------------------------------------------------------------------

static duk_ret_t jsLevelGetBlock(duk_context* ctx) {
	Minecraft* m = MCMOD();
	int id = 0;
	if (m && m->level)
		id = m->level->getTile(duk_get_int(ctx, 0), duk_get_int(ctx, 1), duk_get_int(ctx, 2));
	duk_push_int(ctx, id);
	return 1;
}
static duk_ret_t jsLevelGetData(duk_context* ctx) {
	Minecraft* m = MCMOD();
	int data = 0;
	if (m && m->level)
		data = m->level->getData(duk_get_int(ctx, 0), duk_get_int(ctx, 1), duk_get_int(ctx, 2));
	duk_push_int(ctx, data);
	return 1;
}
static duk_ret_t jsLevelSetBlock(duk_context* ctx) {
	Minecraft* m = MCMOD();
	if (m && m->level) {
		int x = duk_get_int(ctx, 0);
		int y = duk_get_int(ctx, 1);
		int z = duk_get_int(ctx, 2);
		int id = duk_get_int(ctx, 3);
		int data = duk_get_int(ctx, 4);
		// Guard against ids outside the 256-entry Tile::tiles table (negative
		// or >=256 would index out of bounds and crash).
		if (id >= 0 && id < 256) {
			m->level->setTile(x, y, z, id);
			if (data != 0)
				m->level->setData(x, y, z, data);
			// 联机客户端：mod 改的方块必须同步给服务器。客户端这边只改本地
			// （Level 的 listener 里没有"上报服务器"这条），不同步的话服务器
			// 那份世界永远缺这一块 —— 天境传送门方块就这样丢的（门只在客户端
			// 存在，服务器认不出门、也永远不会把玩家送进天境）。
			if (m->level->isClientSide && m->raknetInstance) {
				PlaceBlockPacket pkt(m->player ? m->player->entityId : 0,
				                     x, y, z, 0, id, data);
				m->raknetInstance->send(pkt);
			}
		}
	}
	return 0;
}
// JS level.getDifficulty() - 0 peaceful, 1 easy, 2 normal, 3 hard.
static duk_ret_t jsLevelGetDifficulty(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	Minecraft* mc = me ? me->minecraft() : NULL;
	if (mc && mc->level) {
		duk_push_int(ctx, mc->level->difficulty);
		return 1;
	}
	duk_push_int(ctx, 2);
	return 1;
}

static duk_ret_t jsLevelGetTime(duk_context* ctx) {
	Minecraft* m = MCMOD();
	duk_push_number(ctx, (m && m->level) ? (double)m->level->getTime() : 0);
	return 1;
}
static duk_ret_t jsLevelSetTime(duk_context* ctx) {
	Minecraft* m = MCMOD();
	if (m && m->level)
		m->level->setTime((long)duk_get_number(ctx, 0));
	return 0;
}

// level.getTicks() -> 游戏刻计数（每 tick +1）。模组做节流 / 计时的可靠依据。
// 别拿 level.getTime() 计时 —— 那是存档里的世界时间，基本不变。
static duk_ret_t jsLevelGetTicks(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	duk_push_number(ctx, me ? (double)me->getTickCount() : 0);
	return 1;
}

// ---------------------------------------------------------------------------
// JS Item / Recipes API (M5: custom items + crafting recipes)
// ---------------------------------------------------------------------------

// Item.defineItem(id, name, iconX, iconY) - register a custom item that
// reuses an icon from the item atlas. Its display name is injected into I18n.
// JS Block.defineBlock(id, { name, texture, material }) - register a new
// tile at runtime. The tile uses one terrain-atlas texture index on all
// faces, gets a TileItem so it can be held/placed, and is appended to the
// creative inventory.
// JS Particle.define(name, { texture, size, lifetime, color, gravity, shrink })
// --- UI drawing API (ui.drawText/drawShadow/fillRect/drawImage) ---
static duk_ret_t jsUiDrawText(duk_context* ctx) {
#ifndef STANDALONE_SERVER
	ModEngine* me = ModEngine::instance;
	if (!me || !me->minecraft() || !me->minecraft()->font) return 0;
	std::string text = duk_safe_to_string(ctx, 0);
	float x = (float)duk_get_number(ctx, 1);
	float y = (float)duk_get_number(ctx, 2);
	unsigned int color = (unsigned int)duk_get_uint(ctx, 3);
	me->minecraft()->font->draw(text, x, y, (int)color);
	return 0;
#else
	(void)ctx; return 0;
#endif
}
static duk_ret_t jsUiDrawShadow(duk_context* ctx) {
#ifndef STANDALONE_SERVER
	ModEngine* me = ModEngine::instance;
	if (!me || !me->minecraft() || !me->minecraft()->font) return 0;
	std::string text = duk_safe_to_string(ctx, 0);
	float x = (float)duk_get_number(ctx, 1);
	float y = (float)duk_get_number(ctx, 2);
	unsigned int color = (unsigned int)duk_get_uint(ctx, 3);
	me->minecraft()->font->drawShadow(text, x, y, (int)color);
	return 0;
#else
	(void)ctx; return 0;
#endif
}
static duk_ret_t jsUiFillRect(duk_context* ctx) {
#ifndef STANDALONE_SERVER
	float x = (float)duk_get_number(ctx, 0);
	float y = (float)duk_get_number(ctx, 1);
	float w = (float)duk_get_number(ctx, 2);
	float h = (float)duk_get_number(ctx, 3);
	unsigned int col = (unsigned int)duk_get_uint(ctx, 4);
	float a = ((col >> 24) & 0xff) / 255.0f;
	float r = ((col >> 16) & 0xff) / 255.0f;
	float g = ((col >> 8) & 0xff) / 255.0f;
	float b = (col & 0xff) / 255.0f;
	glDisable2(GL_TEXTURE_2D);
	glEnable2(GL_BLEND);
	glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	Tesselator& t = Tesselator::instance;
	t.begin();
	t.color(r, g, b, a);
	t.vertex(x, y + h, 0);
	t.vertex(x + w, y + h, 0);
	t.vertex(x + w, y, 0);
	t.vertex(x, y, 0);
	t.draw();
	glEnable2(GL_TEXTURE_2D);
	return 0;
#else
	(void)ctx; return 0;
#endif
}
// ── 模组画圆（和摇杆一样圆）：ui.fillCircle / ui.drawCircleRing ──
// 颜色和 fillRect 一样是 ARGB：0xAARRGGBB。
static duk_ret_t jsUiFillCircle(duk_context* ctx) {
#ifndef STANDALONE_SERVER
	const float cx = (float)duk_get_number(ctx, 0);
	const float cy = (float)duk_get_number(ctx, 1);
	const float r  = (float)duk_get_number(ctx, 2);
	const unsigned int col = (unsigned int)duk_get_uint(ctx, 3);
	if (!(r > 0.0f)) return 0;
	const float a0 = ((col >> 24) & 0xff) / 255.0f;
	const float rr = ((col >> 16) & 0xff) / 255.0f;
	const float gg = ((col >>  8) & 0xff) / 255.0f;
	const float bb = ( col        & 0xff) / 255.0f;
	const GLboolean cullWas  = glIsEnabled(GL_CULL_FACE);
	const GLboolean depthWas = glIsEnabled(GL_DEPTH_TEST);
	glDisable2(GL_CULL_FACE);
	glDisable2(GL_DEPTH_TEST);
	glDisable2(GL_TEXTURE_2D);
	glEnable2(GL_BLEND);
	glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	Tesselator& t = Tesselator::instance;
	t.begin();
	t.color(rr, gg, bb, a0);
	const int seg = 36;
	const float step = 6.2831853f / (float)seg;
	for (int i = 0; i < seg; ++i) {
		const float sa = i * step, ea = sa + step;
		// 退化为四边形（中心 + 弧两点 + 中心），绕向与矩形一致
		t.vertex(cx, cy, 0);
		t.vertex(cx + r * (float)cos(ea), cy + r * (float)sin(ea), 0);
		t.vertex(cx + r * (float)cos(sa), cy + r * (float)sin(sa), 0);
		t.vertex(cx, cy, 0);
	}
	t.draw();
	glEnable2(GL_TEXTURE_2D);
	if (cullWas)  glEnable2(GL_CULL_FACE);
	if (depthWas) glEnable2(GL_DEPTH_TEST);
	return 0;
#else
	(void)ctx; return 0;
#endif
}

// ui.drawCircleRing(cx, cy, r, thickness, color) —— 空心圆环（底座/边框）
static duk_ret_t jsUiDrawCircleRing(duk_context* ctx) {
#ifndef STANDALONE_SERVER
	const float cx = (float)duk_get_number(ctx, 0);
	const float cy = (float)duk_get_number(ctx, 1);
	const float R  = (float)duk_get_number(ctx, 2);
	float w        = (float)duk_get_number(ctx, 3);
	const unsigned int col = (unsigned int)duk_get_uint(ctx, 4);
	if (!(R > 0.0f)) return 0;
	if (!(w > 0.0f)) w = 2.0f;
	if (w > R) w = R;
	const float inner = R - w;
	const float a0v = ((col >> 24) & 0xff) / 255.0f;
	const float rr  = ((col >> 16) & 0xff) / 255.0f;
	const float gg  = ((col >>  8) & 0xff) / 255.0f;
	const float bb  = ( col        & 0xff) / 255.0f;
	const GLboolean cullWas  = glIsEnabled(GL_CULL_FACE);
	const GLboolean depthWas = glIsEnabled(GL_DEPTH_TEST);
	glDisable2(GL_CULL_FACE);
	glDisable2(GL_DEPTH_TEST);
	glDisable2(GL_TEXTURE_2D);
	glEnable2(GL_BLEND);
	glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	Tesselator& t = Tesselator::instance;
	t.begin();
	t.color(rr, gg, bb, a0v);
	const int seg = 36;
	const float step = 6.2831853f / (float)seg;
	for (int i = 0; i < seg; ++i) {
		const float sa = i * step, ea = sa + step;
		t.vertex(cx + inner * (float)cos(sa), cy + inner * (float)sin(sa), 0);
		t.vertex(cx + inner * (float)cos(ea), cy + inner * (float)sin(ea), 0);
		t.vertex(cx + R     * (float)cos(ea), cy + R     * (float)sin(ea), 0);
		t.vertex(cx + R     * (float)cos(sa), cy + R     * (float)sin(sa), 0);
	}
	t.draw();
	glEnable2(GL_TEXTURE_2D);
	if (cullWas)  glEnable2(GL_CULL_FACE);
	if (depthWas) glEnable2(GL_DEPTH_TEST);
	return 0;
#else
	(void)ctx; return 0;
#endif
}

static duk_ret_t jsUiDrawImage(duk_context* ctx) {
#ifndef STANDALONE_SERVER
	ModEngine* me = ModEngine::instance;
	if (!me) return 0;
	std::string path = duk_safe_to_string(ctx, 0);
	float x = (float)duk_get_number(ctx, 1);
	float y = (float)duk_get_number(ctx, 2);
	float w = (float)duk_get_number(ctx, 3);
	float h = (float)duk_get_number(ctx, 4);
	unsigned int tex = me->getModTexture(path);
	if (!tex) {
		me->log("ui.drawImage: unknown texture " + path);
		return 0;
	}
	glEnable2(GL_TEXTURE_2D);
	glEnable2(GL_BLEND);
	glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBindTexture(GL_TEXTURE_2D, tex);
	Tesselator& t = Tesselator::instance;
	t.begin();
	t.color(255, 255, 255, 255);
	t.vertexUV(x, y + h, 0, 0, 1);
	t.vertexUV(x + w, y + h, 0, 1, 1);
	t.vertexUV(x + w, y, 0, 1, 0);
	t.vertexUV(x, y, 0, 0, 0);
	t.draw();
	return 0;
#else
	(void)ctx; return 0;
#endif
}
static duk_ret_t jsUiGetWidth(duk_context* ctx) {
#ifndef STANDALONE_SERVER
	ModEngine* me = ModEngine::instance;
	if (!me || !me->minecraft()) return 0;
	// onGuiRender runs in GUI (scaled) coordinates - return the scaled size.
	duk_push_number(ctx, (double)(me->minecraft()->width * Gui::InvGuiScale));
	return 1;
#else
	(void)ctx; return 0;
#endif
}
static duk_ret_t jsUiGetHeight(duk_context* ctx) {
#ifndef STANDALONE_SERVER
	ModEngine* me = ModEngine::instance;
	if (!me || !me->minecraft()) return 0;
	duk_push_number(ctx, (double)(me->minecraft()->height * Gui::InvGuiScale));
	return 1;
#else
	(void)ctx; return 0;
#endif
}
static duk_ret_t jsUiGetScale(duk_context* ctx) {
#ifndef STANDALONE_SERVER
	// GUI scale (Gui::GuiScale): bigger = bigger interface elements.
	duk_push_number(ctx, (double)Gui::GuiScale);
	return 1;
#else
	(void)ctx; return 0;
#endif
}

// JS Dimension.define([id,] { name, getHeight: function(x, z) {...} })
// 编号可省：不传（或传 0 / 主世界保留值）时引擎自动挑一个没被占用的。
static duk_ret_t jsDimensionDefine(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	if (!me) return 0;
	int id = (int)duk_get_int(ctx, 0);
	if (id <= 0 || id == Dimension::NORMAL || id == Dimension::NORMAL_DAYCYCLE)
		id = me->allocDimensionId();
	std::string name = "Dimension " + std::to_string(id);
	bool hasClouds = true;
	int grassId = 2, dirtId = 3, stoneId = 1;
	if (duk_is_object(ctx, 1)) {
		duk_get_prop_string(ctx, 1, "blocks");
		if (duk_is_object(ctx, -1)) {
			duk_get_prop_string(ctx, -1, "grass");
			if (duk_is_number(ctx, -1)) grassId = (int)duk_get_int(ctx, -1);
			duk_pop(ctx);
			duk_get_prop_string(ctx, -1, "dirt");
			if (duk_is_number(ctx, -1)) dirtId = (int)duk_get_int(ctx, -1);
			duk_pop(ctx);
			duk_get_prop_string(ctx, -1, "stone");
			if (duk_is_number(ctx, -1)) stoneId = (int)duk_get_int(ctx, -1);
			duk_pop(ctx);
		}
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "clouds");
		if (duk_is_boolean(ctx, -1)) hasClouds = duk_get_boolean(ctx, -1) != 0;
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "name");
		if (duk_is_string(ctx, -1)) name = duk_safe_to_string(ctx, -1);
		duk_pop(ctx);
		// Store the getHeight callback under a stable global key so the
		// C++ chunk source can find it later.
		duk_get_prop_string(ctx, 1, "getHeight");
		if (duk_is_function(ctx, -1)) {
			std::string key = "__dim_" + std::to_string(id) + "_height";
			duk_put_global_string(ctx, key.c_str());
		} else {
			duk_pop(ctx);
		}
		// Optional getBottom(x, z) - bottom of a floating landmass column
		// (0 = solid down to bedrock). Lets mods build noise-heightmap
		// floating continents instead of ground-up columns.
		duk_get_prop_string(ctx, 1, "getBottom");
		if (duk_is_function(ctx, -1)) {
			std::string key = "__dim_" + std::to_string(id) + "_bottom";
			duk_put_global_string(ctx, key.c_str());
		} else {
			duk_pop(ctx);
		}
		// Optional onChunkGenerate(x, z, setBlock) callback: called after the
		// base terrain of each chunk is generated, letting the mod place
		// arbitrary blocks (floating islands, caves, buildings...). All
		// generation decisions live in the mod script.
		duk_get_prop_string(ctx, 1, "onChunkGenerate");
		if (duk_is_function(ctx, -1)) {
			std::string key = "__dim_" + std::to_string(id) + "_chunkgen";
			duk_put_global_string(ctx, key.c_str());
		} else {
			duk_pop(ctx);
		}
		// 可选 biome(x, z) 回调：等于 Biome.distribution(这个维度编号, fn) ——
		// 维度自带群系分布，脚本维度也能有群系（于是也能按群系刷怪）。
		duk_get_prop_string(ctx, 1, "biome");
		if (duk_is_function(ctx, -1)) {
			std::string key = "__biome_dist_" + std::to_string(id);
			duk_put_global_string(ctx, key.c_str());
			me->markBiomeDistribution(id);
		} else {
			duk_pop(ctx);
		}
	}
	me->defineDimension(id, name, hasClouds, grassId, dirtId, stoneId);
	return 0;
}

// ---- 模组群系：Biome.define / Biome.distribution / Biome.list ----

// 名字比较（大小写不敏感）：给"按名字找群系/认群系"用。
static bool equalsIgnoreCase(const std::string& a, const std::string& b) {
	if (a.size() != b.size()) return false;
	for (size_t i = 0; i < a.size(); ++i)
		if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return false;
	return true;
}

// 按名字找群系编号（原版 11 个 + 模组群系，大小写不敏感）；0 = 没找到。
static int findBiomeIdByName(const std::string& name) {
	if (name.empty()) return 0;
	int vc = 0;
	Biome** van = Biome::all(vc);
	for (int i = 0; i < vc; ++i)
		if (van[i] && equalsIgnoreCase(van[i]->name, name)) return van[i]->id;
	ModEngine* me = ModEngine::instance;
	if (me) {
		const std::map<int, ModEngine::ScriptedBiomeDef>& defs = me->scriptedBiomes();
		for (std::map<int, ModEngine::ScriptedBiomeDef>::const_iterator it = defs.begin(); it != defs.end(); ++it)
			if (equalsIgnoreCase(it->second.name, name)) return it->first;
	}
	return 0;
}

// 读 spawns 里的一张表：[{mob, weight, min, max}, ...]
static void jsReadBiomeSpawnList(duk_context* ctx, duk_idx_t objIdx, const char* key,
								 std::vector<ModEngine::BiomeSpawnEntry>& out) {
	duk_get_prop_string(ctx, objIdx, key);
	if (duk_is_array(ctx, -1)) {
		duk_size_t n = duk_get_length(ctx, -1);
		for (duk_size_t i = 0; i < n; ++i) {
			duk_get_prop_index(ctx, -1, (duk_uarridx_t)i);
			if (duk_is_object(ctx, -1)) {
				ModEngine::BiomeSpawnEntry e;
				duk_get_prop_string(ctx, -1, "mob");
				if (duk_is_number(ctx, -1)) e.mob = (int)duk_get_int(ctx, -1);
				duk_pop(ctx);
				duk_get_prop_string(ctx, -1, "weight");
				if (duk_is_number(ctx, -1)) e.weight = (int)duk_get_int(ctx, -1);
				duk_pop(ctx);
				duk_get_prop_string(ctx, -1, "min");
				if (duk_is_number(ctx, -1)) e.minCount = (int)duk_get_int(ctx, -1);
				duk_pop(ctx);
				duk_get_prop_string(ctx, -1, "max");
				if (duk_is_number(ctx, -1)) e.maxCount = (int)duk_get_int(ctx, -1);
				duk_pop(ctx);
				if (e.mob > 0 && e.weight > 0) {
					if (e.minCount < 1) e.minCount = 1;
					if (e.maxCount < e.minCount) e.maxCount = e.minCount;
					out.push_back(e);
				}
			}
			duk_pop(ctx);
		}
	}
	duk_pop(ctx);
}

// JS Biome.define([id,] { name, temperature, downfall, skyColor, topMaterial,
//   material, surfaceDepth, heightScale, heightBias, lavaLakes,
//   spawns: { monster: [{mob, weight, min, max}], creature: [...], water: [...] },
//   getHeight: function(x, z) { return 高度; },   // 该群系区域的列高（可选）
//   decorate: function(x, z) { ... } })           // 该群系自己装饰（可选）
// 编号可省：引擎自动挑一个没被占用的（原版占 1..11，模组从 12 起）。
// 返回实际编号。显式给的编号已被占用时抛错（不静默挪号 —— 模组的分布回调
// 是按编号认自己群系的）。
static duk_ret_t jsBiomeDefine(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	if (!me) return 0;
	// Biome.define(id, cfg) / Biome.define(cfg) / Biome.define(null, cfg)
	int argBase = duk_is_object(ctx, 1) ? 1 : 0;
	int requestedId = 0;
	if (argBase == 1 && duk_is_number(ctx, 0)) requestedId = (int)duk_get_int(ctx, 0);
	if (!duk_is_object(ctx, argBase)) {
		duk_push_error_object(ctx, DUK_ERR_ERROR, "Biome.define: options object required");
		return duk_throw(ctx);
	}
	ModEngine::ScriptedBiomeDef def;
	def.id = requestedId;
	duk_get_prop_string(ctx, argBase, "name");
	if (duk_is_string(ctx, -1)) def.name = duk_safe_to_string(ctx, -1);
	duk_pop(ctx);
#define BIOME_READ_NUM(prop, field) \
	duk_get_prop_string(ctx, argBase, prop); \
	if (duk_is_number(ctx, -1)) def.field = (int)duk_get_number(ctx, -1); \
	duk_pop(ctx);
	BIOME_READ_NUM("temperature", temperature)
	BIOME_READ_NUM("downfall", downfall)
	BIOME_READ_NUM("skyColor", skyColor)
	BIOME_READ_NUM("topMaterial", topMaterial)
	BIOME_READ_NUM("material", material)
	BIOME_READ_NUM("surfaceDepth", surfaceDepth)
	BIOME_READ_NUM("lavaLakes", lavaLakeChance)
	BIOME_READ_NUM("grassColor", grassColor)
	BIOME_READ_NUM("foliageColor", foliageColor)
	BIOME_READ_NUM("fogColor", fogColor)
	BIOME_READ_NUM("waterFogColor", waterFogColor)
	// snow: true = 雪原（结冰 + 积雪）；false = 强制不雪；不给 = 原版
	duk_get_prop_string(ctx, argBase, "snow");
	if (duk_is_boolean(ctx, -1)) def.snowOverride = duk_get_boolean(ctx, -1) ? 1 : 0;
	duk_pop(ctx);
	BIOME_READ_NUM("trees", treeCount)
	BIOME_READ_NUM("grass", grassCount)
	BIOME_READ_NUM("flowers", flowerCount)
	BIOME_READ_NUM("mushrooms", mushroomChance)
	BIOME_READ_NUM("reeds", reedsCount)
	BIOME_READ_NUM("cactus", cactusCount)
	BIOME_READ_NUM("spawnYMin", spawnYMin)
	BIOME_READ_NUM("spawnYMax", spawnYMax)
	BIOME_READ_NUM("monsterLightMax", monsterLightMax)
#undef BIOME_READ_NUM
#define BIOME_READ_FLOAT(prop, field) \
	duk_get_prop_string(ctx, argBase, prop); \
	if (duk_is_number(ctx, -1)) def.field = (float)duk_get_number(ctx, -1); \
	duk_pop(ctx);
	BIOME_READ_FLOAT("heightScale", heightScale)
	BIOME_READ_FLOAT("heightBias", heightBias)
	BIOME_READ_FLOAT("creatureProbability", creatureProbability)
#undef BIOME_READ_FLOAT
	// 树种（可选）：none/oak/birch/pine/spruce
	duk_get_prop_string(ctx, argBase, "tree");
	if (duk_is_string(ctx, -1)) def.treeKind = duk_safe_to_string(ctx, -1);
	duk_pop(ctx);
	duk_get_prop_string(ctx, argBase, "spawns");
	if (duk_is_object(ctx, -1)) {
		jsReadBiomeSpawnList(ctx, -1, "monster", def.monsters);
		jsReadBiomeSpawnList(ctx, -1, "creature", def.creatures);
		jsReadBiomeSpawnList(ctx, -1, "water", def.water);
	}
	duk_pop(ctx);
	duk_get_prop_string(ctx, argBase, "getHeight");
	def.hasHeight = duk_is_function(ctx, -1) != 0;
	duk_pop(ctx);
	duk_get_prop_string(ctx, argBase, "decorate");
	def.hasDecorate = duk_is_function(ctx, -1) != 0;
	duk_pop(ctx);

	int realId = me->defineBiome(requestedId, def);
	if (realId < 0) {
		duk_push_error_object(ctx, DUK_ERR_ERROR, "Biome.define: biome id %d is already taken", requestedId);
		return duk_throw(ctx);
	}
	// 回调存成"全局函数"（跨 heap 查得到，与 Dimension.define 同一套做法）。
	if (def.hasHeight) {
		std::string key = "__biome_" + std::to_string(realId) + "_height";
		duk_get_prop_string(ctx, argBase, "getHeight");
		duk_put_global_string(ctx, key.c_str());
	}
	if (def.hasDecorate) {
		std::string key = "__biome_" + std::to_string(realId) + "_decorate";
		duk_get_prop_string(ctx, argBase, "decorate");
		duk_put_global_string(ctx, key.c_str());
	}
	duk_push_int(ctx, realId);
	return 1;
}

// JS Biome.distribution(dimId, function(x, z) { return 群系编号; })
// 返回 0 = 该坐标用原版群系。只有挂过回调的维度才进 JS，其它维度照旧。
static duk_ret_t jsBiomeDistribution(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	if (!me) return 0;
	if (!duk_is_number(ctx, 0) || !duk_is_function(ctx, 1)) {
		duk_push_error_object(ctx, DUK_ERR_ERROR, "Biome.distribution: need (dimId, function)");
		return duk_throw(ctx);
	}
	int dimId = (int)duk_get_int(ctx, 0);
	std::string key = "__biome_dist_" + std::to_string(dimId);
	duk_dup(ctx, 1);
	duk_put_global_string(ctx, key.c_str());
	me->markBiomeDistribution(dimId);
	return 0;
}

// JS Biome.list() -> [{id, name, mod}]（含原版 1..11）
static duk_ret_t jsBiomeList(duk_context* ctx) {
	duk_idx_t arr = duk_push_array(ctx);
	duk_uarridx_t n = 0;
	int vc = 0;
	Biome** van = Biome::all(vc);
	for (int i = 0; i < vc; ++i) {
		Biome* b = van[i];
		if (!b) continue;
		duk_idx_t obj = duk_push_object(ctx);
		duk_push_int(ctx, b->id);              duk_put_prop_string(ctx, obj, "id");
		duk_push_string(ctx, b->name.c_str()); duk_put_prop_string(ctx, obj, "name");
		duk_push_boolean(ctx, 0);              duk_put_prop_string(ctx, obj, "mod");
		duk_put_prop_index(ctx, arr, n++);
	}
	ModEngine* me = ModEngine::instance;
	if (me) {
		const std::map<int, ModEngine::ScriptedBiomeDef>& defs = me->scriptedBiomes();
		for (std::map<int, ModEngine::ScriptedBiomeDef>::const_iterator it = defs.begin(); it != defs.end(); ++it) {
			duk_idx_t obj = duk_push_object(ctx);
			duk_push_int(ctx, it->first);                  duk_put_prop_string(ctx, obj, "id");
			duk_push_string(ctx, it->second.name.c_str()); duk_put_prop_string(ctx, obj, "name");
			duk_push_boolean(ctx, 1);                      duk_put_prop_string(ctx, obj, "mod");
			duk_put_prop_index(ctx, arr, n++);
		}
	}
	return 1;
}

// JS Biome.get(编号或名字) -> {id, name, mod, temperature, downfall,
//    topMaterial, material, grassColor, foliageColor, fogColor} 或 null
static duk_ret_t jsBiomeGet(duk_context* ctx) {
	int wantId = 0;
	std::string wantName;
	if (duk_is_number(ctx, 0)) wantId = (int)duk_get_int(ctx, 0);
	else if (duk_is_string(ctx, 0)) wantName = duk_safe_to_string(ctx, 0);
	if (wantId <= 0 && !wantName.empty()) wantId = findBiomeIdByName(wantName);

	Biome* b = NULL;
	if (wantId > 0) {
		if (ModEngine::instance) b = ModEngine::instance->scriptedBiome(wantId);
		if (!b) {
			int vc = 0;
			Biome** van = Biome::all(vc);
			for (int i = 0; i < vc; ++i)
				if (van[i] && van[i]->id == wantId) { b = van[i]; break; }
		}
	}
	if (!b) {
		duk_push_null(ctx);
		return 1;
	}
	duk_push_object(ctx);
	duk_push_int(ctx, b->id);                             duk_put_prop_string(ctx, -2, "id");
	duk_push_string(ctx, b->name.c_str());                duk_put_prop_string(ctx, -2, "name");
	duk_push_boolean(ctx, b->isScriptedBiome() ? 1 : 0);  duk_put_prop_string(ctx, -2, "mod");
	duk_push_number(ctx, b->getTemperature());            duk_put_prop_string(ctx, -2, "temperature");
	duk_push_number(ctx, b->downfall);                    duk_put_prop_string(ctx, -2, "downfall");
	duk_push_int(ctx, b->topMaterial);                    duk_put_prop_string(ctx, -2, "topMaterial");
	duk_push_int(ctx, b->material);                       duk_put_prop_string(ctx, -2, "material");
	duk_push_int(ctx, b->grassColor);                     duk_put_prop_string(ctx, -2, "grassColor");
	duk_push_int(ctx, b->foliageColor);                   duk_put_prop_string(ctx, -2, "foliageColor");
	duk_push_int(ctx, b->fogColor);                       duk_put_prop_string(ctx, -2, "fogColor");
	return 1;
}

// JS Biome.undistribution(dimId) —— 取消某个维度的群系分布回调。
static duk_ret_t jsBiomeUndistribution(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	if (!me) return 0;
	me->undefineBiomeDistribution((int)duk_get_int(ctx, 0));
	return 0;
}

// JS Biome.clearCache() —— 分布回调的结果变了就调它（否则已缓存的坐标还是旧结果）。
static duk_ret_t jsBiomeClearCache(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	if (me) me->clearBiomeCache();
	(void)ctx;
	return 0;
}

// JS level.getBiome(x, z) -> { id, name, mod }
static duk_ret_t jsLevelGetBiome(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	Minecraft* mc = me ? me->minecraft() : NULL;
	if (!mc || !mc->level) return 0;
	Biome* b = mc->level->getBiome((int)duk_get_int(ctx, 0), (int)duk_get_int(ctx, 1));
	duk_push_object(ctx);
	duk_push_int(ctx, b ? b->id : 0);                duk_put_prop_string(ctx, -2, "id");
	duk_push_string(ctx, b ? b->name.c_str() : "");  duk_put_prop_string(ctx, -2, "name");
	duk_push_boolean(ctx, (b && b->isScriptedBiome()) ? 1 : 0); duk_put_prop_string(ctx, -2, "mod");
	return 1;
}

// JS level.findBiome(编号或名字 [, x, z [, 搜索半径]]) -> {x, z} 或 null
// 不传 x/z = 以本地玩家为中心；半径默认 512 格。给 /comebiome 这类指令用。
static duk_ret_t jsLevelFindBiome(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	Minecraft* mc = me ? me->minecraft() : NULL;
	duk_push_null(ctx);
	if (!mc || !mc->level) return 1;

	int wantId = 0;
	std::string wantName;
	if (duk_is_number(ctx, 0)) wantId = (int)duk_get_int(ctx, 0);
	else if (duk_is_string(ctx, 0)) wantName = duk_safe_to_string(ctx, 0);

	int top = duk_get_top(ctx);
	int cx = (top >= 3 && duk_is_number(ctx, 1)) ? (int)duk_get_int(ctx, 1) : (mc->player ? (int)mc->player->x : 0);
	int cz = (top >= 3 && duk_is_number(ctx, 2)) ? (int)duk_get_int(ctx, 2) : (mc->player ? (int)mc->player->z : 0);
	int radius = (top >= 4 && duk_is_number(ctx, 3)) ? (int)duk_get_int(ctx, 3) : 512;

	// 名字 → 编号（原版 + 模组，大小写不敏感）
	if (wantId <= 0 && !wantName.empty())
		wantId = findBiomeIdByName(wantName);
	if (wantId <= 0) {
		duk_pop(ctx);          // 丢掉开头的 null
		duk_push_null(ctx);
		return 1;
	}
	int ox = 0, oz = 0;
	if (!mc->level->findBiomeNear(wantId, cx, cz, radius, ox, oz)) {
		duk_pop(ctx);
		duk_push_null(ctx);
		return 1;
	}
	duk_pop(ctx);
	duk_push_object(ctx);
	duk_push_int(ctx, ox); duk_put_prop_string(ctx, -2, "x");
	duk_push_int(ctx, oz); duk_put_prop_string(ctx, -2, "z");
	return 1;
}

static duk_ret_t jsParticleDefine(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	if (!me) return 0;
	ModEngine::ScriptedParticleDef def;
	def.name = duk_safe_to_string(ctx, 0);
	if (duk_is_object(ctx, 1)) {
		duk_get_prop_string(ctx, 1, "texture");
		if (duk_is_string(ctx, -1)) def.texturePath = duk_safe_to_string(ctx, -1);
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "size");
		if (duk_is_number(ctx, -1)) def.size = (float)duk_get_number(ctx, -1);
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "lifetime");
		if (duk_is_number(ctx, -1)) def.lifetime = (int)duk_get_int(ctx, -1);
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "color");
		if (duk_is_array(ctx, -1)) {
			duk_get_prop_index(ctx, -1, 0);
			if (duk_is_number(ctx, -1)) def.r = (float)duk_get_number(ctx, -1);
			duk_pop(ctx);
			duk_get_prop_index(ctx, -1, 1);
			if (duk_is_number(ctx, -1)) def.g = (float)duk_get_number(ctx, -1);
			duk_pop(ctx);
			duk_get_prop_index(ctx, -1, 2);
			if (duk_is_number(ctx, -1)) def.b = (float)duk_get_number(ctx, -1);
			duk_pop(ctx);
		}
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "gravity");
		if (duk_is_number(ctx, -1)) def.gravity = (float)duk_get_number(ctx, -1);
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "shrink");
		if (duk_is_boolean(ctx, -1)) def.shrink = duk_get_boolean(ctx, -1) != 0;
		duk_pop(ctx);
	}
	me->defineParticle(def);
	return 0;
}

#include <cstdlib>

// ===========================================================================
// 自定义方块：模型 / 动画 / 微方块 / 放置接管
// ===========================================================================

// 坐标打包成一个 64 位键（微方块几何缓存用）。
long long ModEngine::packPos(int x, int y, int z) {
	return ((long long)(x & 0x3FFFFFF) << 38) | ((long long)(y & 0xFFF) << 26) | (long long)(z & 0x3FFFFFF);
}

// 方块回调的名字：__mod_block<kind>_<逻辑id>。
// 存在"全局函数"里（和 defineMob 的 anim 一样）—— 每个 mod 一个独立 heap，
// 只有全局变量能被跨 heap 的查找（callAnimKey 那套）找到；存 heap stash
// 会永远取不到。
std::string ModEngine::blockCbKey(const char* kind, int tileId) {
	int keyId = tileId;
	std::map<int, int>::const_iterator it = _modBlockPhysId.find(tileId);
	if (it != _modBlockPhysId.end())
		keyId = it->second;
	return std::string("__mod_block") + kind + "_" + std::to_string(keyId);
}

void ModEngine::setBlockModel(int tileId, const std::vector<ModBlockPart>& parts) {
	if (tileId <= 0 || tileId >= Tile::NUM_BLOCK_TYPES)
		return;
	ModTile* mt = dynamic_cast<ModTile*>(Tile::tiles[tileId]);
	if (!mt) {
		log("setBlockModel: id " + std::to_string(tileId) + " 不是模组方块，忽略模型");
		return;
	}
	mt->setModelParts(parts);
}

int ModEngine::injectBlockTexture(const std::string& path, int blockId) {
	return injectModBlockTexture(path, blockId);
}

// 模组方块的显示名：物品栏 / 掉落物 / 聊天都走 Tile::getDescriptionId()，
// 它是一个翻译 key（形如 "tile.modblock_200"），真正的文字由 I18n 提供。
void ModEngine::rememberBlockName(int physId, int logicalId, const std::string& name) {
	(void)logicalId;   // 物理 -> 逻辑的映射已在 defineBlock 里记过
	if (physId > 0 && physId < Tile::NUM_BLOCK_TYPES)
		_blockNames[physId] = name;
}

void ModEngine::setBlockName(int tileId, const std::string& name) {
	// 允许传物理 id（defineBlock 的返回值）或逻辑 id。
	int logical = tileId;
	std::map<int, int>::const_iterator it = _modBlockPhysId.find(tileId);
	if (it != _modBlockPhysId.end())
		logical = it->second;
	if (tileId > 0 && tileId < Tile::NUM_BLOCK_TYPES && Tile::tiles[tileId]) {
		Tile::tiles[tileId]->setDescriptionId("modblock_" + std::to_string(logical));
		_blockNames[tileId] = name;
	}
	I18n::setTranslation("tile.modblock_" + std::to_string(logical) + ".name", name);
}

// ---- 动画方块 --------------------------------------------------------------

bool ModEngine::registerAnimatedBlockAt(int tileId, int x, int y, int z) {
	if (tileId <= 0 || tileId >= Tile::NUM_BLOCK_TYPES)
		return false;
	ModTile* mt = dynamic_cast<ModTile*>(Tile::tiles[tileId]);
	if (!mt || !mt->isAnimated())
		return false;
	for (size_t i = 0; i < _animBlocks.size(); ++i) {
		if (_animBlocks[i].x == x && _animBlocks[i].y == y && _animBlocks[i].z == z) {
			_animBlocks[i].tileId = tileId;
			return false;
		}
	}
	if (_animBlocks.size() >= 512) {
		logThrottled("animlimit", "动画方块数量已达上限 512，新放下的动画方块不会再动（"
			+ std::to_string(x) + "," + std::to_string(y) + "," + std::to_string(z) + "）");
		return false;
	}
	AnimBlockRef r;
	r.x = x; r.y = y; r.z = z; r.tileId = tileId;
	_animBlocks.push_back(r);
	return true;
}

// 任意线程：区块重建时看到一个动画方块（它不会进网格）-> 投递给主线程。
void ModEngine::noteAnimatedBlockSeen(int tileId, int x, int y, int z) {
	std::lock_guard<std::mutex> lock(_animMutex);
	for (size_t i = 0; i < _animSeen.size(); ++i)
		if (_animSeen[i].x == x && _animSeen[i].y == y && _animSeen[i].z == z)
			return;
	if (_animSeen.size() > 4096)
		return;
	AnimBlockRef r;
	r.x = x; r.y = y; r.z = z; r.tileId = tileId;
	_animSeen.push_back(r);
}

// 主线程：消费投递队列。新登记的要标脏一次 —— 让所在区块重建，
// 把它的静态几何从网格里去掉（之后它就只由每帧通道画）。
void ModEngine::pumpAnimatedBlocks() {
	std::vector<AnimBlockRef> seen;
	{
		std::lock_guard<std::mutex> lock(_animMutex);
		if (_animSeen.empty())
			return;
		seen.swap(_animSeen);
	}
	for (size_t i = 0; i < seen.size(); ++i) {
		if (!registerAnimatedBlockAt(seen[i].tileId, seen[i].x, seen[i].y, seen[i].z))
			continue;
		if (_minecraft && _minecraft->levelRenderer)
			_minecraft->levelRenderer->setDirty(seen[i].x, seen[i].y, seen[i].z,
			                                    seen[i].x, seen[i].y, seen[i].z);
	}
}

void ModEngine::unregisterAnimatedBlockAt(int x, int y, int z) {
	for (size_t i = 0; i < _animBlocks.size(); ++i) {
		if (_animBlocks[i].x == x && _animBlocks[i].y == y && _animBlocks[i].z == z) {
			_animBlocks.erase(_animBlocks.begin() + i);
			return;
		}
	}
}

void ModEngine::clearAnimatedBlocks() {
	_animBlocks.clear();
}

float ModEngine::blockAnimTime() {
	Level* lv = _minecraft ? _minecraft->level : NULL;
	if (!lv)
		return 0.0f;
	// 世界时间 -> 秒（和生物的 anim 一样：1 秒 20 tick）。
	return (float)(lv->getTime() % 240000) / 20.0f;
}

bool ModEngine::getBlockPartPose(int tileId, const std::string& partName, int x, int y, int z, ModPartPose& out) {
	if (tileId <= 0 || tileId >= Tile::NUM_BLOCK_TYPES)
		return false;
	ModTile* mt = dynamic_cast<ModTile*>(Tile::tiles[tileId]);
	if (!mt || !mt->isAnimated())
		return false;
	Level* lv = _minecraft ? _minecraft->level : NULL;
	float age = lv ? (float)(lv->getTime() & 0xffff) : 0.0f;
	(void)x; (void)y; (void)z;
	return callAnimKey(mt->animKey(), partName, age, blockAnimTime(), -1, out);
}

// ---- 微方块（容器格）-------------------------------------------------------

// 一个部件： "x y z w h d tex [c] [r rx ry rz] [m ox oy oz]"
//   x y z w h d —— 像素（16 = 一整格）；任一维为 0 = 该维退化成平面
//   tex         —— terrain 贴图集槽号；-1 = 用方块自己的贴图
//   c           —— 贴图按部件占格比例裁剪（默认整张铺满部件表面）
//   r rx ry rz  —— 静态旋转（弧度，绕部件中心，依次 X→Y→Z）
//   m ox oy oz  —— 静态平移（像素）
bool ModEngine::parseCellPart(const std::vector<std::string>& t, ModBlockPart& out) {
	if (t.size() < 7)
		return false;
	const float inv = 1.0f / 16.0f;
	float v[6];
	for (int i = 0; i < 6; ++i)
		v[i] = (float)strtod(t[i].c_str(), NULL);
	int tex = (int)strtod(t[6].c_str(), NULL);
	out.x0 = v[0] * inv;
	out.y0 = v[1] * inv;
	out.z0 = v[2] * inv;
	out.x1 = (v[0] + v[3]) * inv;
	out.y1 = (v[1] + v[4]) * inv;
	out.z1 = (v[2] + v[5]) * inv;
	for (int i = 0; i < 6; ++i)
		out.tex[i] = tex;
	out.cx = out.selfCenterX();
	out.cy = out.selfCenterY();
	out.cz = out.selfCenterZ();
	for (size_t i = 7; i < t.size(); ++i) {
		if (t[i] == "c") {
			out.uvMode = 1;
		} else if (t[i] == "r" && i + 3 < t.size()) {
			out.rx = (float)strtod(t[i + 1].c_str(), NULL);
			out.ry = (float)strtod(t[i + 2].c_str(), NULL);
			out.rz = (float)strtod(t[i + 3].c_str(), NULL);
			i += 3;
		} else if (t[i] == "m" && i + 3 < t.size()) {
			out.ox = (float)strtod(t[i + 1].c_str(), NULL) * inv;
			out.oy = (float)strtod(t[i + 2].c_str(), NULL) * inv;
			out.oz = (float)strtod(t[i + 3].c_str(), NULL) * inv;
			i += 3;
		}
	}
	return true;
}

void ModEngine::parseCellGeomString(const std::string& s, std::vector<ModBlockPart>& out) {
	out.clear();
	size_t start = 0;
	while (true) {
		size_t bar = s.find('|', start);
		std::string item = (bar == std::string::npos) ? s.substr(start) : s.substr(start, bar - start);
		std::vector<std::string> tok;
		size_t i = 0;
		while (i < item.size()) {
			while (i < item.size() && isspace((unsigned char)item[i])) ++i;
			size_t j = i;
			while (j < item.size() && !isspace((unsigned char)item[j])) ++j;
			if (j > i)
				tok.push_back(item.substr(i, j - i));
			i = j;
		}
		ModBlockPart p;
		if (parseCellPart(tok, p))
			out.push_back(p);
		if (bar == std::string::npos)
			break;
		start = bar + 1;
	}
}

bool ModEngine::getCellParts(int x, int y, int z, const std::vector<ModBlockPart>*& out) {
	out = NULL;
	long long k = packPos(x, y, z);
	std::lock_guard<std::mutex> lock(_cellPartsMutex);
	// 已经算过这一格（_cellPartsRaw 里有记录，空串也算"已知没有几何"）。
	if (_cellPartsRaw.find(k) != _cellPartsRaw.end()) {
		std::map<long long, std::vector<ModBlockPart> >::iterator it = _cellParts.find(k);
		if (it != _cellParts.end()) {
			out = &it->second;
			return true;
		}
		return false;
	}
	// 没算过：交给主线程补算（区块重建可能跑在后台线程，那里不能读
	// TileEntity、更不能进 Duktape）。补算完会标脏这一格，下一帧就有几何。
	_cellPending.insert(k);
	return false;
}

void ModEngine::pumpCellGeometry() {
	std::set<long long> pending;
	{
		std::lock_guard<std::mutex> lock(_cellPartsMutex);
		if (_cellPending.empty())
			return;
		pending.swap(_cellPending);
	}
	Level* level = _minecraft ? _minecraft->level : NULL;
	if (!level)
		return;
	std::vector<int> dirty;
	for (std::set<long long>::iterator it = pending.begin(); it != pending.end(); ++it) {
		long long k = *it;
		int x = (int)((k >> 38) & 0x3FFFFFF);
		if (x & 0x2000000) x -= 0x4000000;
		int y = (int)((k >> 26) & 0xFFF);
		int z = (int)(k & 0x3FFFFFF);
		if (z & 0x2000000) z -= 0x4000000;
		std::string geom;
		TileEntity* te = level->getTileEntity(x, y, z);
		ModTileEntity* mte = dynamic_cast<ModTileEntity*>(te);
		if (mte && mte->hasKey("geom"))
			geom = mte->getKey("geom");
		std::vector<ModBlockPart> parts;
		if (!geom.empty())
			parseCellGeomString(geom, parts);
		{
			std::lock_guard<std::mutex> lock(_cellPartsMutex);
			if (parts.empty()) {
				_cellParts.erase(k);
				_cellPartsRaw[k] = geom;   // 记住"已知为空"，别反复排队
			} else {
				_cellParts[k] = parts;
				_cellPartsRaw[k] = geom;
			}
		}
		dirty.push_back(x);
		dirty.push_back(y);
		dirty.push_back(z);
	}
	if (!dirty.empty() && _minecraft && _minecraft->levelRenderer) {
		for (size_t i = 0; i + 2 < dirty.size(); i += 3)
			_minecraft->levelRenderer->setDirty(dirty[i], dirty[i + 1], dirty[i + 2],
			                                    dirty[i], dirty[i + 1], dirty[i + 2]);
	}
}

void ModEngine::noteCellGeomChanged(int x, int y, int z) {
	long long k = packPos(x, y, z);
	{
		std::lock_guard<std::mutex> lock(_cellPartsMutex);
		_cellPartsRaw.erase(k);
		_cellParts.erase(k);
		_cellPending.insert(k);
	}
	if (_minecraft && _minecraft->levelRenderer)
		_minecraft->levelRenderer->setDirty(x, y, z, x, y, z);
}

bool ModEngine::getCellBoxes(int x, int y, int z, const AABB* box, std::vector<AABB>& boxes) {
	const std::vector<ModBlockPart>* parts = NULL;
	if (!getCellParts(x, y, z, parts) || !parts)
		return true;   // 几何还没算好 -> 这一帧先没有碰撞箱（下一帧补上）
	for (size_t i = 0; i < parts->size(); ++i) {
		const ModBlockPart& p = (*parts)[i];
		if (p.x1 <= p.x0 || p.y1 <= p.y0 || p.z1 <= p.z0)
			continue;   // 平面部件不挡路
		AABB bb(x + p.x0, y + p.y0, z + p.z0, x + p.x1, y + p.y1, z + p.z1);
		if (box == NULL || box->intersects(bb))
			boxes.push_back(bb);
	}
	return true;
}

// ---- 放置 ------------------------------------------------------------------

// 跨 heap 调一个"全局函数"式的方块回调（placeData）。10 个参数。
// 找到并成功调用返回 true（调用结果由 out 带出），没找到/出错返回 false。
bool ModEngine::callBlockPlaceData(int tileId, int x, int y, int z, int face, float cx, float cy, float cz, int itemValue, int& out) {
	std::lock_guard<std::recursive_mutex> _jsLock(_jsMutex);
	const std::string key = blockCbKey("placedata", tileId);
	float yRot = 0.0f, pitch = 0.0f;
	if (_minecraft && _minecraft->player) {
		yRot = _minecraft->player->yRot;
		pitch = _minecraft->player->xRot;
	}
	std::vector<duk_context*> heaps;
	for (size_t s = 0; s < _sandboxes.size(); ++s)
		if (_sandboxes[s].ctx)
			heaps.push_back(_sandboxes[s].ctx);
	if (_ctx)
		heaps.push_back((duk_context*)_ctx);
	for (size_t h = 0; h < heaps.size(); ++h) {
		duk_context* ctx = heaps[h];
		duk_push_global_object(ctx);
		duk_get_prop_string(ctx, -1, key.c_str());
		if (!duk_is_function(ctx, -1)) {
			duk_pop_2(ctx);
			continue;
		}
		duk_push_int(ctx, x);
		duk_push_int(ctx, y);
		duk_push_int(ctx, z);
		duk_push_int(ctx, face);
		duk_push_number(ctx, cx);
		duk_push_number(ctx, cy);
		duk_push_number(ctx, cz);
		duk_push_int(ctx, itemValue);
		duk_push_number(ctx, yRot);
		duk_push_number(ctx, pitch);
		if (duk_pcall(ctx, 10) != 0) {
			logThrottled("placedata", std::string("方块 placeData 回调出错: ") + duk_safe_to_string(ctx, -1));
			duk_pop_2(ctx);
			return false;
		}
		bool ok = false;
		if (duk_is_number(ctx, -1)) {
			out = (int)duk_get_number(ctx, -1);
			ok = true;
		}
		duk_pop_2(ctx);
		return ok;
	}
	return false;
}

// 放置接管：模组顶层函数 onPlaceAttempt(x,y,z,face,cx,cy,cz,itemId,yRot,pitch)。
//   false / 不返回  -> 不接管，走原版逻辑
//   true            -> 模组自己处理了这次放置
//   对象 {x,y,z,id,data} 或数组 [{...}] -> 让引擎替它放（写上 id + data）
bool ModEngine::tryModPlace(Player* player, Level* level, int x, int y, int z, int face, float cx, float cy, float cz, ItemInstance* item) {
	std::lock_guard<std::recursive_mutex> _jsLock(_jsMutex);
	if (!level)
		return false;
	int itemId = item ? item->id : 0;
	float yRot = player ? player->yRot : 0.0f;
	float pitch = player ? player->xRot : 0.0f;

	std::vector<duk_context*> heaps;
	for (size_t s = 0; s < _sandboxes.size(); ++s)
		if (_sandboxes[s].ctx)
			heaps.push_back(_sandboxes[s].ctx);
	if (_ctx)
		heaps.push_back((duk_context*)_ctx);

	for (size_t h = 0; h < heaps.size(); ++h) {
		duk_context* ctx = heaps[h];
		duk_get_global_string(ctx, "onPlaceAttempt");
		if (!duk_is_function(ctx, -1)) {
			duk_pop(ctx);
			continue;
		}
		duk_push_int(ctx, x);
		duk_push_int(ctx, y);
		duk_push_int(ctx, z);
		duk_push_int(ctx, face);
		duk_push_number(ctx, cx);
		duk_push_number(ctx, cy);
		duk_push_number(ctx, cz);
		duk_push_int(ctx, itemId);
		duk_push_number(ctx, yRot);
		duk_push_number(ctx, pitch);
		if (duk_pcall(ctx, 10) != 0) {
			logThrottled("place-attempt", std::string("onPlaceAttempt 出错: ") + duk_safe_to_string(ctx, -1));
			duk_pop(ctx);
			continue;
		}
		bool handled = false;
		if (duk_is_object(ctx, -1)) {
			// 对象 / 数组：引擎替模组落块。
			if (duk_is_array(ctx, -1)) {
				duk_size_t n = duk_get_length(ctx, -1);
				for (duk_size_t i = 0; i < n; ++i) {
					duk_get_prop_index(ctx, -1, (duk_uarridx_t)i);
					if (duk_is_object(ctx, -1)) {
						int px = x, py = y, pz = z, bid = 0, bdata = 0;
						duk_get_prop_string(ctx, -1, "x"); if (duk_is_number(ctx, -1)) px = duk_get_int(ctx, -1); duk_pop(ctx);
						duk_get_prop_string(ctx, -1, "y"); if (duk_is_number(ctx, -1)) py = duk_get_int(ctx, -1); duk_pop(ctx);
						duk_get_prop_string(ctx, -1, "z"); if (duk_is_number(ctx, -1)) pz = duk_get_int(ctx, -1); duk_pop(ctx);
						duk_get_prop_string(ctx, -1, "id"); if (duk_is_number(ctx, -1)) bid = duk_get_int(ctx, -1); duk_pop(ctx);
						duk_get_prop_string(ctx, -1, "data"); if (duk_is_number(ctx, -1)) bdata = duk_get_int(ctx, -1); duk_pop(ctx);
						if (bid > 0)
							level->setTileAndData(px, py, pz, bid, bdata & 0xf);
					}
					duk_pop(ctx);
				}
			} else {
				int px = x, py = y, pz = z, bid = 0, bdata = 0;
				duk_get_prop_string(ctx, -1, "x"); if (duk_is_number(ctx, -1)) px = duk_get_int(ctx, -1); duk_pop(ctx);
				duk_get_prop_string(ctx, -1, "y"); if (duk_is_number(ctx, -1)) py = duk_get_int(ctx, -1); duk_pop(ctx);
				duk_get_prop_string(ctx, -1, "z"); if (duk_is_number(ctx, -1)) pz = duk_get_int(ctx, -1); duk_pop(ctx);
				duk_get_prop_string(ctx, -1, "id"); if (duk_is_number(ctx, -1)) bid = duk_get_int(ctx, -1); duk_pop(ctx);
				duk_get_prop_string(ctx, -1, "data"); if (duk_is_number(ctx, -1)) bdata = duk_get_int(ctx, -1); duk_pop(ctx);
				if (bid > 0)
					level->setTileAndData(px, py, pz, bid, bdata & 0xf);
			}
			handled = true;
		} else if (duk_is_boolean(ctx, -1) && duk_get_boolean(ctx, -1)) {
			handled = true;   // 模组自己处理了（它自己调了 level.setBlock）
		}
		duk_pop(ctx);
		if (handled)
			return true;
	}
	return false;
}

// 破坏接管：模组顶层函数 onBreakAttempt(x, y, z, face, cx, cy, cz, yRot, pitch)。
// 返回 true = 这次破坏模组自己处理了（引擎不破坏整个方块）—— 微方块靠它
// 只挖掉格内的一个小方块，方块本身留着装剩下的。返回 false/不返回 = 走原版。
bool ModEngine::tryModBreakBlock(int x, int y, int z, int face, float cx, float cy, float cz) {
	std::lock_guard<std::recursive_mutex> _jsLock(_jsMutex);
	float yRot = 0.0f, pitch = 0.0f;
	if (_minecraft && _minecraft->player) {
		yRot = _minecraft->player->yRot;
		pitch = _minecraft->player->xRot;
	}
	std::vector<duk_context*> heaps;
	for (size_t s = 0; s < _sandboxes.size(); ++s)
		if (_sandboxes[s].ctx)
			heaps.push_back(_sandboxes[s].ctx);
	if (_ctx)
		heaps.push_back((duk_context*)_ctx);

	for (size_t h = 0; h < heaps.size(); ++h) {
		duk_context* ctx = heaps[h];
		duk_get_global_string(ctx, "onBreakAttempt");
		if (!duk_is_function(ctx, -1)) {
			duk_pop(ctx);
			continue;
		}
		duk_push_int(ctx, x);
		duk_push_int(ctx, y);
		duk_push_int(ctx, z);
		duk_push_int(ctx, face);
		duk_push_number(ctx, cx);
		duk_push_number(ctx, cy);
		duk_push_number(ctx, cz);
		duk_push_number(ctx, yRot);
		duk_push_number(ctx, pitch);
		if (duk_pcall(ctx, 9) != 0) {
			logThrottled("break-attempt", std::string("onBreakAttempt 出错: ") + duk_safe_to_string(ctx, -1));
			duk_pop(ctx);
			continue;
		}
		bool handled = duk_is_boolean(ctx, -1) && duk_get_boolean(ctx, -1);
		duk_pop(ctx);
		if (handled)
			return true;
	}
	return false;
}

// 从栈上的一个 6 元数组读 box（像素单位）-> 格内坐标。
static void parseBoxArray(duk_context* ctx, int arrIdx, ModBlockPart& p) {
	const float inv = 1.0f / 16.0f;
	float v[6];
	for (int i = 0; i < 6; ++i) {
		duk_get_prop_index(ctx, arrIdx, (duk_uarridx_t)i);
		v[i] = (float)duk_get_number(ctx, -1);
		duk_pop(ctx);
	}
	p.x0 = v[0] * inv;
	p.y0 = v[1] * inv;
	p.z0 = v[2] * inv;
	p.x1 = (v[0] + v[3]) * inv;
	p.y1 = (v[1] + v[4]) * inv;
	p.z1 = (v[2] + v[5]) * inv;
	p.cx = p.selfCenterX();
	p.cy = p.selfCenterY();
	p.cz = p.selfCenterZ();
}

// 解析 model:[{...}] 里的一个部件对象（像素 -> 格内；角度 -> 弧度）。
static void parseBlockPartObject(duk_context* ctx, int idx, ModEngine* me, int physId, ModBlockPart& out, int index) {
	const float inv = 1.0f / 16.0f;
	const float deg2rad = 3.14159265358979f / 180.0f;
	duk_get_prop_string(ctx, idx, "box");
	if (duk_is_array(ctx, -1))
		parseBoxArray(ctx, -1, out);
	duk_pop(ctx);

	duk_get_prop_string(ctx, idx, "name");
	if (duk_is_string(ctx, -1))
		out.name = duk_safe_to_string(ctx, -1);
	duk_pop(ctx);
	if (out.name.empty())
		out.name = "part" + std::to_string(index);

	// bone：这个部件挂在哪根骨骼下。非空时渲染前会套上该骨骼这一帧的动画
	// （骨骼级 position/rotation 会带动它下面所有部件）。
	duk_get_prop_string(ctx, idx, "bone");
	if (duk_is_string(ctx, -1))
		out.bone = duk_safe_to_string(ctx, -1);
	duk_pop(ctx);

	// 贴图：数字 = terrain 槽号；字符串 = 包里的 png（注入贴图集）
	int tex = -1;
	duk_get_prop_string(ctx, idx, "texture");
	if (duk_is_number(ctx, -1))
		tex = (int)duk_get_int(ctx, -1);
	else if (duk_is_string(ctx, -1))
		tex = me->injectBlockTexture(duk_safe_to_string(ctx, -1), physId);
	duk_pop(ctx);
	for (int i = 0; i < 6; ++i)
		out.tex[i] = tex;

	// faces: [下,上,北,南,西,东]（每项数字槽号或 png 路径）
	duk_get_prop_string(ctx, idx, "faces");
	if (duk_is_array(ctx, -1) && duk_get_length(ctx, -1) >= 6) {
		for (int i = 0; i < 6; ++i) {
			duk_get_prop_index(ctx, -1, (duk_uarridx_t)i);
			if (duk_is_number(ctx, -1))
				out.tex[i] = (int)duk_get_int(ctx, -1);
			else if (duk_is_string(ctx, -1))
				out.tex[i] = me->injectBlockTexture(duk_safe_to_string(ctx, -1), physId);
			duk_pop(ctx);
		}
	}
	duk_pop(ctx);

	// pivot（旋转中心，像素）
	duk_get_prop_string(ctx, idx, "pivot");
	if (duk_is_array(ctx, -1) && duk_get_length(ctx, -1) >= 3) {
		duk_get_prop_index(ctx, -1, 0); out.cx = (float)duk_get_number(ctx, -1) * inv; duk_pop(ctx);
		duk_get_prop_index(ctx, -1, 1); out.cy = (float)duk_get_number(ctx, -1) * inv; duk_pop(ctx);
		duk_get_prop_index(ctx, -1, 2); out.cz = (float)duk_get_number(ctx, -1) * inv; duk_pop(ctx);
	}
	duk_pop(ctx);

	// rot（静态旋转，度；内部存弧度）
	duk_get_prop_string(ctx, idx, "rot");
	if (duk_is_array(ctx, -1) && duk_get_length(ctx, -1) >= 3) {
		duk_get_prop_index(ctx, -1, 0); out.rx = (float)duk_get_number(ctx, -1) * deg2rad; duk_pop(ctx);
		duk_get_prop_index(ctx, -1, 1); out.ry = (float)duk_get_number(ctx, -1) * deg2rad; duk_pop(ctx);
		duk_get_prop_index(ctx, -1, 2); out.rz = (float)duk_get_number(ctx, -1) * deg2rad; duk_pop(ctx);
	}
	duk_pop(ctx);

	// move（静态平移，像素）
	duk_get_prop_string(ctx, idx, "move");
	if (duk_is_array(ctx, -1) && duk_get_length(ctx, -1) >= 3) {
		duk_get_prop_index(ctx, -1, 0); out.ox = (float)duk_get_number(ctx, -1) * inv; duk_pop(ctx);
		duk_get_prop_index(ctx, -1, 1); out.oy = (float)duk_get_number(ctx, -1) * inv; duk_pop(ctx);
		duk_get_prop_index(ctx, -1, 2); out.oz = (float)duk_get_number(ctx, -1) * inv; duk_pop(ctx);
	}
	duk_pop(ctx);

	// uv: "crop" = 按部件比例裁剪贴图（默认整张铺满）
	// 也可以给"每面一个 UV 矩形"，配合模型级 modelTexture 用独立大贴图：
	//   uv: { down:[u,v,w,h], up:[...], north:[...], south:[...], west:[...], east:[...] }
	//   或 uv: [[u,v,w,h], ...]（同样按 下上北南西东 顺序）
	// 单位 = 贴图像素，左上原点，和基岩版模型一致；给了 modelTexture 时才生效。
	duk_get_prop_string(ctx, idx, "uv");
	if (duk_is_string(ctx, -1)) {
		const char* u = duk_safe_to_string(ctx, -1);
		if (strcmp(u, "crop") == 0)
			out.uvMode = 1;
	} else if (duk_is_object(ctx, -1)) {
		static const char* FACE_KEYS[6] = { "down", "up", "north", "south", "west", "east" };
		const bool byIndex = (duk_is_array(ctx, -1) != 0);
		for (int f = 0; f < 6; ++f) {
			if (byIndex)
				duk_get_prop_index(ctx, -1, (duk_uarridx_t)f);
			else
				duk_get_prop_string(ctx, -1, FACE_KEYS[f]);
			if (duk_is_array(ctx, -1) && duk_get_length(ctx, -1) >= 4) {
				for (int k = 0; k < 4; ++k) {
					duk_get_prop_index(ctx, -1, (duk_uarridx_t)k);
					out.uvRect[f][k] = (float)duk_get_number(ctx, -1);
					duk_pop(ctx);
				}
			} else if (duk_is_object(ctx, -1)) {
				// 也允许写成 {u,v,w,h}
				const char* kk[4] = { "u", "v", "w", "h" };
				for (int k = 0; k < 4; ++k) {
					duk_get_prop_string(ctx, -1, kk[k]);
					if (duk_is_number(ctx, -1))
						out.uvRect[f][k] = (float)duk_get_number(ctx, -1);
					duk_pop(ctx);
				}
			}
			duk_pop(ctx);
		}
	}
	duk_pop(ctx);

	duk_get_prop_string(ctx, idx, "emissive");
	if (duk_is_boolean(ctx, -1))
		out.emissive = duk_get_boolean(ctx, -1) != 0;
	duk_pop(ctx);

	// data: 3 或 [1,2] —— 只在这些数据值时画这个部件
	duk_get_prop_string(ctx, idx, "data");
	if (duk_is_number(ctx, -1)) {
		out.whenData.push_back((int)duk_get_int(ctx, -1));
	} else if (duk_is_array(ctx, -1)) {
		duk_size_t n = duk_get_length(ctx, -1);
		for (duk_size_t i = 0; i < n; ++i) {
			duk_get_prop_index(ctx, -1, (duk_uarridx_t)i);
			if (duk_is_number(ctx, -1))
				out.whenData.push_back((int)duk_get_int(ctx, -1));
			duk_pop(ctx);
		}
	}
	duk_pop(ctx);
}

// 解析 defineBlock 的几何选项：model > box > height（返回部件列表，空 = 没有模型）。
static void parseBlockModelOption(duk_context* ctx, int objIdx, ModEngine* me, int physId, std::vector<ModBlockPart>& out) {
	out.clear();
	duk_get_prop_string(ctx, objIdx, "model");
	if (duk_is_array(ctx, -1)) {
		duk_size_t n = duk_get_length(ctx, -1);
		for (duk_size_t i = 0; i < n; ++i) {
			duk_get_prop_index(ctx, -1, (duk_uarridx_t)i);
			if (duk_is_object(ctx, -1)) {
				ModBlockPart p;
				parseBlockPartObject(ctx, -1, me, physId, p, (int)i);
				out.push_back(p);
			}
			duk_pop(ctx);
		}
	}
	duk_pop(ctx);
	if (!out.empty())
		return;

	duk_get_prop_string(ctx, objIdx, "box");
	if (duk_is_array(ctx, -1) && duk_get_length(ctx, -1) >= 6) {
		ModBlockPart p;
		p.name = "main";
		parseBoxArray(ctx, -1, p);
		out.push_back(p);
	}
	duk_pop(ctx);
	if (!out.empty())
		return;

	duk_get_prop_string(ctx, objIdx, "height");
	if (duk_is_number(ctx, -1)) {
		float h = (float)duk_get_number(ctx, -1);
		if (h <= 0) h = 1;
		if (h > 1) h = 1;
		ModBlockPart p;
		p.name = "main";
		p.x0 = 0; p.z0 = 0; p.x1 = 1; p.z1 = 1;
		p.y0 = 0; p.y1 = h;
		p.cx = 0.5f; p.cy = h * 0.5f; p.cz = 0.5f;
		out.push_back(p);
	}
	duk_pop(ctx);
}

// 模型级独立贴图：读 modelTexture（包里的 png 路径），把它的像素尺寸回填给每个
// 部件 —— 渲染时按 uvRect/尺寸算归一化 uv，并用这张图的独立 GL 纹理。
// 返回贴图路径（空 = 没有独立贴图，全部走 terrain 图集，老模组行为不变）。
static std::string resolveModelTexture(duk_context* ctx, int objIdx, ModEngine* me, std::vector<ModBlockPart>& parts) {
	duk_get_prop_string(ctx, objIdx, "modelTexture");
	std::string path;
	if (duk_is_string(ctx, -1))
		path = duk_safe_to_string(ctx, -1);
	duk_pop(ctx);
	if (path.empty() || me == NULL || parts.empty())
		return std::string();
	int tw = 0, th = 0;
	if (!me->getModTextureSize(path, tw, th)) {
		// 兜底：模组自报尺寸（模型级 texSize: [w, h]）。
		// 独立纹理是按路径另建的，万一这张 png 没进贴图像素表，也得把 UV 算对，
		// 否则每个面的 uvRect 全部失效、退回图集槽（表现就是"枪上贴着方块图集"）。
		duk_get_prop_string(ctx, objIdx, "texSize");
		if (duk_is_array(ctx, -1) && duk_get_length(ctx, -1) >= 2) {
			duk_get_prop_index(ctx, -1, 0); tw = (int)duk_get_number(ctx, -1); duk_pop(ctx);
			duk_get_prop_index(ctx, -1, 1); th = (int)duk_get_number(ctx, -1); duk_pop(ctx);
		}
		duk_pop(ctx);
	}
	if (tw > 0 && th > 0) {
		for (size_t i = 0; i < parts.size(); ++i) {
			parts[i].texW = (float)tw;
			parts[i].texH = (float)th;
		}
	} else {
		me->log("model: modelTexture 找不到尺寸（表里没有、也没给 texSize）: " + path);
		return std::string();
	}
	return path;
}

static duk_ret_t jsBlockDefineBlock(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	if (!me) return 0;
	int id = (int)duk_get_int(ctx, 0);
	std::string name = "block" + std::to_string(id);
	std::string material = "stone";
	std::string texPath;
	int tex = 1;
	int renderLayer = 0;  // 0 opaque / 1 alphatest / 2 blend
	int renderShape = 0;  // 0 block / 1 cross (flower/bush)
	// Optional per-face textures (like vanilla grass: top/side/bottom).
	// Empty paths fall back to the main texture on that face.
	std::string topTex, sideTex, bottomTex;
	// Mining behaviour: destroyTime (seconds-ish, smaller = faster) and
	// requiresPickaxe (ore blocks drop nothing without a pickaxe).
	float destroyTime = 0.6f;
	bool requiresPickaxe = false;
	bool blockEntity = false;
	int light = 0;   // 0-15 emissive brightness (0 = not emissive)
	if (duk_is_object(ctx, 1)) {
		// 几何是否存在（决定默认渲染层）；模组显式给了 renderLayer/transparent
		// 就听它的。模型贴图常带透明区（一条腿、一本书），默认走 alphatest，
		// 在不透明层会被画成黑块。
		bool hasGeom = false;
		duk_get_prop_string(ctx, 1, "model");
		hasGeom = duk_is_array(ctx, -1) && duk_get_length(ctx, -1) > 0;
		duk_pop(ctx);
		if (!hasGeom) {
			duk_get_prop_string(ctx, 1, "box");
			hasGeom = duk_is_array(ctx, -1);
			duk_pop(ctx);
		}
		if (!hasGeom) {
			duk_get_prop_string(ctx, 1, "height");
			hasGeom = duk_is_number(ctx, -1);
			duk_pop(ctx);
		}
		if (!hasGeom) {
			// 微方块（micro:true）的外观来自格内几何，同样按 alphatest 走
			// （贴图常带透明区，放不透明层会被画成黑块）。
			duk_get_prop_string(ctx, 1, "micro");
			hasGeom = duk_is_boolean(ctx, -1) && duk_get_boolean(ctx, -1);
			duk_pop(ctx);
		}
		bool layerExplicit = false;
		duk_get_prop_string(ctx, 1, "renderLayer");
		if (!duk_is_undefined(ctx, -1)) layerExplicit = true;
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "transparent");
		if (!duk_is_undefined(ctx, -1)) layerExplicit = true;
		duk_pop(ctx);
		if (hasGeom && !layerExplicit)
			renderLayer = 1;

		duk_get_prop_string(ctx, 1, "name");
		if (duk_is_string(ctx, -1)) name = duk_safe_to_string(ctx, -1);
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "texture");
		if (duk_is_number(ctx, -1)) tex = (int)duk_get_int(ctx, -1);
		else if (duk_is_string(ctx, -1)) texPath = duk_safe_to_string(ctx, -1);
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "material");
		if (duk_is_string(ctx, -1)) material = duk_safe_to_string(ctx, -1);
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "transparent");
		if (duk_is_boolean(ctx, -1)) renderLayer = duk_get_boolean(ctx, -1) ? 2 : 0;
		duk_pop(ctx);
		// renderLayer: "opaque" | "alphatest" | "blend" | "sway"
		duk_get_prop_string(ctx, 1, "renderLayer");
		if (duk_is_string(ctx, -1)) {
			const char* rl = duk_safe_to_string(ctx, -1);
			if (strcmp(rl, "alphatest") == 0) renderLayer = 1;
			else if (strcmp(rl, "blend") == 0) renderLayer = 2;
			else if (strcmp(rl, "sway") == 0) renderLayer = 3; // RENDERLAYER_ANIM 动画层(protected, 此处用值)
			else renderLayer = 0;
		}
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "shape");
		if (duk_is_string(ctx, -1)) {
			const char* sh = duk_safe_to_string(ctx, -1);
			if (strcmp(sh, "cross") == 0) renderShape = Tile::SHAPE_CROSS_TEXTURE;
		}
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "topTexture");
		if (duk_is_string(ctx, -1)) topTex = duk_safe_to_string(ctx, -1);
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "sideTexture");
		if (duk_is_string(ctx, -1)) sideTex = duk_safe_to_string(ctx, -1);
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "bottomTexture");
		if (duk_is_string(ctx, -1)) bottomTex = duk_safe_to_string(ctx, -1);
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "destroyTime");
		if (duk_is_number(ctx, -1)) destroyTime = (float)duk_get_number(ctx, -1);
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "requiresPickaxe");
		if (duk_is_boolean(ctx, -1)) requiresPickaxe = duk_get_boolean(ctx, -1) != 0;
		duk_pop(ctx);
		// Lifecycle callbacks (stage 2): store any function-valued options
		// into the heap stash as __mod_tile_<evt>_<id>, then ModTile's
		// virtual overrides dispatch to them via ModEngine::callTileEvent.
		static const struct { const char* opt; const char* evt; } cbs[] = {
			{ "onTick",           "tick"      },
			{ "onUse",            "use"       },
			{ "onPlace",          "place"     },
			{ "onRemove",         "remove"    },
			{ "onNeighborChanged","neighbor"  },
			{ "onStepOn",         "stepon"    },
		};
		for (size_t ci = 0; ci < sizeof(cbs)/sizeof(cbs[0]); ++ci) {
			duk_get_prop_string(ctx, 1, cbs[ci].opt);
			if (duk_is_function(ctx, -1)) {
				std::string key = std::string("__mod_tile_") + cbs[ci].evt + "_" + std::to_string(id);
				duk_push_heap_stash(ctx);
				duk_insert(ctx, -2);            // [fn, stash] -> [stash, fn]
				duk_put_prop_string(ctx, -2, key.c_str());
				duk_pop(ctx);
			} else {
				duk_pop(ctx);
			}
		}
		// Stage 3: blockEntity option -> attach a ModTileEntity on placement.
		duk_get_prop_string(ctx, 1, "blockEntity");
		if (duk_is_boolean(ctx, -1)) blockEntity = duk_get_boolean(ctx, -1) != 0;
		duk_pop(ctx);
		// Emissive light: 0-15. Same range as level.setLight; a block with
		// light>0 glows like a torch/lightgem and lights its surroundings.
		duk_get_prop_string(ctx, 1, "light");
		if (duk_is_number(ctx, -1)) light = duk_get_int(ctx, -1);
		duk_pop(ctx);
	}
	int physId = me->defineBlock(id, name, tex, texPath, material, renderLayer, renderShape, topTex, sideTex, bottomTex, destroyTime, requiresPickaxe, blockEntity, light);
	// ---- 显示名 ----
	// 每个模组方块要有一个自己的翻译 key 并注册到 I18n，否则物品栏 / 掉落物 /
	// 聊天里显示的是翻译键本身（形如 "tile.234.name<"）。key 用**逻辑 id**
	// （模组自己写的那个），这样即使物理槽位变了名字也稳定。
	{
		std::string dkey = "modblock_" + std::to_string(id);
		if (physId > 0 && physId < Tile::NUM_BLOCK_TYPES && Tile::tiles[physId]) {
			Tile::tiles[physId]->setDescriptionId(dkey);
			I18n::setTranslation("tile." + dkey + ".name", name);
			me->log("defineBlock: " + name + " 的显示名 -> tile." + dkey + ".name = " + name);
		}
		me->rememberBlockName(physId, id, name);
	}
	// ---- 自定义几何 / 动画 / 微方块 / 放置（都在物理 id 定下来之后应用：
	//      部件贴图要注入贴图集，而注入需要方块 id）----
	ModTile* mt = (physId > 0 && physId < Tile::NUM_BLOCK_TYPES) ? dynamic_cast<ModTile*>(Tile::tiles[physId]) : NULL;
	if (mt && duk_is_object(ctx, 1)) {
		// ① 几何部件（model > box > height）
		std::vector<ModBlockPart> parts;
		parseBlockModelOption(ctx, 1, me, physId, parts);
		if (!parts.empty()) {
			me->setBlockModel(physId, parts);
			me->log("defineBlock: " + name + " 自定义几何 " + std::to_string(parts.size()) + " 个部件");
		}

		// ② 动画回调 -> 全局函数（和 defineMob 的 anim 同一存法）
		std::string animKey = "__mod_blockanim_" + std::to_string(id);
		duk_get_prop_string(ctx, 1, "anim");
		if (duk_is_function(ctx, -1)) {
			duk_dup(ctx, -1);
			duk_put_global_string(ctx, animKey.c_str());
			mt->setAnimKey(animKey);
			me->log("defineBlock: " + name + " 有动画（每帧重绘）");
		}
		duk_pop(ctx);

		// ③ 放置数据值回调 -> 全局函数
		std::string pdKey = "__mod_blockplacedata_" + std::to_string(id);
		duk_get_prop_string(ctx, 1, "placeData");
		if (duk_is_function(ctx, -1)) {
			duk_dup(ctx, -1);
			duk_put_global_string(ctx, pdKey.c_str());
			mt->setPlaceDataKey(pdKey);
		}
		duk_pop(ctx);

		// ④ collision:false = 完全不挡路（贴地的线/薄片）
		duk_get_prop_string(ctx, 1, "collision");
		if (duk_is_boolean(ctx, -1) && !duk_get_boolean(ctx, -1))
			mt->setNoCollision(true);
		duk_pop(ctx);

		// ⑤ micro:true = 微方块（格内几何/碰撞来自方块实体数据键 "geom"）
		duk_get_prop_string(ctx, 1, "micro");
		if (duk_is_boolean(ctx, -1) && duk_get_boolean(ctx, -1)) {
			mt->setCellGeometry(true);
			mt->setHasBlockEntity(true);
			me->log("defineBlock: " + name + " 是微方块（格内几何走方块实体数据 geom）");
		}
		duk_pop(ctx);
	}
	// Return the REAL physical id so the mod can setBlock/spawnMob with it
	// (auto-allocation may have moved it when the logical slot was taken).
	duk_push_int(ctx, physId);
	return 1;
}

// Block.setState(id, "sway", amp): 给指定方块设渲染状态幅度。当前支持:
//   "sway"  — 随风摇摆幅度(0=关[默认], >0 开启, 1.0=常规)。仅对渲染在
//             动画层(renderLayer:"sway" 或原版植被层)的方块有视觉作用;
//             同层内只摇开了的方块(逐顶点幅度, 互不影响)。
// id 用 defineBlock 返回的真实(物理)id;原版方块直接写方块 id。
// 返回 false = 未知状态 / id 越界。
static duk_ret_t jsBlockSetState(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	if (!me) return 0;
	int id = (int)duk_get_int(ctx, 0);
	int state = -1;
	if (duk_is_string(ctx, 1)) {
		const char* s = duk_safe_to_string(ctx, 1);
		if (strcmp(s, "sway") == 0) state = Tile::RENDER_STATE_SWAY;
		else if (strcmp(s, "Sway") == 0) state = Tile::RENDER_STATE_SWAY;
		else if (strcmp(s, "SWAY") == 0) state = Tile::RENDER_STATE_SWAY;
	}
	if (state < 0) {
		me->log("[Block.setState] unknown state: " + std::string(duk_safe_to_string(ctx, 1)));
		duk_push_false(ctx);
		return 1;
	}
	float amp = (float)duk_require_number(ctx, 2);
	if (id <= 0 || id >= Tile::NUM_BLOCK_TYPES) {
		duk_push_false(ctx);
		return 1;
	}
	Tile::setRenderStateAmp(id, state, amp);
	// 幅度在区块重建时固化进 sway 网格;重建受影响的全部区块使改动生效。
	if (me->minecraft() && me->minecraft()->levelRenderer)
		me->minecraft()->levelRenderer->allChanged();
	duk_push_true(ctx);
	return 1;
}

// Block.setName(id, name): 给模组方块（或原版方块）换显示名。
// id 用 defineBlock 返回的物理 id 或模组写的逻辑 id 都行。
// 名字会进物品栏 / 掉落物 / 聊天里的方块名。
static duk_ret_t jsBlockSetName(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	if (!me)
		return 0;
	int id = (int)duk_get_int(ctx, 0);
	if (!duk_is_string(ctx, 1)) {
		duk_push_false(ctx);
		return 1;
	}
	me->setBlockName(id, duk_safe_to_string(ctx, 1));
	duk_push_true(ctx);
	return 1;
}

// Block.injectTexture(path): 把一个 png 路径塞进 terrain 贴图集，返回槽号。
// 主要给微方块的格内小方块用 —— geom 里的 tex 字段就是槽号，
// 有了它就能让同一格里的不同小方块各用不同贴图。失败返回 -1。
static duk_ret_t jsBlockInjectTexture(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	if (!me || !duk_is_string(ctx, 0)) {
		duk_push_int(ctx, -1);
		return 1;
	}
	duk_push_int(ctx, me->injectBlockTexture(duk_safe_to_string(ctx, 0), 0));
	return 1;
}

static duk_ret_t jsItemDefineItem(duk_context* ctx) {
	int id = duk_get_int(ctx, 0);
	std::string name = "item" + std::to_string(id);
	int iconX = 0, iconY = 0;
	std::string iconPath;       // custom PNG icon (injected into gui/items.png)
	std::string type = "item";  // item | weapon | pickaxe | axe | shovel | armor
	std::string tierName = "stone"; // wood | stone | iron | emerald | gold
	int armorSlot = 0;          // 0 head 1 torso 2 legs 3 feet
	int armorDefense = 2;
	std::string armorMat = "iron"; // cloth | chain | iron | gold | diamond
	bool hasUse = false;
	// Stage 4: durability / attack damage / mining speed / food.
	int durability = 0;
	int attackDamage = 0;    // 0 = leave default (weapon tier) 
	float miningSpeed = 0.0f; // 0 = leave default
	int nutrition = 0;       // >0 = food item
	std::vector<ModBlockPart> itemModel;   // 3D 模型（model / box / height，复用方块解析）
	std::string modelTexPath;              // 模型级独立大贴图（modelTexture，空 = 走图集）
	bool itemHasAnim = false;              // 有 anim 回调（key 等最终 id 定了再注册）
	std::vector<ModBoneDef> itemBones;     // 骨架（bones 选项）
	if (duk_is_object(ctx, 1)) {
		// Item.defineItem(id, { name, icon, type, tier, armorSlot, armorDefense, armorMaterial, onUse })
		duk_get_prop_string(ctx, 1, "name");
		if (duk_is_string(ctx, -1)) name = duk_safe_to_string(ctx, -1);
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "icon");
		if (duk_is_array(ctx, -1)) {
			duk_get_prop_index(ctx, -1, 0); iconX = (int)duk_get_int(ctx, -1); duk_pop(ctx);
			duk_get_prop_index(ctx, -1, 1); iconY = (int)duk_get_int(ctx, -1); duk_pop(ctx);
		} else if (duk_is_string(ctx, -1)) {
			iconPath = duk_safe_to_string(ctx, -1);
		}
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "type");
		if (duk_is_string(ctx, -1)) type = duk_safe_to_string(ctx, -1);
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "tier");
		if (duk_is_string(ctx, -1)) tierName = duk_safe_to_string(ctx, -1);
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "armorSlot");
		if (duk_is_number(ctx, -1)) armorSlot = (int)duk_get_int(ctx, -1);
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "armorDefense");
		if (duk_is_number(ctx, -1)) armorDefense = (int)duk_get_int(ctx, -1);
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "armorMaterial");
		if (duk_is_string(ctx, -1)) armorMat = duk_safe_to_string(ctx, -1);
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "onUse");
		hasUse = duk_is_function(ctx, -1);
		if (!hasUse) duk_pop(ctx);
		// --- Stage 4 options ---
		duk_get_prop_string(ctx, 1, "durability");
		if (duk_is_number(ctx, -1)) durability = (int)duk_get_int(ctx, -1);
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "attackDamage");
		if (duk_is_number(ctx, -1)) attackDamage = (int)duk_get_int(ctx, -1);
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "miningSpeed");
		if (duk_is_number(ctx, -1)) miningSpeed = (float)duk_get_number(ctx, -1);
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "nutrition");
		if (duk_is_number(ctx, -1)) nutrition = (int)duk_get_int(ctx, -1);
		duk_pop(ctx);
		if (nutrition <= 0) {
			// Legacy alias: type:"food" + restoreHealth -> nutrition
			duk_get_prop_string(ctx, 1, "restoreHealth");
			if (duk_is_number(ctx, -1)) nutrition = (int)duk_get_int(ctx, -1);
			duk_pop(ctx);
		}
		// 3D 模型：直接复用方块那套解析（model:[{box,texture,faces,...}] / box / height）。
		// 贴图走 blocks/*.png → 进 terrain.png 图集，所以物品模型也绑 terrain.png。
		parseBlockModelOption(ctx, 1, ModEngine::instance, id, itemModel);
		modelTexPath = resolveModelTexture(ctx, 1, ModEngine::instance, itemModel);
		// bones: { 骨骼名: { pivot:[x,y,z](像素), parent:"父骨骼" } } —— 基岩骨架表
		duk_get_prop_string(ctx, 1, "bones");
		if (duk_is_object(ctx, -1)) {
			duk_enum(ctx, -1, DUK_ENUM_OWN_PROPERTIES_ONLY);
			while (duk_next(ctx, -1, 1)) {
				if (duk_is_object(ctx, -1)) {
					ModBoneDef b;
					const char* bn = duk_safe_to_string(ctx, -2);
					b.name = bn ? bn : "";
					duk_get_prop_string(ctx, -1, "pivot");
					if (duk_is_array(ctx, -1) && duk_get_length(ctx, -1) >= 3) {
						duk_get_prop_index(ctx, -1, 0); b.px = (float)duk_get_number(ctx, -1) * 0.0625f; duk_pop(ctx);
						duk_get_prop_index(ctx, -1, 1); b.py = (float)duk_get_number(ctx, -1) * 0.0625f; duk_pop(ctx);
						duk_get_prop_index(ctx, -1, 2); b.pz = (float)duk_get_number(ctx, -1) * 0.0625f; duk_pop(ctx);
					}
					duk_pop(ctx);
					duk_get_prop_string(ctx, -1, "parent");
					if (duk_is_string(ctx, -1))
						b.parent = duk_safe_to_string(ctx, -1);
					duk_pop(ctx);
					// rot：骨骼自己的静态旋转（度）。动画没给 rotation 时就用它。
					duk_get_prop_string(ctx, -1, "rot");
					if (duk_is_array(ctx, -1) && duk_get_length(ctx, -1) >= 3) {
						const float d2r = 3.14159265358979f / 180.0f;
						duk_get_prop_index(ctx, -1, 0); b.rx = (float)duk_get_number(ctx, -1) * d2r; duk_pop(ctx);
						duk_get_prop_index(ctx, -1, 1); b.ry = (float)duk_get_number(ctx, -1) * d2r; duk_pop(ctx);
						duk_get_prop_index(ctx, -1, 2); b.rz = (float)duk_get_number(ctx, -1) * d2r; duk_pop(ctx);
					}
					duk_pop(ctx);
					if (!b.name.empty())
						itemBones.push_back(b);
				}
				duk_pop_2(ctx);
			}
			duk_pop(ctx);
		}
		duk_pop(ctx);
		// anim 回调：先只记“有没有”，key 要等最终 id 定下来再注册。
		duk_get_prop_string(ctx, 1, "anim");
		itemHasAnim = duk_is_function(ctx, -1);
		duk_pop(ctx);
		if (type == "food" && nutrition <= 0)
			nutrition = 1;
	} else {
		// Legacy: Item.defineItem(id, name, iconX, iconY)
		name = duk_safe_to_string(ctx, 1);
		iconX = (int)duk_get_int(ctx, 2);
		iconY = (int)duk_get_int(ctx, 3);
	}
	if (id <= 0) {
		if (ModEngine::instance)
			ModEngine::instance->log("defineItem: id " + std::to_string(id) + " out of range");
		if (hasUse) duk_pop(ctx);
		return 0;
	}
	// 号被占用时自动向上找空闲槽（和 defineBlock 一样），免得模组之间抢同一个号。
	// 上限 255：物品实例 id = 256 + 物品号，超过就出界了。
	if (id > 255 || Item::items[id] != NULL) {
		int cand = (id < 255) ? id + 1 : 1;
		for (; cand < 256 && Item::items[cand] != NULL; ++cand) {}
		if (cand >= 256) {
			if (ModEngine::instance)
				ModEngine::instance->log("defineItem: id " + std::to_string(id) + " occupied and no free slot (1-255)");
			if (hasUse) duk_pop(ctx);
			return 0;
		}
		if (ModEngine::instance)
			ModEngine::instance->log("defineItem: id " + std::to_string(id) + " taken -> " + std::to_string(cand));
		id = cand;
	}
	const Item::Tier& tier =
		(tierName == "wood")    ? Item::Tier::WOOD :
		(tierName == "iron")    ? Item::Tier::IRON :
		(tierName == "emerald") ? Item::Tier::EMERALD :
		(tierName == "gold")    ? Item::Tier::GOLD :
		Item::Tier::STONE;
	Item* item;
	// Resolve icon: custom PNG path is injected into terrain.png; a
	// [x,y] pair is a vanilla 16px-grid slot index.
	//
	// 为什么是 terrain.png 而不是 gui/items.png：ItemRenderer / ItemInHandRenderer
	// 对 id < 256 的物品一律拿 terrain.png 采样图标槽（见 ItemRenderer.cpp 里
	// "item->id < 256 ? 512.0f : 256.0f"），而模组物品号都 < 256。注进 items.png
	// 的话槽号对不上，会采到 terrain 上原版那一格 —— 表现就是所有模组物品
	// （2D 图标和 3D 模型回退槽）都显示成岩浆。
	int icon = 0;
	if (!iconPath.empty())
		icon = ModEngine::instance ? ModEngine::instance->injectModBlockTexture(iconPath, id) : 1;
	else {
		// Clamp the vanilla 16px-grid slot so out-of-range JS values can't
		// sample outside the items.png atlas (garbage icon / wrong UVs).
		if (iconX < 0) iconX = 0;
		if (iconX > 15) iconX = 15;
		if (iconY < 0) iconY = 0;
		if (iconY > 15) iconY = 15;
		icon = iconX + iconY * 16;
	}
	// Stage 4: custom numeric overrides switch the item to ModItem so the
	// JS-provided durability / attack damage / mining speed take effect.
	// Without overrides, vanilla classes (WeaponItem/PickaxeItem/...) keep
	// their tier-based behaviour. Food always uses ModItem.
	bool modOverrides = durability > 0 || attackDamage > 0 || miningSpeed > 0.0f || nutrition > 0;
	// 有 3D 模型或动画的物品必须用 ModItem 承载（WeaponItem 那些挂不了模型）
	if (!itemModel.empty() || itemHasAnim) modOverrides = true;
	if (type == "weapon" && !modOverrides) item = new WeaponItem(id, tier);
	else if (type == "pickaxe" && !modOverrides) item = new PickaxeItem(id, tier);
	else if (type == "axe" && !modOverrides)     item = new HatchetItem(id, tier);
	else if (type == "shovel" && !modOverrides)  item = new ShovelItem(id, tier);
	else if (type == "armor") {
		const ArmorItem::ArmorMaterial& mat =
			(armorMat == "cloth")   ? ArmorItem::CLOTH :
			(armorMat == "chain")   ? ArmorItem::CHAIN :
			(armorMat == "gold")    ? ArmorItem::GOLD :
			(armorMat == "diamond") ? ArmorItem::DIAMOND :
			ArmorItem::IRON;
		item = new ArmorItem(id, mat, icon, armorSlot);
		if (durability > 0) item->maxDamage = durability;
	}
	else {
		item = (hasUse || modOverrides) ? (Item*)(new ModItem(id, id)) : (Item*)(new Item(id));
		if (ModItem* mi = dynamic_cast<ModItem*>(item)) {
			if (attackDamage > 0) mi->setModAttackDamage(attackDamage);
			if (miningSpeed > 0.0f) mi->setModMiningSpeed(miningSpeed);
			if (nutrition > 0) mi->setModFood(nutrition);
		}
	}
	// Stage 4: durability / stacked-by-data so the item can deplete and
	// break (hurt() removes it at maxDamage like vanilla tools).
	// durability>0 forces modOverrides, so the item is always a ModItem here.
	if (durability > 0) {
		if (ModItem* mi = dynamic_cast<ModItem*>(item))
			mi->setModDurability(durability);
	}
	// 3D 模型：部件面没指定贴图（tex=-1）的，回退到物品自己的图标槽 ——
	// 物品没有 Tile 可让 renderModelBox 取默认贴图（传 NULL 会崩），所以先填满。
	if (!itemModel.empty()) {
		for (size_t pi = 0; pi < itemModel.size(); ++pi)
			for (int f = 0; f < 6; ++f)
				if (itemModel[pi].tex[f] < 0) itemModel[pi].tex[f] = icon;
	}
	if (!itemModel.empty() || itemHasAnim || !itemBones.empty()) {
		if (ModItem* mi = dynamic_cast<ModItem*>(item)) {
			if (!itemModel.empty())
				mi->setModModel(itemModel);
				mi->setModModelTexture(modelTexPath);
			if (!itemBones.empty())
				mi->setModBones(itemBones);
			if (itemHasAnim) {
				std::string ak = "__mod_itemanim_" + std::to_string(id);
				duk_get_prop_string(ctx, 1, "anim");
				if (duk_is_function(ctx, -1)) {
					duk_dup(ctx, -1);
					duk_put_global_string(ctx, ak.c_str());
					mi->setModAnimKey(ak);
				}
				duk_pop(ctx);
			}
		}
	}
	item->setIcon(icon);
	item->setDescriptionId("mod_item_" + std::to_string(id));
	Item::items[id] = item;
	I18n::setTranslation("item.mod_item_" + std::to_string(id) + ".name", name);
	// Persist the onUse callback in heap stash for ModItem::useOn.
	if (hasUse) {
		std::string key = "__mod_item_use_" + std::to_string(id);
		duk_push_heap_stash(ctx);
		duk_insert(ctx, -2);            // [stash, fn] -> [fn, stash]
		duk_put_prop_string(ctx, -2, key.c_str());
		duk_pop(ctx);
	}
	if (ModEngine::instance) {
		ModEngine::instance->addCreativeItem(id);
		ModEngine::instance->_modItems.push_back(id);
	}
	// 返回真实物品号（可能因为自动分配而与模组写的不同），方便后续用它
	// 注册配方 / 生成物品堆栈。失败时上面的分支返回值 0（JS 里是 undefined）。
	duk_push_int(ctx, id);
	return 1;
}

// --- 模组物品的 3D 模型 / 动画查询（物品渲染器用）---
const std::vector<ModBlockPart>* ModEngine::itemModelParts(int itemId) const {
	if (itemId <= 0 || itemId >= Item::MAX_ITEMS)
		return NULL;
	ModItem* mi = dynamic_cast<ModItem*>(Item::items[itemId]);
	if (!mi || !mi->hasModModel())
		return NULL;
	return &mi->modModelParts();
}

bool ModEngine::itemIsAnimated(int itemId) const {
	if (itemId <= 0 || itemId >= Item::MAX_ITEMS)
		return false;
	ModItem* mi = dynamic_cast<ModItem*>(Item::items[itemId]);
	return mi != NULL && mi->isModAnimated();
}

// 物品模型的独立大贴图路径（Item.defineItem 的 modelTexture）；NULL = 走 terrain 图集。
const std::string* ModEngine::itemModelTexture(int itemId) const {
	if (itemId <= 0 || itemId >= Item::MAX_ITEMS)
		return NULL;
	ModItem* mi = dynamic_cast<ModItem*>(Item::items[itemId]);
	if (!mi || mi->modModelTexture().empty())
		return NULL;
	return &mi->modModelTexture();
}

// ===========================================================================
//  投射物（Projectile.defineProjectile）
//  和方块/物品/生物一样，投射物也是"可定义类型"：模组给一个类型号 + 模型/贴图/物理
//  参数，引擎负责生成实体、飞行、撞方块/生物、结算伤害。任何模组都能定义自己的
//  投射物（子弹、火球、飞刀……），不只为某一把枪服务。
// ===========================================================================
struct ModProjectileDef {
	std::vector<ModBlockPart> parts;   // 空 = 没给模型（看不见，但命中照常）
	std::string texture;               // 独立大贴图路径（空 = 走 terrain 图集）
	float texW, texH;
	float gravity;                     // 每刻 y 速度增量（0 = 直线飞）
	float drag;                        // 每刻速度衰减系数（1 = 不衰减）
	float damage;                      // 命中生物结算的伤害
	float size;                        // 碰撞盒边长（格）
	int   life;                        // 存活刻数，到点自毁
	ModProjectileDef()
	:	texW(0), texH(0), gravity(0.0f), drag(1.0f),
		damage(1.0f), size(0.1f), life(100) {}
};
static std::map<int, ModProjectileDef> g_projectiles;

bool ModEngine::hasProjectileDef(int typeId) const {
	return g_projectiles.find(typeId) != g_projectiles.end();
}
const std::vector<ModBlockPart>* ModEngine::projectileModelParts(int typeId) const {
	std::map<int, ModProjectileDef>::const_iterator it = g_projectiles.find(typeId);
	if (it == g_projectiles.end() || it->second.parts.empty())
		return NULL;
	return &it->second.parts;
}
const std::string* ModEngine::projectileTexture(int typeId) const {
	std::map<int, ModProjectileDef>::const_iterator it = g_projectiles.find(typeId);
	if (it == g_projectiles.end() || it->second.texture.empty())
		return NULL;
	return &it->second.texture;
}
float ModEngine::projectileGravity(int typeId) const {
	std::map<int, ModProjectileDef>::const_iterator it = g_projectiles.find(typeId);
	return (it == g_projectiles.end()) ? 0.0f : it->second.gravity;
}
float ModEngine::projectileDrag(int typeId) const {
	std::map<int, ModProjectileDef>::const_iterator it = g_projectiles.find(typeId);
	return (it == g_projectiles.end()) ? 1.0f : it->second.drag;
}
float ModEngine::projectileDamage(int typeId) const {
	std::map<int, ModProjectileDef>::const_iterator it = g_projectiles.find(typeId);
	return (it == g_projectiles.end()) ? 1.0f : it->second.damage;
}
int ModEngine::projectileLife(int typeId) const {
	std::map<int, ModProjectileDef>::const_iterator it = g_projectiles.find(typeId);
	return (it == g_projectiles.end()) ? 100 : it->second.life;
}
float ModEngine::projectileSize(int typeId) const {
	std::map<int, ModProjectileDef>::const_iterator it = g_projectiles.find(typeId);
	return (it == g_projectiles.end()) ? 0.1f : it->second.size;
}

// 通用投射物实体。命中判定照 Arrow::tick 的写法：
//   方块 -> level->clip(from, to)；生物 -> level->getEntities(附近) 里取视线最近的
// 区别是伤害用【定义里的 damage】，不是 Arrow 那套"按飞行速度算"。
// ⚠ 这里刻意继承 Arrow 而不是 Entity：ArrowRenderer 里是 `(Arrow*)entity` 硬转，
// 不是 Arrow 的子类会被当成箭读内存（指针错位 → 飞不出来甚至崩）。
class ModProjectileEntity : public Arrow {
	typedef Arrow super;
	int _typeId;
	int _life;
public:
	ModProjectileEntity(Level* level, int typeId, float x, float y, float z,
	                    float vx, float vy, float vz)
	:	super(level, x, y, z), _typeId(typeId), _life(0)
	{
		float s = ModEngine::instance ? ModEngine::instance->projectileSize(typeId) : 0.1f;
		if (s <= 0.0f) s = 0.1f;
		setSize(s, s);
		moveTo(x, y, z, 0.0f, 0.0f);
		this->xd = vx; this->yd = vy; this->zd = vz;
		this->playerArrow = true;
		// 用模组自己的渲染器（画成定义里的模型），不是箭那个
		this->entityRendererId = ER_MODPROJECTILE_RENDERER;
	}
	int projectileType() const { return _typeId; }
	// 渲染器靠它拿投射物类型号（Entity::getAuxData 是 virtual，Arrow 也覆写了它）
	int getAuxData() { return _typeId; }
	void tick();
};

void ModProjectileEntity::tick() {
	// 刻意【不】调 super::tick()：那是 Arrow 的命中逻辑（伤害按飞行速度算，还会自己 remove）。
	// 只跑 Entity 的位移/碰撞盒更新，命中与伤害由下面自己算。
	Entity::tick();

	ModEngine* me = ModEngine::instance;
	float gravity = me ? me->projectileGravity(_typeId) : 0.0f;
	float drag    = me ? me->projectileDrag(_typeId)    : 1.0f;
	float damage  = me ? me->projectileDamage(_typeId)  : 1.0f;
	int   lifeMax = me ? me->projectileLife(_typeId)    : 100;

	Vec3 from(x, y, z);
	Vec3 to(x + xd, y + yd, z + zd);
	HitResult res = level->clip(from, to, false, true);
	if (res.isHit())
		to.set(res.pos.x, res.pos.y, res.pos.z);

	Entity* hitEntity = NULL;
	EntityList& objects = level->getEntities(this, this->bb.expand(xd, yd, zd).grow(1, 1, 1));
	float nearest = 0.0f;
	for (unsigned int i = 0; i < objects.size(); ++i) {
		Entity* e = objects[i];
		if (e == this || !e->isPickable())
			continue;
		AABB box = e->bb.grow(0.3f, 0.3f, 0.3f);
		HitResult p = box.clip(from, to);
		if (p.isHit()) {
			float dd = from.distanceTo(p.pos);
			if (dd < nearest || nearest == 0.0f) { hitEntity = e; nearest = dd; }
		}
	}
	if (hitEntity != NULL)
		res = HitResult(hitEntity);

	if (res.isHit()) {
		if (res.type == ENTITY && res.entity != NULL) {
			res.entity->hurt(this, (int)(damage + 0.5f));
			if (res.entity->isMob())
				((Mob*)res.entity)->arrowCount++;
		}
		remove();
		return;
	}
	if (++_life >= lifeMax) { remove(); return; }

	// 推进：重力加在 y 速度上，阻力按系数衰减
	yd += gravity;
	xd *= drag; yd *= drag; zd *= drag;
	move(xd, yd, zd);
}

// JS Projectile.defineProjectile(typeId, { model, modelTexture, texSize,
//     gravity, drag, damage, life, size }) —— 定义一个投射物类型。
static duk_ret_t jsProjectileDefineProjectile(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	if (!me) return 0;
	int typeId = (int)duk_get_int(ctx, 0);
	if (typeId <= 0) return 0;

	ModProjectileDef def;
	if (duk_is_object(ctx, 1)) {
		// 模型/贴图复用方块和物品那一套（model / box / height + modelTexture / texSize）
		parseBlockModelOption(ctx, 1, me, 0, def.parts);
		def.texture = resolveModelTexture(ctx, 1, me, def.parts);
		// 每个面的贴图槽兜底成 0：renderModelBox 收到 tt=NULL 时会读 tt->getTexture(f)，
		// 部件没填过 tex[f]（-1）就会踩空指针 —— 给了 uv 的面走 uvRect，用不到这个值。
		for (size_t i = 0; i < def.parts.size(); ++i)
			for (int f = 0; f < 6; ++f)
				if (def.parts[i].tex[f] < 0)
					def.parts[i].tex[f] = 0;

		const char* keys[5] = { "gravity", "drag", "damage" };
		duk_get_prop_string(ctx, 1, "gravity");
		if (duk_is_number(ctx, -1)) def.gravity = (float)duk_get_number(ctx, -1);
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "drag");
		if (duk_is_number(ctx, -1)) def.drag = (float)duk_get_number(ctx, -1);
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "damage");
		if (duk_is_number(ctx, -1)) def.damage = (float)duk_get_number(ctx, -1);
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "life");
		if (duk_is_number(ctx, -1)) def.life = (int)duk_get_int(ctx, -1);
		duk_pop(ctx);
		duk_get_prop_string(ctx, 1, "size");
		if (duk_is_number(ctx, -1)) def.size = (float)duk_get_number(ctx, -1);
		duk_pop(ctx);
		(void)keys;
	}
	if (def.size <= 0.0f)   def.size = 0.1f;
	if (def.damage <= 0.0f) def.damage = 1.0f;
	if (def.life <= 0)      def.life = 100;
	g_projectiles[typeId] = def;

	me->log("projectile: define #" + std::to_string(typeId)
		+ " parts=" + std::to_string((int)def.parts.size())
		+ " dmg=" + std::to_string((int)def.damage)
		+ " tex=" + (def.texture.empty() ? std::string("(atlas)") : def.texture));
	return 0;
}

// JS level.spawnProjectile(typeId, x, y, z, vx, vy, vz) -> 实体 id
static duk_ret_t jsLevelSpawnProjectile(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	if (!me || !me->minecraft() || !me->minecraft()->level)
		return 0;
	int typeId = (int)duk_get_int(ctx, 0);
	if (!me->hasProjectileDef(typeId))
		return 0;
	float x  = (float)duk_get_number(ctx, 1);
	float y  = (float)duk_get_number(ctx, 2);
	float z  = (float)duk_get_number(ctx, 3);
	float vx = (float)duk_get_number(ctx, 4);
	float vy = (float)duk_get_number(ctx, 5);
	float vz = (float)duk_get_number(ctx, 6);
	Level* level = me->minecraft()->level;
	ModProjectileEntity* e = new ModProjectileEntity(level, typeId, x, y, z, vx, vy, vz);
	level->addEntity(e);
	duk_push_int(ctx, e->entityId);
	return 1;
}

bool ModEngine::getItemPartPose(int itemId, const std::string& partName, float age, float time, ModPartPose& out,
                                int firstPersonFlag) {
	if (itemId <= 0 || itemId >= Item::MAX_ITEMS)
		return false;
	ModItem* mi = dynamic_cast<ModItem*>(Item::items[itemId]);
	if (!mi || !mi->isModAnimated())
		return false;
	return callAnimKey(mi->modAnimKey(), partName, age, time, firstPersonFlag, out);
}

// ---------------------------------------------------------------------------
// 骨骼动画的小工具：3x3 旋转 + 平移。
// 旋转合成顺序必须和 TileRenderer::modelVert 完全一致 —— 那里是
//   先绕 x、再绕 y、最后绕 z（即 R = Rz*Ry*Rx，每一步都在同一坐标系里）
// 否则骨骼套上去之后部件会和静态模型错位。
// ---------------------------------------------------------------------------
struct ModXf {
	float r[3][3];
	float t[3];
	float s;		// 均匀缩放（基岩的 bone scale 都是单值，所以只存一个）
};

static void modXfIdent(ModXf& x) {
	x.s = 1.0f;
	for (int i = 0; i < 3; ++i) {
		x.t[i] = 0.0f;
		for (int j = 0; j < 3; ++j)
			x.r[i][j] = (i == j) ? 1.0f : 0.0f;
	}
}

static void modMat3Mul(const float a[3][3], const float b[3][3], float out[3][3]) {
	float m[3][3];
	for (int i = 0; i < 3; ++i)
		for (int j = 0; j < 3; ++j)
			m[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j];
	for (int i = 0; i < 3; ++i)
		for (int j = 0; j < 3; ++j)
			out[i][j] = m[i][j];
}

// 欧拉角（弧度）-> 矩阵，R = Rz*Ry*Rx
static void modRotFromEuler(float rx, float ry, float rz, float out[3][3]) {
	float cx = cosf(rx), sx = sinf(rx);
	float cy = cosf(ry), sy = sinf(ry);
	float cz = cosf(rz), sz = sinf(rz);
	float X[3][3] = { {1, 0, 0}, {0, cx, -sx}, {0, sx, cx} };
	float Y[3][3] = { {cy, 0, sy}, {0, 1, 0}, {-sy, 0, cy} };
	float Z[3][3] = { {cz, -sz, 0}, {sz, cz, 0}, {0, 0, 1} };
	float tmp[3][3];
	modMat3Mul(Y, X, tmp);      // Ry*Rx
	modMat3Mul(Z, tmp, out);    // Rz*Ry*Rx
}

// 矩阵 -> 欧拉角，与 modRotFromEuler 互逆
static void modEulerFromRot(const float R[3][3], float& rx, float& ry, float& rz) {
	float sy = -R[2][0];
	if (sy > 1.0f) sy = 1.0f;
	if (sy < -1.0f) sy = -1.0f;
	ry = asinf(sy);
	float cy = cosf(ry);
	if (cy > 1e-6f || cy < -1e-6f) {
		rx = atan2f(R[2][1], R[2][2]);
		rz = atan2f(R[1][0], R[0][0]);
	} else {                    // 万向锁：rx 与 rz 退化成一个自由度
		rx = atan2f(-R[1][2], R[1][1]);
		rz = 0.0f;
	}
}

// 把 src（部件表）按骨架 bones + animKey 回调算出一帧姿态写进 out。
// 没有 bone 的部件行为和以前一模一样（逐部件增量姿态）—— 老模组一行不用改。
void ModEngine::applyItemAnim(const std::vector<ModBoneDef>& bones,
                              const std::vector<ModBlockPart>& src,
                              const std::string& animKey,
                              float age, float time, int firstPersonFlag,
                              std::vector<ModBlockPart>& out) {
	out = src;
	if (out.empty())
		return;

	bool anyBone = false;
	for (size_t i = 0; i < out.size(); ++i)
		if (!out[i].bone.empty()) { anyBone = true; break; }

	if (!anyBone || bones.empty()) {
		// 旧路径：每个部件问自己的姿态
		if (animKey.empty())
			return;
		for (size_t i = 0; i < out.size(); ++i) {
			ModPartPose pose;
			if (callAnimKey(animKey, out[i].name, age, time, firstPersonFlag, pose)) {
				out[i].rx += pose.rx; out[i].ry += pose.ry; out[i].rz += pose.rz;
				if (pose.hasPos) { out[i].ox = pose.px; out[i].oy = pose.py; out[i].oz = pose.pz; }
			}
		}
		return;
	}

	std::map<std::string, int> idx;
	for (size_t i = 0; i < bones.size(); ++i)
		idx[bones[i].name] = (int)i;

	// 拓扑序（父在前）。父缺失或成环的向后兜底，不会死循环。
	std::vector<int> order;
	std::vector<bool> placed(bones.size(), false);
	bool progress = true;
	while (progress && order.size() < bones.size()) {
		progress = false;
		for (size_t i = 0; i < bones.size(); ++i) {
			if (placed[i]) continue;
			const std::string& p = bones[i].parent;
			bool ok;
			if (p.empty()) {
				ok = true;
			} else {
				std::map<std::string, int>::iterator it = idx.find(p);
				ok = (it == idx.end()) ? true : placed[it->second];
			}
			if (ok) { order.push_back((int)i); placed[i] = true; progress = true; }
		}
	}
	for (size_t i = 0; i < bones.size(); ++i)
		if (!placed[i]) order.push_back((int)i);

	// 每根骨骼每帧只回调一次 JS —— 上千个部件的模型也只有几十次调用。
	std::vector<ModXf> world(bones.size());
	for (size_t i = 0; i < world.size(); ++i)
		modXfIdent(world[i]);

	for (size_t k = 0; k < order.size(); ++k) {
		int bi = order[k];
		const ModBoneDef& b = bones[bi];
		ModXf local;
		modXfIdent(local);

		ModPartPose pose;
		bool has = !animKey.empty() && callAnimKey(animKey, b.name, age, time, firstPersonFlag, pose);

		// 关键语义：动画的 rotation / scale 是**替换**骨骼自己的静态值，不是叠加。
		// （基岩的 hold 动画给 luger 的 [90,0,0] 与它 .geo.json 里的静态 [90,0,0]
		//  是同一个值 —— 叠加就会变成 180°，模型整个翻过来。）
		float rx = b.rx, ry = b.ry, rz = b.rz;
		float sc = 1.0f;
		if (has) {
			rx = pose.rx; ry = pose.ry; rz = pose.rz;
			if (pose.hasScale)
				sc = pose.sx;
		}
		float pos[3] = { 0.0f, 0.0f, 0.0f };
		if (has && pose.hasPos) { pos[0] = pose.px; pos[1] = pose.py; pos[2] = pose.pz; }

		// f(v) = s*R*(v - pivot) + pivot + pos   =>   t = pivot - s*R*pivot + pos
		float piv[3] = { b.px, b.py, b.pz };
		float m[3][3];
		modRotFromEuler(rx, ry, rz, m);
		float rp[3];
		for (int i = 0; i < 3; ++i)
			rp[i] = sc * (m[i][0] * piv[0] + m[i][1] * piv[1] + m[i][2] * piv[2]);
		local.s = sc;
		for (int i = 0; i < 3; ++i)
			for (int j = 0; j < 3; ++j)
				local.r[i][j] = m[i][j];
		for (int i = 0; i < 3; ++i)
			local.t[i] = piv[i] - rp[i] + pos[i];

		const std::string& pn = b.parent;
		std::map<std::string, int>::iterator pit = idx.find(pn);
		if (!pn.empty() && pit != idx.end()) {
			const ModXf& pw = world[pit->second];
			ModXf res;
			modMat3Mul(pw.r, local.r, res.r);
			res.s = pw.s * local.s;
			for (int i = 0; i < 3; ++i)
				res.t[i] = pw.s * (pw.r[i][0] * local.t[0] + pw.r[i][1] * local.t[1]
				                 + pw.r[i][2] * local.t[2]) + pw.t[i];
			world[bi] = res;
		} else {
			world[bi] = local;
		}
	}

	// 把骨骼变换套到部件上。部件当前是 w = R_p*(l - c) + c + move，
	// 套上 M = (s,R,t) 之后:  M(w) = s*(R*R_p)*(l - c) + s*R*(c+move) + t，
	// 仍是同一形状:  R' = R*R_p,  s' = s,  move' = s*R*(c+move) + t - c
	for (size_t i = 0; i < out.size(); ++i) {
		ModBlockPart& p = out[i];
		if (p.bone.empty()) {
			if (!animKey.empty()) {
				ModPartPose pose;
				if (callAnimKey(animKey, p.name, age, time, firstPersonFlag, pose)) {
					p.rx += pose.rx; p.ry += pose.ry; p.rz += pose.rz;
					if (pose.hasPos) { p.ox = pose.px; p.oy = pose.py; p.oz = pose.pz; }
				}
			}
			continue;
		}
		std::map<std::string, int>::iterator it = idx.find(p.bone);
		if (it == idx.end())
			continue;
		const ModXf& M = world[it->second];

		float Rp[3][3], Rnew[3][3];
		modRotFromEuler(p.rx, p.ry, p.rz, Rp);
		modMat3Mul(M.r, Rp, Rnew);

		float c[3] = { p.cx, p.cy, p.cz };
		float cm[3] = { c[0] + p.ox, c[1] + p.oy, c[2] + p.oz };
		float nm[3];
		for (int k2 = 0; k2 < 3; ++k2)
			nm[k2] = M.s * (M.r[k2][0] * cm[0] + M.r[k2][1] * cm[1] + M.r[k2][2] * cm[2]) + M.t[k2];

		modEulerFromRot(Rnew, p.rx, p.ry, p.rz);
		p.sx = p.sy = p.sz = M.s;
		p.ox = nm[0] - c[0];
		p.oy = nm[1] - c[1];
		p.oz = nm[2] - c[2];
	}
}

bool ModEngine::applyItemAnimFor(int itemId, const std::vector<ModBlockPart>& src,
                                 float age, float time, int firstPersonFlag,
                                 std::vector<ModBlockPart>& out) {
	if (itemId <= 0 || itemId >= Item::MAX_ITEMS) {
		out = src;
		return false;
	}
	ModItem* mi = dynamic_cast<ModItem*>(Item::items[itemId]);
	if (!mi) {
		out = src;
		return false;
	}
	applyItemAnim(mi->modBones(), src, mi->modAnimKey(), age, time, firstPersonFlag, out);
	return true;
}

// Build a Recipes::TypeList from a JS object { 'X': id, 'Y': id }.
static void buildRecipeTypes(duk_context* ctx, Recipes::TypeList& types) {
	if (!duk_is_object(ctx, -1))
		return;
	duk_enum(ctx, -1, DUK_ENUM_OWN_PROPERTIES_ONLY);
	while (duk_next(ctx, -1, 1)) {
		const char* c = duk_safe_to_string(ctx, -2);
		int tid = duk_get_int(ctx, -1);
		if (c && c[0] && !c[1]) {
			if (tid >= 0 && tid < 256 && Tile::tiles[tid])
				types.push_back(Recipes::Type(c[0], Tile::tiles[tid]));
			else if (tid >= 0 && tid < Item::MAX_ITEMS && Item::items[tid])
				types.push_back(Recipes::Type(c[0], Item::items[tid]));
		}
		duk_pop_2(ctx);
	}
	duk_pop(ctx);
}

// Recipes.addShapedRecipe(resultId, resultCount, [row0,row1,row2], {X:id, Y:id})
static duk_ret_t jsRecipesAddShapedRecipe(duk_context* ctx) {
	int resultId = duk_get_int(ctx, 0);
	int resultCount = duk_get_int(ctx, 1);
	std::string rows[3];
	int nRows = 0;
	if (duk_is_array(ctx, 2)) {
		int n = (int)duk_get_length(ctx, 2);
		if (n > 3) n = 3;
		for (int i = 0; i < n; ++i) {
			duk_get_prop_index(ctx, 2, (duk_idx_t)i);
			rows[i] = duk_safe_to_string(ctx, -1);
			duk_pop(ctx);
			nRows++;
		}
	}
	duk_dup(ctx, 3);
	Recipes::TypeList types;
	buildRecipeTypes(ctx, types);
	if (nRows < 1) {
		duk_push_error_object(ctx, DUK_ERR_ERROR, "Recipes.addShapedRecipe needs 1-3 rows");
		return duk_throw(ctx);
	}
	Recipes* r = Recipes::getInstance();
	ItemInstance result(resultId, resultCount, 0);
	if (nRows == 1)      r->addShapedRecipe(result, rows[0], types);
	else if (nRows == 2) r->addShapedRecipe(result, rows[0], rows[1], types);
	else                 r->addShapedRecipe(result, rows[0], rows[1], rows[2], types);
	return 0;
}

// Recipes.addShapelessRecipe(resultId, resultCount, [id1, id2, ...])
static duk_ret_t jsRecipesAddShapelessRecipe(duk_context* ctx) {
	int resultId = duk_get_int(ctx, 0);
	int resultCount = duk_get_int(ctx, 1);
	Recipes::TypeList types;
	if (duk_is_array(ctx, 2)) {
		int n = (int)duk_get_length(ctx, 2);
		char c = 'A';
		for (int i = 0; i < n; ++i) {
			duk_get_prop_index(ctx, 2, (duk_idx_t)i);
			int tid = duk_get_int(ctx, -1);
			if (tid >= 0 && tid < 256 && Tile::tiles[tid])
				types.push_back(Recipes::Type(c, Tile::tiles[tid]));
			else if (tid >= 0 && tid < Item::MAX_ITEMS && Item::items[tid])
				types.push_back(Recipes::Type(c, Item::items[tid]));
			++c;
			duk_pop(ctx);
		}
	}
	Recipes* r = Recipes::getInstance();
	ItemInstance result(resultId, resultCount, 0);
	r->addShapelessRecipe(result, types);
	return 0;
}

// ---------------------------------------------------------------------------
// JS timers + config (M-infra)
// ---------------------------------------------------------------------------

static int s_timerSeq = 0;

static duk_ret_t jsTimerImpl(duk_context* ctx, bool repeat) {
	int ms = duk_get_int(ctx, 1);
	if (!duk_is_function(ctx, 0)) {
		duk_push_error_object(ctx, DUK_ERR_ERROR, "setTimeout/setInterval: first arg must be a function");
		return duk_throw(ctx);
	}
	int handle = ++s_timerSeq;
	std::string key = "__mod_timer_fn_" + std::to_string(handle);
	duk_dup(ctx, 0);
	duk_put_global_string(ctx, key.c_str());
	if (ModEngine::instance)
		ModEngine::instance->addTimer(handle, key, ms, repeat);
	duk_push_int(ctx, handle);
	return 1;
}

static duk_ret_t jsSetTimeout(duk_context* ctx) { return jsTimerImpl(ctx, false); }
static duk_ret_t jsSetInterval(duk_context* ctx) { return jsTimerImpl(ctx, true); }

static duk_ret_t jsClearTimer(duk_context* ctx) {
	if (ModEngine::instance)
		ModEngine::instance->clearTimer(duk_get_int(ctx, 0));
	return 0;
}

static duk_ret_t jsSetConfig(duk_context* ctx) {
	if (ModEngine::instance)
		ModEngine::instance->setConfig(duk_safe_to_string(ctx, 0), duk_safe_to_string(ctx, 1));
	return 0;
}

static duk_ret_t jsGetConfig(duk_context* ctx) {
	std::string v;
	if (ModEngine::instance)
		v = ModEngine::instance->getConfig(duk_safe_to_string(ctx, 0), duk_safe_to_string(ctx, 1));
	duk_push_string(ctx, v.c_str());
	return 1;
}

// ---------------------------------------------------------------------------
// JS Mob API (M6: scripted mobs)
// ---------------------------------------------------------------------------

// Mob.defineMob(typeId, { name, texture, health, size:[w,h],
//                         model:[{name,tex:[x,y],box:[x,y,z,w,h,d],pos:[x,y,z]},...],
//                         anim: function(partName, age, limbSwing, limbSwingAmount){...} })
static duk_ret_t jsMobDefineMob(duk_context* ctx) {
	int typeId = duk_get_int(ctx, 0);
	if (typeId < MobTypes::CustomMob) {
		duk_push_error_object(ctx, DUK_ERR_ERROR, "Mob.defineMob: typeId must be >= %d", (int)MobTypes::CustomMob);
		return duk_throw(ctx);
	}
	duk_idx_t objIdx = 1;
	if (!duk_is_object(ctx, objIdx)) {
		duk_push_error_object(ctx, DUK_ERR_ERROR, "Mob.defineMob: config object required");
		return duk_throw(ctx);
	}

	ModEngine::ScriptedMobDef def;
	duk_get_prop_string(ctx, objIdx, "name");
	def.name = duk_safe_to_string(ctx, -1);
	duk_pop(ctx);
	duk_get_prop_string(ctx, objIdx, "texture");
	def.texture = duk_safe_to_string(ctx, -1);
	duk_pop(ctx);
	duk_get_prop_string(ctx, objIdx, "health");
	def.health = duk_get_int(ctx, -1);
	if (def.health <= 0) def.health = 20;
	duk_pop(ctx);

	def.sizeW = 0.6f; def.sizeH = 1.8f;
	def.scale = 1.0f;
	def.hostile = true;   // default: keep engine monster AI (backward compat)
	duk_get_prop_string(ctx, objIdx, "scale");
	if (duk_is_number(ctx, -1)) def.scale = (float)duk_get_number(ctx, -1);
	duk_pop(ctx);
	duk_get_prop_string(ctx, objIdx, "hostile");
	if (duk_is_boolean(ctx, -1)) def.hostile = duk_get_boolean(ctx, -1) != 0;
	duk_pop(ctx);
	def.sizeW = 0.6f; def.sizeH = 1.8f;
	duk_get_prop_string(ctx, objIdx, "size");
	if (duk_is_array(ctx, -1)) {
		duk_get_prop_index(ctx, -1, 0); def.sizeW = (float)duk_get_number(ctx, -1); duk_pop(ctx);
		duk_get_prop_index(ctx, -1, 1); def.sizeH = (float)duk_get_number(ctx, -1); duk_pop(ctx);
	}
	duk_pop(ctx);

	// anim callback -> global key
	def.animKey = "__mod_mob_anim_" + std::to_string(typeId);
	duk_get_prop_string(ctx, objIdx, "anim");
	if (duk_is_function(ctx, -1)) {
		duk_dup(ctx, -1);
		duk_put_global_string(ctx, def.animKey.c_str());
	}
	duk_pop(ctx);

	// model boxes
	def.boxes = new std::vector<ScriptedBoxDef>();
	int texW = 64, texH = 32;
	duk_get_prop_string(ctx, objIdx, "model");
	if (duk_is_array(ctx, -1)) {
		int n = (int)duk_get_length(ctx, -1);
		for (int i = 0; i < n; ++i) {
			duk_get_prop_index(ctx, -1, (duk_idx_t)i);
			if (!duk_is_object(ctx, -1)) { duk_pop(ctx); continue; }
			duk_idx_t b = -1;
			ScriptedBoxDef box;
			duk_get_prop_string(ctx, b, "name"); box.name = duk_safe_to_string(ctx, -1); duk_pop(ctx);
			duk_get_prop_string(ctx, b, "tex");
			if (duk_is_array(ctx, -1)) {
				duk_get_prop_index(ctx, -1, 0); box.texX = duk_get_int(ctx, -1); duk_pop(ctx);
				duk_get_prop_index(ctx, -1, 1); box.texY = duk_get_int(ctx, -1); duk_pop(ctx);
			}
			duk_pop(ctx);
			duk_get_prop_string(ctx, b, "box");
			if (duk_is_array(ctx, -1)) {
				duk_get_prop_index(ctx, -1, 0); box.x = (float)duk_get_number(ctx, -1); duk_pop(ctx);
				duk_get_prop_index(ctx, -1, 1); box.y = (float)duk_get_number(ctx, -1); duk_pop(ctx);
				duk_get_prop_index(ctx, -1, 2); box.z = (float)duk_get_number(ctx, -1); duk_pop(ctx);
				duk_get_prop_index(ctx, -1, 3); box.w = (float)duk_get_number(ctx, -1); duk_pop(ctx);
				duk_get_prop_index(ctx, -1, 4); box.h = (float)duk_get_number(ctx, -1); duk_pop(ctx);
				duk_get_prop_index(ctx, -1, 5); box.d = (float)duk_get_number(ctx, -1); duk_pop(ctx);
			}
			duk_pop(ctx);
			duk_get_prop_string(ctx, b, "pos");
			if (duk_is_array(ctx, -1)) {
				duk_get_prop_index(ctx, -1, 0); box.px = (float)duk_get_number(ctx, -1); duk_pop(ctx);
				duk_get_prop_index(ctx, -1, 1); box.py = (float)duk_get_number(ctx, -1); duk_pop(ctx);
				duk_get_prop_index(ctx, -1, 2); box.pz = (float)duk_get_number(ctx, -1); duk_pop(ctx);
			}
			duk_pop(ctx);
			def.boxes->push_back(box);
			duk_pop(ctx);  // the box object
		}
	}
	duk_pop(ctx);

	// texture size (optional)
	duk_get_prop_string(ctx, objIdx, "texSize");
	if (duk_is_array(ctx, -1)) {
		duk_get_prop_index(ctx, -1, 0); texW = duk_get_int(ctx, -1); duk_pop(ctx);
		duk_get_prop_index(ctx, -1, 1); texH = duk_get_int(ctx, -1); duk_pop(ctx);
	}
	duk_pop(ctx);

#ifndef STANDALONE_SERVER
	def.model = new ScriptedModel(def.animKey, *def.boxes, texW, texH);
#else
	// 服务器不渲染：仍然注册怪物定义（碰撞箱、属性都来自 boxes），只是没有模型。
	// 客户端那侧会从同一份 mod zip 里自己建模型。
	def.model = NULL;
#endif
	if (ModEngine::instance)
		ModEngine::instance->defineScriptedMob(typeId, def);
	else {
		delete def.boxes;
		delete def.model;
	}
	return 0;
}

// level.spawnMob(typeId, x, y, z)
// JS level.spawnParticle(name, x, y, z[, xd, yd, zd]) - spawn one of the
// built-in particles (smoke, flame, explode, bubble, crit, lava, ...).
// JS level.saveAll() - flush all chunks to disk (dimension travel step 1).
// JS level.getCurrentDimension() - dimension the local player's world is in
// right now (engine truth: the rendered level's dimension; falls back to the
// persisted mod state when no level is loaded yet).
static duk_ret_t jsLevelGetCurrentDimension(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	Minecraft* mc = me ? me->minecraft() : NULL;
	int d = 0;
	if (mc && mc->level && mc->level->dimension)
		d = mc->level->dimension->id;
	else if (me)
		d = me->loadDimensionState();
	duk_push_int(ctx, d);
	return 1;
}

// JS level.saveDimensionState([dimId]) - remember the current dimension on disk.
// 不传参数就记"当前维度"；传了 dimId 就先把当前维度设成它再落盘（这样
// level.saveDimensionState(20) 与 level.saveDimensionState() 都有意义）。
static duk_ret_t jsLevelSaveDimensionState(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	if (me) {
		if (duk_get_top(ctx) > 0 && duk_is_number(ctx, 0))
			me->setCurrentDimension((int)duk_get_int(ctx, 0));
		me->saveDimensionState();
	}
	return 0;
}

// JS level.setModState(key, value) / level.getModState(key)
static duk_ret_t jsLevelSetModState(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	if (me) me->setModState(duk_safe_to_string(ctx, 0), duk_safe_to_string(ctx, 1));
	return 0;
}
static duk_ret_t jsLevelGetModState(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	std::string v = me ? me->getModState(duk_safe_to_string(ctx, 0)) : "";
	duk_push_string(ctx, v.c_str());
	return 1;
}

// ---------------------------------------------------------------------------
// Stage 3: block-entity data access (ModTileEntity NBT persistence).
//   level.getBlockEntityData(x, y, z)   -> JS object of key->string, or null
//   level.setBlockEntityData(x,y,z,key,value) -> write one key
// ---------------------------------------------------------------------------
static ModTileEntity* getModTileEntityAt(int x, int y, int z) {
	ModEngine* me = ModEngine::instance;
	Minecraft* mc = me ? me->minecraft() : NULL;
	if (!mc || !mc->level)
		return NULL;
	TileEntity* te = mc->level->getTileEntity(x, y, z);
	return dynamic_cast<ModTileEntity*>(te);
}

// JS mods.comm.get(key) -> string ("" when missing). Shared across mods.
static duk_ret_t jsModsCommGet(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	std::string k = duk_safe_to_string(ctx, 0);
	std::string v;
	if (me && me->commGet(k, v)) {
		duk_push_lstring(ctx, v.c_str(), v.size());
		return 1;
	}
	duk_push_string(ctx, "");
	return 1;
}
// JS mods.comm.set(key, value) - store a string for other mods to read.
static duk_ret_t jsModsCommSet(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	if (me)
		me->commSet(duk_safe_to_string(ctx, 0), duk_safe_to_string(ctx, 1));
	return 0;
}

static duk_ret_t jsLevelGetBlockEntityData(duk_context* ctx) {	ModTileEntity* te = getModTileEntityAt((int)duk_get_int(ctx, 0), (int)duk_get_int(ctx, 1), (int)duk_get_int(ctx, 2));
	if (!te) {
		duk_push_null(ctx);
		return 1;
	}
	duk_push_object(ctx);
	size_t n = te->keyCount();
	for (size_t i = 0; i < n; ++i) {
		std::string k, v;
		if (te->keyAt(i, k, v)) {
			duk_push_string(ctx, v.c_str());
			duk_put_prop_string(ctx, -2, k.c_str());
		}
	}
	return 1;
}

static duk_ret_t jsLevelSetBlockEntityData(duk_context* ctx) {
	int bx = (int)duk_get_int(ctx, 0);
	int by = (int)duk_get_int(ctx, 1);
	int bz = (int)duk_get_int(ctx, 2);
	ModTileEntity* te = getModTileEntityAt(bx, by, bz);
	if (!te)
		return 0;
	std::string key = duk_safe_to_string(ctx, 3);
	if (key.empty())
		return 0;
	te->setKey(key, duk_safe_to_string(ctx, 4));
	// 微方块：格内几何（键 "geom"）改了 -> 解析缓存作废 + 标脏这一格，
	// 下一帧就按新几何画（同时碰撞箱也跟着换）。
	if (key == "geom" && ModEngine::instance)
		ModEngine::instance->noteCellGeomChanged(bx, by, bz);
	return 0;
}

static duk_ret_t jsLevelSaveAll(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	Minecraft* mc = me ? me->minecraft() : NULL;
	if (mc && mc->level)
		mc->level->getChunkSource()->saveAll(true);
	return 0;
}

// JS level.resetChunks(dimId) —— 切换到某个维度。
//
// mod 写法不变，但内部已经不是"清空当前世界的全部区块缓存 + 换生成器 + 同步
// 预生成中心区块"了（那一步就是切维度又慢又卡的根源）。现在目标维度被当成
// "另一个世界"：它有自己的独立存档（主世界文件夹下的 dim<N>/ 子目录，自带
// level.dat 和区块），进入它走的是和"进入世界"完全一样的后台加载 + 加载界面。
static duk_ret_t jsLevelResetChunks(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	Minecraft* mc = me ? me->minecraft() : NULL;
	if (mc && mc->level) {
		int id = (int)duk_get_int(ctx, 0);
		me->log("resetChunks -> dim " + std::to_string(id));

		if (mc->player && !mc->level->isClientSide) {
			// 单机 / 房主：目标维度自己有存档，走"进入世界"那套加载流程。
			// 失败（上一个世界还在加载 / 这个维度没有生成器）就什么都不改 ——
			// 否则"当前维度"会被记成已经过去了，和实际所在的世界对不上。
			if (!mc->beginDimensionTravel(id, mc->player->x, mc->player->y, mc->player->z)) {
				me->log("resetChunks: refused (previous world still loading, or no generator for dim "
				        + std::to_string(id) + ")");
				return 0;
			}
		} else {
			// 联机客户端（世界在服务器手里，本地不落盘）与无本地玩家的服务器：
			// 保留"就地换生成器"的行为。
			mc->level->resetChunks(id);
			if (mc->levelRenderer)
				mc->levelRenderer->allChanged();
		}

		me->loadDimensionState(); // refresh level name
		me->setCurrentDimension(id);
		me->saveDimensionState();
	}
	return 0;
}

// JS level.isHost() - true on the process that hosts the server (or a
// single-player game); false on clients that joined a remote host.
static duk_ret_t jsLevelIsHost(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	Minecraft* mc = me ? me->minecraft() : NULL;
	bool host = false;
	if (mc && mc->level) {
		if (mc->raknetInstance && mc->raknetInstance->isServer())
			host = true;
		else if (!mc->level->isClientSide && mc->netCallback == NULL)
			host = true; // plain single-player
	}
	duk_push_boolean(ctx, host ? 1 : 0);
	return 1;
}

// JS level.listPlayers() - server-side roster [{id,name,x,y,z,dim,local}].
// On the host: the local player + every connected remote player. Empty on
// clients (a client only ever controls its own player).
static duk_ret_t jsLevelListPlayers(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	Minecraft* mc = me ? me->minecraft() : NULL;
	duk_idx_t arr = duk_push_array(ctx);
	int n = 0;
	if (!mc || !mc->level) return 1;

	ServerSideNetworkHandler* ssn = dynamic_cast<ServerSideNetworkHandler*>(mc->netCallback);

	// local (host / single-player) player first
	if (mc->player && !mc->level->isClientSide) {
		Player* p = mc->player;
		duk_idx_t obj = duk_push_object(ctx);
		duk_push_int(ctx, p->entityId);      duk_put_prop_string(ctx, obj, "id");
		duk_push_string(ctx, p->name.c_str()); duk_put_prop_string(ctx, obj, "name");
		duk_push_number(ctx, p->x);          duk_put_prop_string(ctx, obj, "x");
		duk_push_number(ctx, p->y);          duk_put_prop_string(ctx, obj, "y");
		duk_push_number(ctx, p->z);          duk_put_prop_string(ctx, obj, "z");
		duk_push_int(ctx, p->dimension);     duk_put_prop_string(ctx, obj, "dim");
		duk_push_boolean(ctx, 1);            duk_put_prop_string(ctx, obj, "local");
		duk_put_prop_index(ctx, arr, n++);
	}

	if (ssn) {
		const std::map<RakNet::RakNetGUID, Player*>& remotes = ssn->getRemotePlayers();
		for (std::map<RakNet::RakNetGUID, Player*>::const_iterator it = remotes.begin(); it != remotes.end(); ++it) {
			Player* p = it->second;
			if (!p || p == mc->player) continue;
			duk_idx_t obj = duk_push_object(ctx);
			duk_push_int(ctx, p->entityId);      duk_put_prop_string(ctx, obj, "id");
			duk_push_string(ctx, p->name.c_str()); duk_put_prop_string(ctx, obj, "name");
			duk_push_number(ctx, p->x);          duk_put_prop_string(ctx, obj, "x");
			duk_push_number(ctx, p->y);          duk_put_prop_string(ctx, obj, "y");
			duk_push_number(ctx, p->z);          duk_put_prop_string(ctx, obj, "z");
			duk_push_int(ctx, p->dimension);     duk_put_prop_string(ctx, obj, "dim");
			duk_push_boolean(ctx, 0);            duk_put_prop_string(ctx, obj, "local");
			duk_put_prop_index(ctx, arr, n++);
		}
	}
	return 1;
}

// JS level.isOp([name]) - 这个名字是不是 op（不传 = 自己）。
//   每个世界一份名单，存在世界目录的 ops.txt 里（原版游戏不用它，纯给模组）。
static duk_ret_t jsLevelIsOp(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	Minecraft* mc = MCMOD();
	std::string name;
	if (duk_is_string(ctx, 0))
		name = duk_safe_to_string(ctx, 0);
	else if (mc && mc->player)
		name = mc->player->name;
	duk_push_boolean(ctx, (me && me->isOp(name)) ? 1 : 0);
	return 1;
}

// JS level.listOps() - 当前世界的 op 名单（字符串数组）。
static duk_ret_t jsLevelListOps(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	duk_idx_t arr = duk_push_array(ctx);
	if (!me) return 1;
	int n = 0;
	std::set<std::string>& ops = me->worldOps();
	for (std::set<std::string>::const_iterator it = ops.begin(); it != ops.end(); ++it) {
		duk_push_string(ctx, it->c_str());
		duk_put_prop_index(ctx, arr, n++);
	}
	return 1;
}

// JS level.addOp(name) - 加一个 op（立刻写盘）。
static duk_ret_t jsLevelAddOp(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	if (me && duk_is_string(ctx, 0))
		me->addOp(duk_safe_to_string(ctx, 0));
	return 0;
}

// JS level.removeOp(name) - 去掉一个 op（立刻写盘）。
static duk_ret_t jsLevelRemoveOp(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	if (me && duk_is_string(ctx, 0))
		me->removeOp(duk_safe_to_string(ctx, 0));
	return 0;
}

// JS level.changePlayerDimension(playerId, dim, x, y, z) - server-authoritative
// dimension travel for one player. playerId<=0 (or omitted-like 0) means the
// local/host player. Other ids must refer to a connected remote player.
static duk_ret_t jsLevelChangePlayerDimension(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	Minecraft* mc = me ? me->minecraft() : NULL;
	if (!mc || !mc->level) return 0;
	int id = (int)duk_get_int(ctx, 0);
	int dim = (int)duk_get_int(ctx, 1);
	float x = (float)duk_get_number(ctx, 2);
	float y = (float)duk_get_number(ctx, 3);
	float z = (float)duk_get_number(ctx, 4);

	if (id <= 0 || (mc->player && id == mc->player->entityId)) {
		if (me) {
			me->setCurrentDimension(dim);
			me->saveDimensionState();
		}
		mc->teleportPlayerToDimension(dim, x, y, z);
		return 0;
	}

	ServerSideNetworkHandler* ssn = dynamic_cast<ServerSideNetworkHandler*>(mc->netCallback);
	if (ssn) {
		Player* p = ssn->findServerPlayerById(id);
		if (p) {
			ssn->teleportPlayerToDimension(p, dim, x, y, z);
		} else if (me) {
			me->log("changePlayerDimension: no connected player id " + std::to_string(id));
		}
	}
	return 0;
}

// JS player.isForwardHeld() - forward input (keyboard W or touch stick).
static duk_ret_t jsPlayerIsForwardHeld(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	Minecraft* mc = me ? me->minecraft() : NULL;
	if (!mc || !mc->player) { duk_push_boolean(ctx, 0); return 1; }
	bool fwd = Keyboard::isKeyDown(mc->options.keyUp.key);
	if (!fwd) {
		LocalPlayer* lp = dynamic_cast<LocalPlayer*>(mc->player);
		if (lp && lp->input && lp->input->ya > 0)
			fwd = true;   // touch / virtual stick forward
	}
	duk_push_boolean(ctx, fwd ? 1 : 0);
	return 1;
}

// JS player.setSprinting(bool) - sprint (faster walk + wider FOV).
static duk_ret_t jsPlayerSetSprinting(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	Minecraft* mc = me ? me->minecraft() : NULL;
	if (mc && mc->player)
		mc->player->setSprinting(duk_get_boolean(ctx, 0) != 0);
	return 0;
}

// JS player.isCreative() - true in creative/creator game modes.
static duk_ret_t jsPlayerIsCreative(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	Minecraft* mc = me ? me->minecraft() : NULL;
	if (mc && mc->gameMode) {
		duk_push_boolean(ctx, mc->gameMode->isCreativeType() ? 1 : 0);
		return 1;
	}
	duk_push_boolean(ctx, 0);
	return 1;
}

// JS level.setGameMode("creative"|"survival"|1|0) - switch survival/creative.
static duk_ret_t jsLevelSetGameMode(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	Minecraft* mc = me ? me->minecraft() : NULL;
	if (!mc) return 0;
	bool creative = true;
	if (duk_is_number(ctx, 0)) {
		creative = duk_get_int(ctx, 0) != 0;
	} else {
		std::string s = duk_safe_to_string(ctx, 0);
		creative = (s == "creative" || s == "c" || s == "1" || s == "创造");
	}
	mc->setIsCreativeMode(creative);
	return 0;
}

// ---------------------------------------------------------------------------
// Stage 6: world environment controls.
//   level.setLight(x, y, z, brightness)   // 0-15, engine light layer
//   level.setSkyColor(r, g, b)            // 0-1 floats, overrides sky tint
//   level.setWeather("rain"|"thunder"|"clear") // rain/thunder/sunny
// ---------------------------------------------------------------------------
static duk_ret_t jsLevelSetLight(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	Minecraft* mc = me ? me->minecraft() : NULL;
	if (!mc || !mc->level) return 0;
	int x = (int)duk_get_int(ctx, 0);
	int y = (int)duk_get_int(ctx, 1);
	int z = (int)duk_get_int(ctx, 2);
	int b = (int)duk_get_int(ctx, 3);
	if (b < 0) b = 0;
	if (b > 15) b = 15;
	Level* level = mc->level;
	level->setBrightness(LightLayer::Block, x, y, z, b);
	level->updateLight(LightLayer::Block, x - 1, y - 1, z - 1, x + 1, y + 1, z + 1);
	return 0;
}

static duk_ret_t jsLevelSetSkyColor(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	Minecraft* mc = me ? me->minecraft() : NULL;
	if (!mc || !mc->level) return 0;
	float r = (float)duk_get_number(ctx, 0);
	float g = (float)duk_get_number(ctx, 1);
	float b = (float)duk_get_number(ctx, 2);
	// Store on the Level directly (render reads it without ModEngine).
	mc->level->skyR = r; mc->level->skyG = g; mc->level->skyB = b;
	return 0;
}

static duk_ret_t jsLevelSetWeather(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	Minecraft* mc = me ? me->minecraft() : NULL;
	if (!mc || !mc->level) return 0;
	std::string s = duk_safe_to_string(ctx, 0);
	if (s == "rain" || s == "thunder")
		mc->level->rainLevel = (s == "thunder") ? 1.0f : 1.0f;
	else
		mc->level->rainLevel = 0.0f;
	return 0;
}

// JS player.teleport(x, y, z)
static duk_ret_t jsPlayerTeleport(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	Minecraft* mc = me ? me->minecraft() : NULL;
	if (mc && mc->player) {
		float x = (float)duk_get_number(ctx, 0);
		float y = (float)duk_get_number(ctx, 1);
		float z = (float)duk_get_number(ctx, 2);
		Player* p = mc->player;
		p->moveTo(x, y, z, p->yRot, p->xRot);
	}
	return 0;
}

// JS player.setVelocity(xd, yd, zd) - 直接设置玩家速度。
// 模组弹跳方块（云）用：抛起后引擎物理(Mob::travel→Entity::move)接管整个弧线，
// 渲染插值自动生效 = 丝滑。重力自动减速，落回方块碰撞自动清零 yd。
static duk_ret_t jsPlayerSetVelocity(duk_context* ctx) {
	Minecraft* m = MCMOD();
	if (m && m->player) {
		Player* p = m->player;
		p->xd = (float)duk_get_number(ctx, 0);
		p->yd = (float)duk_get_number(ctx, 1);
		p->zd = (float)duk_get_number(ctx, 2);
	}
	return 0;
}

// ── 坐姿 API: player.sit() / sit(x,y,z) / sit(entityId) / stand() / isSitting() ──
// 玩家坐着: 渲染为 riding 坐姿(腿折叠), 不能走动只能转头; 可静止坐或挂实体。
static duk_ret_t jsPlayerSit(duk_context* ctx) {
	Minecraft* m = MCMOD();
	if (!m || !m->player) return 0;
	Player* p = m->player;
	int n = duk_get_top(ctx);
	if (n >= 3 && duk_is_number(ctx, 0)) {
		// sit(x, y, z): 坐到指定坐标(座位方块用)
		p->sitAt((float)duk_get_number(ctx, 0), (float)duk_get_number(ctx, 1), (float)duk_get_number(ctx, 2));
	} else if (n >= 1 && duk_is_number(ctx, 0)) {
		// sit(entityId): 挂到实体上跟随移动(类矿车)
		p->sitOnEntity((int)duk_get_int(ctx, 0));
	} else {
		p->sit();  // 原地坐下
	}
	return 0;
}
static duk_ret_t jsPlayerStand(duk_context* ctx) {
	Minecraft* m = MCMOD();
	if (m && m->player) m->player->stand();
	return 0;
}
static duk_ret_t jsPlayerIsSitting(duk_context* ctx) {
	Minecraft* m = MCMOD();
	duk_push_boolean(ctx, m && m->player && m->player->isSitting());
	return 1;
}

// ---------------------------------------------------------------------------
// 模组：玩家（按 id 寻址）+ 玩家动作
//   引擎里玩家一直只有“自己”（player 对象 = 本地玩家），没有按 id 找别人的
//   途径。这里补上（和 mob 那套对称），动作也挂在 id 上，按玩家分别生效。
// ---------------------------------------------------------------------------

// 取第 idx 个参数作为玩家 id：既接受数字，也接受 Player.self()/Player.list()
// 返回的玩家对象（{id:...}）。
static int playerIdArg(duk_context* ctx, duk_idx_t idx) {
	if (duk_is_object(ctx, idx)) {
		duk_get_prop_string(ctx, idx, "id");
		int id = (int)duk_get_int(ctx, -1);
		duk_pop(ctx);
		return id;
	}
	return (int)duk_get_int(ctx, idx);
}

// 把一个玩家的当前状态推成一个 JS 对象（快照，改它不影响游戏）。
static void pushPlayerSnapshot(duk_context* ctx, Player* p, bool isLocal) {
	duk_idx_t obj = duk_push_object(ctx);
	duk_push_int(ctx, p->entityId);                   duk_put_prop_string(ctx, obj, "id");
	duk_push_string(ctx, p->name.c_str());            duk_put_prop_string(ctx, obj, "name");
	duk_push_number(ctx, p->x);                       duk_put_prop_string(ctx, obj, "x");
	duk_push_number(ctx, p->y);                       duk_put_prop_string(ctx, obj, "y");
	duk_push_number(ctx, p->z);                       duk_put_prop_string(ctx, obj, "z");
	duk_push_number(ctx, p->yRot);                    duk_put_prop_string(ctx, obj, "yRot");
	duk_push_number(ctx, p->xRot);                    duk_put_prop_string(ctx, obj, "xRot");
	duk_push_number(ctx, p->health);                  duk_put_prop_string(ctx, obj, "health");
	duk_push_int(ctx, p->dimension);                  duk_put_prop_string(ctx, obj, "dim");
	duk_push_boolean(ctx, p->isInWater() ? 1 : 0);    duk_put_prop_string(ctx, obj, "isInWater");
	duk_push_boolean(ctx, p->isSneaking() ? 1 : 0);   duk_put_prop_string(ctx, obj, "isSneaking");
	duk_push_boolean(ctx, p->onGround ? 1 : 0);       duk_put_prop_string(ctx, obj, "onGround");
	duk_push_boolean(ctx, p->isSitting() ? 1 : 0);    duk_put_prop_string(ctx, obj, "isSitting");
	duk_push_boolean(ctx, isLocal ? 1 : 0);           duk_put_prop_string(ctx, obj, "local");
	std::string cur;
	if (ModEngine::instance)
		cur = ModEngine::instance->getPlayerAction(p->entityId);
	duk_push_string(ctx, cur.c_str());                duk_put_prop_string(ctx, obj, "action");
}

// JS Player.self() - 本地玩家快照；没有本地玩家（纯服务器）时返回 null。
static duk_ret_t jsPlayerSelf(duk_context* ctx) {
	Minecraft* m = MCMOD();
	if (!m || !m->player) {
		duk_push_null(ctx);
		return 1;
	}
	pushPlayerSnapshot(ctx, m->player, true);
	return 1;
}

// JS Player.list() - 玩家数组。
//   单机/客户端：只有自己（引擎在客户端只保留自己的 player 对象）。
//   服务器/房主：自己 + 每个连进来的玩家。
static duk_ret_t jsPlayerList(duk_context* ctx) {
	Minecraft* m = MCMOD();
	duk_idx_t arr = duk_push_array(ctx);
	if (!m) return 1;
	int n = 0;
	if (m->player) {
		pushPlayerSnapshot(ctx, m->player, true);
		duk_put_prop_index(ctx, arr, n++);
	}
	ServerSideNetworkHandler* ssn = dynamic_cast<ServerSideNetworkHandler*>(m->netCallback);
	if (ssn) {
		const std::map<RakNet::RakNetGUID, Player*>& remotes = ssn->getRemotePlayers();
		for (std::map<RakNet::RakNetGUID, Player*>::const_iterator it = remotes.begin(); it != remotes.end(); ++it) {
			Player* p = it->second;
			if (!p || p == m->player) continue;
			pushPlayerSnapshot(ctx, p, false);
			duk_put_prop_index(ctx, arr, n++);
		}
	}
	return 1;
}

// JS Player.get(idOrPlayer) - 按 id 找玩家；找不到返回 null。
static duk_ret_t jsPlayerGetById(duk_context* ctx) {
	Minecraft* m = MCMOD();
	const int id = playerIdArg(ctx, 0);
	Player* found = NULL;
	bool isLocal = false;
	if (m) {
		if (m->player && m->player->entityId == id) {
			found = m->player;
			isLocal = true;
		} else {
			ServerSideNetworkHandler* ssn = dynamic_cast<ServerSideNetworkHandler*>(m->netCallback);
			if (ssn) {
				const std::map<RakNet::RakNetGUID, Player*>& remotes = ssn->getRemotePlayers();
				for (std::map<RakNet::RakNetGUID, Player*>::const_iterator it = remotes.begin(); it != remotes.end(); ++it) {
					if (it->second && it->second->entityId == id) { found = it->second; break; }
				}
			}
		}
	}
	if (!found) {
		duk_push_null(ctx);
		return 1;
	}
	pushPlayerSnapshot(ctx, found, isLocal);
	return 1;
}

// 按 entityId 找玩家：本机玩家，或服务器/房主模式下连进来的玩家。
// （和 jsPlayerGetById 用的是同一套查找，单独抽出来给 sit/stand 用。）
static Player* findPlayerByIdForScript(int id) {
	Minecraft* m = MCMOD();
	if (!m || id <= 0)
		return NULL;
	if (m->player && m->player->entityId == id)
		return m->player;
	ServerSideNetworkHandler* ssn = dynamic_cast<ServerSideNetworkHandler*>(m->netCallback);
	if (ssn) {
		const std::map<RakNet::RakNetGUID, Player*>& remotes = ssn->getRemotePlayers();
		for (std::map<RakNet::RakNetGUID, Player*>::const_iterator it = remotes.begin(); it != remotes.end(); ++it) {
			if (it->second && it->second->entityId == id)
				return it->second;
		}
	}
	return NULL;
}

// JS player.getId() - 本次事件里的那个玩家的 id（右键/聊天的发起者）。
//   服务器上 mc->player 恒为空，只能靠事件上下文玩家（modPlayerForScript）。
//   既没上下文也没本机玩家时返回 -1。
static duk_ret_t jsPlayerGetId(duk_context* ctx) {
	Player* p = modPlayerForScript();
	duk_push_int(ctx, p ? p->entityId : -1);
	return 1;
}

// JS Player.sit(id, x, y, z) - 让【指定玩家】坐到指定坐标（座位方块用）。
//   复用引擎既有的坐姿机制（Player::sitAt：坐下禁移动 + 坐姿渲染 + 高度下调 +
//   tickSit 对齐坐标），只是把入口从“本机玩家”换成按 id 找。
//   服务器端同样可用 —— 这是“谁点椅子谁坐下”在联机里能生效的关键。
static duk_ret_t jsPlayerSitById(duk_context* ctx) {
	Player* p = findPlayerByIdForScript(playerIdArg(ctx, 0));
	if (!p)
		return 0;
	p->sitAt((float)duk_get_number(ctx, 1), (float)duk_get_number(ctx, 2), (float)duk_get_number(ctx, 3));
	return 0;
}

// JS Player.stand(id) - 让【指定玩家】起身。
static duk_ret_t jsPlayerStandById(duk_context* ctx) {
	Player* p = findPlayerByIdForScript(playerIdArg(ctx, 0));
	if (p)
		p->stand();
	return 0;
}

// JS Player.defineAction(name, fn)
//   fn(part, age, time) -> {x,y,z} | null
//   part: "head"|"body"|"arm0"|"arm1"|"leg0"|"leg1"；
//   返回 null/undefined = 这个部件走原版姿态（游泳只管手臂和腿就很自然）。
static duk_ret_t jsPlayerDefineAction(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	if (!me) return 0;
	if (!duk_is_string(ctx, 0) || !duk_is_function(ctx, 1)) {
		me->log("Player.defineAction(name, fn): 需要 (动作名, 函数)");
		return 0;
	}
	const std::string name = duk_safe_to_string(ctx, 0);
	const std::string key = "__mod_player_action_" + name;
	duk_dup(ctx, 1);
	duk_put_global_string(ctx, key.c_str());
	me->definePlayerAction(name, key);
	return 0;
}

// JS Player.setAction(idOrPlayer, name|null) - 按玩家分别挂/取消动作。
static duk_ret_t jsPlayerSetAction(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	if (!me) return 0;
	const int id = playerIdArg(ctx, 0);
	std::string action;
	if (duk_is_string(ctx, 1))
		action = duk_safe_to_string(ctx, 1);
	me->setPlayerAction(id, action);
	return 0;
}

// JS Player.setModel(idOrPlayer, spec|null)
//   spec = 数字 / 数字串（脚本生物 typeId，用 Mob.defineMob 定义过的那份外形）
//          -> 穿上；null/undefined -> 脱下来，变回玩家自己。
//   体型（碰撞箱）跟着外形走；联机时别人也看得到。
static duk_ret_t jsPlayerSetModel(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	if (!me) return 0;
	const int id = playerIdArg(ctx, 0);
	std::string spec;
	if (duk_is_number(ctx, 1)) {
		char buf[32];
		sprintf(buf, "%d", (int)duk_get_int(ctx, 1));
		spec = buf;
	} else if (duk_is_string(ctx, 1)) {
		spec = duk_safe_to_string(ctx, 1);
	}
	me->setPlayerModel(id, spec);
	return 0;
}

// JS Player.getModel(idOrPlayer) - 当前外形（没有返回 null）。
static duk_ret_t jsPlayerGetModel(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	const int id = playerIdArg(ctx, 0);
	std::string spec;
	if (me) spec = me->getPlayerModel(id);
	if (spec.empty()) duk_push_null(ctx);
	else duk_push_string(ctx, spec.c_str());
	return 1;
}

// JS Player.getAction(idOrPlayer) - 当前动作名（没有返回 null）。
static duk_ret_t jsPlayerGetAction(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	const int id = playerIdArg(ctx, 0);
	std::string action;
	if (me) action = me->getPlayerAction(id);
	if (action.empty()) duk_push_null(ctx);
	else duk_push_string(ctx, action.c_str());
	return 1;
}

// JS ui.showLoading(text) / ui.hideLoading()
static duk_ret_t jsUiShowLoading(duk_context* ctx) {
#ifndef STANDALONE_SERVER
	ModEngine* me = ModEngine::instance;
	if (me) me->setLoadingOverlay(duk_safe_to_string(ctx, 0));
	return 0;
#else
	(void)ctx; return 0;
#endif
}
static duk_ret_t jsUiHideLoading(duk_context* ctx) {
#ifndef STANDALONE_SERVER
	ModEngine* me = ModEngine::instance;
	if (me) me->setLoadingOverlay("");
	return 0;
#else
	(void)ctx; return 0;
#endif
}

// ---------------------------------------------------------------------------
// Stage 5: UI.openScreen(config) / UI.closeScreen()
//   UI.openScreen({
//     title: "设置",
//     buttons: [ {id, text, x, y, w, h}, ... ],
//     texts:   [ {x, y, text, color}, ... ],
//     images:  [ {x, y, w, h, path}, ... ],
//     input:   {id, x, y, w, h, default},          // one input box
//     onButton: function(id){}, onText: function(id,text){},
//     onClose: function(){}, onRender: function(){}
//   });
// ---------------------------------------------------------------------------
static duk_ret_t jsUiOpenScreen(duk_context* ctx) {
#ifndef STANDALONE_SERVER
	ModEngine* me = ModEngine::instance;
	if (!me) return 0;
	Minecraft* mc = me->minecraft();
	if (!mc) return 0;
	if (!duk_is_object(ctx, 0)) {
		me->log("UI.openScreen: config object required");
		return 0;
	}
	ScriptedScreen* s = new ScriptedScreen();
	duk_get_prop_string(ctx, 0, "title");
	if (duk_is_string(ctx, -1)) s->title = duk_safe_to_string(ctx, -1);
	duk_pop(ctx);
	// buttons[]
	duk_get_prop_string(ctx, 0, "buttons");
	if (duk_is_array(ctx, -1)) {
		int n = (int)duk_get_length(ctx, -1);
		for (int i = 0; i < n; ++i) {
			duk_get_prop_index(ctx, -1, i);
			if (duk_is_object(ctx, -1)) {
				ScriptedScreen::Btn b;
				b.x = b.y = b.w = b.h = 0;
				duk_get_prop_string(ctx, -1, "id");   b.id = (int)duk_get_int(ctx, -1); duk_pop(ctx);
				duk_get_prop_string(ctx, -1, "text"); b.text = duk_safe_to_string(ctx, -1); duk_pop(ctx);
				duk_get_prop_string(ctx, -1, "x");    b.x = (int)duk_get_int(ctx, -1); duk_pop(ctx);
				duk_get_prop_string(ctx, -1, "y");    b.y = (int)duk_get_int(ctx, -1); duk_pop(ctx);
				duk_get_prop_string(ctx, -1, "w");    b.w = (int)duk_get_int(ctx, -1); duk_pop(ctx);
				duk_get_prop_string(ctx, -1, "h");    b.h = (int)duk_get_int(ctx, -1); duk_pop(ctx);
				if (b.w <= 0) b.w = 100;
				if (b.h <= 0) b.h = 24;
				s->btnDefs.push_back(b);
			}
			duk_pop(ctx);
		}
	}
	duk_pop(ctx);
	// texts[]
	duk_get_prop_string(ctx, 0, "texts");
	if (duk_is_array(ctx, -1)) {
		int n = (int)duk_get_length(ctx, -1);
		for (int i = 0; i < n; ++i) {
			duk_get_prop_index(ctx, -1, i);
			if (duk_is_object(ctx, -1)) {
				ScriptedScreen::Txt t;
				t.x = t.y = 0; t.color = 0xffffffff;
				duk_get_prop_string(ctx, -1, "x");     t.x = (int)duk_get_int(ctx, -1); duk_pop(ctx);
				duk_get_prop_string(ctx, -1, "y");     t.y = (int)duk_get_int(ctx, -1); duk_pop(ctx);
				duk_get_prop_string(ctx, -1, "text");  t.text = duk_safe_to_string(ctx, -1); duk_pop(ctx);
				duk_get_prop_string(ctx, -1, "color"); t.color = (int)duk_get_int(ctx, -1); duk_pop(ctx);
				s->texts.push_back(t);
			}
			duk_pop(ctx);
		}
	}
	duk_pop(ctx);
	// images[]
	duk_get_prop_string(ctx, 0, "images");
	if (duk_is_array(ctx, -1)) {
		int n = (int)duk_get_length(ctx, -1);
		for (int i = 0; i < n; ++i) {
			duk_get_prop_index(ctx, -1, i);
			if (duk_is_object(ctx, -1)) {
				ScriptedScreen::Img im;
				duk_get_prop_string(ctx, -1, "x");     im.x = (int)duk_get_int(ctx, -1); duk_pop(ctx);
				duk_get_prop_string(ctx, -1, "y");     im.y = (int)duk_get_int(ctx, -1); duk_pop(ctx);
				duk_get_prop_string(ctx, -1, "w");     im.w = (int)duk_get_int(ctx, -1); duk_pop(ctx);
				duk_get_prop_string(ctx, -1, "h");     im.h = (int)duk_get_int(ctx, -1); duk_pop(ctx);
				duk_get_prop_string(ctx, -1, "path");  im.path = duk_safe_to_string(ctx, -1); duk_pop(ctx);
				s->images.push_back(im);
			}
			duk_pop(ctx);
		}
	}
	duk_pop(ctx);
	// input {id, x, y, w, h, default}
	duk_get_prop_string(ctx, 0, "input");
	if (duk_is_object(ctx, -1)) {
		ScriptedScreen::Inp in;
		in.x = in.y = in.w = in.h = 0; in.focused = true;
		duk_get_prop_string(ctx, -1, "id");      in.id = (int)duk_get_int(ctx, -1); duk_pop(ctx);
		duk_get_prop_string(ctx, -1, "x");       in.x = (int)duk_get_int(ctx, -1); duk_pop(ctx);
		duk_get_prop_string(ctx, -1, "y");       in.y = (int)duk_get_int(ctx, -1); duk_pop(ctx);
		duk_get_prop_string(ctx, -1, "w");       in.w = (int)duk_get_int(ctx, -1); duk_pop(ctx);
		duk_get_prop_string(ctx, -1, "h");       in.h = (int)duk_get_int(ctx, -1); duk_pop(ctx);
		duk_get_prop_string(ctx, -1, "default"); in.text = duk_safe_to_string(ctx, -1); duk_pop(ctx);
		if (in.w <= 0) in.w = 200;
		if (in.h <= 0) in.h = 24;
		s->inputs.push_back(in);
		s->_activeInput = 0;
		s->wantKeyboard = true;
	}
	duk_pop(ctx);
	// cells[] (clickable grid, container-like)
	duk_get_prop_string(ctx, 0, "cells");
	if (duk_is_array(ctx, -1)) {
		int n = (int)duk_get_length(ctx, -1);
		for (int i = 0; i < n; ++i) {
			duk_get_prop_index(ctx, -1, i);
			if (duk_is_object(ctx, -1)) {
				ScriptedScreen::Cell c;
				c.x = c.y = c.w = c.h = 0; c.color = 0xffffffff;
				duk_get_prop_string(ctx, -1, "id");    c.id = (int)duk_get_int(ctx, -1); duk_pop(ctx);
				duk_get_prop_string(ctx, -1, "x");     c.x = (int)duk_get_int(ctx, -1); duk_pop(ctx);
				duk_get_prop_string(ctx, -1, "y");     c.y = (int)duk_get_int(ctx, -1); duk_pop(ctx);
				duk_get_prop_string(ctx, -1, "w");     c.w = (int)duk_get_int(ctx, -1); duk_pop(ctx);
				duk_get_prop_string(ctx, -1, "h");     c.h = (int)duk_get_int(ctx, -1); duk_pop(ctx);
				duk_get_prop_string(ctx, -1, "text");  c.text = duk_safe_to_string(ctx, -1); duk_pop(ctx);
				duk_get_prop_string(ctx, -1, "color"); c.color = (int)duk_get_int(ctx, -1); duk_pop(ctx);
				if (c.w <= 0) c.w = 40;
				if (c.h <= 0) c.h = 40;
				s->cells.push_back(c);
			}
			duk_pop(ctx);
		}
	}
	duk_pop(ctx);
	// Callbacks into the heap stash.
	static const struct { const char* opt; const char* key; } cbs[] = {
		{ "onButton", "__mod_screen_onButton" },
		{ "onText",   "__mod_screen_onText"   },
		{ "onClose",  "__mod_screen_onClose"  },
		{ "onRender", "__mod_screen_onRender" },
		{ "onCell",   "__mod_screen_onCell"   },
	};
	for (size_t i = 0; i < sizeof(cbs)/sizeof(cbs[0]); ++i) {
		duk_get_prop_string(ctx, 0, cbs[i].opt);
		if (duk_is_function(ctx, -1)) {
			duk_push_heap_stash(ctx);
			duk_insert(ctx, -2);            // [fn, stash] -> [stash, fn]
			duk_put_prop_string(ctx, -2, cbs[i].key);
			duk_pop(ctx);
		} else {
			duk_pop(ctx);
		}
	}
	// Defer the actual setScreen to the next tick: onChat etc. run inside
	// the caller's event handling, and ChatInputScreen::submit() calls
	// setScreen(NULL) right after fireEvent("onChat") — which would clobber
	// our screen through the screenMutex scheduling. ModEngine::tick opens
	// the pending screen afterwards.
	me->setPendingScreen(s);
	return 0;
#else
	(void)ctx; return 0;
#endif
}

static duk_ret_t jsUiCloseScreen(duk_context* ctx) {
#ifndef STANDALONE_SERVER
	ModEngine* me = ModEngine::instance;
	if (!me || !me->minecraft()) return 0;
	me->notifyScreenClose();
	me->minecraft()->setScreen(NULL);
	return 0;
#else
	(void)ctx; return 0;
#endif
}

// ---- UI override registry (09 · UI 覆盖系统) ----
// Element key syntax: "<screen>.<element>" (e.g. "mainmenu.button.2",
// "mainmenu.title"). screen = startmenu/pause/options/play etc.
static bool splitUiKey(const std::string& full, std::string& screen, std::string& elem) {
	size_t dot = full.find('.');
	if (dot == std::string::npos || dot == 0 || dot + 1 >= full.size()) return false;
	screen = full.substr(0, dot);
	elem = full.substr(dot + 1);
	return true;
}
// Parse one UI.override props object into a UiElementOverride (only the
// properties present in the JS object are marked has*).
static void uiParseProps(duk_context* ctx, ModEngine::UiElementOverride& o) {
	// x / y (position) — either both or neither
	duk_get_prop_string(ctx, -1, "x");
	if (duk_is_number(ctx, -1)) { o.x = (int)duk_get_int(ctx, -1); o.hasPos = true; }
	duk_pop(ctx);
	duk_get_prop_string(ctx, -1, "y");
	if (duk_is_number(ctx, -1)) { o.y = (int)duk_get_int(ctx, -1); o.hasPos = true; }
	duk_pop(ctx);
	duk_get_prop_string(ctx, -1, "w");
	if (duk_is_number(ctx, -1)) { o.w = (int)duk_get_int(ctx, -1); o.hasSize = true; }
	duk_pop(ctx);
	duk_get_prop_string(ctx, -1, "h");
	if (duk_is_number(ctx, -1)) { o.h = (int)duk_get_int(ctx, -1); o.hasSize = true; }
	duk_pop(ctx);
	duk_get_prop_string(ctx, -1, "text");
	if (duk_is_string(ctx, -1)) { o.text = duk_safe_to_string(ctx, -1); o.hasText = true; }
	duk_pop(ctx);
	duk_get_prop_string(ctx, -1, "visible");
	if (duk_is_boolean(ctx, -1)) { o.visible = duk_get_boolean(ctx, -1) != 0; o.hasVisible = true; }
	duk_pop(ctx);
	duk_get_prop_string(ctx, -1, "alpha");
	if (duk_is_number(ctx, -1)) { o.alpha = (float)duk_get_number(ctx, -1); o.hasAlpha = true; }
	duk_pop(ctx);
	duk_get_prop_string(ctx, -1, "image");
	if (duk_is_string(ctx, -1)) { o.imagePath = duk_safe_to_string(ctx, -1); o.hasImage = true; }
	duk_pop(ctx);
}
// UI.override("mainmenu.button.2", {x:.., y:.., w:.., h:.., text:.., visible:.., alpha:.., image:..})
static duk_ret_t jsUiOverride(duk_context* ctx) {
#ifndef STANDALONE_SERVER
	ModEngine* me = ModEngine::instance;
	if (!me) return 0;
	std::string screen, elem;
	if (!splitUiKey(duk_safe_to_string(ctx, 0), screen, elem) || !duk_is_object(ctx, 1)) {
		me->log("UI.override: usage UI.override(\"<screen>.<element>\", {props})");
		return 0;
	}
	ModEngine::UiScreenDef& def = me->_uiScreens[screen];
	ModEngine::UiElementOverride& o = def.elements[elem];
	uiParseProps(ctx, o);
	me->log("UI.override " + screen + "." + elem + " registered");
	return 0;
#else
	(void)ctx; return 0;
#endif
}
// UI.setImage("mainmenu.title", "ui/my_title.png")   (null = clear image override)
static duk_ret_t jsUiSetImage(duk_context* ctx) {
#ifndef STANDALONE_SERVER
	ModEngine* me = ModEngine::instance;
	if (!me) return 0;
	std::string screen, elem;
	if (!splitUiKey(duk_safe_to_string(ctx, 0), screen, elem)) {
		me->log("UI.setImage: usage UI.setImage(\"<screen>.<element>\", \"png\")");
		return 0;
	}
	ModEngine::UiScreenDef& def = me->_uiScreens[screen];
	ModEngine::UiElementOverride& o = def.elements[elem];
	if (duk_is_string(ctx, 1) && duk_get_string(ctx, 1) && duk_get_string(ctx, 1)[0]) {
		o.imagePath = duk_safe_to_string(ctx, 1);
		o.hasImage = true;
	} else {
		o.hasImage = false;  // remove image override -> vanilla source
		o.imagePath.clear();
	}
	me->log("UI.setImage " + screen + "." + elem + (o.hasImage ? " -> " + o.imagePath : " cleared"));
	return 0;
#else
	(void)ctx; return 0;
#endif
}
// UI.addButton("mainmenu", {key,text,x,y,w,h,image?,onClick?})
static duk_ret_t jsUiAddButton(duk_context* ctx) {
#ifndef STANDALONE_SERVER
	ModEngine* me = ModEngine::instance;
	if (!me) return 0;
	const char* screenC = duk_safe_to_string(ctx, 0);
	if (!screenC[0] || !duk_is_object(ctx, 1)) {
		me->log("UI.addButton: usage UI.addButton(\"<screen>\", {key,text,x,y,w,h,onClick})");
		return 0;
	}
	std::string screen = screenC;
	ModEngine::UiScreenDef& def = me->_uiScreens[screen];
	ModEngine::UiAddedButton b;
	duk_get_prop_string(ctx, 1, "key");
	if (duk_is_string(ctx, -1)) b.key = duk_safe_to_string(ctx, -1);
	duk_pop(ctx);
	duk_get_prop_string(ctx, 1, "text");
	if (duk_is_string(ctx, -1)) b.text = duk_safe_to_string(ctx, -1);
	duk_pop(ctx);
	duk_get_prop_string(ctx, 1, "image");
	if (duk_is_string(ctx, -1)) b.imagePath = duk_safe_to_string(ctx, -1);
	duk_pop(ctx);
	duk_get_prop_string(ctx, 1, "x"); if (duk_is_number(ctx, -1)) b.x = (int)duk_get_int(ctx, -1); duk_pop(ctx);
	duk_get_prop_string(ctx, 1, "y"); if (duk_is_number(ctx, -1)) b.y = (int)duk_get_int(ctx, -1); duk_pop(ctx);
	duk_get_prop_string(ctx, 1, "w"); if (duk_is_number(ctx, -1)) b.w = (int)duk_get_int(ctx, -1); duk_pop(ctx);
	duk_get_prop_string(ctx, 1, "h"); if (duk_is_number(ctx, -1)) b.h = (int)duk_get_int(ctx, -1); duk_pop(ctx);
	if (b.key.empty()) {
		me->log("UI.addButton: key required");
		return 0;
	}
	// onClick -> heap stash "__mod_ui_btn_<key>"
	duk_get_prop_string(ctx, 1, "onClick");
	if (duk_is_function(ctx, -1)) {
		std::string stashKey = "__mod_ui_btn_" + b.key;
		duk_push_heap_stash(ctx);
		duk_insert(ctx, -2);            // [fn, stash] -> [stash, fn]
		duk_put_prop_string(ctx, -2, stashKey.c_str());
		duk_pop(ctx);
	} else {
		duk_pop(ctx);
	}
	def.addButtons.push_back(b);
	me->log("UI.addButton " + screen + " + " + b.key);
	return 0;
#else
	(void)ctx; return 0;
#endif
}
// mouse.take(button, take) —— 模组把某个鼠标键「收归己有」。
// 接管后原版不再用这个键挖方块/放方块。手机上没有第二个鼠标键，模组拿着枪时
// 就用它挡住左键的挖掘行为（否则触摸屏幕 = 左键 = 一直挖方块）。
static duk_ret_t jsMouseTake(duk_context* ctx) {
#ifndef STANDALONE_SERVER
	ModEngine* me = ModEngine::instance;
	if (!me) return 0;
	me->setMouseTaken(duk_get_int(ctx, 0), duk_get_boolean(ctx, 1) != 0);
	return 0;
#else
	(void)ctx; return 0;
#endif
}

// UI.addHudKey({key,x,y,w,h,map}) —— 注册一个「按键映射」按钮：
//   map: 'mouse0'（鼠标左键）| 'mouse1'（鼠标右键）| 'key:82'（键盘键码，R = 82）
// 按住时引擎往输入系统注入对应的按下（Mouse::feed / Keyboard::feed），抬起时回收；
// 模组收到的还是标准的 onMouse / onKey，不用为手机写任何特殊逻辑。
static duk_ret_t jsUiAddHudKey(duk_context* ctx) {
#ifndef STANDALONE_SERVER
	ModEngine* me = ModEngine::instance;
	if (!me) return 0;
	if (!duk_is_object(ctx, 0)) {
		me->log("UI.addHudKey: usage UI.addHudKey({key,x,y,w,h,map:'mouse0'|'mouse1'|'key:82'})");
		return 0;
	}
	ModEngine::UiHudKey k;
	duk_get_prop_string(ctx, 0, "key");
	if (duk_is_string(ctx, -1)) k.key = duk_safe_to_string(ctx, -1);
	duk_pop(ctx);
	if (k.key.empty()) {
		me->log("UI.addHudKey: key required");
		return 0;
	}
	duk_get_prop_string(ctx, 0, "x"); if (duk_is_number(ctx, -1)) k.x = duk_get_int(ctx, -1); duk_pop(ctx);
	duk_get_prop_string(ctx, 0, "y"); if (duk_is_number(ctx, -1)) k.y = duk_get_int(ctx, -1); duk_pop(ctx);
	duk_get_prop_string(ctx, 0, "w"); if (duk_is_number(ctx, -1)) k.w = duk_get_int(ctx, -1); duk_pop(ctx);
	duk_get_prop_string(ctx, 0, "h"); if (duk_is_number(ctx, -1)) k.h = duk_get_int(ctx, -1); duk_pop(ctx);
	if (k.w < 8) k.w = 8;
	if (k.h < 8) k.h = 8;
	duk_get_prop_string(ctx, 0, "map");
	std::string map;
	if (duk_is_string(ctx, -1)) map = duk_safe_to_string(ctx, -1);
	duk_pop(ctx);
	if (map == "mouse0") {
		k.mapKind = 0;
	} else if (map == "mouse1") {
		k.mapKind = 1;
	} else if (map.compare(0, 4, "key:") == 0) {
		k.mapKind = 2;
		k.mapCode = atoi(map.c_str() + 4);
	} else {
		me->log("UI.addHudKey " + k.key + ": bad map '" + map + "' (want mouse0/mouse1/key:<code>)");
		return 0;
	}
	for (size_t i = 0; i < me->_hudKeys.size(); ++i) {
		if (me->_hudKeys[i].key == k.key) {
			me->_hudKeys[i] = k;
			me->markHudLayoutChanged();
			me->log("UI.addHudKey " + k.key + " updated");
			return 0;
		}
	}
	me->_hudKeys.push_back(k);
	me->markHudLayoutChanged();
	me->log("UI.addHudKey " + k.key + " -> " + map);
	return 0;
#else
	(void)ctx; return 0;
#endif
}

// UI.addHudButton({key,x,y,w,h}) —— 注册一个屏幕上的模组按钮（GUI 坐标，与
// onGuiRender 的 ui 绘制同一套）。引擎负责命中与派发回调（key 会作为参数传给
// onDown/onUp/onClick）；按钮长什么样、画在哪，还是模组在 onGuiRender 里用
// ui.fillRect / ui.drawImage 自己画。
static duk_ret_t jsUiAddHudButton(duk_context* ctx) {
#ifndef STANDALONE_SERVER
	ModEngine* me = ModEngine::instance;
	if (!me) return 0;
	if (!duk_is_object(ctx, 0)) {
		me->log("UI.addHudButton: usage UI.addHudButton({key,x,y,w,h,onDown?,onUp?,onClick?})");
		return 0;
	}
	ModEngine::UiHudButton b;
	duk_get_prop_string(ctx, 0, "key");
	if (duk_is_string(ctx, -1)) b.key = duk_safe_to_string(ctx, -1);
	duk_pop(ctx);
	if (b.key.empty()) {
		me->log("UI.addHudButton: key required");
		return 0;
	}
	duk_get_prop_string(ctx, 0, "x"); if (duk_is_number(ctx, -1)) b.x = duk_get_int(ctx, -1); duk_pop(ctx);
	duk_get_prop_string(ctx, 0, "y"); if (duk_is_number(ctx, -1)) b.y = duk_get_int(ctx, -1); duk_pop(ctx);
	duk_get_prop_string(ctx, 0, "w"); if (duk_is_number(ctx, -1)) b.w = duk_get_int(ctx, -1); duk_pop(ctx);
	duk_get_prop_string(ctx, 0, "h"); if (duk_is_number(ctx, -1)) b.h = duk_get_int(ctx, -1); duk_pop(ctx);
	if (b.w < 8) b.w = 8;
	if (b.h < 8) b.h = 8;

	// 回调存进本堆 stash：__mod_hud_<key>_down / _up / _click
	const char* props[3]    = { "onDown", "onUp", "onClick" };
	const char* suffixes[3] = { "_down",  "_up", "_click" };
	for (int i = 0; i < 3; ++i) {
		duk_get_prop_string(ctx, 0, props[i]);        // [fn|other]
		if (duk_is_function(ctx, -1)) {
			duk_push_heap_stash(ctx);                 // [fn stash]
			duk_insert(ctx, -2);                      // [stash fn]
			std::string k = "__mod_hud_" + b.key + suffixes[i];
			duk_put_prop_string(ctx, -2, k.c_str());  // [stash]
			duk_pop(ctx);                             // []
		} else {
			duk_pop(ctx);                             // []
		}
	}

	// 同一个 key 再注册 = 覆盖（模组重载/改布局时方便）
	for (size_t i = 0; i < me->_hudButtons.size(); ++i) {
		if (me->_hudButtons[i].key == b.key) {
			me->_hudButtons[i] = b;
			me->markHudLayoutChanged();
			me->log("UI.addHudButton " + b.key + " updated");
			return 0;
		}
	}
	me->_hudButtons.push_back(b);
	me->markHudLayoutChanged();
	me->log("UI.addHudButton " + b.key);
	return 0;
#else
	(void)ctx; return 0;
#endif
}

// ── UI.setControlStyle(key, style)：模组改原版控件的外观 ──────
// style = { x,y,w,h, hidden, opacity, shape, fill, border, borderWidth }
//   x/y/w/h  GUI 坐标（和 ui.getWidth 同一套）；不给就保持引擎算出的默认位置
//   opacity  图案/填充透明度 0..1
//   shape    0/缺省 = 原版贴图（仅调透明度）, 1 = 纯色矩形, 2 = 圆环
// 例：把跳跃键缩小投到屏幕右边缘、画成半透明圆：
//   UI.setControlStyle('jump', {x:520,y:150,w:36,h:36,shape:2,opacity:0.55})
static bool jsObjNum(duk_context* ctx, int objIdx, const char* name, float* out) {
	duk_get_prop_string(ctx, objIdx, name);
	const bool ok = duk_is_number(ctx, -1) != 0;
	if (ok) *out = (float)duk_get_number(ctx, -1);
	duk_pop(ctx);
	return ok;
}

static bool jsObjBool(duk_context* ctx, int objIdx, const char* name, bool* out) {
	duk_get_prop_string(ctx, objIdx, name);
	const bool ok = duk_is_boolean(ctx, -1) != 0;
	if (ok) *out = (duk_get_boolean(ctx, -1) != 0);
	duk_pop(ctx);
	return ok;
}

static bool jsObjUint(duk_context* ctx, int objIdx, const char* name, unsigned int* out) {
	duk_get_prop_string(ctx, objIdx, name);
	const bool ok = duk_is_number(ctx, -1) != 0;
	if (ok) *out = (unsigned int)duk_get_uint(ctx, -1);
	duk_pop(ctx);
	return ok;
}

static duk_ret_t jsUiSetControlStyle(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	if (!me) return 0;
	if (!duk_is_string(ctx, 0) || !duk_is_object(ctx, 1)) {
		me->log("UI.setControlStyle: usage UI.setControlStyle('jump', {x,y,w,h,hidden,opacity,shape,fill,border})");
		return 0;
	}
	const std::string key = duk_safe_to_string(ctx, 0);
	ModEngine::ControlStyle st;
	float v; bool b; unsigned int u;
	if (jsObjNum(ctx, 1, "x", &v)) { st.x = v; st.hasRect = true; }
	if (jsObjNum(ctx, 1, "y", &v)) { st.y = v; st.hasRect = true; }
	if (jsObjNum(ctx, 1, "w", &v)) { st.w = v; st.hasRect = true; }
	if (jsObjNum(ctx, 1, "h", &v)) { st.h = v; st.hasRect = true; }
	if (jsObjBool(ctx, 1, "hidden", &b))     st.hidden = b;
	if (jsObjNum(ctx, 1, "opacity", &v))   { st.opacity = v; st.hasOpacity = true; }
	if (jsObjNum(ctx, 1, "shape", &v))       st.shape = (int)v;
	if (jsObjUint(ctx, 1, "fill", &u))       st.fill = u;
	if (jsObjUint(ctx, 1, "border", &u))     st.border = u;
	if (jsObjNum(ctx, 1, "borderWidth", &v)) st.borderWidth = v;
	me->setControlStyle(key, st);
	return 0;
}

// UI.clearControlStyle(key)（不给 key 就全部清掉，恢复原版外观）
static duk_ret_t jsUiClearControlStyle(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	if (!me) return 0;
	const std::string key = duk_is_string(ctx, 0) ? duk_safe_to_string(ctx, 0) : std::string();
	me->clearControlStyle(key);
	return 0;
}

// UI.getControlRect('jump') -> {x,y,w,h,visible} | null（GUI 坐标）
static duk_ret_t jsUiGetControlRect(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	if (!me || !duk_is_string(ctx, 0)) return 0;
	const ModEngine::ControlRect* r = me->controlRect(duk_safe_to_string(ctx, 0));
	if (!r) { duk_push_null(ctx); return 1; }
	duk_push_object(ctx);
	duk_push_number(ctx, r->x); duk_put_prop_string(ctx, -2, "x");
	duk_push_number(ctx, r->y); duk_put_prop_string(ctx, -2, "y");
	duk_push_number(ctx, r->w); duk_put_prop_string(ctx, -2, "w");
	duk_push_number(ctx, r->h); duk_put_prop_string(ctx, -2, "h");
	duk_push_boolean(ctx, r->visible ? 1 : 0); duk_put_prop_string(ctx, -2, "visible");
	return 1;
}

// UI.clear("<screen>") or UI.clear() — wipe overrides (restore vanilla).
static duk_ret_t jsUiClear(duk_context* ctx) {
#ifndef STANDALONE_SERVER
	ModEngine* me = ModEngine::instance;
	if (!me) return 0;
	if (duk_get_string(ctx, 0) && duk_get_string(ctx, 0)[0]) {
		me->_uiScreens.erase(duk_safe_to_string(ctx, 0));
		me->log("UI.clear " + std::string(duk_safe_to_string(ctx, 0)));
	} else {
		me->_uiScreens.clear();
		me->clearHudButtons();
		me->log("UI.clear all");
	}
	return 0;
#else
	(void)ctx; return 0;
#endif
}

// UI.skin("mainmenu.button.2", {tl,t,tr,l,c,r,bl,b,br}) — nine-slice skin.
// The 9 parts are zip pngs; corners keep their own pixel size (all four must
// be the same size), edges/centre stretch to fill the element rect.
static const char* kSkinKeys[ModEngine::kSkinParts] = {
	"tl", "t", "tr", "l", "c", "r", "bl", "b", "br"
};
static duk_ret_t jsUiSkin(duk_context* ctx) {
#ifndef STANDALONE_SERVER
	ModEngine* me = ModEngine::instance;
	if (!me) return 0;
	std::string screen, elem;
	if (!splitUiKey(duk_safe_to_string(ctx, 0), screen, elem) || !duk_is_object(ctx, 1)) {
		me->log("UI.skin: usage UI.skin(\"<screen>.<element>\", {tl,t,tr,l,c,r,bl,b,br})");
		return 0;
	}
	ModEngine::UiElementOverride& o = me->_uiScreens[screen].elements[elem];
	int filled = 0;
	for (int i = 0; i < ModEngine::kSkinParts; ++i) {
		duk_get_prop_string(ctx, 1, kSkinKeys[i]);
		if (duk_is_string(ctx, -1)) {
			o.skin[i] = duk_safe_to_string(ctx, -1);
			++filled;
		} else {
			o.skin[i].clear();
		}
		duk_pop(ctx);
	}
	o.hasSkin = (filled > 0);
	me->log("UI.skin " + screen + "." + elem + " parts=" + std::to_string(filled));
	return 0;
#else
	(void)ctx; return 0;
#endif
}

static duk_ret_t jsLevelSpawnParticle(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	if (!me || !me->minecraft() || !me->minecraft()->level)
		return 0;
	std::string name = duk_safe_to_string(ctx, 0);
	float x = (float)duk_get_number(ctx, 1);
	float y = (float)duk_get_number(ctx, 2);
	float z = (float)duk_get_number(ctx, 3);
	float xd = duk_get_top(ctx) > 4 ? (float)duk_get_number(ctx, 4) : 0.0f;
	float yd = duk_get_top(ctx) > 5 ? (float)duk_get_number(ctx, 5) : 0.0f;
	float zd = duk_get_top(ctx) > 6 ? (float)duk_get_number(ctx, 6) : 0.0f;
	// JS-defined particles first, built-in particles as fallback.
	if (me->spawnScriptedParticle(name, x, y, z, xd, yd, zd))
		return 0;
	me->minecraft()->level->addParticle(name, x, y, z, xd, yd, zd);
	return 0;
}

// JS level.playSound(name, x, y, z[, volume, pitch]) - play a registered
// sound (step.grass, random.click, mob.creeper, ...) at a world position.
static duk_ret_t jsLevelPlaySound(duk_context* ctx) {
#ifndef STANDALONE_SERVER
	ModEngine* me = ModEngine::instance;
	if (!me || !me->minecraft() || !me->minecraft()->level)
		return 0;
	const char* name = duk_safe_to_string(ctx, 0);
	float x = (float)duk_get_number(ctx, 1);
	float y = (float)duk_get_number(ctx, 2);
	float z = (float)duk_get_number(ctx, 3);
	float volume = duk_get_top(ctx) > 4 ? (float)duk_get_number(ctx, 4) : 1.0f;
	float pitch  = duk_get_top(ctx) > 5 ? (float)duk_get_number(ctx, 5) : 1.0f;
	SoundEngine* se = me->minecraft()->soundEngine;
	if (se)
		se->play(name, x, y, z, volume, pitch);
	return 0;
#else
	(void)ctx; return 0;
#endif
}

// JS level.spawnItemEntity(x, y, z, itemId, count) - drop items on the
// ground as a pickable ItemEntity.
static duk_ret_t jsLevelSpawnItemEntity(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	if (!me || !me->minecraft() || !me->minecraft()->level)
		return 0;
	float x = (float)duk_get_number(ctx, 0);
	float y = (float)duk_get_number(ctx, 1);
	float z = (float)duk_get_number(ctx, 2);
	int id = duk_get_int(ctx, 3);
	int count = duk_get_top(ctx) > 4 ? duk_get_int(ctx, 4) : 1;
	if (count < 1) count = 1;
	Level* level = me->minecraft()->level;
	if (id < 0 || id >= Item::MAX_ITEMS || !Item::items[id])
		return 0;
	ItemEntity* e = new ItemEntity(level, x, y, z, ItemInstance(id, count, 0));
	level->addEntity(e);
	// Return the entity id so JS can control it (e.g. gravitite ore floats
	// upward via mob.setPos).
	duk_push_int(ctx, e->entityId);
	return 1;
}

// JS level.playMusic(name, volume) - play a non-positional (global) sound,
// e.g. background music from the mod's sounds/*.wav.
static duk_ret_t jsLevelPlayMusic(duk_context* ctx) {
#ifndef STANDALONE_SERVER
	ModEngine* me = ModEngine::instance;
	if (!me || !me->minecraft())
		return 0;
	const char* name = duk_safe_to_string(ctx, 0);
	float volume = duk_get_top(ctx) > 1 ? (float)duk_get_number(ctx, 1) : 1.0f;
	SoundEngine* se = me->minecraft()->soundEngine;
	if (se)
		se->playUI(name, volume, 1.0f);
	return 0;
#else
	(void)ctx; return 0;
#endif
}

// JS level.stopMusic() - stop every currently playing sound source (used by
// mods when leaving the Aether so the track doesn't keep playing in the
// overworld; a playing OpenAL source can only be stopped from C++).
static duk_ret_t jsLevelStopMusic(duk_context* ctx) {
#ifndef STANDALONE_SERVER
	ModEngine* me = ModEngine::instance;
	Minecraft* mc = me ? me->minecraft() : NULL;
	if (mc && mc->soundEngine)
		mc->soundEngine->stopAllSounds();
	return 0;
#else
	(void)ctx; return 0;
#endif
}

// JS level.spawnVanilla(typeName, x, y, z) - spawn a vanilla mob.
// typeName: zombie/creeper/skeleton/pigzombie/cow/pig/sheep/chicken/wolf.
static duk_ret_t jsLevelSpawnVanilla(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	Minecraft* mc = me ? me->minecraft() : NULL;
	if (!mc || !mc->level) { duk_push_int(ctx, -1); return 1; }
	float x = (float)duk_get_number(ctx, 1);
	float y = (float)duk_get_number(ctx, 2);
	float z = (float)duk_get_number(ctx, 3);
	Mob* mob = NULL;
	std::string type = duk_safe_to_string(ctx, 0);
	if (type == "zombie")     mob = new Zombie(mc->level);
	else if (type == "creeper")    mob = new Creeper(mc->level);
	else if (type == "skeleton")   mob = new Skeleton(mc->level);
	else if (type == "pigzombie")  mob = new PigZombie(mc->level);
	else if (type == "cow")   mob = new Cow(mc->level);
	else if (type == "pig")   mob = new Pig(mc->level);
	else if (type == "sheep") mob = new Sheep(mc->level);
	else if (type == "chicken") mob = new Chicken(mc->level);
	if (mob && MobSpawner::addMob(mc->level, mob, x, y, z, 0, 0, true)) {
		me->log("spawned vanilla " + type + " at " + std::to_string((int)x) + "," + std::to_string((int)y) + "," + std::to_string((int)z));
		duk_push_int(ctx, mob->entityId);
		return 1;
	}
	if (mob) delete mob;
	duk_push_int(ctx, -1);
	return 1;
}

// level.spawnPainting(x, y, z, dir [, motive]) -> 实体 id（放不下时 -1）
//   dir: 0=南 1=西 2=北 3=东（与 Direction 一致；画贴在这一格的那一面上）
//   motive: 可选，指定画面内容（Motive 名）；不写就按空间随机挑一个放得下的。
// 位置就是整数格坐标（和原版挂画一样贴着格面），不需要小数。
static duk_ret_t jsLevelSpawnPainting(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	Minecraft* mc = me ? me->minecraft() : NULL;
	if (!mc || !mc->level) {
		duk_push_int(ctx, -1);
		return 1;
	}
	Level* level = mc->level;
	int x = (int)duk_get_int(ctx, 0);
	int y = (int)duk_get_int(ctx, 1);
	int z = (int)duk_get_int(ctx, 2);
	int dir = (int)duk_get_int(ctx, 3);
	Painting* p = NULL;
	if (duk_is_string(ctx, 4))
		p = new Painting(level, x, y, z, dir, duk_safe_to_string(ctx, 4));
	else
		p = new Painting(level, x, y, z, dir);
	if (!p->survives()) {          // 空间不够 / 贴不住
		delete p;
		duk_push_int(ctx, -1);
		return 1;
	}
	level->addEntity(p);
	duk_push_int(ctx, p->entityId);
	return 1;
}

static Entity* findEntityById(int id) {	ModEngine* me = ModEngine::instance;
	Minecraft* mc = me ? me->minecraft() : NULL;
	if (!mc || !mc->level)
		return NULL;
	for (EntityList::iterator it = mc->level->entities.begin(); it != mc->level->entities.end(); ++it) {
		Entity* e = *it;
		if (e && !e->removed && e->entityId == id)
			return e;
	}
	return NULL;
}

static duk_ret_t jsLevelSpawnMob(duk_context* ctx) {
	int typeId = duk_get_int(ctx, 0);
	float x = (float)duk_get_number(ctx, 1);
	float y = (float)duk_get_number(ctx, 2);
	float z = (float)duk_get_number(ctx, 3);
	Mob* mob = NULL;
	if (ModEngine::instance)
		mob = ModEngine::instance->spawnScriptedMob(typeId, x, y, z);
	duk_push_int(ctx, mob ? mob->entityId : -1);
	return 1;
}

// --- JS entity-handle API (mob.*) ------------------------------------
static duk_ret_t jsMobSetPos(duk_context* ctx) {
	int id = (int)duk_get_int(ctx, 0);
	float x = (float)duk_get_number(ctx, 1);
	float y = (float)duk_get_number(ctx, 2);
	float z = (float)duk_get_number(ctx, 3);
	Entity* e = findEntityById(id);
	if (e) e->moveTo(x, y, z, e->yRot, e->xRot);
	return 0;
}

// JS mob.setVelocity(entityId, xd, yd, zd) - set an entity's velocity.
// The engine physics (Mob::travel -> Entity::move) then moves it smoothly
// (render interpolation = no teleport jitter). Used by the gravitite ore
// which floats upward like the player's cloud bounce.
static duk_ret_t jsMobSetVelocity(duk_context* ctx) {
	int id = (int)duk_get_int(ctx, 0);
	float xd = (float)duk_get_number(ctx, 1);
	float yd = (float)duk_get_number(ctx, 2);
	float zd = (float)duk_get_number(ctx, 3);
	Entity* e = findEntityById(id);
	if (e) { e->xd = xd; e->yd = yd; e->zd = zd; }
	return 0;
}
static duk_ret_t jsMobGetPos(duk_context* ctx) {
	int id = (int)duk_get_int(ctx, 0);
	Entity* e = findEntityById(id);
	duk_push_array(ctx);
	if (e) {
		duk_push_number(ctx, e->x); duk_put_prop_index(ctx, -2, 0);
		duk_push_number(ctx, e->y); duk_put_prop_index(ctx, -2, 1);
		duk_push_number(ctx, e->z); duk_put_prop_index(ctx, -2, 2);
	}
	return 1;
}
static duk_ret_t jsMobGetHealth(duk_context* ctx) {
	int id = (int)duk_get_int(ctx, 0);
	Entity* e = findEntityById(id);
	Mob* m = dynamic_cast<Mob*>(e);
	if (m) { duk_push_int(ctx, m->health); return 1; }
	duk_push_int(ctx, -1);
	return 1;
}
static duk_ret_t jsMobGetMaxHealth(duk_context* ctx) {
	int id = (int)duk_get_int(ctx, 0);
	Entity* e = findEntityById(id);
	Mob* m = dynamic_cast<Mob*>(e);
	if (m) { duk_push_int(ctx, m->getMaxHealth()); return 1; }
	duk_push_int(ctx, -1);
	return 1;
}
static duk_ret_t jsMobSetHealth(duk_context* ctx) {
	// JS mob.setHealth(entityId, hp) - set any mob's health directly
	// (hp <= 0 makes the mob die through the normal tick path).
	int id = (int)duk_get_int(ctx, 0);
	int hp = (int)duk_get_int(ctx, 1);
	Entity* e = findEntityById(id);
	Mob* m = dynamic_cast<Mob*>(e);
	if (m) m->health = hp;
	return 0;
}
static duk_ret_t jsMobDamage(duk_context* ctx) {
	// JS mob.damage(entityId, dmg) - deal damage through the normal hurt
	// system (works on any Entity; Mob overrides hurt to apply real damage).
	int id = (int)duk_get_int(ctx, 0);
	int dmg = (int)duk_get_int(ctx, 1);
	Entity* e = findEntityById(id);
	if (e && dmg > 0) e->hurt(NULL, dmg);
	return 0;
}
static duk_ret_t jsMobGetNearby(duk_context* ctx) {
	// JS mob.getNearby(x, y, z, radius, maxCount) -> [{id,type,x,y,z,hp}]
	// List entities within radius of a point (AI perception).
	// type: 0=Player, 32..36=vanilla mobs, 40+=custom (ScriptedMob).
	float x = (float)duk_get_number(ctx, 0);
	float y = (float)duk_get_number(ctx, 1);
	float z = (float)duk_get_number(ctx, 2);
	float radius = (float)duk_get_number(ctx, 3);
	int maxCount = (int)duk_get_int(ctx, 4);
	if (maxCount <= 0) maxCount = 8;
	if (maxCount > 32) maxCount = 32;
	Minecraft* mc = MCMOD();
	duk_idx_t arr = duk_push_array(ctx);
	if (!mc || !mc->level)
		return 1;
	float r2 = radius * radius;
	int n = 0;
	for (EntityList::iterator it = mc->level->entities.begin(); it != mc->level->entities.end() && n < maxCount; ++it) {
		Entity* e = *it;
		if (!e || e->removed)
			continue;
		float dx = e->x - x, dy = e->y - y, dz = e->z - z;
		if (dx * dx + dy * dy + dz * dz > r2)
			continue;
		duk_idx_t obj = duk_push_object(ctx);
		duk_push_int(ctx, e->entityId);     duk_put_prop_string(ctx, obj, "id");
		duk_push_int(ctx, e->getEntityTypeId()); duk_put_prop_string(ctx, obj, "type");
		duk_push_number(ctx, e->x);         duk_put_prop_string(ctx, obj, "x");
		duk_push_number(ctx, e->y);         duk_put_prop_string(ctx, obj, "y");
		duk_push_number(ctx, e->z);         duk_put_prop_string(ctx, obj, "z");
		Mob* mob = dynamic_cast<Mob*>(e);
		duk_push_int(ctx, mob ? mob->health : -1); duk_put_prop_string(ctx, obj, "hp");
		duk_put_prop_index(ctx, arr, n++);
	}
	return 1;
}
static duk_ret_t jsMobRemove(duk_context* ctx) {
	int id = (int)duk_get_int(ctx, 0);
	Entity* e = findEntityById(id);
	if (e) e->remove();
	return 0;
}
static duk_ret_t jsMobSetRot(duk_context* ctx) {
	int id = (int)duk_get_int(ctx, 0);
	float yaw = (float)duk_get_number(ctx, 1);
	float pitch = (float)duk_get_number(ctx, 2);
	Entity* e = findEntityById(id);
	if (e) { e->yRot = yaw; e->xRot = pitch; }
	return 0;
}


// Log path: TEMP so it works regardless of the game's working directory.
#if defined(__ANDROID__)
static const char* kLogPath = "/sdcard/Android/data/com.reconnern.mcpe/files/mcpe_mod.log";
#else
static const char* kLogPath = "C:\\Users\\coj2\\AppData\\Local\\Temp\\mcpe_mod.log";
#if defined(_WIN32)
static LONG WINAPI ModCrashHandler(EXCEPTION_POINTERS* ep) {
	FILE* f = fopen(kLogPath, "a");
	if (f) {
		fprintf(f, "[mod] CRASH: code=0x%08X addr=%p module=exe\n",
			(unsigned)ep->ExceptionRecord->ExceptionCode,
			ep->ExceptionRecord->ExceptionAddress);
		fclose(f);
	}
	return EXCEPTION_CONTINUE_SEARCH;
}
static void ModTerminateHandler() {
	FILE* f = fopen(kLogPath, "a");
	if (f) { fprintf(f, "[mod] TERMINATE (uncaught C++ exception/abort)\n"); fclose(f); }
	abort();
}
static void ModPurecallHandler() {
	FILE* f = fopen(kLogPath, "a");
	if (f) {
		fprintf(f, "[mod] PURECALL from 0x%p\n", _ReturnAddress());
		fclose(f);
	}
	ExitProcess(3);
}
static void ModInvalidParamHandler(const wchar_t* expr, const wchar_t* func, const wchar_t* file, unsigned line, uintptr_t) {
	FILE* f = fopen(kLogPath, "a");
	if (f) {
		fprintf(f, "[mod] INVALID_PARAM file=0x%p line=%u\n", (void*)file, line);
		fclose(f);
	}
	// MSVC CRT docs: the invalid-parameter handler must terminate the
	// process; returning continues with undefined behaviour.
	ExitProcess(3);
}
static void ModSignalHandler(int sig) {
	FILE* f = fopen(kLogPath, "a");
	if (f) {
		unsigned long base = (unsigned long)GetModuleHandle(NULL);
		fprintf(f, "[mod] SIGNAL %d  exeBase=%08lX\n", sig, base);
		if (ModEngine::instance)
			fprintf(f, "[mod]   lastEvent=%s mod=%s\n",
				ModEngine::instance->lastEventName.c_str(),
				ModEngine::instance->lastEventMod.c_str());
		void* frames[64];
		unsigned short n = CaptureStackBackTrace(0, 64, frames, NULL);
		for (int i = 0; i < (int)n; i++) {
			unsigned long a = (unsigned long)frames[i];
			if (a >= base && a < base + 0x400000)
				fprintf(f, "[mod]   st[%d]=%08lX rva=%08lX\n", i, a, a - base);
			else
				fprintf(f, "[mod]   st[%d]=%08lX (sys)\n", i, a);
		}
		fclose(f);
	}
	exit(1);
}
#endif

#endif

// Full path to the mods directory, anchored to the executable's folder so
// mods are found regardless of the working directory the game was started
// from (double-click vs. shortcut vs. debugger).
// Recursively create a directory (and parents). Android-only helper.
#if defined(__ANDROID__)
static void mkdirs(const std::string& path) {
	size_t pos = 0;
	while ((pos = path.find('/', pos + 1)) != std::string::npos) {
		std::string sub = path.substr(0, pos);
		if (!sub.empty())
			mkdir(sub.c_str(), 0777);
	}
	mkdir(path.c_str(), 0777);
}
#endif

static std::string fullModsDir() {
#if defined(__ANDROID__)
	// App-external files dir (visible in file managers, no permission needed).
	static const std::string kAndroidModsDir = "/sdcard/Android/data/com.reconnern.mcpe/files/mods";
	mkdirs(kAndroidModsDir);
	return kAndroidModsDir;
#elif defined(_WIN32)
	char buf[MAX_PATH];
	GetModuleFileNameA(NULL, buf, sizeof(buf));
	std::string p(buf);
	size_t slash = p.find_last_of("\\/");
	std::string dir = (slash == std::string::npos) ? "." : p.substr(0, slash);
	std::string result = dir + "\\mods";
	// TEMP diag: log the resolved path once.
	static bool logged = false;
	if (!logged) {
		logged = true;
		ModEngine* me = ModEngine::instance;
		if (me) me->log(std::string("fullModsDir resolved: exe=") + p + " mods=" + result);
	}
	return result;
#else
	return "mods";
#endif
}

// mods 目录下某个文件的完整路径。
// 注意分隔符：Android 是 Linux，必须用 '/'。这些读取/写入模组文件的地方原来
// 硬编码了 '\\'（Win 版遗留），于是 Android 上 fopen 拿到的是
// ".../mods\taczgun.zip"——一个合法但根本不存在的文件名，结果所有模组 zip 都
// 打不开、loadEnabledMods 加载数为 0。
static std::string modFilePath(const std::string& file) {
	return fullModsDir() + "/" + file;
}

// 定义在本文件后半部分；这里前置声明，供 installModFile 做文件名校验。
static bool validModFileName(const std::string& file);

// ---------------------------------------------------------------------------
// JS global bindings
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------

// JS: modLog("text") -> append to the mod log file (also used by native errors).
static duk_ret_t jsModLog(duk_context* ctx) {
	const char* msg = duk_safe_to_string(ctx, 0);
	ModEngine::instance ? ModEngine::instance->modLog(msg) : (void)0;
	return 0;
}

// JS readFile(path) -> file content as string ("" if missing/unreadable).
// JS writeFile(path, content) -> true on success (AI bridge / persistence).
// NOTE: unrestricted path access - use with care (mods are trusted here).
static duk_ret_t jsReadFile(duk_context* ctx) {
	const char* path = duk_get_string(ctx, 0);
	if (!path || !path[0]) { duk_push_string(ctx, ""); return 1; }
	FILE* f = fopen(path, "rb");
	if (!f) { duk_push_string(ctx, ""); return 1; }
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	std::string buf;
	if (sz > 0) {
		buf.resize((size_t)sz);
		if (fread(&buf[0], 1, (size_t)sz, f) != (size_t)sz) buf.clear();
	}
	fclose(f);
	duk_push_lstring(ctx, buf.data(), (duk_size_t)buf.size());
	return 1;
}
static duk_ret_t jsWriteFile(duk_context* ctx) {
	const char* path = duk_get_string(ctx, 0);
	const char* data = duk_get_string(ctx, 1);
	if (!path || !path[0]) { duk_push_boolean(ctx, 0); return 1; }
	FILE* f = fopen(path, "wb");
	if (!f) { duk_push_boolean(ctx, 0); return 1; }
	if (data) fwrite(data, 1, strlen(data), f);
	fclose(f);
	duk_push_boolean(ctx, 1);
	return 1;
}

// JS: modRequire(name) -> load and run another script from the mods dir
// (multi-file mods). Loads each file at most once.
static std::map<std::string, bool> s_loadedModules;
static duk_ret_t jsModRequire(duk_context* ctx) {
	const char* name = duk_safe_to_string(ctx, 0);
	if (!name || !name[0]) {
		duk_push_string(ctx, "modRequire: empty name");
		return duk_throw(ctx);
	}
	std::string path = fullModsDir() + "/";
	path += name;
	if (path.find(".js") == std::string::npos)
		path += ".js";
	if (s_loadedModules[path])
		return 0;  // already loaded

	FILE* f = fopen(path.c_str(), "rb");
	if (!f) {
		duk_push_error_object(ctx, DUK_ERR_ERROR, "modRequire: %s not found", name);
		return duk_throw(ctx);
	}
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	std::vector<char> buf((size_t)(sz < 0 ? 0 : sz) + 1, 0);
	if (sz > 0) fread(buf.data(), 1, (size_t)sz, f);
	fclose(f);

	if (duk_peval_string(ctx, buf.data()) != 0) {
		// Propagate the error to the caller.
		return duk_throw(ctx);
	}
	duk_pop(ctx);
	s_loadedModules[path] = true;
	return 0;
}

// ---------------------------------------------------------------------------
// Metadata + modlist helpers
// ---------------------------------------------------------------------------

static std::string trim(const std::string& s) {
	size_t a = s.find_first_not_of(" \t\r\n");
	if (a == std::string::npos) return "";
	size_t b = s.find_last_not_of(" \t\r\n");
	return s.substr(a, b - a + 1);
}

// Parse "// key: value" header lines from a script's leading comment block.
static std::string parseMeta(const std::string& js, const std::string& key) {
	const std::string tag = "// " + key + ":";
	size_t pos = 0;
	while (pos < js.size()) {
		size_t eol = js.find('\n', pos);
		if (eol == std::string::npos) eol = js.size();
		std::string line = js.substr(pos, eol - pos);
		// Only look at the leading comment block (before first code).
		size_t firstCode = js.find_first_not_of(" \t\r\n");
		if (firstCode != std::string::npos && line.find("//") != 0 && pos > firstCode)
			break;
		size_t t = line.find(tag);
		if (t != std::string::npos)
			return trim(line.substr(t + tag.size()));
		pos = eol + 1;
	}
	return "";
}

// Load the set of enabled file names from mods/modlist.json.
// Format: { "enabled": [ "a.js", "b.js" ] }
static std::vector<std::string> loadEnabledList() {
	std::vector<std::string> out;
	FILE* f = fopen((fullModsDir() + "/modlist.json").c_str(), "rb");
	if (!f) return out;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz <= 0 || sz > 1024 * 1024) { fclose(f); return out; }
	std::vector<char> buf((size_t)sz + 1, 0);
	fread(buf.data(), 1, (size_t)sz, f);
	fclose(f);
	std::string s(buf.data());
	// Very small JSON subset parser: find "enabled" then all "..." tokens.
	size_t p = s.find("enabled");
	if (p == std::string::npos) return out;
	p = s.find('[', p);
	if (p == std::string::npos) return out;
	size_t q = s.find(']', p);
	if (q == std::string::npos) return out;
	std::string arr = s.substr(p + 1, q - p - 1);
	size_t i = 0;
	while (i < arr.size()) {
		size_t a = arr.find('"', i);
		if (a == std::string::npos) break;
		size_t b = arr.find('"', a + 1);
		if (b == std::string::npos) break;
		out.push_back(arr.substr(a + 1, b - a - 1));
		i = b + 1;
	}
	return out;
}

static void saveEnabledList(const std::vector<std::string>& enabled) {
	FILE* f = fopen((fullModsDir() + "/modlist.json").c_str(), "wb");
	if (!f) return;
	fprintf(f, "{\n  \"enabled\": [\n");
	for (size_t i = 0; i < enabled.size(); ++i) {
		fprintf(f, "    \"%s\"%s\n", enabled[i].c_str(),
			(i + 1 < enabled.size()) ? "," : "");
	}
	fprintf(f, "  ]\n}\n");
	fclose(f);
}

// ---------------------------------------------------------------------------
// ModEngine
// ---------------------------------------------------------------------------

ModEngine* ModEngine::instance = NULL;
bool ModEngine::s_playerSkinIs64 = false;

static FILE* s_modLogFile = NULL;
static void closeModLogFile();  // defined with log() below

ModEngine::ModEngine(Minecraft* mc)
:	_minecraft(mc), _eventPlayer(NULL), _ctx(NULL), _currentDim(0), _pendingScreen(NULL),
	_skyR(-1.0f), _skyG(-1.0f), _skyB(-1.0f), _worldOpsValid(false),
	_modStateLoaded(false), _modStateDirty(false), _modStateTicksSinceWrite(0), _tickCount(0),
	_hudLayoutVersion(0),
	_hudMouseHeld(0)
{
	_logPath = kLogPath;

	// Keep the log file bounded: it is append-only across runs and had grown
	// to several MB. At startup, if the previous run's log is large, move it
	// aside (keeping one generation for crash post-mortems) so the new
	// session starts clean.
#if !defined(__ANDROID__)
	{
		FILE* probe = fopen(kLogPath, "rb");
		if (probe) {
			fseek(probe, 0, SEEK_END);
			long sz = ftell(probe);
			fclose(probe);
			if (sz > 8 * 1024 * 1024) {
				remove("C:\\Users\\coj2\\AppData\\Local\\Temp\\mcpe_mod.old.log");
				rename(kLogPath, "C:\\Users\\coj2\\AppData\\Local\\Temp\\mcpe_mod.old.log");
			}
		}
	}
#endif
}

// Duktape fatal error handler (one per heap): log the reason before abort.
static void modDukFatal(void* udata, const char* msg) {
	FILE* f = fopen(kLogPath, "a");
	if (f) {
		fprintf(f, "[mod] DUK_FATAL heap=%p: %s\n", udata, msg ? msg : "?");
		if (ModEngine::instance)
			fprintf(f, "[mod]   lastEvent=%s mod=%s\n",
				ModEngine::instance->lastEventName.c_str(),
				ModEngine::instance->lastEventMod.c_str());
		fclose(f);
	}
	abort();
}

ModEngine::~ModEngine() {
	for (size_t i = 0; i < _sandboxes.size(); ++i)
		if (_sandboxes[i].ctx)
			duk_destroy_heap(_sandboxes[i].ctx);
	_sandboxes.clear();
	// Free scripted mob heap objects (boxes/model are owned, new'd in defineMob).
	for (std::map<int, ScriptedMobDef>::iterator it = _scriptedMobs.begin(); it != _scriptedMobs.end(); ++it) {
		delete it->second.boxes;
		delete it->second.model;
	}
	_scriptedMobs.clear();
	if (_ctx) {
		duk_destroy_heap((duk_context*)_ctx);
		_ctx = NULL;
	}
	closeModLogFile();
	if (instance == this)
		instance = NULL;
}

bool ModEngine::init() {
#if defined(_WIN32)
	SetUnhandledExceptionFilter(ModCrashHandler);
	_set_purecall_handler(ModPurecallHandler);
	_set_invalid_parameter_handler(ModInvalidParamHandler);
	std::set_terminate(ModTerminateHandler);
	signal(SIGABRT, ModSignalHandler);
	signal(SIGSEGV, ModSignalHandler);
	signal(SIGILL, ModSignalHandler);
#endif
	duk_context* ctx = duk_create_heap(NULL, NULL, NULL, NULL, modDukFatal);
	if (!ctx)
		return false;
	_ctx = ctx;
	instance = this;

	registerBindings(ctx);
	loadConfig();
	log("ModEngine initialized");
	return true;
}

static duk_ret_t jsAssetsReplaceImage(duk_context* ctx);
static duk_ret_t jsAssetsReplaceBlockIcon(duk_context* ctx);
static duk_ret_t jsCommandsRegister(duk_context* ctx);

// ---------------------------------------------------------------------------
// 服务器命令（mod 注册）
//
// JS 侧：Commands.register("heal", function(sender, args) { ...; return "回了你一口血"; });
// 服务器收到 "/heal Steve" 时：查 _commandNames → 取出 handler → 调用
//   handler(senderName, argsString)
// 返回字符串就作为回复发给执行者；抛异常则把错误文本回给他（并记日志）。
// 也就是说“命令”完全由服务器 mod 提供，客户端不再本地执行
// （见 ChatInputScreen::submit）。
// ---------------------------------------------------------------------------
void ModEngine::registerCommandName(const std::string& name)
{
	if (!name.empty())
		_commandNames.insert(name);
}

bool ModEngine::isRegisteredCommand(const std::string& name) const
{
	return _commandNames.find(name) != _commandNames.end();
}

// 在已经找到 handler 的那个堆里调用它。
// 进入时栈是 [..., global, handler]；duk_pcall(nargs) 会把 handler 和 nargs 个参数
// 一起吃掉、只压回一个返回值，所以返回时栈是 [..., global]（这里只弹 1 个）。
static void invokeRegisteredHandler(ModEngine* me, duk_context* ctx, const std::string& name,
                                    const std::string& args, const std::string& senderName,
                                    std::string& reply)
{
	duk_push_string(ctx, senderName.c_str());
	duk_push_string(ctx, args.c_str());
	if (duk_pcall(ctx, 2) != 0) {
		std::string err = duk_safe_to_string(ctx, -1);
		me->log("[cmd] /" + name + " threw: " + err);
		reply = "Command error: " + err;
	} else if (duk_is_string(ctx, -1)) {
		const char* str = duk_safe_to_string(ctx, -1);
		if (str)
			reply = str;
	}
	duk_pop(ctx);   // 只弹返回值：handler 和两个参数在 pcall 时就被吃掉了
}

bool ModEngine::runRegisteredCommand(const std::string& line, const std::string& senderName, std::string& reply)
{
	reply.clear();
	if (!_ctx)
		return false;

	std::string body = line;
	if (!body.empty() && body[0] == '/')
		body = body.substr(1);
	while (!body.empty() && (body[body.size() - 1] == ' ' || body[body.size() - 1] == '\r' || body[body.size() - 1] == '\n'))
		body.erase(body.size() - 1);

	std::string name = body, args;
	size_t sp = body.find(' ');
	if (sp != std::string::npos) {
		name = body.substr(0, sp);
		args = body.substr(sp + 1);
	}
	if (name.empty() || !isRegisteredCommand(name))
		return false;

	// 每个模组一个独立 Duktape 堆（loadEnabledMods 里给每个 mod duk_create_heap），
	// 而 Commands.register 是在**那个模组自己的堆里**执行的 —— handler 存进的是该堆
	// 全局对象的 __cmd_<name>。所以这里必须像 fireEvent 一样逐个堆去找；
	// 只看主堆的话，模组注册的指令永远取不到 handler（等于从来没生效过）。
	// 顺序也与 fireEvent 保持一致：先各模组堆，再回落主堆（单模组/内置脚本情形）。
	// Duktape 不是线程安全的，进 JS 的入口都在这把递归锁下串行化（同 fireEvent）。
	std::lock_guard<std::recursive_mutex> _jsLock(_jsMutex);
	std::string key = "__cmd_" + name;

	// 1) 各模组自己的堆
	for (size_t s = 0; s < _sandboxes.size(); ++s) {
		duk_context* sctx = _sandboxes[s].ctx;
		if (!sctx)
			continue;
		duk_push_global_object(sctx);
		duk_get_prop_string(sctx, -1, key.c_str());
		if (!duk_is_function(sctx, -1)) {
			duk_pop_2(sctx);   // 值 + 全局对象
			continue;
		}
		invokeRegisteredHandler(this, sctx, name, args, senderName, reply);
		duk_pop(sctx);         // 全局对象
		return true;
	}

	// 2) 回落：主堆的全局（没有 sandbox 的情形）
	duk_context* ctx = (duk_context*)_ctx;
	duk_push_global_object(ctx);
	duk_get_prop_string(ctx, -1, key.c_str());
	if (!duk_is_function(ctx, -1)) {
		duk_pop_2(ctx);
		return false;
	}
	invokeRegisteredHandler(this, ctx, name, args, senderName, reply);
	duk_pop(ctx);              // 全局对象
	return true;
}

static duk_ret_t jsCommandsRegister(duk_context* ctx)
{
	ModEngine* me = ModEngine::instance;
	if (!me)
		return 0;

	std::string name = duk_safe_to_string(ctx, 0);
	if (name.empty() || !duk_is_function(ctx, 1))
		return 0;
	if (name[0] == '/')
		name = name.substr(1);
	if (name.empty())
		return 0;

	// 处理函数挂到全局 __cmd_<name>，执行时再取出来调用（Duktape 里把 JS 函数
	// “留到以后用”最简单的办法，不用手工管 stash 引用计数）。
	duk_push_global_object(ctx);
	duk_dup(ctx, 1);
	duk_put_prop_string(ctx, -2, ("__cmd_" + name).c_str());
	duk_pop(ctx);

	// 可选第三参数：指令简述（聊天输入框的补全列表右侧显示）。老模组只传
	// 两个参数，这里就是空串，行为与以前完全一致。
	if (duk_get_top(ctx) >= 3 && duk_is_string(ctx, 2)) {
		const char* d = duk_safe_to_string(ctx, 2);
		if (d && d[0])
			me->_commandDescs[name] = d;
	}

	me->registerCommandName(name);
	me->log("[cmd] registered /" + name);
	return 0;
}

void ModEngine::registerBindings(duk_context* ctx) {
	if (!ctx)
		ctx = (duk_context*)_ctx;
	if (!ctx)
		return;

	duk_push_global_object(ctx);
	duk_push_c_function(ctx, jsModLog, 1);
	duk_put_prop_string(ctx, -2, "modLog");
	duk_pop(ctx);

	duk_push_global_object(ctx);
	duk_push_c_function(ctx, jsReadFile, 1);
	duk_put_prop_string(ctx, -2, "readFile");
	duk_push_c_function(ctx, jsWriteFile, 2);
	duk_put_prop_string(ctx, -2, "writeFile");
	duk_pop(ctx);

	duk_push_global_object(ctx);
	duk_push_c_function(ctx, jsModRequire, 1);
	duk_put_prop_string(ctx, -2, "modRequire");
	duk_pop(ctx);

	// Global `player` object
	duk_push_object(ctx);
	duk_push_c_function(ctx, jsPlayerGetX, 0);        duk_put_prop_string(ctx, -2, "getX");
	duk_push_c_function(ctx, jsPlayerGetY, 0);        duk_put_prop_string(ctx, -2, "getY");
	duk_push_c_function(ctx, jsPlayerGetZ, 0);        duk_put_prop_string(ctx, -2, "getZ");
	duk_push_c_function(ctx, jsPlayerGetYRot, 0);     duk_put_prop_string(ctx, -2, "getYRot");
	duk_push_c_function(ctx, jsPlayerGetXRot, 0);     duk_put_prop_string(ctx, -2, "getXRot");
	duk_push_c_function(ctx, jsPlayerSetPos, 3);      duk_put_prop_string(ctx, -2, "setPos");
	duk_push_c_function(ctx, jsPlayerGetHealth, 0);   duk_put_prop_string(ctx, -2, "getHealth");
	duk_push_c_function(ctx, jsPlayerSetHealth, 1);   duk_put_prop_string(ctx, -2, "setHealth");
	duk_push_c_function(ctx, jsPlayerDamage, 1);      duk_put_prop_string(ctx, -2, "damage");
	duk_push_c_function(ctx, jsPlayerAddItem, 2);     duk_put_prop_string(ctx, -2, "addItem");
	duk_push_c_function(ctx, jsPlayerCountItem, 1);   duk_put_prop_string(ctx, -2, "countItem");
	duk_push_c_function(ctx, jsPlayerRemoveItem, 2);  duk_put_prop_string(ctx, -2, "removeItem");
	duk_push_c_function(ctx, jsPlayerGetHeldItemDamage, 0); duk_put_prop_string(ctx, -2, "getHeldItemDamage");
	duk_push_c_function(ctx, jsPlayerGetHeldItemMaxDamage, 0); duk_put_prop_string(ctx, -2, "getHeldItemMaxDamage");
	duk_push_c_function(ctx, jsPlayerGetHeldItemId, 0); duk_put_prop_string(ctx, -2, "getHeldItemId");
	duk_push_c_function(ctx, jsPlayerSetHeldItemDamage, 1); duk_put_prop_string(ctx, -2, "setHeldItemDamage");
	duk_push_c_function(ctx, jsPlayerIsUseHeld, 0);    duk_put_prop_string(ctx, -2, "isUseHeld");
	duk_push_c_function(ctx, jsPlayerSetItemPose, 1);  duk_put_prop_string(ctx, -2, "setItemPose");
	duk_push_c_function(ctx, jsPlayerSetZoom, 1);      duk_put_prop_string(ctx, -2, "setZoom");
	duk_push_c_function(ctx, jsPlayerSendMessage, 1); duk_put_prop_string(ctx, -2, "sendMessage");
	duk_push_c_function(ctx, jsPlayerTeleport, 3);     duk_put_prop_string(ctx, -2, "teleport");
	duk_push_c_function(ctx, jsPlayerIsCreative, 0);   duk_put_prop_string(ctx, -2, "isCreative");
	duk_push_c_function(ctx, jsLevelSetGameMode, 1);   duk_put_prop_string(ctx, -2, "setGameMode");	duk_push_c_function(ctx, jsPlayerIsForwardHeld, 0); duk_put_prop_string(ctx, -2, "isForwardHeld");
	duk_push_c_function(ctx, jsPlayerSetSprinting, 1);  duk_put_prop_string(ctx, -2, "setSprinting");
	duk_push_c_function(ctx, jsPlayerSetVelocity, 3); duk_put_prop_string(ctx, -2, "setVelocity");
	duk_push_c_function(ctx, jsPlayerSit, 3);          duk_put_prop_string(ctx, -2, "sit");
	duk_push_c_function(ctx, jsPlayerStand, 0);        duk_put_prop_string(ctx, -2, "stand");
	duk_push_c_function(ctx, jsPlayerIsSitting, 0);    duk_put_prop_string(ctx, -2, "isSitting");
	duk_push_c_function(ctx, jsPlayerGetId, 0);        duk_put_prop_string(ctx, -2, "getId");
	duk_put_global_string(ctx, "player");
	// Global `mouse` 对象：mouse.take(button, take) —— 接管鼠标键（见 jsMouseTake）
	duk_push_object(ctx);
	duk_push_c_function(ctx, jsMouseTake, 2);          duk_put_prop_string(ctx, -2, "take");
	duk_put_global_string(ctx, "mouse");
	// Global `Player` object（玩家查询 + 动作；按 id 寻址，和 mob 那套对称）
	duk_push_object(ctx);
	duk_push_c_function(ctx, jsPlayerSelf, 0);         duk_put_prop_string(ctx, -2, "self");
	duk_push_c_function(ctx, jsPlayerList, 0);         duk_put_prop_string(ctx, -2, "list");
	duk_push_c_function(ctx, jsPlayerGetById, 1);      duk_put_prop_string(ctx, -2, "get");
	duk_push_c_function(ctx, jsPlayerDefineAction, 2); duk_put_prop_string(ctx, -2, "defineAction");
	duk_push_c_function(ctx, jsPlayerSetAction, 2);    duk_put_prop_string(ctx, -2, "setAction");
	duk_push_c_function(ctx, jsPlayerGetAction, 1);    duk_put_prop_string(ctx, -2, "getAction");
	duk_push_c_function(ctx, jsPlayerSetModel, 2);     duk_put_prop_string(ctx, -2, "setModel");
	duk_push_c_function(ctx, jsPlayerGetModel, 1);     duk_put_prop_string(ctx, -2, "getModel");
	// 座位：按 id 坐/起（player.sit 只能操作本机玩家，联机时不够用）
	duk_push_c_function(ctx, jsPlayerSitById, 4);      duk_put_prop_string(ctx, -2, "sit");
	duk_push_c_function(ctx, jsPlayerStandById, 1);    duk_put_prop_string(ctx, -2, "stand");
	duk_put_global_string(ctx, "Player");

	// Global `mob` object (entity-handle API for JS-driven behavior)
	duk_push_object(ctx);
	duk_push_c_function(ctx, jsMobSetPos, 4);          duk_put_prop_string(ctx, -2, "setPos");
	duk_push_c_function(ctx, jsMobSetVelocity, 4);     duk_put_prop_string(ctx, -2, "setVelocity");
	duk_push_c_function(ctx, jsMobGetPos, 1);          duk_put_prop_string(ctx, -2, "getPos");
	duk_push_c_function(ctx, jsMobGetHealth, 1);       duk_put_prop_string(ctx, -2, "getHealth");
	duk_push_c_function(ctx, jsMobGetMaxHealth, 1);    duk_put_prop_string(ctx, -2, "getMaxHealth");
	duk_push_c_function(ctx, jsMobSetHealth, 2);       duk_put_prop_string(ctx, -2, "setHealth");
	duk_push_c_function(ctx, jsMobDamage, 2);          duk_put_prop_string(ctx, -2, "damage");
	duk_push_c_function(ctx, jsMobGetNearby, 5);       duk_put_prop_string(ctx, -2, "getNearby");
	duk_push_c_function(ctx, jsMobRemove, 1);          duk_put_prop_string(ctx, -2, "remove");
	duk_push_c_function(ctx, jsMobSetRot, 3);          duk_put_prop_string(ctx, -2, "setRot");
	duk_put_global_string(ctx, "mob");

	// Global `level` object
	duk_push_object(ctx);
	duk_push_c_function(ctx, jsLevelGetBlock, 3);     duk_put_prop_string(ctx, -2, "getBlock");
	duk_push_c_function(ctx, jsLevelGetData, 3);      duk_put_prop_string(ctx, -2, "getData");
	duk_push_c_function(ctx, jsLevelSetBlock, 5);     duk_put_prop_string(ctx, -2, "setBlock");
	duk_push_c_function(ctx, jsLevelGetTime, 0);      duk_put_prop_string(ctx, -2, "getTime");
	duk_push_c_function(ctx, jsLevelGetTicks, 0);     duk_put_prop_string(ctx, -2, "getTicks");
	duk_push_c_function(ctx, jsLevelGetDifficulty, 0); duk_put_prop_string(ctx, -2, "getDifficulty");
	duk_push_c_function(ctx, jsLevelSetTime, 1);      duk_put_prop_string(ctx, -2, "setTime");
	duk_push_c_function(ctx, jsLevelSpawnMob, 4);     duk_put_prop_string(ctx, -2, "spawnMob");
	duk_push_c_function(ctx, jsLevelSpawnVanilla, 4);  duk_put_prop_string(ctx, -2, "spawnVanilla");
	duk_push_c_function(ctx, jsLevelSpawnPainting, 5); duk_put_prop_string(ctx, -2, "spawnPainting");
	duk_push_c_function(ctx, jsLevelSpawnParticle, 7); duk_put_prop_string(ctx, -2, "spawnParticle");
	duk_push_c_function(ctx, jsLevelPlaySound, 6);    duk_put_prop_string(ctx, -2, "playSound");
	duk_push_c_function(ctx, jsLevelSpawnProjectile, 7);  duk_put_prop_string(ctx, -2, "spawnProjectile");
	duk_push_c_function(ctx, jsLevelSpawnItemEntity, 5); duk_put_prop_string(ctx, -2, "spawnItemEntity");
	duk_push_c_function(ctx, jsLevelPlayMusic, 2);    duk_put_prop_string(ctx, -2, "playMusic");
	duk_push_c_function(ctx, jsLevelStopMusic, 0);    duk_put_prop_string(ctx, -2, "stopMusic");
	duk_push_c_function(ctx, jsLevelSaveAll, 0);       duk_put_prop_string(ctx, -2, "saveAll");
	duk_push_c_function(ctx, jsLevelResetChunks, 1);   duk_put_prop_string(ctx, -2, "resetChunks");
	duk_push_c_function(ctx, jsLevelIsHost, 0);        duk_put_prop_string(ctx, -2, "isHost");
	duk_push_c_function(ctx, jsLevelListPlayers, 0);   duk_put_prop_string(ctx, -2, "listPlayers");
	duk_push_c_function(ctx, jsLevelChangePlayerDimension, 5); duk_put_prop_string(ctx, -2, "changePlayerDimension");
	duk_push_c_function(ctx, jsLevelGetCurrentDimension, 0); duk_put_prop_string(ctx, -2, "getCurrentDimension");
	duk_push_c_function(ctx, jsLevelGetBiome, 2);            duk_put_prop_string(ctx, -2, "getBiome");
	duk_push_c_function(ctx, jsLevelFindBiome, 4);           duk_put_prop_string(ctx, -2, "findBiome");
	duk_push_c_function(ctx, jsLevelSaveDimensionState, 0);  duk_put_prop_string(ctx, -2, "saveDimensionState");
	duk_push_c_function(ctx, jsLevelSetModState, 2);         duk_put_prop_string(ctx, -2, "setModState");
	duk_push_c_function(ctx, jsLevelGetModState, 1);         duk_put_prop_string(ctx, -2, "getModState");
	duk_push_c_function(ctx, jsLevelGetBlockEntityData, 3);  duk_put_prop_string(ctx, -2, "getBlockEntityData");
	duk_push_c_function(ctx, jsLevelSetBlockEntityData, 5);  duk_put_prop_string(ctx, -2, "setBlockEntityData");
	duk_push_c_function(ctx, jsLevelSetLight, 4);            duk_put_prop_string(ctx, -2, "setLight");
	duk_push_c_function(ctx, jsLevelSetSkyColor, 3);         duk_put_prop_string(ctx, -2, "setSkyColor");
	duk_push_c_function(ctx, jsLevelSetWeather, 1);          duk_put_prop_string(ctx, -2, "setWeather");
	duk_push_c_function(ctx, jsLevelSetGameMode, 1);         duk_put_prop_string(ctx, -2, "setGameMode");
	duk_push_c_function(ctx, jsLevelIsOp, 1);          duk_put_prop_string(ctx, -2, "isOp");
	duk_push_c_function(ctx, jsLevelListOps, 0);       duk_put_prop_string(ctx, -2, "listOps");
	duk_push_c_function(ctx, jsLevelAddOp, 1);         duk_put_prop_string(ctx, -2, "addOp");
	duk_push_c_function(ctx, jsLevelRemoveOp, 1);      duk_put_prop_string(ctx, -2, "removeOp");
	duk_put_global_string(ctx, "level");

	// Global `mods` object: cross-mod communication shared across the
	// isolated mod heaps (values are strings, stored engine-side).
	duk_push_object(ctx);
	duk_push_c_function(ctx, jsModsCommGet, 1);   duk_put_prop_string(ctx, -2, "get");
	duk_push_c_function(ctx, jsModsCommSet, 2);   duk_put_prop_string(ctx, -2, "set");
	duk_put_global_string(ctx, "mods");

	// Global `Mob` object (scripted mob registration)
	duk_push_object(ctx);
	duk_push_c_function(ctx, jsMobDefineMob, 2);      duk_put_prop_string(ctx, -2, "defineMob");
	duk_put_global_string(ctx, "Mob");

	// Global `Item` object (custom item registration)
	duk_push_object(ctx);
	duk_push_c_function(ctx, jsItemDefineItem, 4);    duk_put_prop_string(ctx, -2, "defineItem");
	// Assets object: resource-pack style texture overrides.
	duk_push_object(ctx);
	duk_push_c_function(ctx, jsAssetsReplaceImage, 4);
	duk_put_prop_string(ctx, -2, "replaceImage");
	duk_push_c_function(ctx, jsAssetsReplaceBlockIcon, 2);
	duk_put_prop_string(ctx, -2, "replaceBlockIcon");
	duk_put_global_string(ctx, "Assets");
	duk_put_global_string(ctx, "Item");

	// Global `Block` object (custom tile registration)
	duk_push_object(ctx);
	duk_push_c_function(ctx, jsBlockDefineBlock, 2);  duk_put_prop_string(ctx, -2, "defineBlock");
	duk_push_c_function(ctx, jsBlockSetState, 3);     duk_put_prop_string(ctx, -2, "setState");
	duk_push_c_function(ctx, jsBlockSetName, 2);      duk_put_prop_string(ctx, -2, "setName");
	duk_push_c_function(ctx, jsBlockInjectTexture, 1); duk_put_prop_string(ctx, -2, "injectTexture");
	duk_put_global_string(ctx, "Block");

	// Global `Particle` object (custom particle registration)
	duk_push_object(ctx);
	duk_push_c_function(ctx, jsParticleDefine, 2);    duk_put_prop_string(ctx, -2, "define");
	duk_put_global_string(ctx, "Particle");

	// Global `ui` object (GUI drawing, called from onGuiRender)
	duk_push_object(ctx);
	duk_push_c_function(ctx, jsUiDrawText, 4);       duk_put_prop_string(ctx, -2, "drawText");
	duk_push_c_function(ctx, jsUiDrawShadow, 4);     duk_put_prop_string(ctx, -2, "drawShadow");
	duk_push_c_function(ctx, jsUiFillRect, 5);       duk_put_prop_string(ctx, -2, "fillRect");
	duk_push_c_function(ctx, jsUiFillCircle, 4);     duk_put_prop_string(ctx, -2, "fillCircle");
	duk_push_c_function(ctx, jsUiDrawCircleRing, 5); duk_put_prop_string(ctx, -2, "drawCircleRing");
	duk_push_c_function(ctx, jsUiDrawImage, 5);      duk_put_prop_string(ctx, -2, "drawImage");
	duk_push_c_function(ctx, jsUiGetWidth, 0);       duk_put_prop_string(ctx, -2, "getWidth");
	duk_push_c_function(ctx, jsUiGetHeight, 0);      duk_put_prop_string(ctx, -2, "getHeight");
	duk_push_c_function(ctx, jsUiGetScale, 0);       duk_put_prop_string(ctx, -2, "getScale");
	duk_push_c_function(ctx, jsUiShowLoading, 1);    duk_put_prop_string(ctx, -2, "showLoading");
	duk_push_c_function(ctx, jsUiHideLoading, 0);    duk_put_prop_string(ctx, -2, "hideLoading");
	duk_push_c_function(ctx, jsUiOpenScreen, 1);     duk_put_prop_string(ctx, -2, "openScreen");
	duk_push_c_function(ctx, jsUiCloseScreen, 0);    duk_put_prop_string(ctx, -2, "closeScreen");
	duk_put_global_string(ctx, "ui");

	// Projectile.defineProjectile(typeId, {...}) —— 和 Block / Item / Mob 一样是“可定义类型”，
	// 任何模组都能用它定义自己的投射物（子弹/火球/飞刀）。
	duk_push_object(ctx);
	duk_push_c_function(ctx, jsProjectileDefineProjectile, 2);
	duk_put_prop_string(ctx, -2, "defineProjectile");
	duk_put_global_string(ctx, "Projectile");

	// Global `UI` object (09 · UI 覆盖系统：resource-pack style vanilla-UI editing)
	duk_push_object(ctx);
	duk_push_c_function(ctx, jsUiOverride, 2);    duk_put_prop_string(ctx, -2, "override");
	duk_push_c_function(ctx, jsUiSetImage, 2);    duk_put_prop_string(ctx, -2, "setImage");
	duk_push_c_function(ctx, jsUiAddButton, 2);   duk_put_prop_string(ctx, -2, "addButton");
	duk_push_c_function(ctx, jsUiAddHudButton, 1);duk_put_prop_string(ctx, -2, "addHudButton");
	duk_push_c_function(ctx, jsUiAddHudKey, 1);   duk_put_prop_string(ctx, -2, "addHudKey");
	duk_push_c_function(ctx, jsUiSetControlStyle, 2);   duk_put_prop_string(ctx, -2, "setControlStyle");
	duk_push_c_function(ctx, jsUiClearControlStyle, 1); duk_put_prop_string(ctx, -2, "clearControlStyle");
	duk_push_c_function(ctx, jsUiGetControlRect, 1);    duk_put_prop_string(ctx, -2, "getControlRect");
	duk_push_c_function(ctx, jsUiSkin, 2);        duk_put_prop_string(ctx, -2, "skin");
	duk_push_c_function(ctx, jsUiClear, 1);       duk_put_prop_string(ctx, -2, "clear");
	duk_put_global_string(ctx, "UI");

	// Global `Dimension` object (scripted dimension registration)
	duk_push_object(ctx);
	duk_push_c_function(ctx, jsDimensionDefine, 2);   duk_put_prop_string(ctx, -2, "define");
	duk_put_global_string(ctx, "Dimension");

	// Global `Biome` object (模组群系注册 + 查询)
	duk_push_object(ctx);
	duk_push_c_function(ctx, jsBiomeDefine, 2);       duk_put_prop_string(ctx, -2, "define");
	duk_push_c_function(ctx, jsBiomeDistribution, 2); duk_put_prop_string(ctx, -2, "distribution");
	duk_push_c_function(ctx, jsBiomeList, 0);         duk_put_prop_string(ctx, -2, "list");
	duk_push_c_function(ctx, jsBiomeGet, 1);          duk_put_prop_string(ctx, -2, "get");
	duk_push_c_function(ctx, jsBiomeUndistribution, 1); duk_put_prop_string(ctx, -2, "undistribution");
	duk_push_c_function(ctx, jsBiomeClearCache, 0);   duk_put_prop_string(ctx, -2, "clearCache");
	duk_put_global_string(ctx, "Biome");

	// Global `Recipes` object (crafting recipes)
	duk_push_object(ctx);
	duk_push_c_function(ctx, jsRecipesAddShapedRecipe, 4);    duk_put_prop_string(ctx, -2, "addShapedRecipe");
	duk_push_c_function(ctx, jsRecipesAddShapelessRecipe, 3); duk_put_prop_string(ctx, -2, "addShapelessRecipe");
	duk_put_global_string(ctx, "Recipes");

	// Global timers + config functions
	duk_push_c_function(ctx, jsSetTimeout, 2);    duk_put_global_string(ctx, "setTimeout");
	duk_push_c_function(ctx, jsSetInterval, 2);   duk_put_global_string(ctx, "setInterval");
	duk_push_c_function(ctx, jsClearTimer, 1);    duk_put_global_string(ctx, "clearTimeout");
	duk_push_c_function(ctx, jsSetConfig, 2);     duk_put_global_string(ctx, "setConfig");
	duk_push_c_function(ctx, jsGetConfig, 2);     duk_put_global_string(ctx, "getConfig");

	// 同一份 mod zip 双端加载时用它区分"我该跑哪一半"：
	//   独立服务器：isServer=true / isClient=false  → 只跑权威逻辑（方块/事件/数值）
	//   客户端进程：isServer=false / isClient=true  → 跑表现（贴图/UI/光影/音效）
	// 服务器专属 API（ui.*/GL.*/贴图注入）在服务器会被静默忽略，所以 mod 里凡是
	// 画东西/改贴图的代码都应当放进 `if (isClient) { ... }`。
#ifdef STANDALONE_SERVER
	duk_push_boolean(ctx, 1);  duk_put_global_string(ctx, "isServer");
	duk_push_boolean(ctx, 0);  duk_put_global_string(ctx, "isClient");
#else
	duk_push_boolean(ctx, 0);  duk_put_global_string(ctx, "isServer");
	duk_push_boolean(ctx, 1);  duk_put_global_string(ctx, "isClient");
#endif

	// Commands.register(name, handler)：mod 注册服务器命令（/name）。
	// handler(senderName, argsString) 的字符串返回值会作为回复发给执行者。
	duk_push_global_object(ctx);
	duk_push_object(ctx);
	duk_push_c_function(ctx, jsCommandsRegister, 2);
	duk_put_prop_string(ctx, -2, "register");
	duk_put_prop_string(ctx, -2, "Commands");
	duk_pop(ctx);

	// Stage 11 · render-mod API: raw desktop GL + offscreen scene + camera.
#ifndef STANDALONE_SERVER
	// 裸 GL / Render.* 绑定（服务器没有 GL 上下文）
	registerGlJsBindings(ctx);
#endif
}

static bool endsWithLower(const std::string& s, const std::string& suffix) {
	if (s.size() < suffix.size())
		return false;
	for (size_t i = 0; i < suffix.size(); ++i)
		if (tolower((unsigned char)s[s.size() - suffix.size() + i]) != tolower((unsigned char)suffix[i]))
			return false;
	return true;
}

static ModInfo scanZipMeta(const std::string& full, const std::string& file, const std::vector<std::string>& enabled) {
	ModInfo mi;
	mi.file = file;
	std::string js;
	std::vector<ModZipEntry> entries;
	if (modZipRead(full, entries)) {
		const ModZipEntry* main = modZipFind(entries, "main.js");
		if (main) {
			std::vector<unsigned char> src;
			if (modZipExtract(full, *main, src) && !src.empty()) {
				size_t n = src.size() < 8191 ? src.size() : 8191;
				js.assign((const char*)src.data(), n);
			}
		}
		// Cover image: first .png under a cover/ folder in the package.
		for (size_t i = 0; i < entries.size(); ++i) {
			const std::string& n = entries[i].name;
			if (n.size() < 8) continue; // "cover/x.png" minimum
			if (n.compare(0, 6, "cover/") != 0 && n.compare(0, 6, "COVER/") != 0)
				continue;
			size_t dot = n.rfind('.');
			if (dot == std::string::npos) continue;
			std::string ext = n.substr(dot);
			for (size_t k = 0; k < ext.size(); ++k)
				ext[k] = (char)tolower((unsigned char)ext[k]);
			if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") {
				mi.cover = n;
				break;
			}
		}
	}
	mi.name = parseMeta(js, "name");
	if (mi.name.empty()) {
		mi.name = file;
		size_t dot = mi.name.rfind('.');
		if (dot != std::string::npos)
			mi.name = mi.name.substr(0, dot);
	}
	mi.author = parseMeta(js, "author");
	mi.version = parseMeta(js, "version");
	mi.description = parseMeta(js, "description");
	mi.enabled = false;
	for (size_t i = 0; i < enabled.size(); ++i)
		if (enabled[i] == file) { mi.enabled = true; break; }
	return mi;
}

std::string ModEngine::getEnabledModsJoined() {
	std::vector<ModInfo> mods = scanMods();
	std::string out;
	for (size_t i = 0; i < mods.size(); ++i) {
		if (!mods[i].enabled) continue;
		if (!out.empty()) out += ", ";
		out += mods[i].name.empty() ? mods[i].file : mods[i].name;
	}
	return out;
}

// World mod-guard: compare the mod list recorded in a world's level.dat
// (written on first save) against the currently enabled mods. Returns a
// non-empty warning naming the missing / newly added mods, or "" when the
// sets match. Called from the main thread right after joining a world.
std::string ModEngine::buildModNameList() {
	std::vector<ModInfo> mods = scanMods();
	std::string now;
	for (size_t i = 0; i < mods.size(); ++i) {
		if (!mods[i].enabled) continue;
		if (!now.empty()) now += ", ";
		now += mods[i].name.empty() ? mods[i].file : mods[i].name;
	}
	return now;
}

std::string ModEngine::checkWorldMods(const std::string& saved) {
	std::string now = buildModNameList();
	log("[worldguard] check: saved='" + saved + "' now='" + now + "'");
	if (saved.empty() || saved == now)
		return "";

	// Split both lists on commas and diff them so the warning can name
	// exactly which mods are missing / newly added.
	std::vector<std::string> wasList, nowList;
	std::string cur;
	for (size_t i = 0; i <= saved.size(); ++i) {
		if (i == saved.size() || saved[i] == ',') { if (!cur.empty()) { wasList.push_back(cur); cur.clear(); } }
		else if (saved[i] != ' ') cur += saved[i];
	}
	cur.clear();
	for (size_t i = 0; i <= now.size(); ++i) {
		if (i == now.size() || now[i] == ',') { if (!cur.empty()) { nowList.push_back(cur); cur.clear(); } }
		else if (now[i] != ' ') cur += now[i];
	}
	std::string missing, added;
	for (size_t i = 0; i < wasList.size(); ++i) {
		bool found = false;
		for (size_t j = 0; j < nowList.size(); ++j) if (nowList[j] == wasList[i]) { found = true; break; }
		if (!found) { if (!missing.empty()) missing += ", "; missing += wasList[i]; }
	}
	for (size_t i = 0; i < nowList.size(); ++i) {
		bool found = false;
		for (size_t j = 0; j < wasList.size(); ++j) if (wasList[j] == nowList[i]) { found = true; break; }
		if (!found) { if (!added.empty()) added += ", "; added += nowList[i]; }
	}
	std::string warning = "世界模组不匹配!";
	if (!missing.empty()) warning += " [缺少: " + missing + "]";
	if (!added.empty()) warning += " [新增: " + added + "]";
	log("[worldguard] mismatch -> " + warning);
	return warning;
}

std::vector<ModInfo> ModEngine::scanMods() {
	std::vector<ModInfo> mods;
	std::vector<std::string> enabled = loadEnabledList();
#if defined(_WIN32)
	const char* pattern = "\\*.zip";
	WIN32_FIND_DATAA fd;
	HANDLE h = FindFirstFileA((fullModsDir() + pattern).c_str(), &fd);
	if (h != INVALID_HANDLE_VALUE) {
		do {
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				continue;
			std::string full = fullModsDir() + "\\";
			full += fd.cFileName;
			mods.push_back(scanZipMeta(full, fd.cFileName, enabled));
		} while (FindNextFileA(h, &fd));
		FindClose(h);
	}
#elif defined(__ANDROID__)
	DIR* d = opendir(fullModsDir().c_str());
	if (d) {
		struct dirent* e;
		while ((e = readdir(d)) != NULL) {
			const char* fn = e->d_name;
			size_t len = strlen(fn);
			if (len < 4 || strcmp(fn + len - 4, ".zip") != 0)
				continue;
			std::string full = fullModsDir() + "/" + fn;
			mods.push_back(scanZipMeta(full, fn, enabled));
		}
		closedir(d);
	}
#endif
	return mods;
}


std::string ModEngine::installModFile(const std::string& srcPath) {
#if defined(_WIN32)
	const char* base = srcPath.c_str();
	const char* slash = strrchr(base, '\\');
	if (!slash) slash = strrchr(base, '/');
	std::string fname = slash ? slash + 1 : base;
	if (!endsWithLower(fname, ".zip"))
		return "";
	std::string dst = fullModsDir() + "\\";
	dst += fname;
	if (!CopyFileA(srcPath.c_str(), dst.c_str(), TRUE)) {
		// TRUE = overwrite an existing file with the same name.
		log("installModFile: CopyFile failed: " + srcPath + " (err=" + std::to_string((int)GetLastError()) + ")");
		return "";
	}
	log("installed mod: " + fname + " -> " + dst);
	return fname;
#else
	// Android 以及其它 POSIX 平台。Java 侧的文件选择器已经把 content:// 复制成了
	// 真实路径（见 MainActivity / nativeOnModFilePicked），这里按文件名拷进 mods/。
	// 以前这里是 `return "";`，所以手机上的“安装模组”按钮其实是空的。
	const char* base = srcPath.c_str();
	const char* slash = strrchr(base, '/');
	std::string fname = slash ? slash + 1 : base;
	if (!endsWithLower(fname, ".zip") || !validModFileName(fname))
		return "";
	std::string dst = modFilePath(fname);
	std::ifstream in(srcPath.c_str(), std::ios::binary);
	if (!in) {
		log("installModFile: cannot open source: " + srcPath);
		return "";
	}
	std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	in.close();
	std::ofstream out(dst.c_str(), std::ios::binary | std::ios::trunc);
	if (!out) {
		log("installModFile: cannot write: " + dst);
		return "";
	}
	out.write(data.data(), (std::streamsize)data.size());
	out.close();
	if (!out) {
		log("installModFile: write failed: " + dst);
		return "";
	}
	log("installed mod: " + fname + " -> " + dst);
	return fname;
#endif
}

int ModEngine::allocDimensionId() const {
	// 主世界占了 0 与 10（NORMAL / NORMAL_DAYCYCLE），模组维度从 11 起自动
	// 分配，向后跳过已被其它模组占用的编号。
	int id = 11;
	while (id < 4096 && _scriptedDimensions.find(id) != _scriptedDimensions.end()) ++id;
	return id;
}

void ModEngine::defineDimension(int id, const std::string& name, bool hasClouds, int grassId, int dirtId, int stoneId) {
	ScriptedDimensionDef d;
	d.id = id;
	d.name = name;
	d.hasClouds = hasClouds;
	d.grassId = grassId;
	d.dirtId = dirtId;
	d.stoneId = stoneId;
	_scriptedDimensions[id] = d;
	log("defined dimension id=" + std::to_string(id) + " name=" + name);
}

void ModEngine::dimensionBlocks(int id, int& grass, int& dirt, int& stone) const {
	std::map<int, ScriptedDimensionDef>::const_iterator it = _scriptedDimensions.find(id);
	if (it != _scriptedDimensions.end()) {
		grass = it->second.grassId;
		dirt = it->second.dirtId;
		stone = it->second.stoneId;
	}
}

// ---- 模组群系 ----

long long ModEngine::packBiomeKey(int dimId, int x, int z) {
	// 4 格对齐：同一个 4x4 单元里只问一次 JS。
	long long k = (long long)(dimId & 0xffff) << 48;
	k |= (long long)((x >> 2) & 0xffffff) << 24;
	k |= (long long)((z >> 2) & 0xffffff);
	return k;
}

int ModEngine::allocBiomeId() const {
	// 已用最大编号 + 1，向后跳过被占用的。原版占 1..11。
	int maxId = 11;
	for (std::map<int, ScriptedBiomeDef>::const_iterator it = _scriptedBiomes.begin(); it != _scriptedBiomes.end(); ++it)
		if (it->first > maxId) maxId = it->first;
	int id = maxId + 1;
	while (id < 4096 && hasScriptedBiome(id)) ++id;
	return id;
}

int ModEngine::defineBiome(int requestedId, const ScriptedBiomeDef& def) {
	int bid = requestedId;
	if (bid <= 0) {
		bid = allocBiomeId();
	} else {
		// 原版 1..11 是固定契约，模组不得占用
		if (bid >= 1 && bid <= 11) {
			log("defineBiome: id " + std::to_string(bid) + " is used by a vanilla biome");
			return -1;
		}
		if (hasScriptedBiome(bid)) {
			log("defineBiome: id " + std::to_string(bid) + " already defined");
			return -1;
		}
	}
	ScriptedBiomeDef d = def;
	d.id = bid;
	_scriptedBiomes[bid] = d;
	// 实体化：ScriptedBiome 是真正的 Biome 子类，地形/刷怪/天空色都从它读。
	std::map<int, Biome*>::iterator old = _scriptedBiomeObjs.find(bid);
	if (old != _scriptedBiomeObjs.end()) { delete old->second; old->second = NULL; }
	_scriptedBiomeObjs[bid] = new ScriptedBiome(bid, d);
	_biomeCache.clear();   // 定义变了，缓存作废
	log("defined biome id=" + std::to_string(bid) + " name=" + d.name);
	return bid;
}

Biome* ModEngine::scriptedBiome(int id) {
	std::map<int, Biome*>::iterator it = _scriptedBiomeObjs.find(id);
	return it == _scriptedBiomeObjs.end() ? NULL : it->second;
}

void ModEngine::clearBiomeCache() {
	std::lock_guard<std::recursive_mutex> _jsLock(_jsMutex);
	_biomeCache.clear();
}

void ModEngine::undefineBiomeDistribution(int dimId) {
	std::lock_guard<std::recursive_mutex> _jsLock(_jsMutex);
	_biomeDistDims.erase(dimId);
	_biomeCache.clear();       // 分布变了，缓存作废
}

// 玩家跨群系 → onBiomeEnter / onBiomeLeave（给"进群系播音乐/发提示"这类模组用）。
// 只在客户端跑（服务器没有"本地玩家"）；每 tick 一次，代价 = 一次群系查询。
void ModEngine::tickBiomeTracking() {
	Minecraft* mc = minecraft();
	if (!mc || !mc->level || !mc->player) {
		_lastBiomeId = -2;
		return;
	}
	Biome* b = mc->level->getBiome((int)mc->player->x, (int)mc->player->z);
	int id = b ? b->id : 0;
	if (_lastBiomeId == -2) {       // 进世界/切维度后第一次：只记不触发
		_lastBiomeId = id;
		return;
	}
	if (id == _lastBiomeId) return;
	int oldId = _lastBiomeId;
	_lastBiomeId = id;
	if (oldId > 0) fireEvent("onBiomeLeave", oldId);
	if (id > 0) fireEvent("onBiomeEnter", id, oldId);
}

// 该坐标该用哪个模组群系（0 = 用原版）。按 4 格对齐缓存，所以逐格调用也只会在
// 每个群系单元进一次 JS。加锁：缓存与 JS 都要串行（生成线程 + 主线程都会问）。
int ModEngine::biomeIdAt(int dimId, int x, int z) {
	std::lock_guard<std::recursive_mutex> _jsLock(_jsMutex);
	if (_biomeDistDims.find(dimId) == _biomeDistDims.end())
		return 0;
	long long key = packBiomeKey(dimId, x, z);
	std::map<long long, int>::iterator c = _biomeCache.find(key);
	if (c != _biomeCache.end())
		return c->second;
	int result = 0;
	std::string fn = "__biome_dist_" + std::to_string(dimId);
	for (size_t s = 0; s < _sandboxes.size() && result == 0; ++s) {
		duk_context* ctx = _sandboxes[s].ctx;
		if (!ctx) continue;
		duk_get_global_string(ctx, fn.c_str());
		if (!duk_is_function(ctx, -1)) { duk_pop(ctx); continue; }
		duk_push_int(ctx, x);
		duk_push_int(ctx, z);
		if (duk_pcall(ctx, 2) != 0) {
			logThrottled("biome-dist", std::string("JS error in ") + fn + ": " + duk_safe_to_string(ctx, -1));
			duk_pop(ctx);
			continue;
		}
		int id = (int)duk_get_int(ctx, -1);
		duk_pop(ctx);
		// 只认已注册的模组群系；其它值一律回落到原版群系。
		if (id > 0 && hasScriptedBiome(id)) result = id;
	}
	if (_biomeCache.size() > 200000) _biomeCache.clear();   // 上限兜底
	_biomeCache[key] = result;
	return result;
}

// 群系自定义列高（getHeight 回调）；false = 该群系没定义。
bool ModEngine::callBiomeHeight(int biomeId, int x, int z, int& heightOut) {
	if (!hasScriptedBiome(biomeId)) return false;
	std::lock_guard<std::recursive_mutex> _jsLock(_jsMutex);
	std::string fn = "__biome_" + std::to_string(biomeId) + "_height";
	for (size_t s = 0; s < _sandboxes.size(); ++s) {
		duk_context* ctx = _sandboxes[s].ctx;
		if (!ctx) continue;
		duk_get_global_string(ctx, fn.c_str());
		if (!duk_is_function(ctx, -1)) { duk_pop(ctx); continue; }
		duk_push_int(ctx, x);
		duk_push_int(ctx, z);
		if (duk_pcall(ctx, 2) != 0) {
			logThrottled("biome-height", std::string("JS error in ") + fn + ": " + duk_safe_to_string(ctx, -1));
			duk_pop(ctx);
			continue;
		}
		heightOut = (int)duk_get_number(ctx, -1);
		duk_pop(ctx);
		return true;
	}
	return false;
}

// 群系装饰回调（区块 postProcess 时调一次）；true = 模组自己装饰过，
// 调用方跳过原版植被（树/花/草丛/甘蔗/仙人掌），矿物照旧。
bool ModEngine::callBiomeDecorate(int biomeId, int x, int z) {
	if (!hasScriptedBiome(biomeId)) return false;
	std::lock_guard<std::recursive_mutex> _jsLock(_jsMutex);
	std::string fn = "__biome_" + std::to_string(biomeId) + "_decorate";
	for (size_t s = 0; s < _sandboxes.size(); ++s) {
		duk_context* ctx = _sandboxes[s].ctx;
		if (!ctx) continue;
		duk_get_global_string(ctx, fn.c_str());
		if (!duk_is_function(ctx, -1)) { duk_pop(ctx); continue; }
		duk_push_int(ctx, x);
		duk_push_int(ctx, z);
		if (duk_pcall(ctx, 2) != 0) {
			logThrottled("biome-decorate", std::string("JS error in ") + fn + ": " + duk_safe_to_string(ctx, -1));
			duk_pop(ctx);
			continue;
		}
		duk_pop(ctx);
		return true;
	}
	return false;
}

Dimension* ModEngine::createScriptedDimension(int id) {
	if (!this) {
		log("createScriptedDimension: THIS IS NULL");
		return NULL;
	}
	log("createScriptedDimension id=" + std::to_string(id) + " mapSize=" + std::to_string((int)_scriptedDimensions.size()));
	std::map<int, ScriptedDimensionDef>::iterator it = _scriptedDimensions.find(id);
	if (it == _scriptedDimensions.end()) {
		log("createScriptedDimension: id " + std::to_string(id) + " not registered");
		return NULL;
	}
	Dimension* d = new ScriptedDimension(id, it->second.hasClouds);
	log("createScriptedDimension: created " + std::to_string(id));
	return d;
}

bool ModEngine::isCriticalHit(int victimEntityId) {
	const int args[1] = { victimEntityId };
	bool r = false;
	callModGlobal("__mod_is_critical", args, 1, &r);
	return r;
}

bool ModEngine::canEatFood(int itemId) {
	const int args[1] = { itemId };
	bool r = true;
	callModGlobal("__mod_can_eat", args, 1, &r);
	return r;
}

// Generic helper for the "can ..." veto hooks: looks up a JS global function
// by name, calls it with the given int args, treats absence as "allowed" and
// a thrown JS error as "allowed" (log only).
// Call a veto function (top of stack is the function object); returns its
// boolean result (true = allow). On JS error logs and returns true (allow).
static bool callVetoFn(duk_context* ctx, const char* fnName, const int* args, int argc) {
	for (int i = 0; i < argc; ++i)
		duk_push_int(ctx, args[i]);
	if (duk_pcall(ctx, argc) != 0) {
		if (ModEngine::instance)
			ModEngine::instance->log(std::string("JS error in ") + fnName + ": " + duk_safe_to_string(ctx, -1));
		duk_pop(ctx);
		return true;
	}
	bool ok = duk_get_boolean(ctx, -1) != 0;
	duk_pop(ctx);
	return ok;
}

// Call a global function defined in the FIRST mod sandbox that has it.
// Returns true if any mod defined it; outResult receives that function's
// boolean result (true when absent / on JS error).
bool ModEngine::callModGlobal(const char* name, const int* args, int argc, bool* outResult) {
	std::lock_guard<std::recursive_mutex> _jsLock(_jsMutex);
	for (size_t s = 0; s < _sandboxes.size(); ++s) {
		duk_context* ctx = _sandboxes[s].ctx;
		if (!ctx)
			continue;
		duk_get_global_string(ctx, name);
		if (!duk_is_function(ctx, -1)) {
			duk_pop(ctx);
			continue;
		}
		for (int i = 0; i < argc; ++i)
			duk_push_int(ctx, args[i]);
		if (duk_pcall(ctx, argc) != 0) {
			log(std::string("JS error in ") + name + ": " + duk_safe_to_string(ctx, -1));
			duk_pop(ctx);
			if (outResult) *outResult = true;
			return true;
		}
		bool ok = duk_get_boolean(ctx, -1) != 0;
		duk_pop(ctx);
		if (outResult) *outResult = ok;
		return true;
	}
	if (outResult) *outResult = true;
	return false;
}

// Veto query across every mod sandbox, with mod namespacing:
//   1) legacy fixed name (e.g. __mod_can_attack)   - backward compat
//   2) every namespaced global matching __mod_<any>_<suffix>
//      (e.g. __mod_block_can_attack, __mod_aether_can_attack)
// Any function returning false -> vetoed (AND semantics).
bool ModEngine::vetoAll(const char* baseName, const int* args, int argc) {
	const char* suffix = (strncmp(baseName, "__mod_", 6) == 0) ? baseName + 6 : NULL;
	const size_t slen = suffix ? strlen(suffix) : 0;
	for (size_t s = 0; s < _sandboxes.size(); ++s) {
		duk_context* ctx = _sandboxes[s].ctx;
		if (!ctx)
			continue;
		// 1) legacy fixed name
		duk_get_global_string(ctx, baseName);
		if (duk_is_function(ctx, -1)) {
			bool ok = callVetoFn(ctx, baseName, args, argc);
			if (!ok)
				return false;
		} else {
			duk_pop(ctx);
		}
		if (!suffix || !*suffix)
			continue;
		// 2) namespaced variants in this sandbox
		duk_push_global_object(ctx);
		duk_enum(ctx, -1, DUK_ENUM_OWN_PROPERTIES_ONLY);
		while (duk_next(ctx, -1, 1)) {
			const char* key = duk_safe_to_string(ctx, -2);
			const size_t klen = strlen(key);
			bool matches = klen > 6 + slen + 1 &&
				strncmp(key, "__mod_", 6) == 0 &&
				strncmp(key + klen - slen, suffix, slen) == 0 &&
				key[klen - slen - 1] == '_' &&
				strcmp(key, baseName) != 0;
			if (matches && duk_is_function(ctx, -1)) {
				bool ok = callVetoFn(ctx, key, args, argc);   // consumes value (fn)
				duk_pop(ctx);                                  // pop key
				if (!ok) {
					duk_pop(ctx);   // enum
					duk_pop(ctx);   // global
					return false;
				}
			} else {
				duk_pop_2(ctx);    // key + value
			}
		}
		duk_pop(ctx);  // enum
		duk_pop(ctx);  // global
	}
	return true;
}

// JS global __mod_can_break(x, y, z, face) -> bool; absence = allowed.
bool ModEngine::canBreakBlock(int x, int y, int z, int face) {
	const int args[4] = { x, y, z, face };
	return vetoAll("__mod_can_break", args, 4);
}

// JS global __mod_can_place(x, y, z, face, itemId) -> bool; absence = allowed.
bool ModEngine::canPlaceBlock(int x, int y, int z, int face, int itemId) {
	const int args[5] = { x, y, z, face, itemId };
	return vetoAll("__mod_can_place", args, 5);
}

// JS global __mod_can_attack(victimId) -> bool; absence = allowed.
bool ModEngine::canAttack(int victimEntityId) {
	const int args[1] = { victimEntityId };
	return vetoAll("__mod_can_attack", args, 1);
}

// ---- 鼠标事件（JS onMouse(button, down, heldMs)）----
// 当前被模组接管的鼠标键：0 = 左键，1 = 右键。fireMouseEvent 每次更新，
// 原版挖方块/放方块前用 mouseTaken 查（见 Minecraft::tick）。
static bool g_modMouseTaken[2] = { false, false };

// 问一遍所有模组的 onMouse：任何一个返回 false 就算"这个键归模组管"（AND 语义，
// 与上面 vetoAll 一致）。只在按键状态【变化】时被调用（Minecraft::tickInput 的
// 边沿检测），所以不会每 tick 进 JS。
bool ModEngine::fireMouseEvent(int button, bool down, int heldMs) {
	std::lock_guard<std::recursive_mutex> _jsLock(_jsMutex);
	bool accepted = true;   // 默认没人接管 → 原版照旧
	if (_registeredEvents.count("onMouse")) {
		const char key[] = "__mod_evt_onMouse";
		for (size_t s = 0; s < _sandboxes.size(); ++s) {
			duk_context* sctx = _sandboxes[s].ctx;
			if (!sctx)
				continue;
			duk_push_heap_stash(sctx);                   // [stash]
			duk_get_prop_string(sctx, -1, key);          // [stash arr]
			if (!duk_is_array(sctx, -1)) {
				duk_pop_2(sctx);                         // []
				continue;
			}
			int n = (int)duk_get_length(sctx, -1);
			for (int i = 0; i < n; ++i) {
				duk_get_prop_index(sctx, -1, i);         // [stash arr fn]
				if (duk_is_function(sctx, -1)) {
					duk_push_int(sctx, button);
					duk_push_int(sctx, down ? 1 : 0);
					duk_push_int(sctx, heldMs);
					if (duk_pcall(sctx, 3) != 0) {       // [stash arr (ret|err)]
						logThrottled("onMouse", std::string("JS error in onMouse: ")
							+ duk_safe_to_string(sctx, -1));
					} else if (duk_is_boolean(sctx, -1) && !duk_get_boolean(sctx, -1)) {
						accepted = false;                // 这个模组要接手这个键
					}
					duk_pop(sctx);                       // [stash arr]
				} else {
					duk_pop(sctx);                       // [stash arr]
				}
				if (!accepted)
					break;                               // 已有人接管，不必再问
			}
			duk_pop_2(sctx);                             // []
			if (!accepted)
				break;
		}
	} else {
		// 没模组注册 onMouse：直接找顶层函数（和 fireEvent 同款兜底，兼容老用法）。
		duk_context* ctx = (duk_context*)_ctx;
		if (ctx) {
			duk_push_global_object(ctx);                 // [global]
			duk_get_prop_string(ctx, -1, "onMouse");     // [global fn]
			if (duk_is_function(ctx, -1)) {
				duk_push_int(ctx, button);
				duk_push_int(ctx, down ? 1 : 0);
				duk_push_int(ctx, heldMs);
				if (duk_pcall(ctx, 3) != 0) {            // [global (ret|err)]
					logThrottled("onMouse", std::string("JS error in onMouse: ")
						+ duk_safe_to_string(ctx, -1));
				} else if (duk_is_boolean(ctx, -1) && !duk_get_boolean(ctx, -1)) {
					accepted = false;
				}
			}
			duk_pop_2(ctx);                              // []
		}
	}
	if (button >= 0 && button < 2)
		g_modMouseTaken[button] = !accepted;
	return !accepted;
}

bool ModEngine::mouseTaken(int button) const {
	// 手指正按在模组注册的 HUD 按钮上：这个「左键」归模组，原版别去挖方块
	if (!_hudPointerKeys.empty() && button == 0) return true;
	return (button >= 0 && button < 2) ? g_modMouseTaken[button] : false;
}

// JS global __mod_on_player_hurt(dmg, sourceEntityId) -> bool.
// Returns true when the damage was NOT handled (player should take damage),
// false when a mod blocked it (e.g. converted it into sword durability).
bool ModEngine::onPlayerHurtBlocked(int dmg, int sourceEntityId) {
	const int args[2] = { dmg, sourceEntityId };
	return vetoAll("__mod_on_player_hurt", args, 2);
}

// mod 状态：内存里改，每 ~0.5 秒（或强制时）才落一次盘。
void ModEngine::loadModStateIfNeeded() {
	Minecraft* mc = minecraft();
	std::string world = (mc && mc->level) ? mc->level->getLevelName() : std::string();
	if (_modStateLoaded && _modStateWorld == world)
		return;
	_modStateWorld = world;
	_modStateLoaded = true;
	_modState.clear();
	_modStateDirty = false;
	_modStateTicksSinceWrite = 0;
	if (!mc || !mc->level)
		return;
	FILE* f = fopen((fullModsDir() + "/" + world + ".state").c_str(), "r");
	if (!f)
		return;
	char buf[512];
	while (fgets(buf, sizeof(buf), f)) {
		std::string line(buf);
		while (!line.empty() && (line[line.size()-1] == '\n' || line[line.size()-1] == '\r'))
			line.erase(line.size()-1);
		size_t eq = line.find('=');
		if (eq != std::string::npos && eq > 0)
			_modState[line.substr(0, eq)] = line.substr(eq + 1);
	}
	fclose(f);
}

void ModEngine::flushModState(bool force) {
	if (!_modStateDirty && !force)
		return;
	Minecraft* mc = minecraft();
	if (!mc || !mc->level)
		return;
	// 节流：普通情况下每 10 tick（约 0.5 秒）才真正写盘一次。
	if (!force && ++_modStateTicksSinceWrite < 10)
		return;
	_modStateTicksSinceWrite = 0;
	FILE* f = fopen((fullModsDir() + "/" + mc->level->getLevelName() + ".state").c_str(), "w");
	if (!f)
		return;
	for (std::map<std::string, std::string>::const_iterator it = _modState.begin(); it != _modState.end(); ++it) {
		fputs(it->first.c_str(), f);
		fputc('=', f);
		fputs(it->second.c_str(), f);
		fputc('\n', f);
	}
	fclose(f);
	_modStateDirty = false;
}

void ModEngine::setModState(const std::string& key, const std::string& value) {
	if (key.empty())
		return;
	Minecraft* mc = minecraft();
	if (!mc || !mc->level)
		return;
	loadModStateIfNeeded();
	_modState[key] = value;
	_modStateDirty = true;
}

std::string ModEngine::getModState(const std::string& key) {
	if (key.empty())
		return "";
	Minecraft* mc = minecraft();
	if (!mc || !mc->level)
		return "";
	loadModStateIfNeeded();
	std::map<std::string, std::string>::const_iterator it = _modState.find(key);
	return (it == _modState.end()) ? std::string() : it->second;
}
void ModEngine::saveDimensionState() {
	if (!_currentLevelName.empty()) {
		std::string path = fullModsDir() + "\dimstate_" + _currentLevelName + ".txt";
		FILE* f = fopen(path.c_str(), "w");
		if (f) {
			// Always write the dimension (including 0 = overworld): skipping
			// dim 0 left the previous dimension's state file behind, so
			// getCurrentDimension() kept reporting the Aether after returning.
			fprintf(f, "%d", _currentDim);
			fclose(f);
		}
		log("dimension state saved: level=" + _currentLevelName
			+ " dim=" + std::to_string(_currentDim));
	}
}

int ModEngine::loadDimensionState() {
	// Refreshes _currentLevelName from the currently loaded world.
	Minecraft* mc = minecraft();
	if (mc && mc->level) {
		_currentLevelName = mc->level->getLevelName();
		std::string path = fullModsDir() + "\dimstate_" + _currentLevelName + ".txt";
		FILE* f = fopen(path.c_str(), "r");
		if (f) {
			int d = 0;
			if (fscanf(f, "%d", &d) == 1)
				_currentDim = d;
			fclose(f);
		}
	}
	return _currentDim;
}

void ModEngine::defineParticle(const ScriptedParticleDef& def) {
	ScriptedParticleDef d = def;
	d.textureId = getModTexture(def.texturePath);
	_scriptedParticles[def.name] = d;
	log("defined particle: " + def.name + " tex=" + std::to_string(d.textureId)
		+ " size=" + std::to_string(d.size) + " life=" + std::to_string(d.lifetime));
}

bool ModEngine::spawnScriptedParticle(const std::string& name, float x, float y, float z, float xd, float yd, float zd) {
#ifndef STANDALONE_SERVER
	std::map<std::string, ScriptedParticleDef>::iterator it = _scriptedParticles.find(name);
	if (it == _scriptedParticles.end())
		return false;
	Minecraft* mc = minecraft();
	if (!mc || !mc->level || !mc->particleEngine)
		return false;
	ScriptedParticle::Def d;
	d.name = it->second.name;
	d.textureId = it->second.textureId;
	d.size = it->second.size;
	d.lifetime = it->second.lifetime;
	d.r = it->second.r;
	d.g = it->second.g;
	d.b = it->second.b;
	d.gravity = it->second.gravity;
	d.shrink = it->second.shrink;
	ScriptedParticle* p = new ScriptedParticle(mc->level, x, y, z, xd, yd, zd, d);
	mc->particleEngine->addCustom(p);
	return true;
#else
	return false;   // 服务器没有粒子引擎
#endif
}

int ModEngine::defineBlock(int id, const std::string& name, int textureId, const std::string& texturePath, const std::string& material, int renderLayer, int renderShape, const std::string& topTex, const std::string& sideTex, const std::string& bottomTex, float destroyTime, bool requiresPickaxe, bool blockEntity, int light) {
	if (id < 1 || id >= 256) {
		log("defineBlock: id " + std::to_string(id) + " out of range (1-255)");
		return -1;
	}
	// Id auto-allocation: the logical id the mod wrote is used when free;
	// if taken (vanilla / another mod / a mod item sharing the id space),
	// scan upward for the first free slot so mods never collide. Allocation
	// is deterministic: with the same enabled-mod set the scan order is the
	// same on every (re)load, so physical ids stay stable across reloads.
	//
	// Snapshot priority: when a world is loaded its level.dat may record
	// the logical->physical mapping this save was written with (applyBlockSnapshot
	// fills _blockSnapshot). If it does, reuse that physical id whenever the
	// slot is free. At process start the snapshot is empty (no world yet), so
	// this only engages when mods are reloaded AFTER entering a world
	// (e.g. toggling a mod in the mod manager) — keeping the ids of blocks
	// already placed in the save stable.
	int physId = id;
	{
		std::map<int, int>::const_iterator snap = _blockSnapshot.find(id);
		if (snap != _blockSnapshot.end() && snap->second >= 1 && snap->second < 256
			&& Tile::tiles[snap->second] == NULL && Item::items[snap->second] == NULL) {
			physId = snap->second;
			log("defineBlock: logical id " + std::to_string(id) + " -> snapshot physical id " + std::to_string(physId) + " (" + name + ")");
		} else if (Tile::tiles[physId] != NULL || Item::items[physId] != NULL) {
			for (int cand = id + 1; cand < 256; ++cand) {
				if (Tile::tiles[cand] == NULL && Item::items[cand] == NULL) {
					physId = cand;
					break;
				}
			}
			if (physId == id) {
				log("defineBlock: id " + std::to_string(id) + " occupied and no free slot (1-255)");
				return -1;
			}
			log("defineBlock: logical id " + std::to_string(id) + " taken -> physical id " + std::to_string(physId) + " (" + name + ")");
		}
	}
	const Material* mat = Material::stone;
	if (material == "wood")      mat = Material::wood;
	else if (material == "dirt") mat = Material::dirt;
	else if (material == "sand") mat = Material::sand;
	else if (material == "rock") mat = Material::stone;
	else if (material == "cloth") mat = Material::cloth;
	else if (material == "plant") mat = Material::plant;

	int finalTex = textureId;
	if (!texturePath.empty())
		finalTex = injectModBlockTexture(texturePath, physId);

	// Resolve per-face textures (top/side/bottom), injecting each into the
	// terrain atlas like the main texture.
	int faceTex[6];
	for (int i = 0; i < 6; ++i) faceTex[i] = -1;
	if (!topTex.empty())    faceTex[1] = injectModBlockTexture(topTex, physId);
	if (!bottomTex.empty()) faceTex[0] = injectModBlockTexture(bottomTex, physId);
	if (!sideTex.empty()) {
		int s = injectModBlockTexture(sideTex, physId);
		for (int i = 2; i < 6; ++i) faceTex[i] = s;
	}

	Tile* tile = (new ModTile(physId, mat, finalTex, renderLayer, renderShape, faceTex)); // constructor calls init()
	if (ModTile* mt = dynamic_cast<ModTile*>(tile)) {
		mt->setMineTime(destroyTime);
		mt->setRequiresPickaxe(requiresPickaxe);
		// Stage 2: if the mod registered an onTick callback, enable random
		// ticking so tick() gets dispatched to JS each tick.
		if (hasTileCallback(id, "tick"))
			mt->setModTicking(true);
		// Stage 3: blockEntity -> ModTile attaches a ModTileEntity onPlace.
		mt->setHasBlockEntity(blockEntity);
		// Emissive light: like torch/furnace_lit, 0-15 -> 0-0.9375 of max.
		// Tile::setLightEmission is protected; write the public static
		// lightEmission table directly (same effect).
		if (light > 0) {
			if (light > 15) light = 15;
			Tile::lightEmission[physId] = (int)(Level::MAX_BRIGHTNESS * (light / 16.0f));
		}
	}
	Item::items[physId] = new TileItem(physId - 256);
	_creativeItems.push_back(physId);
	_blockNames[physId] = name;
	_modTiles.push_back(physId);
	// Also track the Item slot so unloading the mod clears Item::items[physId]
	// too — otherwise the tile's TileItem survives in inventories as a
	// ghost item (renders, but can't be placed) after the mod is disabled.
	_modItems.push_back(physId);
	// Keep the physical->logical mapping so ModTile events (physical id)
	// can find the JS callbacks (keyed by logical id).
	_modBlockPhysId[physId] = id;
	const char* layerName = (renderLayer == 2) ? "blend" : (renderLayer == 1) ? "alphatest" : "opaque";
	log("defined block id=" + std::to_string(physId) + " (logical " + std::to_string(id) + ") name=" + name + " tex=" + std::to_string(finalTex) + " mat=" + material + " layer=" + layerName + " light=" + std::to_string(light));
	return physId;
}

// ---------------------------------------------------------------------------
// Block-id snapshot (persisted block-id stability, Stage A2)
// ---------------------------------------------------------------------------

void ModEngine::applyBlockSnapshot(const std::map<int, int>& snapshot) {
	_blockSnapshot = snapshot;
	log("applyBlockSnapshot: " + std::to_string(_blockSnapshot.size()) + " entry(ies) (record-only)");

	// The snapshot is RECORD-ONLY: it is kept in memory (and re-saved to
	// level.dat on every save) for future use, but it does NOT steer
	// defineBlock here. Why:
	//  - The enabled mods are already loaded (and their blocks allocated)
	//    when a world is entered; re-running defineBlock would require a
	//    full reloadEnabledMods().
	//  - reloading mid-entry races the world-generation thread: the
	//    generator reads Tile::tiles while the rebuild clears/re-registers
	//    it, producing placeholder "update" blocks (248/249) in place of
	//    mod blocks, and can SIGSEGV in mod onTick handlers.
	//  - The enabled-mod combination is guarded by worldguard anyway: a
	//    mismatched save is blocked at the world-select screen (unless the
	//    player explicitly chooses "ignore and continue"), so the physical
	//    ids of a save's blocks always match the ids assigned at load time.
	// Same-combination saves stay stable via deterministic allocation
	// (identical scan order on every load).
	(void)_blockSnapshot;
}

void ModEngine::snapshotBlockIds(std::map<int, int>& out) const {
	// Reverse the physical->logical map: every mod block currently registered
	// contributes (logical, physical).
	out.clear();
	for (std::map<int, int>::const_iterator it = _modBlockPhysId.begin(); it != _modBlockPhysId.end(); ++it)
		out[it->second] = it->first;
}

// Terrain-atlas slots used for injected custom block textures (511 down).
// terrain.png is 256x512 = 16 cols x 32 rows. Slots 0-255 hold ALL vanilla
// textures (including the destroy-crack overlay at 240-249), so mod textures
// are injected into the new extension rows only: slots 511-256. Never touch
// 0-255, or vanilla block/crack/liquid textures get overwritten.
static int g_nextBlockSlot = 511;

int ModEngine::injectModBlockTexture(const std::string& path, int blockId) {
#ifndef STANDALONE_SERVER
	std::map<std::string, ModPixelData>::iterator it = _modTexturePixels.find(path);
	if (it == _modTexturePixels.end()) {
		log("injectModBlockTexture: no pixels for " + path);
		return 1;
	}
	Minecraft* mc = minecraft();
	if (!mc || !mc->textures) {
		log("injectModBlockTexture: no textures");
		return 1;
	}
	TextureId terrainId = mc->textures->loadAndBindTexture("terrain.png");
	if (terrainId == Textures::InvalidId) {
		log("injectModBlockTexture: terrain not loaded");
		return 1;
	}
	const ModPixelData& pd = it->second;
	// Reuse the slot already allocated for this texture path on an earlier
	// (re)load of the mod: the pixels are still in the atlas, so re-injecting
	// would just burn another slot for the same image.
	std::map<std::string, int>::iterator sit = _modBlockSlotByPath.find(path);
	if (sit != _modBlockSlotByPath.end())
		return sit->second;
	int slot = g_nextBlockSlot--;
	if (slot < 256) {
		log("injectModBlockTexture: no spare slots left");
		return 1;
	}
	int col = slot & 0xf;   // 16-col layout: low nibble = column (0-15)
	int row = slot >> 4;    // high bits = row (0-31 for slots up to 511)
	int iw = pd.w > 16 ? 16 : pd.w;
	int ih = pd.h > 16 ? 16 : pd.h;
	glBindTexture(GL_TEXTURE_2D, (GLuint)terrainId);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexSubImage2D(GL_TEXTURE_2D, 0, col * 16, row * 16, iw, ih, GL_RGBA, GL_UNSIGNED_BYTE, pd.rgba.data());
	_modBlockSlotByPath[path] = slot;
	log("injected block texture " + path + " (block " + std::to_string(blockId) + ") -> terrain slot " + std::to_string(slot));
	return slot;
#else
	return 1;   // 服务器没有贴图图集
#endif
}

// gui/items.png slots for custom item icons (239 down; rows 13-14 are empty).
static int g_nextItemIconSlot = 239;

int ModEngine::injectModItemIcon(const std::string& path, int itemId) {
#ifndef STANDALONE_SERVER
	std::map<std::string, ModPixelData>::iterator it = _modTexturePixels.find(path);
	if (it == _modTexturePixels.end()) {
		log("injectModItemIcon: no pixels for " + path);
		return 1;
	}
	Minecraft* mc = minecraft();
	if (!mc || !mc->textures) {
		log("injectModItemIcon: no textures");
		return 1;
	}
	TextureId texId = mc->textures->loadAndBindTexture("gui/items.png");
	if (texId == Textures::InvalidId) {
		log("injectModItemIcon: items.png not loaded");
		return 1;
	}
	int slot = g_nextItemIconSlot--;
	if (slot < 200) {
		log("injectModItemIcon: no spare icon slots left");
		return 1;
	}
	const ModPixelData& pd = it->second;
	int col = slot & 0xf;
	int row = slot >> 4;
	glBindTexture(GL_TEXTURE_2D, (GLuint)texId);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexSubImage2D(GL_TEXTURE_2D, 0, col * 16, row * 16, pd.w, pd.h, GL_RGBA, GL_UNSIGNED_BYTE, pd.rgba.data());
	log("injected item icon " + path + " (item " + std::to_string(itemId) + ") -> items.png slot " + std::to_string(slot));
	return slot;
#else
	return 1;
#endif
}

// Invoke an Item.defineItem onUse JS callback (stored in heap stash as
// __mod_item_use_<id>). Returns true if a callback ran.
bool ModEngine::callItemUseOn(int id, int x, int y, int z, int face) {
	std::string key = "__mod_item_use_" + std::to_string(id);
	return dispatchStashAll(key.c_str(), x, y, z, face);
}

bool ModEngine::hasTileCallback(int tileId, const char* evt) const {
	// tileId here is the LOGICAL id (defineBlock is called with it); stash
	// callbacks are keyed by logical id too, so no translation needed.
	std::string key = std::string("__mod_tile_") + evt + "_" + std::to_string(tileId);
	return hasStashFnAll(key.c_str());
}

// Stage 3: ModTileEntity per-tick forward. JS global __mod_tile_entity_tick
// (x, y, z); absence = no-op.
void ModEngine::callTileEntityTick(int x, int y, int z) {
	dispatchStashAll("__mod_tile_entity_tick", x, y, z);
}

// ---------------------------------------------------------------------------
// Stage 5: scripted screens. The JS UI.openScreen config is stored in the
// heap stash under "__mod_screen_<key>" keys; these helpers dispatch the
// events back to the JS callbacks.
// ---------------------------------------------------------------------------
void ModEngine::notifyScreenButton(int buttonId) {
	dispatchStashAll("__mod_screen_onButton", buttonId);
}

void ModEngine::notifyScreenCell(int cellId) {
	dispatchStashAll("__mod_screen_onCell", cellId);
}

void ModEngine::notifyScreenText(int inputId, const std::string& text) {
	dispatchStashAll("__mod_screen_onText", inputId, text.c_str());
}

void ModEngine::notifyScreenClose() {
	dispatchStashAll("__mod_screen_onClose");
}

void ModEngine::notifyScreenRender() {
	dispatchStashAll("__mod_screen_onRender");
}

bool ModEngine::uiQuery(const std::string& screen, const std::string& key, UiElementOverride& out) const {
	std::map<std::string, UiScreenDef>::const_iterator it = _uiScreens.find(screen);
	if (it == _uiScreens.end()) return false;
	std::map<std::string, UiElementOverride>::const_iterator e = it->second.elements.find(key);
	if (e == it->second.elements.end()) return false;
	out = e->second;
	return true;
}

void ModEngine::uiNotifyButtonClick(const std::string& key) {
	dispatchStashAll(("__mod_ui_btn_" + key).c_str(), key.c_str());
}

// ── HUD 触摸按钮 ───────────────────────────────────────────────────────────
// 命中判定用的是 GUI 坐标（和 onGuiRender 里 ui.getWidth()/fillRect 同一套），
// 引擎在触摸落下时会先把物理坐标乘 Gui::InvGuiScale 再调这里。
bool ModEngine::hudButtonAt(float gx, float gy, std::string* outKey) const {
	for (size_t i = 0; i < _hudButtons.size(); ++i) {
		const UiHudButton& b = _hudButtons[i];
		if (gx >= (float)b.x && gx < (float)(b.x + b.w) &&
		    gy >= (float)b.y && gy < (float)(b.y + b.h)) {
			if (outKey) *outKey = b.key;
			return true;
		}
	}
	return false;
}

// 按下：派发 _down（按住用）和 _click（点一下用）。模组只实现需要的那一个，
// 两个都实现就会都收到（自己避免重复处理即可）。
void ModEngine::hudButtonDown(const std::string& key) {
	std::lock_guard<std::recursive_mutex> _jsLock(_jsMutex);
	dispatchStashAll(("__mod_hud_" + key + "_down").c_str(), key);
	dispatchStashAll(("__mod_hud_" + key + "_click").c_str(), key);
}

void ModEngine::hudButtonUp(const std::string& key) {
	std::lock_guard<std::recursive_mutex> _jsLock(_jsMutex);
	dispatchStashAll(("__mod_hud_" + key + "_up").c_str(), key);
}

// 按下/抬起都按指针记账（头文件里有说明）：注入与回收一律走这里
void ModEngine::hudPress(int pointer, const std::string& key) {
	_hudPointerKeys[pointer] = key;
	hudKeyDown(key);       // 按键映射按钮：注入鼠标键/键盘键（不是映射就 no-op）
	hudButtonDown(key);    // JS 回调按钮：派发 onDown/onClick（不是回调按钮就 no-op）
}

void ModEngine::hudRelease(int pointer) {
	std::map<int, std::string>::iterator it = _hudPointerKeys.find(pointer);
	if (it == _hudPointerKeys.end()) return;
	const std::string key = it->second;
	_hudPointerKeys.erase(it);
	hudKeyUp(key);         // 回收注入的鼠标键/键盘键
	hudButtonUp(key);      // 派发 onUp
}

bool ModEngine::hudPointerActive(int pointer) const {
	return _hudPointerKeys.find(pointer) != _hudPointerKeys.end();
}

int ModEngine::hudFirstPointer() const {
	return _hudPointerKeys.empty() ? -1 : _hudPointerKeys.begin()->first;
}

// ── 原版控件样式覆盖 ──────────────────────────────────
const ModEngine::ControlStyle* ModEngine::controlStyle(const std::string& key) const {
	std::map<std::string, ControlStyle>::const_iterator it = _controlStyles.find(key);
	return (it == _controlStyles.end()) ? NULL : &it->second;
}

void ModEngine::setControlStyle(const std::string& key, const ControlStyle& st) {
	std::map<std::string, ControlStyle>::iterator it = _controlStyles.find(key);
	if (it != _controlStyles.end()) {
		// 模组很可能在 onGuiRender 里每帧调一次：内容没变就不要重建布局
		const ControlStyle& o = it->second;
		const bool same = (o.hasRect == st.hasRect) && (o.hidden == st.hidden)
			&& (o.shape == st.shape) && (o.hasOpacity == st.hasOpacity)
			&& (o.opacity == st.opacity) && (o.fill == st.fill)
			&& (o.border == st.border) && (o.borderWidth == st.borderWidth)
			&& (!st.hasRect || (o.x == st.x && o.y == st.y && o.w == st.w && o.h == st.h));
		if (same) return;
	}
	ControlStyle s = st;
	s.used = true;
	_controlStyles[key] = s;
	markHudLayoutChanged();      // 位置/大小/隐藏变了 → 引擎重建触摸布局
}

void ModEngine::clearControlStyle(const std::string& key) {
	if (key.empty()) _controlStyles.clear();
	else             _controlStyles.erase(key);
	markHudLayoutChanged();
}

void ModEngine::setControlRect(const std::string& key, float x, float y, float w, float h, bool visible) {
	ControlRect r;
	r.x = x; r.y = y; r.w = w; r.h = h; r.visible = visible;
	_controlRects[key] = r;
}

const ModEngine::ControlRect* ModEngine::controlRect(const std::string& key) const {
	std::map<std::string, ControlRect>::const_iterator it = _controlRects.find(key);
	return (it == _controlRects.end()) ? NULL : &it->second;
}

// 模组显式接管鼠标键（mouse.take）：接管后原版就不再把这个键当挖方块/放方块
void ModEngine::setMouseTaken(int button, bool take) {
	if (button < 0 || button > 1) return;
	g_modMouseTaken[button] = take;
}

// ── HUD 按键映射按钮 ───────────────────────────────────────────────────────
// 按住时往输入系统注入对应的鼠标键/键盘键；模组收到的还是标准 onMouse/onKey，
// 所以桌面那套逻辑（左键开火、右键开镜/换弹、R 换弹）一行都不用改。
bool ModEngine::hudKeyAt(float gx, float gy, std::string* outKey) const {
	for (size_t i = 0; i < _hudKeys.size(); ++i) {
		const UiHudKey& k = _hudKeys[i];
		if (gx >= (float)k.x && gx < (float)(k.x + k.w) &&
		    gy >= (float)k.y && gy < (float)(k.y + k.h)) {
			if (outKey) *outKey = k.key;
			return true;
		}
	}
	return false;
}

void ModEngine::hudKeyDown(const std::string& key) {
	for (size_t i = 0; i < _hudKeys.size(); ++i) {
		const UiHudKey& k = _hudKeys[i];
		if (k.key != key) continue;
		if (k.mapKind == 0 || k.mapKind == 1) {
			_hudMouseHeld |= (1 << k.mapKind);
			Mouse::feed(k.mapKind == 0 ? MouseAction::ACTION_LEFT : MouseAction::ACTION_RIGHT,
			            1, Mouse::getX(), Mouse::getY());
		} else if (k.mapKind == 2) {
			Keyboard::feed((unsigned char)k.mapCode, 1);
			_hudKeyCodesHeld.push_back(k.mapCode);
		}
		log("hudKeyDown " + key + (k.mapKind == 2 ? (" key=" + std::to_string(k.mapCode))
		                                          : (" mouse=" + std::to_string(k.mapKind))));
		return;
	}
}

void ModEngine::hudKeyUp(const std::string& key) {
	for (size_t i = 0; i < _hudKeys.size(); ++i) {
		const UiHudKey& k = _hudKeys[i];
		if (k.key != key) continue;
		if (k.mapKind == 0 || k.mapKind == 1) {
			_hudMouseHeld &= ~(1 << k.mapKind);
			Mouse::feed(k.mapKind == 0 ? MouseAction::ACTION_LEFT : MouseAction::ACTION_RIGHT,
			            0, Mouse::getX(), Mouse::getY());
		} else if (k.mapKind == 2) {
			Keyboard::feed((unsigned char)k.mapCode, 0);
			for (size_t j = 0; j < _hudKeyCodesHeld.size(); ++j)
				if (_hudKeyCodesHeld[j] == k.mapCode) {
					_hudKeyCodesHeld.erase(_hudKeyCodesHeld.begin() + j);
					break;
				}
		}
		return;
	}
}

// 禁用 / 重载模组时调用：清掉所有 HUD 按钮与按键映射，并把注入的输入回收掉
void ModEngine::clearHudButtons() {
	_hudButtons.clear();
	_hudKeys.clear();
	for (size_t i = 0; i < _hudKeyCodesHeld.size(); ++i)
		Keyboard::feed((unsigned char)_hudKeyCodesHeld[i], 0);
	_hudKeyCodesHeld.clear();
	_hudMouseHeld = 0;
	_hudPointerKeys.clear();
	_hudLayoutVersion++;
}

bool ModEngine::hasStashFnAll(const char* key) const {
	duk_context* mctx = (duk_context*)_ctx;
	if (mctx) {
		duk_push_heap_stash(mctx);
		duk_get_prop_string(mctx, -1, key);
		bool ok = duk_is_function(mctx, -1);
		duk_pop_2(mctx);
		if (ok) return true;
	}
	for (size_t s = 0; s < _sandboxes.size(); ++s) {
		duk_context* ctx = _sandboxes[s].ctx;
		if (!ctx) continue;
		duk_push_heap_stash(ctx);
		duk_get_prop_string(ctx, -1, key);
		bool ok = duk_is_function(ctx, -1);
		duk_pop_2(ctx);
		if (ok) return true;
	}
	return false;
}

// ---- Scripted dimension callbacks (cross sandbox) ----

// Look up __dim_<id>_height across mod heaps; default 64.
float ModEngine::callDimensionHeight(int dimId, int x, int z) {
	std::lock_guard<std::recursive_mutex> _jsLock(_jsMutex);
	std::string fn = "__dim_" + std::to_string(dimId) + "_height";
	for (size_t s = 0; s < _sandboxes.size(); ++s) {
		duk_context* ctx = _sandboxes[s].ctx;
		if (!ctx) continue;
		duk_get_global_string(ctx, fn.c_str());
		if (!duk_is_function(ctx, -1)) {
			duk_pop(ctx);
			continue;
		}
		duk_push_int(ctx, x);
		duk_push_int(ctx, z);
		if (duk_pcall(ctx, 2) != 0) {
			logThrottled("dim-height", std::string("JS error in ") + fn + ": " + duk_safe_to_string(ctx, -1));
			duk_pop(ctx);
			continue;
		}
		float h = (float)duk_get_number(ctx, -1);
		duk_pop(ctx);
		return h;
	}
	return 64.0f;
}

// Look up __dim_<id>_bottom across mod heaps; default 0.
float ModEngine::callDimensionBottom(int dimId, int x, int z) {
	std::lock_guard<std::recursive_mutex> _jsLock(_jsMutex);
	std::string fn = "__dim_" + std::to_string(dimId) + "_bottom";
	for (size_t s = 0; s < _sandboxes.size(); ++s) {
		duk_context* ctx = _sandboxes[s].ctx;
		if (!ctx) continue;
		duk_get_global_string(ctx, fn.c_str());
		if (!duk_is_function(ctx, -1)) {
			duk_pop(ctx);
			continue;
		}
		duk_push_int(ctx, x);
		duk_push_int(ctx, z);
		if (duk_pcall(ctx, 2) != 0) {
			logThrottled("dim-bottom", std::string("JS error in ") + fn + ": " + duk_safe_to_string(ctx, -1));
			duk_pop(ctx);
			continue;
		}
		float h = (float)duk_get_number(ctx, -1);
		duk_pop(ctx);
		return h;
	}
	return 0.0f;
}

// Look up __dim_<id>_chunkgen across mod heaps; true if one ran. The mod
// fills the chunk via a setBlock(lx, y, lz, id[, data]) closure.
extern duk_ret_t jsChunkSetBlock(duk_context* ctx);   // ScriptedChunkSource.cpp
bool ModEngine::callDimensionChunkGen(int dimId, int x, int z, LevelChunk* chunk) {
	std::lock_guard<std::recursive_mutex> _jsLock(_jsMutex);
	std::string fn = "__dim_" + std::to_string(dimId) + "_chunkgen";
	for (size_t s = 0; s < _sandboxes.size(); ++s) {
		duk_context* ctx = _sandboxes[s].ctx;
		if (!ctx) continue;
		duk_get_global_string(ctx, fn.c_str());
		if (!duk_is_function(ctx, -1)) {
			duk_pop(ctx);
			continue;
		}
		duk_push_int(ctx, x);
		duk_push_int(ctx, z);
		// setBlock closure bound to this chunk (same heap as the generator).
		duk_push_c_function(ctx, jsChunkSetBlock, 4);
		duk_push_pointer(ctx, chunk);
		duk_put_prop_string(ctx, -2, "chunk");
		if (duk_pcall(ctx, 3) != 0) {
			log(std::string("[chunkgen] JS error in ") + fn + ": " + duk_safe_to_string(ctx, -1));
			duk_pop(ctx);
			continue;
		}
		duk_pop(ctx);
		return true;
	}
	return false;
}

// Resource-pack style override: overwrite a rectangular region of an atlas
// (e.g. "gui/gui.png", "terrain.png", "gui/title.png") with a package png.
// core: blit raw RGBA (w x h) at (dstX,dstY); wholeImage path rebuilds CPU cache.
static bool replaceImageCore(Minecraft* mc, ModEngine* me, const std::string& texPath,
	int dstX, int dstY, const std::vector<unsigned char>& rgba, int w, int h) {
#ifndef STANDALONE_SERVER
	TextureId texId = mc->textures->loadAndBindTexture(texPath.c_str());
	if (texId == Textures::InvalidId) {
		me->log("replaceImage: cannot load " + texPath);
		return false;
	}
	glBindTexture(GL_TEXTURE_2D, (GLuint)texId);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	// 整图替换（dst=0,0 且 mod 图尺寸 == 目标整图尺寸）：glTexImage2D 整张重建
	// 并同步 CPU 缓存（loadAndGetTextureData 返回替换后 w/h —— 0.8.1 标题
	// 变形根因修复）。否则是图集槽位/区域矩形覆盖：只 glTexSubImage2D 改显存，
	// 不碰 CPU 缓存（unload 还原时两者一致）。
	TextureData* td = mc->textures->loadAndGetTextureData(texPath.c_str());
	bool wholeImage = (td && dstX == 0 && dstY == 0 && w == td->w && h == td->h);
	if (wholeImage) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
		mc->textures->replaceTextureData(texPath.c_str(), w, h, rgba.data());
	} else {
		glTexSubImage2D(GL_TEXTURE_2D, 0, dstX, dstY, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
	}
	// Remember the texture so a later mod reload can revert it.
	bool known = false;
	for (size_t i = 0; i < me->_overriddenTextures.size(); ++i)
		if (me->_overriddenTextures[i] == texPath) { known = true; break; }
	if (!known) me->_overriddenTextures.push_back(texPath);
	return true;
#else
	return false;
#endif
}

bool ModEngine::replaceImage(const std::string& texPath, int dstX, int dstY, const std::string& srcPng) {
#ifndef STANDALONE_SERVER
	std::map<std::string, ModPixelData>::iterator it = _modTexturePixels.find(srcPng);
	if (it == _modTexturePixels.end()) {
		log("replaceImage: no pixels for " + srcPng);
		return false;
	}
	Minecraft* mc = minecraft();
	if (!mc || !mc->textures) return false;
	const ModPixelData& pd = it->second;
	bool ok = replaceImageCore(mc, this, texPath, dstX, dstY, pd.rgba, pd.w, pd.h);
	log(std::string("replaceImage ") + srcPng + " -> " + texPath + " at (" + std::to_string(dstX) + "," + std::to_string(dstY) + ") size " + std::to_string(pd.w) + "x" + std::to_string(pd.h) + (ok ? "" : " FAILED"));
	return ok;
#else
	return false;
#endif
}

bool ModEngine::replaceImageScaled(const std::string& texPath, int dstX, int dstY, const std::string& srcPng, int scaleW, int scaleH) {
#ifndef STANDALONE_SERVER
	std::map<std::string, ModPixelData>::iterator it = _modTexturePixels.find(srcPng);
	if (it == _modTexturePixels.end()) {
		log("replaceImageScaled: no pixels for " + srcPng);
		return false;
	}
	Minecraft* mc = minecraft();
	if (!mc || !mc->textures) return false;
	const ModPixelData& pd = it->second;
	if (scaleW <= 0 || scaleH <= 0) { log("replaceImageScaled: bad scale"); return false; }
	// nearest-neighbour scale into a 48x48 (or scaleW x scaleH) buffer
	std::vector<unsigned char> out((size_t)scaleW * scaleH * 4);
	for (int y = 0; y < scaleH; ++y) {
		int sy = (y * pd.h) / scaleH;
		if (sy >= pd.h) sy = pd.h - 1;
		for (int x = 0; x < scaleW; ++x) {
			int sx = (x * pd.w) / scaleW;
			if (sx >= pd.w) sx = pd.w - 1;
			const unsigned char* s = &pd.rgba[(sy * pd.w + sx) * 4];
			unsigned char* d = &out[(y * scaleW + x) * 4];
			d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3];
		}
	}
	bool ok = replaceImageCore(mc, this, texPath, dstX, dstY, out, scaleW, scaleH);
	log(std::string("replaceImageScaled ") + srcPng + " -> " + texPath + " at (" + std::to_string(dstX) + "," + std::to_string(dstY) + ") -> " + std::to_string(scaleW) + "x" + std::to_string(scaleH));
	return ok;
#else
	return false;
#endif
}

// JS Assets.replaceImage(texturePath, dstX, dstY, srcPngPath)
static duk_ret_t jsAssetsReplaceImage(duk_context* ctx) {
	ModEngine* me = ModEngine::instance;
	if (!me) return 0;
	const char* texPath = duk_safe_to_string(ctx, 0);
	int dstX = (int)duk_get_int(ctx, 1);
	int dstY = (int)duk_get_int(ctx, 2);
	const char* srcPng = duk_safe_to_string(ctx, 3);
	if (!me->replaceImage(texPath, dstX, dstY, srcPng))
		me->log("replaceImage failed: " + std::string(srcPng) + " -> " + std::string(texPath));
	return 0;
}

// Assets.replaceBlockIcon(blockId, srcPngPath)
// 覆盖方块在背包/物品栏图集(gui_blocks.png)里的图标槽——terrain.png 的
// replaceImage 只管世界里的方块纹理，背包图标的槽位映射在 gui_blocks。
static duk_ret_t jsAssetsReplaceBlockIcon(duk_context* ctx) {
#ifdef STANDALONE_SERVER
	(void)ctx; return 0;   // 服务器没有贴图图集
#else
	ModEngine* me = ModEngine::instance;
	if (!me) return 0;
	int blockId = (int)duk_get_int(ctx, 0);
	const char* srcPng = duk_safe_to_string(ctx, 1);
	if (!srcPng[0]) { me->log("replaceBlockIcon: png path required"); return 0; }
	int slot = ItemRenderer::getGuiBlocksSlot(blockId);
	if (slot < 0) {
		me->log("replaceBlockIcon: block " + std::to_string(blockId) + " has no gui_blocks icon");
		return 0;
	}
	// gui_blocks.png 是 512x512；slot<128 时 10 列 × 48px（见 ItemRenderer）。
	// 图标槽整块 48x48 会被缩到 16px 显示，mod 图按任意尺寸放大填满槽。
	int dstX = (slot % 10) * 48;
	int dstY = (slot / 10) * 48;
	if (!me->replaceImageScaled("gui/gui_blocks.png", dstX, dstY, srcPng, 48, 48))
		me->log("replaceBlockIcon failed: " + std::string(srcPng) + " -> gui_blocks slot " + std::to_string(slot));
	return 0;
#endif
}

bool ModEngine::setEnabled(const std::string& file, bool enabled) {
	log("setEnabled(" + file + ", " + (enabled ? "true" : "false") + ") called");
	std::vector<std::string> list = loadEnabledList();
	bool found = false;
	for (size_t i = 0; i < list.size(); ++i)
		if (list[i] == file) { found = true; break; }
	if (enabled && !found) {
		list.push_back(file);
		saveEnabledList(list);
		log("enabling mod: " + file);
	} else if (!enabled && found) {
		std::vector<std::string> nlist;
		for (size_t i = 0; i < list.size(); ++i)
			if (list[i] != file) nlist.push_back(list[i]);
		saveEnabledList(nlist);
		log("disabling mod: " + file);
	} else {
		return true;
	}
	// Rebuild the whole JS environment so the toggle applies immediately
	// (a disabled mod's scripts/entities must not stay active in-game).
	reloadEnabledMods();
	return true;
}

// ---- Multiplayer mod auto-sync ---------------------------------------------

// Only bare .zip file names are accepted (no separators / traversal).
static bool validModFileName(const std::string& file) {
	if (file.size() < 5 || file.size() > 200)
		return false;
	if (file.find('\\') != std::string::npos || file.find('/') != std::string::npos ||
		file.find(':') != std::string::npos || file[0] == '.')
		return false;
	if (!endsWithLower(file, ".zip"))
		return false;
	for (size_t i = 0; i < file.size(); ++i) {
		char c = file[i];
		bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
		if (!ok) return false;
	}
	return true;
}

std::vector<std::string> ModEngine::getEnabledModFiles() {
	return loadEnabledList();
}

long ModEngine::modFileSize(const std::string& file) {
	if (!validModFileName(file))
		return -1;
	std::string path = modFilePath(file);
	FILE* f = fopen(path.c_str(), "rb");
	if (!f) return -1;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fclose(f);
	return sz;
}

bool ModEngine::readModFileBytes(const std::string& file, std::vector<unsigned char>& out) {
	out.clear();
	if (!validModFileName(file))
		return false;
	long sz = modFileSize(file);
	if (sz <= 0 || sz > 64L * 1024 * 1024)
		return false;
	std::string path = modFilePath(file);
	FILE* f = fopen(path.c_str(), "rb");
	if (!f) return false;
	out.resize((size_t)sz);
	size_t got = fread(out.data(), 1, (size_t)sz, f);
	fclose(f);
	return got == (size_t)sz;
}

bool ModEngine::writeModFileBytes(const std::string& file, const std::vector<unsigned char>& data) {
	if (!validModFileName(file))
		return false;
	if (data.size() > 64L * 1024 * 1024)
		return false;
	std::string path = modFilePath(file);
	FILE* f = fopen(path.c_str(), "wb");
	if (!f) return false;
	size_t wrote = data.empty() ? 0 : fwrite(data.data(), 1, data.size(), f);
	fclose(f);
	return wrote == data.size();
}

bool ModEngine::applyEnabledList(const std::vector<std::string>& files) {
	std::vector<std::string> clean;
	for (size_t i = 0; i < files.size() && i < 256; ++i)
		if (validModFileName(files[i]))
			clean.push_back(files[i]);
	saveEnabledList(clean);
	if (_ctx)
		reloadEnabledMods();
	log("applyEnabledList: enabled " + std::to_string(clean.size()) + " mod(s)");
	return true;
}

void ModEngine::reloadEnabledMods() {
	if (!_ctx)
		return;
	// Unregister mod custom sounds first (their SoundDesc::frames point into
	// _modSoundPcm); freeing the PCM before dropping the registrations would
	// leave dangling frames in the SoundRepository. Also stops the previous
	// reload's registrations from accumulating duplicates.
	if (minecraft() && minecraft()->soundEngine) {
		for (size_t i = 0; i < _modSoundNames.size(); ++i)
			minecraft()->soundEngine->unregisterCustomSound(_modSoundNames[i]);
	}
	_modSoundNames.clear();
	_modSoundPcm.clear();
	// Destroy per-mod sandbox heaps first.
	for (size_t i = 0; i < _sandboxes.size(); ++i)
		if (_sandboxes[i].ctx)
			duk_destroy_heap(_sandboxes[i].ctx);
	_sandboxes.clear();
	duk_destroy_heap((duk_context*)_ctx);
	_ctx = NULL;
	// Free scripted mob heap objects (boxes/model are owned, new'd in defineMob).
	for (std::map<int, ScriptedMobDef>::iterator it = _scriptedMobs.begin(); it != _scriptedMobs.end(); ++it) {
		delete it->second.boxes;
		delete it->second.model;
	}
	_scriptedMobs.clear();
	_timers.clear();
	_scriptedDimensions.clear();
	// 模组群系也要清：否则禁用的模组会留下“幽灵群系”（地形里还在用，却没
	// JS 回调了）。实例是引擎 new 的，这里负责 delete。
	for (std::map<int, Biome*>::iterator bit = _scriptedBiomeObjs.begin(); bit != _scriptedBiomeObjs.end(); ++bit)
		delete bit->second;
	_scriptedBiomeObjs.clear();
	_scriptedBiomes.clear();
	_biomeCache.clear();
	_biomeDistDims.clear();
	_modTextures.clear();
	_modTexturePixels.clear();
	g_projectiles.clear();    // 投射物定义也要清，reload 后由模组重新注册
	// 模组注册的命令名/简述也要清：下面整块 JS 环境（含各模组堆）会重建，
	// 旧名字留着的话补全列表还会列出已被禁用的模组指令，而那已经没有处理
	// 函数了 —— 点了/输了只会得到“未找到该指令”。
	_commandNames.clear();
	_commandDescs.clear();
	// UI overrides (09): clear so a disabled mod restores the vanilla UI.
	_uiScreens.clear();
	// HUD 按钮/按键映射也一样：禁用/重载模组后不能残留旧矩形（否则那片区域会
	// 一直被当成「模组按钮」，既不挖方块也转不了视角）；注入的键也要回收。
	clearHudButtons();
	// Note: g_nextBlockSlot is NOT reset here. Slots stay allocated so a
	// reload reuses them via _modBlockSlotByPath instead of burning new ones
	// (and a reset to 255 would break the >=256 guard, making every later
	// injectModBlockTexture fail). Fresh process -> static init 511.
	// Drop stale mod-registered tiles/items so a reload can re-register
	// them without "already occupied" conflicts.
	for (size_t i = 0; i < _modTiles.size(); ++i) {
		if (_modTiles[i] > 0 && _modTiles[i] < 256) {
			// Clear the ticking flag too: a stale shouldTick entry would make
			// Level::tickTiles call Tile::tiles[id]->tick() on a NULL slot
			// (crash) after we drop the tile below.
			Tile::shouldTick[_modTiles[i]] = false;
			if (Tile::tiles[_modTiles[i]])
				Tile::tiles[_modTiles[i]] = NULL;
		}
	}
	for (size_t i = 0; i < _modItems.size(); ++i) {
		if (_modItems[i] > 0 && _modItems[i] < Item::MAX_ITEMS && Item::items[_modItems[i]])
			Item::items[_modItems[i]] = NULL;
	}
	_modTiles.clear();
	_modItems.clear();
	_creativeItems.clear();
	// Drop stale physical->logical mappings: tiles are re-registered below
	// and a leftover mapping would mis-translate ModTile events after a
	// different mod combination reallocates the physical slots.
	_modBlockPhysId.clear();

	duk_context* ctx = duk_create_heap(NULL, NULL, NULL, NULL, modDukFatal);
	if (!ctx) {
		log("reloadEnabledMods: heap creation failed");
		return;
	}
	_ctx = ctx;
	registerBindings(ctx);
	loadConfig();
	log("reloadEnabledMods: rebuilding from modlist.json");
	loadEnabledMods();
}

void ModEngine::loadEnabledMods() {
	clearModErrors();
	// Revert resource-pack style overrides from the previous load so a
	// disabled mod restores the vanilla assets (then enabled mods re-apply
	// theirs below via replaceImage).
#ifndef STANDALONE_SERVER
	// 还原资源包式贴图覆盖（服务器没有贴图，直接跳过）
#ifndef STANDALONE_SERVER
	// 还原资源包式贴图覆盖（服务器没有贴图，直接跳过）
#ifndef STANDALONE_SERVER
	// 还原资源包式贴图覆盖（服务器没有贴图，直接跳过）
	if (minecraft() && minecraft()->textures) {
		for (size_t i = 0; i < _overriddenTextures.size(); ++i)
			minecraft()->textures->unloadTexture(_overriddenTextures[i]);
		_overriddenTextures.clear();
	}
#endif
#endif
#endif
	std::vector<ModInfo> mods = scanMods();
	int loaded = 0;
	for (size_t i = 0; i < mods.size(); ++i) {
		if (!mods[i].enabled) continue;
		std::string path = modFilePath(mods[i].file);
		log("loading mod: " + mods[i].name);
		// Each enabled mod gets its own independent Duktape heap so top-level
		// globals never collide between mods. Engine bindings are registered
		// per-heap; event handlers are collected per-heap afterwards.
		duk_context* mctx = duk_create_heap(NULL, NULL, NULL, NULL, modDukFatal);
		if (!mctx) {
			log("loadEnabledMods: heap creation failed for " + mods[i].name);
			continue;
		}
		registerBindings(mctx);
		ModSandbox sb;
		sb.name = mods[i].name;
		sb.ctx = mctx;
		_sandboxes.push_back(sb);
		if (runScriptZip(path, mctx)) {
			registerModEvents(mctx);
			++loaded;
		}
	}
	log("loadEnabledMods: loaded " + std::to_string(loaded) + " mod(s)");
}

bool ModEngine::evalScript(const std::string& src, const std::string& displayName) {
	return evalScriptInto((duk_context*)_ctx, src, displayName);
}

bool ModEngine::evalScriptInto(duk_context* ctx, const std::string& src, const std::string& displayName) {
	if (!ctx)
		return false;
	if (duk_peval_string(ctx, src.c_str()) != 0) {
		const char* err = duk_safe_to_string(ctx, -1);
		std::string msg = "JS error in ";
		msg += displayName;
		msg += ": ";
		msg += err;
		log(msg);
		_modErrors[displayName] = err;
		duk_pop(ctx);
		return false;
	}
	duk_pop(ctx);
	return true;
}

bool ModEngine::runScript(const std::string& path) {
	FILE* f = fopen(path.c_str(), "rb");
	if (!f)
		return false;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz < 0) {
		fclose(f);
		return false;
	}
	std::vector<char> buf((size_t)sz + 1, 0);
	if (sz > 0)
		fread(buf.data(), 1, (size_t)sz, f);
	fclose(f);
	return evalScript(std::string(buf.data(), (size_t)sz), path);
}

// --- Mod packages (.zip) ---

// libpng read callback that consumes from a memory buffer.
namespace {
struct PngMemSrc {
	const unsigned char* p;
	size_t left;
};
void pngReadMemFn(png_structp png, png_bytep data, png_size_t len) {
	PngMemSrc* m = (PngMemSrc*)png_get_io_ptr(png);
	size_t n = (len < m->left) ? len : m->left;
	memcpy(data, m->p, n);
	m->p += n;
	m->left -= n;
}
bool decodePngMem(const unsigned char* data, size_t size, int& w, int& h, std::vector<unsigned char>& out) {
	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png)
		return false;
	png_infop info = png_create_info_struct(png);
	if (!info) {
		png_destroy_read_struct(&png, NULL, NULL);
		return false;
	}
	if (setjmp(png_jmpbuf(png))) {
		png_destroy_read_struct(&png, &info, NULL);
		return false;
	}
	PngMemSrc m = { data, size };
	png_set_read_fn(png, &m, pngReadMemFn);
	png_read_info(png, info);
	w = (int)png_get_image_width(png, info);
	h = (int)png_get_image_height(png, info);
	png_set_expand(png);        // palettes / low bit depth -> 8-bit
	png_set_strip_16(png);      // 16-bit -> 8-bit
	png_set_gray_to_rgb(png);   // gray -> RGB
	png_set_add_alpha(png, 0xff, PNG_FILLER_AFTER); // RGB -> RGBA
	png_read_update_info(png, info);
	size_t rowbytes = png_get_rowbytes(png, info);
	out.resize(rowbytes * (size_t)h);
	std::vector<png_bytep> rows((size_t)h);
	for (int y = 0; y < h; ++y)
		rows[(size_t)y] = out.data() + (size_t)y * rowbytes;
	png_read_image(png, rows.data());
	png_read_end(png, info);
	png_destroy_read_struct(&png, &info, NULL);
	return true;
}
unsigned int makeGlTexture(int w, int h, const unsigned char* rgba) {
	GLuint id = 0;
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
	return (unsigned int)id;
}
} // namespace

// Parse a RIFF/WAVE file (PCM only) into a SoundDesc + raw PCM bytes.
static bool parseWavData(const std::vector<unsigned char>& data, SoundDesc& desc, std::vector<unsigned char>& pcm) {
	if (data.size() < 44 || memcmp(&data[0], "RIFF", 4) != 0 || memcmp(&data[8], "WAVE", 4) != 0)
		return false;
	size_t pos = 12;
	int channels = 1, rate = 22050, bits = 16;
	while (pos + 8 <= data.size()) {
		unsigned int sz = (unsigned)data[pos+4] | ((unsigned)data[pos+5] << 8) |
			((unsigned)data[pos+6] << 16) | ((unsigned)data[pos+7] << 24);
		if (memcmp(&data[pos], "fmt ", 4) == 0 && sz >= 16) {
			channels = data[pos+10] | (data[pos+11] << 8);
			rate = data[pos+12] | (data[pos+13] << 8) | (data[pos+14] << 16) | (data[pos+15] << 24);
			bits = data[pos+22] | (data[pos+23] << 8);
			// WAV spec: bitsPerSample is at fmt-chunk offset 22 (data[pos+22]
			// == absolute byte 34 for a standard 44-byte header). Earlier we
			// "fixed" this to pos+28 which lands inside the data chunk and
			// made OpenAL misread the format -> screechy horror-movie audio.
		} else if (memcmp(&data[pos], "data", 4) == 0) {
			size_t end = pos + 8 + sz;
			if (end > data.size()) end = data.size();
			pcm.assign(data.begin() + pos + 8, data.begin() + end);
			break;
		}
		pos += 8 + sz + (sz & 1);
	}
	if (pcm.empty() || bits == 0)
		return false;
	desc = SoundDesc((char*)pcm.data(), (int)pcm.size(), channels, bits / 8, rate);
	return true;
}

// Decode an ogg/vorbis stream (from the mod zip) straight to 16-bit PCM with
// stb_vorbis - original sample rate preserved, no lossy re-encode.
static bool parseOggData(const std::vector<unsigned char>& data, SoundDesc& desc, std::vector<unsigned char>& pcm) {
	if (data.size() < 8 || memcmp(&data[0], "OggS", 4) != 0)
		return false;
	int channels = 0, rate = 0;
	short* out = NULL;
	int frames = stb_vorbis_decode_memory(&data[0], (int)data.size(), &channels, &rate, &out);
	if (frames <= 0 || !out || channels <= 0 || rate <= 0) {
		if (out) free(out);
		return false;
	}
	int bytes = frames * channels * 2;
	pcm.assign((const unsigned char*)out, (const unsigned char*)out + bytes);
	free(out);
	desc = SoundDesc((char*)pcm.data(), bytes, channels, 2, rate);
	return true;
}

bool ModEngine::runScriptZip(const std::string& path, duk_context* ctx) {
	std::vector<ModZipEntry> entries;
	if (!modZipRead(path, entries)) {
		log("zip mod failed to open: " + path);
		return false;
	}
	const ModZipEntry* main = modZipFind(entries, "main.js");
	if (!main)
		main = modZipFind(entries, "index.js");
	if (!main) {
		log("zip mod has no main.js: " + path);
		return false;
	}
	std::vector<unsigned char> src;
	if (!modZipExtract(path, *main, src)) {
		log("zip mod: failed to extract main.js");
		return false;
	}
	// Register every .png in the package BEFORE running main.js so the
	// script can reference textures (entities, particles) right away.
	for (size_t i = 0; i < entries.size(); ++i) {
		const std::string& n = entries[i].name;
		if (n.size() < 4)
			continue;
		std::string lower = n.substr(n.size() - 4);
		for (size_t k = 0; k < lower.size(); ++k)
			lower[k] = (char)tolower((unsigned char)lower[k]);
		if (lower != ".png")
			continue;
#ifdef STANDALONE_SERVER
		// 服务器不渲染（没有 GL 上下文、没有贴图库）：把 PNG 解码出来再上传纯属
		// 白干，贴图多的 mod 能把服务器内存吃掉一大块。只记个名字占位，
		// 让按名字查贴图的代码不至于拿到空指针。
		_modTextures[n] = 0;
		continue;
#endif
		std::vector<unsigned char> img;
		if (!modZipExtract(path, entries[i], img)) {
			log("zip mod: failed to extract " + n);
			continue;
		}
		int w = 0, h = 0;
		std::vector<unsigned char> rgba;
		if (!decodePngMem(img.data(), img.size(), w, h, rgba)) {
			log("zip mod: failed to decode png " + n);
			continue;
		}
		unsigned int texId = makeGlTexture(w, h, rgba.data());
		_modTextures[n] = texId;
		ModPixelData pd;
		pd.rgba = rgba;
		pd.w = w;
		pd.h = h;
		_modTexturePixels[n] = pd;
		log("zip mod texture: " + n + " (" + std::to_string(w) + "x" + std::to_string(h) + ")");
	}
	// Register every .wav/.ogg under sounds/ BEFORE running main.js so the
	// script can play them via level.playSound("name", ...) where name is the
	// file name without the extension. .ogg is decoded to PCM via stb_vorbis
	// (original-quality source audio, no lossy conversion).
	for (size_t i = 0; i < entries.size(); ++i) {
		const std::string& n = entries[i].name;
		if (n.size() < 12 || n.compare(0, 7, "sounds/") != 0)
			continue;
		std::string lower = n.substr(n.size() - 4);
		for (size_t k = 0; k < lower.size(); ++k)
			lower[k] = (char)tolower((unsigned char)lower[k]);
		if (lower != ".wav" && lower != ".ogg")
			continue;
#ifdef STANDALONE_SERVER
		// 服务器不播音：音频解码（stb_vorbis 解 ogg 很吃 CPU 与内存）同样白干。
		// 只记名字（mod 脚本可能按名字查有没有这个声音）。
		_modSoundNames.push_back(n.substr(7, n.size() - 7 - 4));
		continue;
#endif
		std::vector<unsigned char> raw;
		if (!modZipExtract(path, entries[i], raw)) {
			log("zip mod: failed to extract " + n);
			continue;
		}
		SoundDesc desc;
		std::vector<unsigned char> pcm;
		bool ok = (lower == ".ogg") ? parseOggData(raw, desc, pcm) : parseWavData(raw, desc, pcm);
		if (!ok) {
			log("zip mod: bad/unsupported audio " + n);
			continue;
		}
		_modSoundPcm.push_back(pcm);
		desc.frames = (char*)_modSoundPcm.back().data();  // re-point after the move
		std::string name = n.substr(7, n.size() - 7 - 4); // "sounds/foo.wav" -> "foo"
		if (minecraft() && minecraft()->soundEngine)
			minecraft()->soundEngine->registerCustomSound(name, desc);
		_modSoundNames.push_back(name);
		log("zip mod sound: " + name + " (" + lower + ")");
	}

	log("running zip mod: " + path);
	bool ok = evalScriptInto(ctx ? ctx : (duk_context*)_ctx, std::string((const char*)src.data(), src.size()), path);
	// NOTE: event registration is done by the caller (loadEnabledMods) once
	// per sandbox; do not register here or handlers would run twice.
	return ok;
}

// After a mod's main.js runs, copy its global event functions (onTick,
// onChat, ...) into per-event registries so multiple mods can coexist
// without one overwriting another's handlers. The registries live in the
// Duktape registry, keyed "__mod_evt_<name>" -> array of functions.
void ModEngine::registerModEvents(duk_context* ctx) {
	if (!ctx)
		return;
	static const char* evts[] = {
		"onTick", "onChat", "onJoinWorld", "onBreakBlock",
		"onPlaceBlock", "onMobTick", "onGuiRender",
		"onJump", "onAttack", "onEatFood", "onMobJump",
		"onLeaveWorld", "onRenderComposite", "onKey",
		"onPlayerTick", "onMouse",
		// 群系：玩家跨群系时触发（ModEngine::tickBiomeTracking）。
		"onBiomeEnter", "onBiomeLeave"
	};
	const int numEvts = (int)(sizeof(evts) / sizeof(evts[0]));
	for (int i = 0; i < numEvts; ++i) {
		const char* name = evts[i];
		std::string key = std::string("__mod_evt_") + name;
		duk_push_heap_stash(ctx);
		duk_get_prop_string(ctx, -1, key.c_str());
		if (!duk_is_array(ctx, -1)) {
			duk_pop(ctx);
			duk_push_array(ctx);
			duk_dup(ctx, -1);
			duk_put_prop_string(ctx, -3, key.c_str());
		}
		duk_get_global_string(ctx, name);
		if (duk_is_function(ctx, -1)) {
			int idx = (int)duk_get_length(ctx, -2);
			duk_put_prop_index(ctx, -2, idx);
			_registeredEvents.insert(name);
			log("registered event " + std::string(name));
		} else {
			duk_pop(ctx);
		}
		duk_pop_2(ctx);
	}
}

unsigned int ModEngine::getModTexture(const std::string& path) const {
	std::map<std::string, unsigned int>::const_iterator it = _modTextures.find(path);
	return (it != _modTextures.end()) ? it->second : 0;
}

bool ModEngine::getModTextureSize(const std::string& path, int& w, int& h) const {
	std::map<std::string, ModPixelData>::const_iterator it = _modTexturePixels.find(path);
	if (it == _modTexturePixels.end()) return false;
	w = it->second.w;
	h = it->second.h;
	return true;
}

unsigned int ModEngine::getModCoverTexture(const std::string& file) {
	// Cached per mod file name.
	std::map<std::string, unsigned int>::const_iterator hit = _modCovers.find(file);
	if (hit != _modCovers.end())
		return hit->second;

	unsigned int texId = 0;
	std::string full = fullModsDir() + "/" + file;
	std::vector<ModZipEntry> entries;
	if (modZipRead(full, entries)) {
		for (size_t i = 0; i < entries.size(); ++i) {
			const std::string& n = entries[i].name;
			if (n.size() < 8) continue;
			if (n.compare(0, 6, "cover/") != 0 && n.compare(0, 6, "COVER/") != 0)
				continue;
			size_t dot = n.rfind('.');
			if (dot == std::string::npos) continue;
			std::string ext = n.substr(dot);
			for (size_t k = 0; k < ext.size(); ++k)
				ext[k] = (char)tolower((unsigned char)ext[k]);
			if (ext != ".png" && ext != ".jpg" && ext != ".jpeg")
				continue;
			std::vector<unsigned char> img;
			if (!modZipExtract(full, entries[i], img))
				continue;
			int w = 0, h = 0;
			std::vector<unsigned char> rgba;
			if (!decodePngMem(img.data(), img.size(), w, h, rgba))
				continue;
			texId = makeGlTexture(w, h, rgba.data());
			_modCoverSizes[file] = std::make_pair(w, h);
			break;
		}
	}
	_modCovers[file] = texId;
	return texId;
}

bool ModEngine::getModCoverSize(const std::string& file, int& w, int& h) {
	// 确保已尝试解码（getModCoverTexture 会填充 _modCoverSizes）
	getModCoverTexture(file);
	std::map<std::string, std::pair<int,int> >::const_iterator it = _modCoverSizes.find(file);
	if (it == _modCoverSizes.end())
		return false;
	w = it->second.first;
	h = it->second.second;
	return w > 0 && h > 0;
}

// Buffered log file: the FILE* is opened once and reused (with fflush after
// every write) instead of fopen/fprintf/fclose per call. Mods that log from
// per-tick JS (or error handlers) were doing a disk open+close every tick,
// which showed up as huge frame spikes.
static void closeModLogFile() {
	if (s_modLogFile) {
		fclose(s_modLogFile);
		s_modLogFile = NULL;
	}
}

// Runtime mod-log switch (see ModEngine.h): startup phase and F3 debug write
// the log; plain gameplay does not (avoids per-call fopen/fflush disk I/O).
bool g_modLogStartupPhase = true;
bool g_modLogF3Debug = false;

void ModEngine::log(const std::string& msg, bool force) {
	// Default: silent. Logging every event to disk (with fflush) was a real
	// in-frame cost; keep it for the startup phase, F3 debugging, and 模组自己
	// 主动调 modLog() 的时候（force）。
	if (!force && !g_modLogStartupPhase && !g_modLogF3Debug)
		return;
	// 日志可能来自加载线程（例如脚本维度加载报错）。和 JS 入口共一把锁，
	// 避免两个线程同时写同一个 FILE*。
	std::lock_guard<std::recursive_mutex> _jsLock(_jsMutex);

#if defined(__ANDROID__)
	__android_log_print(ANDROID_LOG_INFO, "mcpe_mod", "%s", msg.c_str());
	std::string dir = kLogPath;
	size_t sl = dir.find_last_of('/');
	if (sl != std::string::npos) { dir = dir.substr(0, sl); mkdirs(dir); }
#endif
	// 服务器没有 F3 面板可看，日志必须同时上控制台，否则 mod 出问题没法排查。
	// 文件那份在服务器的 logs/mod.log，这里这份是给“当场看得见”用的。
#if defined(STANDALONE_SERVER)
	LOGI("[mod] %s\n", msg.c_str());
	// 服务器写自己目录下的 logs/mod.log。不直接用 kLogPath：上面那个路径把某台
	// 机器上的 Windows 用户名写死了，换台电脑/换个用户名就写不进去，日志静默消失。
	static const char* kServerLogPath = "logs/mod.log";
	const char* logPath = kServerLogPath;
#else
	const char* logPath = kLogPath;
#endif
	if (!s_modLogFile)
		s_modLogFile = fopen(logPath, "a");
	if (s_modLogFile) {
		fprintf(s_modLogFile, "[mod] %s\n", msg.c_str());
		fflush(s_modLogFile);
	}
}

// 模组主动打的日志（JS 的 modLog）：绕开"启动阶段 / F3"门控 —— 模组自己要求
// 记的就得记下来，这样排查问题时不用改引擎。代价是**每秒限流 20 条**，免得
// 模组在循环里打日志把帧率和磁盘拖垮（性能优先）。
void ModEngine::modLog(const std::string& msg) {
	static long long s_sec = -1;
	static int s_count = 0;
	long long sec = _tickCount / 20;   // 按游戏刻折算的"秒"
	if (sec != s_sec) {
		s_sec = sec;
		s_count = 0;
	}
	if (++s_count > 20)
		return;
	log(msg, true);
}

void ModEngine::logThrottled(const std::string& event, const std::string& msg) {
	std::lock_guard<std::recursive_mutex> _jsLock(_jsMutex);
	static long long lastLog = 0;
	static std::string lastEvent;
	long long now = getTimeMs();
	if (event == lastEvent && now - lastLog < 2000)
		return;  // same event errored <2s ago: skip (log would repeat every tick)
	lastEvent = event;
	lastLog = now;
	log(msg);
}

// ---------------------------------------------------------------------------
// Timers, config, error tracking
// ---------------------------------------------------------------------------

void ModEngine::addTimer(int handle, const std::string& fnKey, int ms, bool repeat) {
	ModTimer t;
	t.handle = handle;
	t.fnKey = fnKey;
	t.interval = (ms < 10) ? 10 : ms;
	t.nextMs = getTimeMs() + t.interval;
	t.repeat = repeat;
	_timers.push_back(t);
}

void ModEngine::clearTimer(int handle) {
	for (size_t i = 0; i < _timers.size(); ++i) {
		if (_timers[i].handle == handle) {
			_timers.erase(_timers.begin() + i);
			return;
		}
	}
}

void ModEngine::tick() {
	// 游戏刻计数（给 level.getTicks() 用；模组靠它计时/节流）。
	++_tickCount;
	// mod 状态延迟落盘（每 ~0.5 秒一次；mod 频繁调 setModState 也不会卡）。
	flushModState(false);
	// 微方块几何补算 + 动画方块登记（都在主线程做：区块重建可能跑在后台
	// 线程，那里不能读 TileEntity / 不能进 Duktape）。
	pumpCellGeometry();
	pumpAnimatedBlocks();
	// 和 JS 入口共锁：加载线程可能在生成脚本维度地形，两边不能同时进 Duktape。
	std::lock_guard<std::recursive_mutex> _jsLock(_jsMutex);
	// Stage 5: open a screen the mod requested during an event callback
	// (e.g. onChat). Deferring past the callback avoids the caller's own
	// setScreen(NULL) (ChatInputScreen::submit) overriding our screen via
	// the screenMutex scheduling.
#ifndef STANDALONE_SERVER
	// 模组请求打开的自定义界面（服务器没有界面可开）
#ifndef STANDALONE_SERVER
	// 模组请求打开的自定义界面（服务器没有界面可开）
#ifndef STANDALONE_SERVER
	// 模组请求打开的自定义界面（服务器没有界面可开）
	if (_pendingScreen) {
		ScriptedScreen* s = _pendingScreen;
		_pendingScreen = NULL;
		if (minecraft())
			minecraft()->setScreen(s);
	}
#endif
#endif
#endif
	if (_timers.empty())
		return;
	long long now = getTimeMs();
	duk_context* ctx = (duk_context*)_ctx;
	// Collect due handles first, then dispatch: a callback may call
	// clearTimer() (erasing entries from _timers), which would otherwise
	// shift indices mid-iteration and mis-handle the next entry.
	std::vector<int> dueHandles;
	for (size_t k = 0; k < _timers.size(); ++k) {
		if (now >= _timers[k].nextMs)
			dueHandles.push_back(_timers[k].handle);
	}
	for (size_t k = 0; k < dueHandles.size(); ++k) {
		int h = dueHandles[k];
		// Re-locate by handle: an earlier callback may have cleared this one.
		size_t idx = (size_t)-1;
		for (size_t m = 0; m < _timers.size(); ++m)
			if (_timers[m].handle == h) { idx = m; break; }
		if (idx == (size_t)-1)
			continue;  // already removed by a callback
		bool repeat = _timers[idx].repeat;
		// The timer callback lives in the defining mod's own heap.
		bool fired = false;
		for (size_t s = 0; s < _sandboxes.size(); ++s) {
			duk_context* sctx = _sandboxes[s].ctx;
			if (!sctx) continue;
			duk_get_global_string(sctx, _timers[idx].fnKey.c_str());
			if (!duk_is_function(sctx, -1)) {
				duk_pop(sctx);
				continue;
			}
			if (duk_pcall(sctx, 0) != 0) {
				std::string msg = "JS timer error: ";
				msg += duk_safe_to_string(sctx, -1);
				log(msg);
				duk_pop(sctx);
			} else {
				duk_pop(sctx);
			}
			fired = true;
			break;
		}
		if (!fired) {
			duk_get_global_string(ctx, _timers[idx].fnKey.c_str());
			if (duk_is_function(ctx, -1)) {
				if (duk_pcall(ctx, 0) != 0) {
					std::string msg = "JS timer error: ";
					msg += duk_safe_to_string(ctx, -1);
					log(msg);
				}
				duk_pop(ctx);  // err or result
			} else {
				duk_pop(ctx);  // non-function value
			}
		}
		// Re-locate again: the callback itself may have cleared this timer.
		for (size_t m = 0; m < _timers.size(); ++m) {
			if (_timers[m].handle == h) {
				if (repeat)
					_timers[m].nextMs = getTimeMs() + _timers[m].interval;
				else
					_timers.erase(_timers.begin() + m);
				break;
			}
		}
	}
}

void ModEngine::loadConfig() {
	_config.clear();
	FILE* f = fopen((fullModsDir() + "/modconfig.txt").c_str(), "rb");
	if (!f) return;
	char line[512];
	while (fgets(line, sizeof(line), f)) {
		char* eq = strchr(line, '=');
		if (!eq) continue;
		*eq = 0;
		std::string key = line;
		std::string val = eq + 1;
		while (!key.empty() && (key[key.size()-1] == '\n' || key[key.size()-1] == '\r')) key.erase(key.size()-1);
		while (!val.empty() && (val[val.size()-1] == '\n' || val[val.size()-1] == '\r')) val.erase(val.size()-1);
		_config[key] = val;
	}
	fclose(f);
}

void ModEngine::saveConfig() {
	FILE* f = fopen((fullModsDir() + "/modconfig.txt").c_str(), "wb");
	if (!f) return;
	for (std::map<std::string, std::string>::const_iterator it = _config.begin(); it != _config.end(); ++it)
		fprintf(f, "%s=%s\n", it->first.c_str(), it->second.c_str());
	fclose(f);
}

std::string ModEngine::getConfig(const std::string& key, const std::string& def) const {
	std::map<std::string, std::string>::const_iterator it = _config.find(key);
	return (it == _config.end()) ? def : it->second;
}

void ModEngine::setConfig(const std::string& key, const std::string& value) {
	_config[key] = value;
	saveConfig();
}

std::string ModEngine::getModError(const std::string& file) const {
	std::map<std::string, std::string>::const_iterator it = _modErrors.find(file);
	if (it != _modErrors.end())
		return it->second;
	// Also accept the full "mods/xxx.js" path as key.
	std::string path = fullModsDir() + "/";
	path += file;
	it = _modErrors.find(path);
	return (it == _modErrors.end()) ? "" : it->second;
}

void ModEngine::clearModErrors() {
	_modErrors.clear();
}

// ---------------------------------------------------------------------------
// Scripted mobs (M6)
// ---------------------------------------------------------------------------

void ModEngine::defineScriptedMob(int typeId, const ScriptedMobDef& def) {
	// Free a previous def with the same id (reload scenario).
	std::map<int, ScriptedMobDef>::iterator it = _scriptedMobs.find(typeId);
	if (it != _scriptedMobs.end()) {
		delete it->second.boxes;
		delete it->second.model;
		_scriptedMobs.erase(it);
	}
	_scriptedMobs[typeId] = def;
	log("defined scripted mob id=" + std::to_string(typeId) + " name=" + def.name + " boxes=" + std::to_string(def.boxes->size()));
}

bool ModEngine::getScriptedMobDef(int typeId, std::string& texture, float& sizeW, float& sizeH, int& maxHealth, float& scale, bool& hostile, ScriptedModel*& model) const {
	std::map<int, ScriptedMobDef>::const_iterator it = _scriptedMobs.find(typeId);
	if (it == _scriptedMobs.end())
		return false;
	texture = it->second.texture;
	sizeW = it->second.sizeW;
	sizeH = it->second.sizeH;
	maxHealth = it->second.health;
	scale = it->second.scale;
	hostile = it->second.hostile;
	model = it->second.model;
	return true;
}

Mob* ModEngine::createScriptedMob(Level* level, int typeId) {
	std::map<int, ScriptedMobDef>::const_iterator it = _scriptedMobs.find(typeId);
	if (it == _scriptedMobs.end())
		return NULL;
	return new ScriptedMob(level, typeId);
}

Mob* ModEngine::spawnScriptedMob(int typeId, float x, float y, float z) {
	std::map<int, ScriptedMobDef>::const_iterator it = _scriptedMobs.find(typeId);
	if (it == _scriptedMobs.end()) {
		log("spawnScriptedMob: type " + std::to_string(typeId) + " not defined");
		return NULL;
	}
	Minecraft* m = minecraft();
	if (!m || !m->level)
		return NULL;
	ScriptedMob* mob = new ScriptedMob(m->level, typeId);
	if (MobSpawner::addMob(m->level, mob, x, y, z, 0, 0, true)) {
		log("spawned scripted mob " + std::to_string(typeId) + " at " + std::to_string((int)x) + "," + std::to_string((int)y) + "," + std::to_string((int)z));
		return mob;
	}
	return NULL;
}

// 用 UTF-8 路径开文件：世界名可能是中文，Windows 上必须走宽字符 API
// （普通 fopen 遇到非 ASCII 路径会直接失败）。
static FILE* modOpenUtf8(const std::string& pathUtf8, bool forWrite) {
	if (pathUtf8.empty())
		return NULL;
#if defined(_WIN32)
	int n = MultiByteToWideChar(CP_UTF8, 0, pathUtf8.c_str(), -1, NULL, 0);
	if (n <= 0)
		return NULL;
	std::wstring w(n, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, pathUtf8.c_str(), -1, &w[0], n);
	return _wfopen(w.c_str(), forWrite ? L"wb" : L"rb");
#else
	return fopen(pathUtf8.c_str(), forWrite ? "wb" : "rb");
#endif
}

// op 名单的一行：去首尾空白（含 \r\n）。
static std::string trimOpsLine(const std::string& s) {
	size_t a = 0, b = s.size();
	while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
	while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
	return s.substr(a, b - a);
}

// 读一个部件姿态对象（栈顶）：旋转 x/y/z（缺失/非数字当 0）+ 可选位置 px/py/pz。
// 只写要动的那几个轴是允许的；绝不能把 NaN/∞ 传出去 —— 它会让这个部件的
// 变换矩阵失效（部件直接消失）。
static void readPartPose(duk_context* ctx, ModPartPose& out) {
	out.rx = out.ry = out.rz = 0.0f;
	out.hasPos = false;
	out.px = out.py = out.pz = 0.0f;
	out.hasScale = false;
	out.sx = out.sy = out.sz = 1.0f;
	duk_get_prop_string(ctx, -1, "x"); out.rx = duk_is_number(ctx, -1) ? (float)duk_get_number(ctx, -1) : 0.0f; duk_pop(ctx);
	duk_get_prop_string(ctx, -1, "y"); out.ry = duk_is_number(ctx, -1) ? (float)duk_get_number(ctx, -1) : 0.0f; duk_pop(ctx);
	duk_get_prop_string(ctx, -1, "z"); out.rz = duk_is_number(ctx, -1) ? (float)duk_get_number(ctx, -1) : 0.0f; duk_pop(ctx);
	duk_get_prop_string(ctx, -1, "px"); if (duk_is_number(ctx, -1)) { out.px = (float)duk_get_number(ctx, -1); out.hasPos = true; } duk_pop(ctx);
	duk_get_prop_string(ctx, -1, "py"); if (duk_is_number(ctx, -1)) { out.py = (float)duk_get_number(ctx, -1); out.hasPos = true; } duk_pop(ctx);
	duk_get_prop_string(ctx, -1, "pz"); if (duk_is_number(ctx, -1)) { out.pz = (float)duk_get_number(ctx, -1); out.hasPos = true; } duk_pop(ctx);
	// scale：给了就**替换**骨骼的静态 scale（基岩用它把模型缩到设计尺寸，
	// scale 0 则是隐藏部件）
	duk_get_prop_string(ctx, -1, "sx"); if (duk_is_number(ctx, -1)) { out.sx = (float)duk_get_number(ctx, -1); out.hasScale = true; } duk_pop(ctx);
	duk_get_prop_string(ctx, -1, "sy"); if (duk_is_number(ctx, -1)) { out.sy = (float)duk_get_number(ctx, -1); out.hasScale = true; } duk_pop(ctx);
	duk_get_prop_string(ctx, -1, "sz"); if (duk_is_number(ctx, -1)) { out.sz = (float)duk_get_number(ctx, -1); out.hasScale = true; } duk_pop(ctx);
	if (!(out.rx == out.rx) || out.rx > 1e6f || out.rx < -1e6f) out.rx = 0.0f;
	if (!(out.ry == out.ry) || out.ry > 1e6f || out.ry < -1e6f) out.ry = 0.0f;
	if (!(out.rz == out.rz) || out.rz > 1e6f || out.rz < -1e6f) out.rz = 0.0f;
	if (!(out.px == out.px) || out.px > 1e6f || out.px < -1e6f) out.px = 0.0f;
	if (!(out.py == out.py) || out.py > 1e6f || out.py < -1e6f) out.py = 0.0f;
	if (!(out.pz == out.pz) || out.pz > 1e6f || out.pz < -1e6f) out.pz = 0.0f;
}

bool ModEngine::callAnimKey(const std::string& animKey, const std::string& partName, float age, float time, int firstPersonFlag, ModPartPose& out) {
	std::lock_guard<std::recursive_mutex> _jsLock(_jsMutex);
	if (animKey.empty())
		return false;
	// The anim callback lives in the defining mod's own heap.
	for (size_t s = 0; s < _sandboxes.size(); ++s) {
		duk_context* ctx = _sandboxes[s].ctx;
		if (!ctx) continue;
		duk_push_global_object(ctx);
		duk_get_prop_string(ctx, -1, animKey.c_str());
		if (!duk_is_function(ctx, -1)) {
			duk_pop_2(ctx);
			continue;
		}
		duk_push_string(ctx, partName.c_str());
		duk_push_number(ctx, age);
		duk_push_number(ctx, time);
		int nargs = 3;
		if (firstPersonFlag >= 0) { duk_push_boolean(ctx, firstPersonFlag ? 1 : 0); nargs = 4; }
		if (duk_pcall(ctx, nargs) != 0) {
			duk_pop_2(ctx);  // result(undefined path) + global
			continue;
		}
		// 返回对象可以是 null/undefined（这个部件不动）或 {x,y,z[,px,py,pz]}。
		bool ok = false;
		if (duk_is_object(ctx, -1)) {
			readPartPose(ctx, out);
			ok = true;
		}
		duk_pop_2(ctx);  // result + global
		return ok;
	}
	duk_context* ctx = (duk_context*)_ctx;
	if (!ctx)
		return false;
	duk_push_global_object(ctx);
	duk_get_prop_string(ctx, -1, animKey.c_str());
	if (!duk_is_function(ctx, -1)) {
		duk_pop_2(ctx);
		return false;
	}
	duk_push_string(ctx, partName.c_str());
	duk_push_number(ctx, age);
	duk_push_number(ctx, time);
	int nargs = 3;
	if (firstPersonFlag >= 0) { duk_push_boolean(ctx, firstPersonFlag ? 1 : 0); nargs = 4; }
	if (duk_pcall(ctx, nargs) != 0) {
		duk_pop_2(ctx);  // result(undefined path) + global
		return false;
	}
	// 返回对象可以是 null/undefined（这个部件不动）或 {x,y,z[,px,py,pz]}。
	bool ok = false;
	if (duk_is_object(ctx, -1)) {
		readPartPose(ctx, out);
		ok = true;
	}
	duk_pop_2(ctx);  // result + global
	return ok;
}

// 生物 anim 用：不传“第一人称”标记（第 4 个参数），也不理位置（生物模型自带）。
bool ModEngine::getAnimRotation(const std::string& animKey, const std::string& partName, float age, float time, float& rx, float& ry, float& rz) {
	ModPartPose pose;
	if (!callAnimKey(animKey, partName, age, time, -1, pose))
		return false;
	rx = pose.rx; ry = pose.ry; rz = pose.rz;
	return true;
}

// ---------------------------------------------------------------------------
// 模组：玩家动作（按玩家分别）
//   Player.defineAction 注册动作（回调存在定义它的那个 mod heap 里），
//   Player.setAction 把某个玩家挂到某个动作上。渲染时 HumanoidModel 每个
//   部件每帧查一次（getPlayerAnim -> getAnimRotation，复用同一套 JS 调用）。
// ---------------------------------------------------------------------------
void ModEngine::definePlayerAction(const std::string& name, const std::string& animKey) {
	std::lock_guard<std::recursive_mutex> _jsLock(_jsMutex);
	_playerActionKeys[name] = animKey;
}

void ModEngine::applyPlayerAction(int playerId, const std::string& actionName) {
	std::lock_guard<std::recursive_mutex> _jsLock(_jsMutex);
	if (actionName.empty())
		_playerActionByPlayer.erase(playerId);
	else
		_playerActionByPlayer[playerId] = actionName;
}

std::string ModEngine::getPlayerAction(int playerId) {
	std::lock_guard<std::recursive_mutex> _jsLock(_jsMutex);
	std::map<int, std::string>::const_iterator it = _playerActionByPlayer.find(playerId);
	return (it == _playerActionByPlayer.end()) ? std::string() : it->second;
}

bool ModEngine::setPlayerAction(int playerId, const std::string& actionName) {
	if (playerId < 0)
		return false;
	applyPlayerAction(playerId, actionName);
	sendPlayerAppearance(playerId);
	return true;
}

bool ModEngine::hasPlayerAction(int playerId) {
	std::lock_guard<std::recursive_mutex> _jsLock(_jsMutex);
	std::map<int, std::string>::const_iterator it = _playerActionByPlayer.find(playerId);
	if (it == _playerActionByPlayer.end())
		return false;
	return _playerActionKeys.find(it->second) != _playerActionKeys.end();
}

bool ModEngine::getPlayerAnim(int playerId, const std::string& partName, float age, float time, bool firstPerson, ModPartPose& out) {
	std::lock_guard<std::recursive_mutex> _jsLock(_jsMutex);
	std::map<int, std::string>::const_iterator it = _playerActionByPlayer.find(playerId);
	if (it == _playerActionByPlayer.end())
		return false;
	std::map<std::string, std::string>::const_iterator d = _playerActionKeys.find(it->second);
	if (d == _playerActionKeys.end())
		return false;
	// 递归锁：这里再上锁是安全的（callAnimKey 自己也会锁）。
	return callAnimKey(d->second, partName, age, time, firstPerson ? 1 : 0, out);
}

void ModEngine::clearPlayerActions() {
	std::lock_guard<std::recursive_mutex> _jsLock(_jsMutex);
	_playerActionKeys.clear();
	_playerActionByPlayer.clear();
}

// ---------------------------------------------------------------------------
// 模组用的 op 名单（每个世界一份，跟着存档走；原版游戏完全不用它）
//   <世界目录>/ops.txt，一行一个名字（UTF-8，# 开头的行忽略）。
//   名单为空时，第一个进世界的玩家自动成为 op —— 否则模组没法判权限。
//   联机连别人服务器时名单只在内存（那种情况该由服务器定权限）。
// ---------------------------------------------------------------------------
std::string ModEngine::worldOpsPath() {
	Minecraft* m = minecraft();
	if (!m || !m->level)
		return std::string();
	std::string world = m->level->getLevelName();
	if (world.empty())
		return std::string();
	// 联机客户端不落盘：世界在服务器手里，本地没有属于它的世界目录。
	if (m->level->isClientSide)
		return std::string();
	ExternalFileLevelStorageSource* ext = dynamic_cast<ExternalFileLevelStorageSource*>(m->getLevelSource());
	if (!ext)
		return std::string();
	return ext->getFullPath(world) + "/ops.txt";
}

void ModEngine::saveWorldOps() {
	std::string path = worldOpsPath();
	if (path.empty())
		return;
	std::string out;
	for (std::set<std::string>::const_iterator it = _worldOps.begin(); it != _worldOps.end(); ++it)
		out += *it + "\n";
	FILE* f = modOpenUtf8(path, true);
	if (f) {
		fwrite(out.c_str(), 1, out.size(), f);
		fclose(f);
	}
}

std::set<std::string>& ModEngine::worldOps() {
	Minecraft* m = minecraft();
	std::string world = (m && m->level) ? m->level->getLevelName() : std::string();
	if (_worldOpsValid && _worldOpsWorld == world)
		return _worldOps;          // 同一个世界，用缓存的名单
	_worldOpsWorld = world;
	_worldOpsValid = true;
	_worldOps.clear();

	FILE* f = modOpenUtf8(worldOpsPath(), false);
	if (f) {
		char line[512];
		while (fgets(line, sizeof(line), f)) {
			std::string s = trimOpsLine(std::string(line));
			if (!s.empty() && s[0] != '#')
				_worldOps.insert(s);
		}
		fclose(f);
	}

	// 名单是空的 -> 把第一个进世界的玩家记为 op（否则模组没有可用的管理员）。
	// 创建世界时若关掉了「创建者获得 op」，这里就不自动加（之后可用 /op 手动加）。
	bool autoOpAllowed = true;
	if (m && m->level && m->level->getLevelData())
		autoOpAllowed = m->level->getLevelData()->getAutoOp();
	if (autoOpAllowed && _worldOps.empty() && m && m->player && !m->player->name.empty()) {
		_worldOps.insert(m->player->name);
		saveWorldOps();
		log("world ops: 名单为空, 自动把 " + m->player->name + " 记为 op (" + world + ")");
	}
	return _worldOps;
}

bool ModEngine::isOp(const std::string& name) {
	if (name.empty())
		return false;
	std::set<std::string>& ops = worldOps();
	return ops.find(name) != ops.end();
}

bool ModEngine::addOp(const std::string& name) {
	if (name.empty())
		return false;
	std::set<std::string>& ops = worldOps();
	if (ops.find(name) != ops.end())
		return true;
	ops.insert(name);
	saveWorldOps();
	return true;
}

bool ModEngine::removeOp(const std::string& name) {
	if (name.empty())
		return false;
	std::set<std::string>& ops = worldOps();
	if (ops.erase(name) == 0)
		return false;
	saveWorldOps();
	return true;
}

// ---------------------------------------------------------------------------
// 模组：玩家外形（让玩家变成自定义生物的样子）
//   Player.setModel(id, typeId) 把玩家挂到一份外形定义上（Mob.defineMob 定义过的）。
//   渲染时 PlayerRenderer 临时换成那份模型再画、画完还原；
//   体型（碰撞箱）跟着外形定义里的 sizeW/sizeH 走 —— “变成什么就是什么”。
// ---------------------------------------------------------------------------
Player* ModEngine::findPlayerEntity(int playerId) {
	Minecraft* m = minecraft();
	if (!m) return NULL;
	if (m->player && m->player->entityId == playerId) return m->player;
	ServerSideNetworkHandler* ssn = dynamic_cast<ServerSideNetworkHandler*>(m->netCallback);
	if (ssn) {
		const std::map<RakNet::RakNetGUID, Player*>& remotes = ssn->getRemotePlayers();
		for (std::map<RakNet::RakNetGUID, Player*>::const_iterator it = remotes.begin(); it != remotes.end(); ++it) {
			if (it->second && it->second->entityId == playerId) return it->second;
		}
	}
	return NULL;
}

bool ModEngine::scriptedModelSize(const std::string& spec, float& w, float& h) {
	int typeId = atoi(spec.c_str());
	if (typeId <= 0) return false;
	std::map<int, ScriptedMobDef>::const_iterator it = _scriptedMobs.find(typeId);
	if (it == _scriptedMobs.end()) return false;
	w = it->second.sizeW;
	h = it->second.sizeH;
	return (w > 0.0f && h > 0.0f);
}

void ModEngine::applyAppearance(int playerId, const std::string& actionName, const std::string& modelSpec) {
	applyPlayerAction(playerId, actionName);
	applyPlayerModel(playerId, modelSpec);
}

void ModEngine::applyPlayerModel(int playerId, const std::string& spec) {
	{
		std::lock_guard<std::recursive_mutex> _jsLock(_jsMutex);
		if (spec.empty())
			_playerModelByPlayer.erase(playerId);
		else
			_playerModelByPlayer[playerId] = spec;
	}
	// 体型跟着外形走。服务器和客户端必须一致，否则服务器算的碰撞/位置
	// 和客户端看到的对不上（联机时会互相“拉回”）。
	Player* p = findPlayerEntity(playerId);
	if (!p) return;
	float w = 0.6f, h = 1.8f;          // 脱掉外形 -> 还原成人形
	if (!spec.empty() && !scriptedModelSize(spec, w, h))
		return;                          // 未知外形：体型不动
	p->setScriptedBodySize(w, h);
}

std::string ModEngine::getPlayerModel(int playerId) {
	std::lock_guard<std::recursive_mutex> _jsLock(_jsMutex);
	std::map<int, std::string>::const_iterator it = _playerModelByPlayer.find(playerId);
	return (it == _playerModelByPlayer.end()) ? std::string() : it->second;
}

// 渲染热路径用：只要个 yes/no，不必构造字符串。
bool ModEngine::hasPlayerModel(int playerId) {
	std::lock_guard<std::recursive_mutex> _jsLock(_jsMutex);
	return _playerModelByPlayer.find(playerId) != _playerModelByPlayer.end();
}

bool ModEngine::setPlayerModel(int playerId, const std::string& spec) {
	if (playerId < 0)
		return false;
	applyPlayerModel(playerId, spec);
	sendPlayerAppearance(playerId);
	return true;
}

#ifndef STANDALONE_SERVER
Model* ModEngine::getPlayerModelObject(int playerId) {
	std::string spec = getPlayerModel(playerId);
	if (spec.empty()) return NULL;
	std::map<int, ScriptedMobDef>::iterator it = _scriptedMobs.find(atoi(spec.c_str()));
	if (it == _scriptedMobs.end()) return NULL;
	return it->second.model;   // ScriptedModel*（服务器侧可能为 NULL —— 服务器不渲染）
}
#else
Model* ModEngine::getPlayerModelObject(int playerId) { (void)playerId; return NULL; }
#endif

std::string ModEngine::getPlayerModelTexture(int playerId) {
	std::string spec = getPlayerModel(playerId);
	if (spec.empty()) return std::string();
	std::map<int, ScriptedMobDef>::const_iterator it = _scriptedMobs.find(atoi(spec.c_str()));
	if (it == _scriptedMobs.end()) return std::string();
	return it->second.texture;
}

void ModEngine::sendPlayerAppearance(int playerId) {
	Minecraft* m = minecraft();
	if (!m || !m->raknetInstance || !m->level)
		return;
	// 房主/服务器：广播给所有客户端（服务器模式下 send 就是广播）。
	// 连别人的服务器：只上报“我自己的”，由服务器转发给同世界其他人。
	const bool isServer = m->raknetInstance->isServer();
	const bool isMine = (m->player && playerId == m->player->entityId);
	if (!isServer && !isMine)
		return;
	PlayerPosePacket pkt(playerId,
	                     RakNet::RakString(getPlayerAction(playerId).c_str()),
	                     RakNet::RakString(getPlayerModel(playerId).c_str()));
	m->raknetInstance->send(pkt);
}

// ─────────────────────────────────────────────────────────────
// Player skin (主菜单纸娃娃皮肤选择): skins/current.txt + 任意 png/jpg
// skins/ 锚定在 exe 所在目录(仓库根, 与 mods/ 同级), current.txt 记录
// 当前皮肤的绝对路径。应用 = 图片缩放(最近邻)到 64x32 后整张替换
// mob/char.png 的显存 + CPU 缓存 —— 游戏内玩家、背包纸娃娃、主菜单
// 纸娃娃共享该纹理, 全部生效; 重启后 applySavedPlayerSkin 自动恢复。
// ─────────────────────────────────────────────────────────────

// Directory containing the running exe ("" on failure); skins/ lives there.
static std::string exeDirectory() {
#if defined(_WIN32)
	char buf[MAX_PATH];
	GetModuleFileNameA(NULL, buf, sizeof(buf));
	std::string p(buf);
	size_t slash = p.find_last_of("\\/");
	return (slash == std::string::npos) ? std::string(".") : p.substr(0, slash);
#else
	return std::string(".");
#endif
}

// Recursively create a directory (portable: win _mkdir, posix mkdir).
static void ensureDir(const std::string& dir) {
#if defined(_WIN32)
	std::string cur;
	size_t pos = 0;
	while ((pos = dir.find_first_of("\\/", pos + 1)) != std::string::npos) {
		cur = dir.substr(0, pos);
		if (!cur.empty())
			CreateDirectoryA(cur.c_str(), NULL);
	}
	if (!dir.empty())
		CreateDirectoryA(dir.c_str(), NULL);
#else
	size_t pos = 0;
	while ((pos = dir.find('/', pos + 1)) != std::string::npos) {
		std::string sub = dir.substr(0, pos);
		if (!sub.empty())
			mkdir(sub.c_str(), 0777);
	}
	mkdir(dir.c_str(), 0777);
#endif
}

static std::string skinDirPath() { return exeDirectory() + "/skins"; }
static std::string skinStatePath() { return skinDirPath() + "/current.txt"; }

static bool readWholeFile(const std::string& path, std::vector<unsigned char>& out) {
	FILE* f = fopen(path.c_str(), "rb");
	if (!f) return false;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz <= 0) { fclose(f); return false; }
	out.resize((size_t)sz);
	size_t rd = fread(out.data(), 1, (size_t)sz, f);
	fclose(f);
	return rd == (size_t)sz;
}

// Decode an image file into RGBA via stb_image (png/jpg/bmp/gif).
static bool decodeImageFile(const std::string& path, int& w, int& h, std::vector<unsigned char>& rgba) {
	std::vector<unsigned char> bytes;
	if (!readWholeFile(path, bytes) || bytes.empty())
		return false;
	int n = 0;
	unsigned char* px = stbi_load_from_memory(bytes.data(), (int)bytes.size(), &w, &h, &n, 4);
	if (!px) return false;
	rgba.assign(px, px + (size_t)w * h * 4);
	stbi_image_free(px);
	return true;
}

// Nearest-neighbour scale into (dw x dh) RGBA.
static bool scaleImageTo(const unsigned char* src, int sw, int sh,
	int dw, int dh, std::vector<unsigned char>& out) {
	if (!src || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return false;
	out.resize((size_t)dw * dh * 4);
	for (int y = 0; y < dh; ++y) {
		int sy = (y * sh) / dh; if (sy >= sh) sy = sh - 1;
		for (int x = 0; x < dw; ++x) {
			int sx = (x * sw) / dw; if (sx >= sw) sx = sw - 1;
			const unsigned char* s = &src[(sy * sw + sx) * 4];
			unsigned char* d = &out[(y * dw + x) * 4];
			d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3];
		}
	}
	return true;
}

std::string ModEngine::currentSkinPath() {
	FILE* f = fopen(skinStatePath().c_str(), "r");
	if (!f) return "";
	char buf[4096];
	if (!fgets(buf, sizeof(buf), f)) { fclose(f); return ""; }
	fclose(f);
	std::string s = buf;
	while (!s.empty() && (s[s.size()-1] == '\n' || s[s.size()-1] == '\r'))
		s.erase(s.size()-1);
	return s;
}

bool ModEngine::hasSavedSkin() {
	std::string p = currentSkinPath();
	if (p.empty()) return false;
	FILE* f = fopen(p.c_str(), "rb");
	if (!f) return false;
	fclose(f);
	return true;
}

bool ModEngine::applyPlayerSkinFromFile(const std::string& imagePath) {
#ifndef STANDALONE_SERVER
	Minecraft* mc = minecraft();
	if (!mc || !mc->textures) return false;

	int sw = 0, sh = 0;
	std::vector<unsigned char> rgba;
	if (!decodeImageFile(imagePath, sw, sh, rgba)) {
		log("applyPlayerSkin: cannot decode " + imagePath);
		return false;
	}
	// 统一输出 64x64 画布(玩家模型 modern64 按 64x64 归一化):
	//  - 64x64 皮肤: 原样(上半 inner, 下半 outer 第二层)
	//  - 64x32 老皮肤: 放到画布上半, 下半补透明(无第二层)
	//  - 其它尺寸: 整张缩放到 64x64
	// 一律整张重建纹理 → 切肤无残留。
	const int TW = 64, TH = 64;
	std::vector<unsigned char> canvas((size_t)TW * TH * 4, 0); // 默认全透明
	bool is64 = (sw == 64 && sh == 64);
	bool is32 = (sw == 64 && sh == 32);
	if (is64) {
		canvas.swap(rgba);
	} else if (is32) {
		memcpy(canvas.data(), rgba.data(), (size_t)64 * 32 * 4); // 上半
	} else {
		// 其它尺寸先缩到 64x32 放上半(近似), 高度>32 的图压扁。
		std::vector<unsigned char> top;
		if (scaleImageTo(rgba.data(), sw, sh, TW, 32, top))
			memcpy(canvas.data(), top.data(), top.size());
	}
	// 仅 64x64 输入启用 outer 第二层
	ModEngine::s_playerSkinIs64 = (sw == 64 && sh == 64);

	// Ensure the target texture is loaded, then replace GPU + CPU copies.
	const char* texPath = "mob/char.png";
	TextureId id = mc->textures->loadAndBindTexture(texPath);
	if (id == Textures::InvalidId) {
		log("applyPlayerSkin: mob/char.png not loadable");
		return false;
	}
	glBindTexture(GL_TEXTURE_2D, (GLuint)id);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	// 整张重建 64x64(即使之前已是 64x64 也全量覆盖 → 无残留旧图)。
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TW, TH, 0, GL_RGBA, GL_UNSIGNED_BYTE, canvas.data());
	// 恢复 GL 全局状态: 引擎其它 glTexImage2D 依赖默认 4 字节对齐
	// (不恢复 → mod 图标/动态纹理等上传错乱, 表现为 GUI 贴图花/消失)。
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	// CPU-side cache so later reads/layout see the new pixels, and a graphics
	// reset re-uploads the skin instead of the vanilla file.
	mc->textures->replaceTextureData(texPath, TW, TH, canvas.data());

	// Persist for next launch.
	ensureDir(skinDirPath());
	FILE* f = fopen(skinStatePath().c_str(), "w");
	if (f) {
		fprintf(f, "%s", imagePath.c_str());
		fclose(f);
	}
	log(std::string("applyPlayerSkin: ") + imagePath + " -> mob/char.png 64x64" + (ModEngine::s_playerSkinIs64 ? " [double-layer]" : ""));
	return true;
#else
	return false;
#endif
}

bool ModEngine::applySavedPlayerSkin() {
#ifndef STANDALONE_SERVER
	std::string p = currentSkinPath();
	if (p.empty()) return false;
	if (!applyPlayerSkinFromFile(p)) {
		// Unreadable -> forget it and fall back to vanilla.
		remove(skinStatePath().c_str());
		return false;
	}
	return true;
#else
	return false;
#endif
}

bool ModEngine::applyDefaultSkinTo64() {
#ifndef STANDALONE_SERVER
	Minecraft* mc = minecraft();
	if (!mc || !mc->textures) return false;
	const char* texPath = "mob/char.png";
	// 加载(若未加载)并取原始 CPU 像素: 原版 64x32。
	TextureData* td = mc->textures->loadAndGetTextureData(texPath);
	if (!td || !td->data) return false;
	if (td->w == 64 && td->h == 64)
		return true;  // 已是 64x64(理论上不会走到)
	const int TW = 64, TH = 64;
	std::vector<unsigned char> canvas((size_t)TW * TH * 4, 0);
	int copyW = td->w > 64 ? 64 : td->w;
	int copyH = td->h > 32 ? 32 : td->h;
	for (int y = 0; y < copyH; ++y) {
		memcpy(canvas.data() + (size_t)y * TW * 4,
			td->data + (size_t)y * td->w * 4, (size_t)copyW * 4);
	}
	TextureId id = mc->textures->loadAndBindTexture(texPath);
	if (id == Textures::InvalidId) return false;
	glBindTexture(GL_TEXTURE_2D, (GLuint)id);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TW, TH, 0, GL_RGBA, GL_UNSIGNED_BYTE, canvas.data());
	mc->textures->replaceTextureData(texPath, TW, TH, canvas.data());
	ModEngine::s_playerSkinIs64 = false;
	return true;
#else
	return false;
#endif
}

void ModEngine::clearPlayerSkin() {
#ifndef STANDALONE_SERVER
	Minecraft* mc = minecraft();
	if (mc && mc->textures)
		mc->textures->unloadTexture("mob/char.png");
	remove(skinStatePath().c_str());
#else
	return;
#endif
}
