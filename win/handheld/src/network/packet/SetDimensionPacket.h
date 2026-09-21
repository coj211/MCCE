#ifndef NET_MINECRAFT_NETWORK_PACKET__SetDimensionPacket_H__
#define NET_MINECRAFT_NETWORK_PACKET__SetDimensionPacket_H__

#include "../Packet.h"

// Server -> client: the receiving player is being moved into dimension world
// `dimensionId` at (x,y,z). The client drops its current world view, rebuilds
// its local level for the new dimension and re-requests chunks from there.
// The server follows this packet with the new world's entity/player snapshot.
class SetDimensionPacket : public Packet
{
public:
	int dimensionId;
	float x, y, z;

	SetDimensionPacket()
	:	dimensionId(0),
		x(0), y(64), z(0)
	{
	}

	SetDimensionPacket(int dimensionId, float x, float y, float z)
	:	dimensionId(dimensionId),
		x(x), y(y), z(z)
	{
	}

	void write(RakNet::BitStream* bitStream)
	{
		bitStream->Write((RakNet::MessageID)(ID_USER_PACKET_ENUM + PACKET_SETDIMENSION));
		bitStream->Write(dimensionId);
		bitStream->Write(x);
		bitStream->Write(y);
		bitStream->Write(z);
	}

	void read(RakNet::BitStream* bitStream)
	{
		bitStream->Read(dimensionId);
		bitStream->Read(x);
		bitStream->Read(y);
		bitStream->Read(z);
	}

	void handle(const RakNet::RakNetGUID& source, NetEventCallback* callback)
	{
		callback->handle(source, (SetDimensionPacket*)this);
	}
};

#endif /*NET_MINECRAFT_NETWORK_PACKET__SetDimensionPacket_H__*/
