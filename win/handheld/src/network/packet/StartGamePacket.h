#ifndef NET_MINECRAFT_NETWORK_PACKET__StartGamePacket_H__
#define NET_MINECRAFT_NETWORK_PACKET__StartGamePacket_H__

#include "../Packet.h"
#include "../../world/level/LevelSettings.h"

class StartGamePacket : public Packet
{
public:
	long levelSeed;
	int levelGeneratorVersion;
	int gameType;
	// Multi-world support (fields appended at the tail so older peers that
	// only read the original fields still parse the packet correctly).
	int worldType;    // WorldType::Old / WorldType::Infinite
	int dimensionId;  // which dimension world this player is joining into

	int entityId;
	float x, y, z;

	StartGamePacket()
	:	levelSeed(0),
		levelGeneratorVersion(0),
		gameType(GameType::Creative),
		worldType(WorldType::Old),
		dimensionId(0)
	{
	}

	StartGamePacket(long seed, int levelGeneratorVersion, int gameType, int entityId, float x, float y, float z,
		int worldType = WorldType::Old, int dimensionId = 0)
	:	levelSeed(seed),
		levelGeneratorVersion(levelGeneratorVersion),
		gameType(gameType),
		worldType(worldType),
		dimensionId(dimensionId),
		entityId(entityId),
		x(x),
		y(y),
		z(z)
	{
	}

	void write(RakNet::BitStream* bitStream)
	{
		bitStream->Write((RakNet::MessageID)(ID_USER_PACKET_ENUM + PACKET_STARTGAME));

		bitStream->Write(levelSeed);
		bitStream->Write(levelGeneratorVersion);
		bitStream->Write(gameType);
		bitStream->Write(entityId);
		bitStream->Write(x);
		bitStream->Write(y);
		bitStream->Write(z);
		bitStream->Write(worldType);
		bitStream->Write(dimensionId);
	}

	void read(RakNet::BitStream* bitStream)
	{
		bitStream->Read(levelSeed);
		bitStream->Read(levelGeneratorVersion);
		bitStream->Read(gameType);
		bitStream->Read(entityId);
		bitStream->Read(x);
		bitStream->Read(y);
		bitStream->Read(z);
		bitStream->Read(worldType);
		bitStream->Read(dimensionId);
	}

	void handle(const RakNet::RakNetGUID& source, NetEventCallback* callback)
	{
		callback->handle(source, (StartGamePacket*)this);
	}
};

#endif /*NET_MINECRAFT_NETWORK_PACKET__StartGamePacket_H__*/
