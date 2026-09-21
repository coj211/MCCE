#pragma once
#include <_types.h>
// 适配头：0.8.1 的 Item 用 0.6.1 的实现
#include "../../../world/item/Item.h"
// Item::shears 是 ShearsItem*；CreativeInventoryScreen 需要完整类型做派生转换
#include "../../../world/item/ShearsItem.h"
