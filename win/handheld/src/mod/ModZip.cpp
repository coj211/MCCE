#include "ModZip.h"

#include <cstdio>
#include <cstring>

#include <zlib.h>

// Case-insensitive string compare (platform-neutral).
#if defined(_WIN32)
#define MOD_STRICMP _stricmp
#else
#define MOD_STRICMP strcasecmp
#endif

// ---------------------------------------------------------------------------
// Minimal zip reader: parses the End-Of-Central-Directory + Central
// Directory and uses zlib's raw inflate for DEFLATE entries. Enough for
// reading .js sources and .png textures out of a mod package.
// ---------------------------------------------------------------------------

static bool readAt(FILE* f, long off, void* buf, size_t len) {
	if (fseek(f, off, SEEK_SET) != 0)
		return false;
	return fread(buf, 1, len, f) == len;
}

static unsigned int rd32(const unsigned char* p) {
	return (unsigned int)p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

static unsigned short rd16(const unsigned char* p) {
	return (unsigned short)(p[0] | (p[1] << 8));
}

bool modZipRead(const std::string& path, std::vector<ModZipEntry>& out) {
	FILE* f = fopen(path.c_str(), "rb");
	if (!f)
		return false;
	out.clear();

	fseek(f, 0, SEEK_END);
	long fileSize = ftell(f);
	if (fileSize < 22) {
		fclose(f);
		return false;
	}

	// Search backwards for the End-Of-Central-Directory signature.
	long back = (fileSize < 65557) ? fileSize : 65557;
	std::vector<unsigned char> tail((size_t)back);
	if (!readAt(f, fileSize - back, tail.data(), (size_t)back)) {
		fclose(f);
		return false;
	}
	long eocd = -1;
	for (long i = (long)tail.size() - 22; i >= 0; --i) {
		if (tail[i] == 0x50 && tail[i+1] == 0x4b && tail[i+2] == 0x05 && tail[i+3] == 0x06) {
			eocd = fileSize - back + i;
			break;
		}
	}
	if (eocd < 0) {
		fclose(f);
		return false;
	}

	unsigned char e[22];
	if (!readAt(f, eocd, e, 22)) {
		fclose(f);
		return false;
	}
	unsigned short entryCount = rd16(e + 10);
	unsigned int cdOff = rd32(e + 16);

	unsigned int pos = cdOff;
	for (unsigned short i = 0; i < entryCount; ++i) {
		unsigned char h[46];
		if (!readAt(f, pos, h, 46))
			break;
		if (!(h[0] == 0x50 && h[1] == 0x4b && h[2] == 0x01 && h[3] == 0x02))
			break; // not a central directory entry - stop

		unsigned short method = rd16(h + 10);
		unsigned int compSize = rd32(h + 20);
		unsigned int uncompSize = rd32(h + 24);
		unsigned short nameLen = rd16(h + 28);
		unsigned short extraLen = rd16(h + 30);
		unsigned short commentLen = rd16(h + 32);
		unsigned int localOff = rd32(h + 42);

		std::string name;
		if (nameLen > 0) {
			name.resize(nameLen);
			readAt(f, pos + 46, &name[0], nameLen);
		}

		// Local file header: data starts after its name + extra fields.
		unsigned char lh[30];
		unsigned int dataOff = localOff + 30;
		if (readAt(f, localOff, lh, 30)) {
			unsigned short lnameLen = rd16(lh + 26);
			unsigned short lextraLen = rd16(lh + 28);
			dataOff = localOff + 30 + lnameLen + lextraLen;
		}

		ModZipEntry en;
		en.name = name;
		en.method = method;
		en.compSize = compSize;
		en.uncompSize = uncompSize;
		en.dataOffset = dataOff;
		out.push_back(en);

		pos += 46 + nameLen + extraLen + commentLen;
	}

	fclose(f);
	return !out.empty();
}

bool modZipExtract(const std::string& path, const ModZipEntry& e, std::vector<unsigned char>& out) {
	FILE* f = fopen(path.c_str(), "rb");
	if (!f)
		return false;
	bool ok = false;

	// Guard against zip-bomb / corrupt archives: cap the uncompressed size
	// (memory) and the compressed read (matches the cap below).
	const unsigned int kMaxUncomp = 128u << 20;  // 128 MB per entry
	const unsigned int kMaxComp = 64u << 20;     // 64 MB compressed input
	if (e.uncompSize > kMaxUncomp || e.compSize > kMaxComp) {
		fclose(f);
		return false;
	}

	if (e.method == 0) {
		// Stored: raw copy.
		out.resize(e.uncompSize);
		ok = readAt(f, e.dataOffset, out.data(), e.uncompSize);
	} else if (e.method == 8) {
		// Deflate: raw stream (no zlib header).
		std::vector<unsigned char> comp(e.compSize ? e.compSize : 1);
		if (!readAt(f, e.dataOffset, comp.data(), e.compSize)) {
			fclose(f);
			return false;
		}
		out.resize(e.uncompSize ? e.uncompSize : 1);
		z_stream zs;
		memset(&zs, 0, sizeof(zs));
		if (inflateInit2(&zs, -MAX_WBITS) != Z_OK) {
			fclose(f);
			return false;
		}
		zs.next_in = comp.data();
		zs.avail_in = (uInt)e.compSize;
		zs.next_out = out.data();
		zs.avail_out = (uInt)out.size();
		int r = inflate(&zs, Z_FINISH);
		inflateEnd(&zs);
		ok = (r == Z_STREAM_END);
	}

	fclose(f);
	return ok;
}

const ModZipEntry* modZipFind(const std::vector<ModZipEntry>& entries, const std::string& name) {
	for (size_t i = 0; i < entries.size(); ++i) {
		if (MOD_STRICMP(entries[i].name.c_str(), name.c_str()) == 0)
			return &entries[i];
	}
	return NULL;
}
