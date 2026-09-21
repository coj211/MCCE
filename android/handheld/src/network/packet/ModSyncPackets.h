#ifndef NET_MINECRAFT_NETWORK_PACKET__ModSyncPackets_H__
#define NET_MINECRAFT_NETWORK_PACKET__ModSyncPackets_H__

#include "../Packet.h"
#include <vector>
#include <string>

// ---------------------------------------------------------------------------
// Server -> client: the list of mod files the host enables for this world.
// ---------------------------------------------------------------------------
class ModListPacket : public Packet
{
public:
	struct Entry {
		std::string file;   // e.g. "aether_mod.zip"
		int size;           // bytes, -1 if unknown
	};
	std::vector<Entry> entries;

	ModListPacket() {}
	ModListPacket(const std::vector<Entry>& entries) : entries(entries) {}

	void write(RakNet::BitStream* bitStream)
	{
		bitStream->Write((RakNet::MessageID)(ID_USER_PACKET_ENUM + PACKET_MODSLIST));
		int count = (int)entries.size();
		bitStream->Write(count);
		for (int i = 0; i < count; ++i) {
			RakNet::RakString name(entries[i].file.c_str());
			bitStream->Write(name);
			bitStream->Write(entries[i].size);
		}
	}

	void read(RakNet::BitStream* bitStream)
	{
		int count = 0;
		bitStream->Read(count);
		if (count < 0 || count > 256)
			count = 0;
		entries.clear();
		for (int i = 0; i < count; ++i) {
			RakNet::RakString name;
			bitStream->Read(name);
			int size = -1;
			bitStream->Read(size);
			Entry e;
			e.file = name.C_String();
			e.size = size;
			entries.push_back(e);
		}
	}

	void handle(const RakNet::RakNetGUID& source, NetEventCallback* callback)
	{
		callback->handle(source, (ModListPacket*)this);
	}
};

// ---------------------------------------------------------------------------
// Client -> server: "please send me these mod files" (empty = client already
// has everything and is ready to proceed).
// ---------------------------------------------------------------------------
class ModRequestPacket : public Packet
{
public:
	std::vector<std::string> files;

	ModRequestPacket() {}
	ModRequestPacket(const std::vector<std::string>& files) : files(files) {}

	void write(RakNet::BitStream* bitStream)
	{
		bitStream->Write((RakNet::MessageID)(ID_USER_PACKET_ENUM + PACKET_MODREQUEST));
		int count = (int)files.size();
		bitStream->Write(count);
		for (int i = 0; i < count; ++i) {
			RakNet::RakString name(files[i].c_str());
			bitStream->Write(name);
		}
	}

	void read(RakNet::BitStream* bitStream)
	{
		int count = 0;
		bitStream->Read(count);
		if (count < 0 || count > 256)
			count = 0;
		files.clear();
		for (int i = 0; i < count; ++i) {
			RakNet::RakString name;
			bitStream->Read(name);
			files.push_back(name.C_String());
		}
	}

	void handle(const RakNet::RakNetGUID& source, NetEventCallback* callback)
	{
		callback->handle(source, (ModRequestPacket*)this);
	}
};

// ---------------------------------------------------------------------------
// Server -> client: one chunk of a mod zip. Chunks of one file are sent in
// order; a new `file` name starts the next file.
// ---------------------------------------------------------------------------
class ModFilePacket : public Packet
{
public:
	std::string file;
	int offset;   // absolute byte offset in the file
	int total;    // total file size
	std::vector<unsigned char> data;

	ModFilePacket() : offset(0), total(0) {}
	ModFilePacket(const std::string& file, int offset, int total, const unsigned char* data, int len)
	:	file(file), offset(offset), total(total), data(data, data + len)
	{}

	void write(RakNet::BitStream* bitStream)
	{
		bitStream->Write((RakNet::MessageID)(ID_USER_PACKET_ENUM + PACKET_MODFILE));
		RakNet::RakString name(file.c_str());
		bitStream->Write(name);
		bitStream->Write(offset);
		bitStream->Write(total);
		int len = (int)data.size();
		bitStream->Write(len);
		if (len > 0)
			bitStream->Write((const char*)data.data(), len);
	}

	void read(RakNet::BitStream* bitStream)
	{
		RakNet::RakString name;
		bitStream->Read(name);
		file = name.C_String();
		bitStream->Read(offset);
		bitStream->Read(total);
		int len = 0;
		bitStream->Read(len);
		if (len < 0 || len > 32 * 1024 * 1024)
			len = 0;
		data.resize(len);
		if (len > 0)
			bitStream->Read((char*)data.data(), len);
	}

	void handle(const RakNet::RakNetGUID& source, NetEventCallback* callback)
	{
		callback->handle(source, (ModFilePacket*)this);
	}
};

// ---------------------------------------------------------------------------
// Client -> server: all requested mods are now installed & enabled; the
// server may continue the join (send StartGamePacket).
// ---------------------------------------------------------------------------
class ModsReadyPacket : public Packet
{
public:
	ModsReadyPacket() {}

	void write(RakNet::BitStream* bitStream)
	{
		bitStream->Write((RakNet::MessageID)(ID_USER_PACKET_ENUM + PACKET_MODSREADY));
	}

	void read(RakNet::BitStream* bitStream)
	{
	}

	void handle(const RakNet::RakNetGUID& source, NetEventCallback* callback)
	{
		callback->handle(source, (ModsReadyPacket*)this);
	}
};

#endif /*NET_MINECRAFT_NETWORK_PACKET__ModSyncPackets_H__*/
