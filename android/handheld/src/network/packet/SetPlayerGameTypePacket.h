#ifndef NET_MINECRAFT_NETWORK_PACKET__SetPlayerGameTypePacket_H__
#define NET_MINECRAFT_NETWORK_PACKET__SetPlayerGameTypePacket_H__

#include "../Packet.h"

//
// 服务器 -> 客户端：告诉**这一个**客户端"你的游戏模式变了"。
//
// 为什么需要它：游戏模式是"按玩家分别"的（服务器可以只给某个人开创造），
// 而客户端只在收到 StartGame 时才按 gameType 建自己的玩家；StartGame 会重建
// 整个世界，不能拿来做热切换。所以加这个轻量包，客户端收到后就地切换自己的
// GameMode 实例并重算能力，不用重连。
//
// 单机 / 房主模式永远不会收到这个包（只有独立服务器在管理员切模式时发）。
//
class SetPlayerGameTypePacket : public Packet
{
public:
	int gameType;

	SetPlayerGameTypePacket()
	:	gameType(0)
	{
	}

	SetPlayerGameTypePacket(int gameType)
	:	gameType(gameType)
	{
	}

	void write(RakNet::BitStream* bitStream)
	{
		bitStream->Write((RakNet::MessageID)(ID_USER_PACKET_ENUM + PACKET_SETPLAYERGAMETYPE));
		bitStream->Write(gameType);
	}

	void read(RakNet::BitStream* bitStream)
	{
		bitStream->Read(gameType);
	}

	void handle(const RakNet::RakNetGUID& source, NetEventCallback* callback)
	{
		callback->handle(source, (SetPlayerGameTypePacket*)this);
	}
};

#endif /*NET_MINECRAFT_NETWORK_PACKET__SetPlayerGameTypePacket_H__*/
