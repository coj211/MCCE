#pragma once
#include <_types.h>
// 适配头：0.8.1 的 Tile 用 0.6.1 的实现
#include "../../../world/level/tile/Tile.h"
// Tile::leaves 是 LeafTile*；CreativeInventoryScreen 需要完整类型做派生到基类转换
#include "../../../world/level/tile/LeafTile.h"
