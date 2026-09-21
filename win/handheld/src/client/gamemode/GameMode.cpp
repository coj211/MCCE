#include "GameMode.h"
#include "../Minecraft.h"
#if defined(_WIN32) || defined(__ANDROID__)
#include "../../mod/ModEngine.h"
#endif
#include "../../network/packet/UseItemPacket.h"
#include "../../network/packet/PlayerActionPacket.h"
#include "../../world/level/Level.h"
#include "../../world/level/tile/ModTile.h"
#include "../../world/item/PickaxeItem.h"
#include "../../world/item/Item.h"
#include "../../world/item/ItemInstance.h"
#include "../player/LocalPlayer.h"
#ifndef STANDALONE_SERVER
#include "../sound/SoundEngine.h"
#include "../particle/ParticleEngine.h"
#endif
#include "../../network/RakNetInstance.h"
#include "../../network/packet/RemoveBlockPacket.h"
#ifndef STANDALONE_SERVER
#include "../renderer/LevelRenderer.h"
#endif
#include "../../world/level/material/Material.h"

GameMode::GameMode( Minecraft* minecraft)
:	minecraft(minecraft),
	destroyProgress(0),
	hitPointX(0), hitPointY(0), hitPointZ(0),
	oDestroyProgress(0),
	destroyTicks(0),
	destroyDelay(0)
{
}

/*virtual*/
Player* GameMode::createPlayer(Level* level) {
    return new LocalPlayer(minecraft, level, minecraft->user, level->dimension->id, isCreativeType());
}

/*virtual*/
void GameMode::interact(Player* player, Entity* entity) {
    player->interact(entity);
}

/*virtual*/
void GameMode::attack(Player* player, Entity* entity) {
	if (minecraft->level->adventureSettings.noPvP && entity->isPlayer())
		return;
	if (minecraft->level->adventureSettings.noPvM && entity->isMob())
		return;
#if defined(_WIN32) || defined(__ANDROID__)
	// Veto hook: a mod can prevent attacking this entity.
	if (ModEngine::instance && !ModEngine::instance->canAttack(entity->entityId))
		return;
#endif
    player->attack(entity);
#if defined(_WIN32) || defined(__ANDROID__)
	if (ModEngine::instance)
		ModEngine::instance->fireEvent("onAttack", entity->entityId);
#endif
}

/* virtual */
void GameMode::startDestroyBlock( int x, int y, int z, int face ) {
	if(minecraft->player->getCarriedItem() != NULL && minecraft->player->getCarriedItem()->id == Item::bow->id)
		return;
	destroyBlock(x, y, z, face);
}

