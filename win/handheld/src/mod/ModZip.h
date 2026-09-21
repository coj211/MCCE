#ifndef NET_MINECRAFT_MOD_MODZIP_H__
#define NET_MINECRAFT_MOD_MODZIP_H__

#include <string>
#include <vector>

// One file entry inside a mod package (.zip). The raw data lives at
// dataOffset in the archive file; extract with modZipExtract().
struct ModZipEntry {
	std::string name;      // path inside the archive, e.g. "main.js"
	unsigned int compSize;
	unsigned int uncompSize;
	unsigned int dataOffset; // absolute file offset of the compressed data
	int method;            // 0 = stored, 8 = deflate
};

// Parse the central directory of a zip file. Returns false if the file
// is not a readable zip archive.
bool modZipRead(const std::string& path, std::vector<ModZipEntry>& out);

// Decompress one entry into `out`. Returns false on error.
bool modZipExtract(const std::string& path, const ModZipEntry& e, std::vector<unsigned char>& out);

// Convenience: find an entry by exact name (case-insensitive), or NULL.
const ModZipEntry* modZipFind(const std::vector<ModZipEntry>& entries, const std::string& name);

#endif /*NET_MINECRAFT_MOD_MODZIP_H__*/
