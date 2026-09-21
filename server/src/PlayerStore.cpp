#include "PlayerStore.h"
#include "ServerPaths.h"

#include "platform/log.h"
// 注意顺序：RakDataIO.h 直接使用 RakNet::BitStream 却自己不 include 它，
// 必须先把 raknet 头引进来（同一个坑 ExternalFileLevelStorage.cpp 也是这么躲的）。
#include "raknet/BitStream.h"
#include "util/RakDataIO.h"
#include "nbt/CompoundTag.h"
#include "nbt/NbtIo.h"
#include "world/entity/player/Player.h"
#include "world/level/Level.h"
#include "world/level/storage/ExternalFileLevelStorage.h"
#include "world/level/storage/FolderMethods.h"

#include <cstdio>

namespace {

// 与 level.dat 相同的容器：version 2 = NBT 载荷
const int kStoreVersion = 2;
// 玩家文件不该有多大，超过就当损坏（防止读到一个畸形 size 去分配巨量内存）
const int kMaxPayload = 8 * 1024 * 1024;

// 存档路径要走宽字符 API：玩家名可能是中文，窄字符的 fopen/_wremove 用的是本地
// 代码页（GBK），UTF-8 的名字会直接找不到文件（详见 ServerPaths.h）。
FILE* openStoreFile(const std::string& path, bool forWrite)
{
#ifdef _WIN32
	return _wfopen(ServerPaths::toWide(path).c_str(), forWrite ? L"wb" : L"rb");
#else
	return fopen(path.c_str(), forWrite ? "wb" : "rb");
#endif
}

int removeStoreFile(const std::string& path)
{
#ifdef _WIN32
	return _wremove(ServerPaths::toWide(path).c_str());
#else
	return remove(path.c_str());
#endif
}

// 读取 [int version][int size][bytes]，成功返回 true 并填充 payload
bool readContainer(const std::string& path, int& version, std::string& payload)
{
	FILE* file = openStoreFile(path, false);
	if (!file)
		return false;

	bool ok = false;
	int sz = 0;
	if (fread(&version, sizeof(version), 1, file) == 1 &&
	    fread(&sz, sizeof(sz), 1, file) == 1 &&
	    sz > 0 && sz <= kMaxPayload) {
		payload.resize((size_t)sz);
		ok = (fread(&payload[0], 1, (size_t)sz, file) == (size_t)sz);
	}
	fclose(file);

	if (!ok)
		payload.clear();
	return ok;
}

// 写出 [int version][int size][bytes]（先写临时文件再改名，避免写一半崩了毁存档）
bool writeContainer(const std::string& path, const std::string& payload)
{
	std::string tmp = path + ".tmp";
	FILE* file = openStoreFile(tmp, true);
	if (!file)
		return false;

	int version = kStoreVersion;
	int sz = (int)payload.size();
	bool ok = (fwrite(&version, sizeof(version), 1, file) == 1) &&
	          (fwrite(&sz, sizeof(sz), 1, file) == 1) &&
	          (fwrite(payload.data(), 1, payload.size(), file) == payload.size());
	fclose(file);

	if (!ok) {
		removeStoreFile(tmp);
		return false;
	}

	removeStoreFile(path);
#ifdef _WIN32
	if (_wrename(ServerPaths::toWide(tmp).c_str(), ServerPaths::toWide(path).c_str()) != 0) {
#else
	if (rename(tmp.c_str(), path.c_str()) != 0) {
#endif
		removeStoreFile(tmp);
		return false;
	}
	return true;
}

} // namespace

std::string PlayerStore::safeFileName(const std::string& playerName)
{
	std::string out;
	out.reserve(playerName.size());
	for (size_t i = 0; i < playerName.size(); ++i) {
		unsigned char c = (unsigned char)playerName[i];
		// 挡掉路径分隔符、Windows 保留字符、控制字符；其余（含中文 UTF-8 字节）原样保留
		if (c < 0x20 || c == '/' || c == '\\' || c == ':' || c == '*' ||
		    c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
			out += '_';
		else
			out += (char)c;
	}
	// 收尾：去掉尾部的点和空格（Windows 会吃掉它们，导致名字对不上）
	while (!out.empty() && (out[out.size() - 1] == '.' || out[out.size() - 1] == ' '))
		out.erase(out.size() - 1);
	if (out.empty())
		out = "unknown";
	if (out.size() > 64)
		out = out.substr(0, 64);
	return out;
}

std::string PlayerStore::dirFor(Level* level)
{
	std::string base = ".";
	if (level) {
		if (ExternalFileLevelStorage* st = dynamic_cast<ExternalFileLevelStorage*>(level->getLevelStorage()))
			base = st->getLevelPath();
	}
	return base + "/players";
}

bool PlayerStore::load(Level* level, Player* p)
{
	if (!p || p->name.empty())
		return false;

	int version = 0;
	std::string payload;
	if (!readContainer(dirFor(level) + "/" + safeFileName(p->name) + ".dat", version, payload))
		return false;

	if (version < kStoreVersion) {
		LOGI("[player] %s: 旧版(v%d)玩家存档，忽略（按新玩家处理）\n", p->name.c_str(), version);
		return false;
	}

	RakNet::BitStream bs((unsigned char*)&payload[0], (int)payload.size(), false);
	RakDataInput in(bs);
	CompoundTag* tag = NbtIo::read(&in);
	if (!tag)
		return false;

	// 位置/朝向/动量/着火/空气/落地 + Player::readAdditionalSaveData（背包、血量、饥饿…）
	p->load(tag);

	// 服务器侧为每个玩家单独记的游戏模式（-1 = 跟随服务器全局配置）
	if (tag->contains("ServerGameType", Tag::TAG_Int))
		p->serverGameType = tag->getInt("ServerGameType");

	tag->deleteChildren();
	delete tag;
	return true;
}

bool PlayerStore::save(Level* level, Player* p)
{
	if (!p || p->name.empty())
		return false;

	std::string dir = dirFor(level);
	createFolderIfNotExists(dir.c_str());

	CompoundTag* tag = new CompoundTag();
	p->saveWithoutId(tag);
	tag->putInt("ServerGameType", p->serverGameType);

	RakNet::BitStream data;
	RakDataOutput out(data);
	NbtIo::write(tag, &out);
	tag->deleteChildren();
	delete tag;

	std::string payload((const char*)data.GetData(), (size_t)data.GetNumberOfBytesUsed());
	return writeContainer(dir + "/" + safeFileName(p->name) + ".dat", payload);
}