/*virtual*/
bool GameMode::destroyBlock(int x, int y, int z, int face) {
    Level* level = minecraft->level;
    Tile* oldTile = (level->getTile(x, y, z) > 0 && level->getTile(x, y, z) < Tile::NUM_BLOCK_TYPES) ? Tile::tiles[level->getTile(x, y, z)] : NULL;
	if (!oldTile)
		return false;

    if (level->adventureSettings.immutableWorld) {
        if (oldTile != (Tile*)Tile::leaves
         && oldTile->material != Material::plant) {
             return false;
        }
    }
#if defined(_WIN32) || defined(__ANDROID__)
	// 模组接管破坏（带精确命中点，相对格子 0..1）：微方块那类“一格内多个小东西”
	// 需要靠点击点决定挖掉哪一个。返回 true = 模组处理了，整个方块保持原样。
	{
		float hx = hitPointX - (float)x;
		float hy = hitPointY - (float)y;
		float hz = hitPointZ - (float)z;
		// 兜底：命中点不在这一格里（没被赋值 / 不是鼠标点出来的），就用玩家
		// 视线重新求一次交点 —— 比退化成格子中心准得多。
		if (hx < -0.001f || hx > 1.001f || hy < -0.001f || hy > 1.001f
			|| hz < -0.001f || hz > 1.001f) {
			hx = 0.5f; hy = 0.5f; hz = 0.5f;
			if (minecraft->player) {
				HitResult hr = minecraft->player->pick(getPickRange(), 1.0f);
				if (hr.type == TILE && hr.x == x && hr.y == y && hr.z == z) {
					hx = (float)hr.pos.x - (float)x;
					hy = (float)hr.pos.y - (float)y;
					hz = (float)hr.pos.z - (float)z;
				}
			}
		}
		if (ModEngine::instance && ModEngine::instance->tryModBreakBlock(x, y, z, face, hx, hy, hz))
			return false;
	}
#endif
#ifndef STANDALONE_SERVER
	minecraft->particleEngine->destroy(x, y, z);
#endif
#if defined(_WIN32) || defined(__ANDROID__)
	// Veto hook: a mod can prevent this block from being broken.
	if (ModEngine::instance && !ModEngine::instance->canBreakBlock(x, y, z, face))
		return false;
#endif
	int data = level->getData(x, y, z);
	int oldTileId = oldTile->id;
    bool changed = level->setTile(x, y, z, 0);
    if (changed) {
#if defined(_WIN32) || defined(__ANDROID__)
		if (ModEngine::instance)
			ModEngine::instance->fireEvent("onBreakBlock", x, y, z, face, oldTileId, data);
#endif
#ifndef STANDALONE_SERVER
        minecraft->soundEngine->play(oldTile->soundType->getBreakSound(), x + 0.5f, y + 0.5f, z + 0.5f, (oldTile->soundType->getVolume() + 1) / 2, oldTile->soundType->getPitch() * 0.8f);
#endif
        // Mod ore blocks: without a pickaxe they drop nothing (break the
        // block but no loot), so players must mine Aether ores with a pick.
        bool canHarvest = true;
#if defined(_WIN32) || defined(__ANDROID__)
        if (ModTile* mt = dynamic_cast<ModTile*>(oldTile)) {
            if (mt->requiresPickaxe()) {
                ItemInstance* carried = minecraft->player ? minecraft->player->getCarriedItem() : NULL;
                bool hasPick = false;
                if (carried && carried->id >= 0 && carried->id < Item::MAX_ITEMS && Item::items[carried->id])
                    hasPick = dynamic_cast<PickaxeItem*>(Item::items[carried->id]) != NULL;
                canHarvest = hasPick;
            }
        }
#endif
        if (canHarvest)
            oldTile->destroy(level, x, y, z, data);
		if (minecraft->options.destroyVibration) minecraft->platform()->vibrate(24);
		if (minecraft->isOnline()) {
			RemoveBlockPacket packet(minecraft->player, x, y, z);
			minecraft->raknetInstance->send(packet);
		}
	}
    return changed;
}
/*virtual*/
bool GameMode::useItemOn(Player* player, Level* level, ItemInstance* item, int x, int y, int z, int face, const Vec3& hit) {
	float clickX = hit.x - x;
	float clickY = hit.y - y;
	float clickZ = hit.z - z;
	item = player->inventory->getSelected();
	if(level->isClientSide) {
		UseItemPacket packet(x, y, z, face, item, player->entityId, clickX, clickY, clickZ);
		minecraft->raknetInstance->send(packet);
	}
    int t = level->getTile(x, y, z);
	if (t == Tile::invisible_bedrock->id) return false;
    if (t > 0 && Tile::tiles[t] && Tile::tiles[t]->use(level, x, y, z, player))
		return true;

	if (item == NULL) return false;
#if defined(_WIN32) || defined(__ANDROID__)
	// Veto hook: a mod can prevent this block/item placement.
	if (ModEngine::instance && !ModEngine::instance->canPlaceBlock(x, y, z, face, item->id))
		return false;
	// 放置接管：模组可以决定这次放置放什么、放哪、放几块、写什么数据值
	// （顶层函数 onPlaceAttempt）。返回 true = 模组处理了，不再走原逻辑。
	if (ModEngine::instance && ModEngine::instance->tryModPlace(player, level, x, y, z, face, clickX, clickY, clickZ, item))
		return true;
#endif
	if(isCreativeType()) {
		int aux = item->getAuxValue();
		int count = item->count;
		bool success = item->useOn(player, level, x, y, z, face, clickX, clickY, clickZ);
		item->setAuxValue(aux);
		item->count = count;
#if defined(_WIN32) || defined(__ANDROID__)
		if (success && ModEngine::instance)
			ModEngine::instance->fireEvent("onPlaceBlock", x, y, z, face, item->id);
#endif
		return success;
	} else {
		bool success = item->useOn(player, level, x, y, z, face, clickX, clickY, clickZ);
#if defined(_WIN32) || defined(__ANDROID__)
		if (success && ModEngine::instance)
			ModEngine::instance->fireEvent("onPlaceBlock", x, y, z, face, item->id);
#endif
		return success;
	}
}

bool GameMode::useItem( Player* player, Level* level, ItemInstance* item ) {
	if (item == NULL) return false;
	int oldCount = item->count;

	ItemInstance* itemInstance = item->use(level, player);
#if defined(_WIN32) || defined(__ANDROID__)
	if (ModEngine::instance)
		ModEngine::instance->fireEvent("onUseItem", item->id);
#endif
	if(level->isClientSide) {
		UseItemPacket packet(item, player->entityId, player->aimDirection);
		minecraft->raknetInstance->send(packet);
	}
	if (itemInstance != item || (itemInstance != NULL && itemInstance->count != oldCount)) {
	    //player.inventory.items[player.inventory.selected] = itemInstance;
	    //if (itemInstance.count == 0) {
	    //    player.inventory.items[player.inventory.selected] = NULL;
	    //}
	    return true;
	}
	return false;
}

ItemInstance* GameMode::handleInventoryMouseClick( int containerId, int slotNum, int buttonNum, Player* player ) {
	//return player.containerMenu.clicked(slotNum, buttonNum, player);
	return NULL;
}

void GameMode::handleCloseInventory( int containerId, Player* player ) {
	//player.containerMenu.removed(player);
	//player.containerMenu = player.inventoryMenu;
}

float GameMode::getPickRange() {
	return 5.0f;
}

void GameMode::initPlayer( Player* player ) {
	initAbilities(player->abilities);
}

void GameMode::releaseUsingItem(Player* player){
	if(minecraft->level->isClientSide) {
		PlayerActionPacket packet(PlayerActionPacket::RELEASE_USE_ITEM,  0, 0, 0, 0, player->entityId);
		minecraft->raknetInstance->send(packet);
	}
	player->releaseUsingItem();
}

void GameMode::tick() {
	oDestroyProgress = destroyProgress;
}

void GameMode::render( float a ) {
#ifndef STANDALONE_SERVER
	if (destroyProgress <= 0) {
		minecraft->gui.progress = 0;
		minecraft->levelRenderer->destroyProgress = 0;
	} else {
		float dp = oDestroyProgress + (destroyProgress - oDestroyProgress) * a;
		minecraft->gui.progress = dp;
		minecraft->levelRenderer->destroyProgress = dp;
	}
#endif
}
