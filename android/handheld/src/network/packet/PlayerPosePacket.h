#ifndef NET_MINECRAFT_NETWORK_PACKET__PlayerPosePacket_H__
#define NET_MINECRAFT_NETWORK_PACKET__PlayerPosePacket_H__

#include "../Packet.h"

//
// 玩家姿态/动作（模组用）：把"某个玩家现在在做什么动作"告诉别人。
//
// 方向：
//   客户端 -> 服务器：我自己的动作变了（我在游泳 / 我不游了）。
//   服务器 -> 同世界其他玩家：广播（服务器侧的 mod 也可以直接给某个人挂动作）。
//
// action 为空串 = 取消动作，回原版姿态。
// model  为空串 = 没有外形（玩家原样）；数字串 = 脚本生物定义的 typeId（外形）。
//
// 单机 / 房主模式不发这个包（没有别的客户端要通知：房主自己的动作本地直接生效，
// 连进来的玩家会各自上报自己的）。老客户端不认识这个包 -> createPacket 返回 NULL
// -> 直接忽略，不会崩。
//
class PlayerPosePacket : public Packet
{
public:
	int playerId;
	RakNet::RakString action;
	RakNet::RakString model;

	PlayerPosePacket()
	:	playerId(0)
	{
	}

	PlayerPosePacket(int playerId, const RakNet::RakString& action, const RakNet::RakString& model)
	:	playerId(playerId),
		action(action),
		model(model)
	{
	}

	void write(RakNet::BitStream* bitStream)
	{
		bitStream->Write((RakNet::MessageID)(ID_USER_PACKET_ENUM + PACKET_PLAYERPOSE));
		bitStream->Write(playerId);
		bitStream->Write(action);
		bitStream->Write(model);
	}

	void read(RakNet::BitStream* bitStream)
	{
		bitStream->Read(playerId);
		bitStream->Read(action);
		bitStream->Read(model);
	}

	void handle(const RakNet::RakNetGUID& source, NetEventCallback* callback)
	{
		callback->handle(source, (PlayerPosePacket*)this);
	}
};

#endif /*NET_MINECRAFT_NETWORK_PACKET__PlayerPosePacket_H__*/
