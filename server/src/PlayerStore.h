#ifndef MINECRAFT_SEVER_PLAYERSTORE_H__
#define MINECRAFT_SEVER_PLAYERSTORE_H__

#include <string>

class Player;
class Level;

//
// 每个玩家一份存档：<世界目录>/players/<名字>.dat
//
// 为什么需要它：引擎自带的存档只给"本地玩家"留了一个坑位
// （`LevelData::createTag(players)` 里只取 `players[0]`，见 LevelData.cpp:157），
// 独立服务器上每个远程玩家退出后就没了——空背包、出生点、10 血重来。
//
// 文件格式与 level.dat 一致：`[int version=2][int size][NBT 字节]`，
// 直接用引擎现成的 NbtIo，内容和单机存档里的玩家 tag 同构
// （Entity::saveWithoutId 的位置/朝向/动量 + Player::addAdditonalSaveData 的背包等）。
//
namespace PlayerStore
{
	// 世界存档目录下的 players/（按世界路径推导，不写死 "world"）
	std::string dirFor(Level* level);

	// 玩家名 → 安全文件名。名字来自客户端上报，必须挡掉路径分隔符等，
	// 否则 `../../xxx` 能写到存档目录外面去。
	std::string safeFileName(const std::string& playerName);

	// 读回并套用到玩家（位置/朝向/血量/饥饿/经验/背包/游戏模式）。
	// 文件不存在 = 新玩家，返回 false 且不动玩家。
	bool load(Level* level, Player* p);

	// 写回。返回是否成功。
	bool save(Level* level, Player* p);
}

#endif // MINECRAFT_SEVER_PLAYERSTORE_H__
