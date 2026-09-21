#ifndef NET_MINECRAFT_CLIENT_RENDERER__MeshBuffer_H__
#define NET_MINECRAFT_CLIENT_RENDERER__MeshBuffer_H__

// 0.8.1 背包渲染管线：MeshBuffer（VBO 封装）。由 Touch::InventoryPane 等
// 批量渲染物品图标/方块。用 gles.h 的 GL 封装（gl*2 宏），不依赖 unigl.h。

#include "gles.h"
#include <cstdint>

struct MeshBuffer {
	struct VertexFormat {
		enum Field {
			FIELD0,   // position (3 floats)
			FIELD1,   // texture  (2 floats)
			FIELD2,   // color    (4 bytes)
			FIELD3    // normal   (3 bytes)
		};
		uint8_t offsets[4];
		uint8_t stride;
		char align, align1, align2;

		VertexFormat();
		void bindArrays() const;
		void enableField(VertexFormat::Field);
	};

	int32_t arrayBuffer;
	int32_t arrayElementsBuffer;
	int32_t arraysCount;
	int32_t elementsCount;
	float transformX, transformY, transformZ;
	int32_t drawMode, drawType;
	const MeshBuffer::VertexFormat* vertexFormat;

	MeshBuffer();
	MeshBuffer(MeshBuffer&& o);
	MeshBuffer(const MeshBuffer::VertexFormat* vf, void* verts, int32_t vertCount, void* indices, uint32_t idxCount, uint32_t idxSize, uint32_t drawMode);
	bool _load(const MeshBuffer::VertexFormat* vf, void* verts, int32_t vertCount, void* indices, uint32_t idxCount, uint32_t idxSize, uint32_t drawMode);
	void _move(MeshBuffer& o);
	bool isValid();
	bool load(const MeshBuffer::VertexFormat* vf, void* verts, int32_t vertCount, void* indices, uint32_t idxCount, uint32_t idxSize, uint32_t drawMode);
	MeshBuffer& operator=(MeshBuffer&& o);
	void render();
	void reset();
	~MeshBuffer();
};

#endif /*NET_MINECRAFT_CLIENT_RENDERER__MeshBuffer_H__*/
