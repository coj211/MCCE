#include "MeshBuffer.h"
#include "GLBufferPool.h"
#include <string.h>

MeshBuffer::MeshBuffer() {
	this->transformX = 0;
	this->transformY = 0;
	this->transformZ = 0;
	memset(this, 0, sizeof(MeshBuffer));
}

MeshBuffer::MeshBuffer(MeshBuffer&& o) : MeshBuffer() {
	this->_move(o);
}

MeshBuffer::MeshBuffer(const MeshBuffer::VertexFormat* vf, void* verts, int32_t vertCount, void* indices, uint32_t idxCount, uint32_t idxSize, uint32_t drawMode) {
	this->arrayBuffer = 0;
	this->arrayElementsBuffer = 0;
	this->arraysCount = 0;
	this->elementsCount = 0;
	this->drawMode = 0;
	this->transformX = 0;
	this->transformY = 0;
	this->transformZ = 0;
	this->load(vf, verts, vertCount, indices, idxCount, idxSize, drawMode);
}

bool MeshBuffer::_load(const MeshBuffer::VertexFormat* vf, void* verts, int32_t vertCount, void* indices, uint32_t idxCount, uint32_t idxSize, uint32_t drawMode) {
	GLenum v11;
	int32_t v12;

	if (!verts || vertCount <= 2 || !drawMode || !vf) return 0;
	this->arraysCount = vertCount;
	this->drawMode = drawMode;
	this->elementsCount = idxCount;
	v11 = GL_UNSIGNED_SHORT;
	this->vertexFormat = vf;
	if (idxSize != 2) v11 = GL_UNSIGNED_BYTE;
	this->drawType = v11;
	v12 = vf->stride * vertCount;
	if (!this->arrayBuffer) {
		int32_t buf = glBufferPool.get();
		this->arrayBuffer = buf;
		if (!buf) return 0;
	}
	glBindBuffer2(GL_ARRAY_BUFFER, this->arrayBuffer);
	glBufferData2(GL_ARRAY_BUFFER, v12, verts, GL_STATIC_DRAW);
	if (this->elementsCount && idxSize && indices) {
		if (!this->arrayElementsBuffer) {
			int32_t v14 = glBufferPool.get();
			this->arrayElementsBuffer = v14;
			if (!v14) return 0;
		}
		glBindBuffer2(GL_ELEMENT_ARRAY_BUFFER, this->arrayElementsBuffer);
		glBufferData2(GL_ELEMENT_ARRAY_BUFFER, this->elementsCount * idxSize, indices, GL_STATIC_DRAW);
	} else {
		if (this->arrayElementsBuffer) glBufferPool.release(this->arrayElementsBuffer);
		this->arrayElementsBuffer = 0;
	}
	return glGetError() == 0;
}

void MeshBuffer::_move(MeshBuffer& o) {
	this->reset();
	this->arrayBuffer = o.arrayBuffer;
	this->arraysCount = o.arraysCount;
	this->vertexFormat = o.vertexFormat;
	this->arrayElementsBuffer = o.arrayElementsBuffer;
	this->elementsCount = o.elementsCount;
	this->drawType = o.drawType;
	this->drawMode = o.drawMode;
	o.arrayElementsBuffer = 0;
	o.arrayBuffer = 0;
	o.elementsCount = 0;
	o.arraysCount = 0;
	o.vertexFormat = 0;
}

bool MeshBuffer::isValid() {
	if (this->arrayBuffer) {
		if (this->arraysCount <= 3) return 0;
		if (this->drawMode) {
			return this->vertexFormat != 0;
		}
	}
	return 0;
}

MeshBuffer& MeshBuffer::operator=(MeshBuffer&& o) {
	this->_move(o);
	return *this;
}

void MeshBuffer::render() {
	if (this->arrayBuffer && this->arraysCount > 3 && this->drawMode && this->vertexFormat) {
		glBindBuffer2(GL_ARRAY_BUFFER, this->arrayBuffer);
		glBindBuffer2(GL_ELEMENT_ARRAY_BUFFER, this->arrayElementsBuffer);
		this->vertexFormat->bindArrays();
		if (this->elementsCount) {
			glDrawElements(this->drawMode, this->elementsCount, this->drawType, 0);
		} else {
			glDrawArrays(this->drawMode, 0, this->arraysCount);
		}
	}
}

bool MeshBuffer::load(const MeshBuffer::VertexFormat* vf, void* verts, int32_t vertCount, void* indices, uint32_t idxCount, uint32_t idxSize, uint32_t drawMode) {
	if (this->_load(vf, verts, vertCount, indices, idxCount, idxSize, drawMode)) {
		return 1;
	}
	this->reset();
	return 0;
}

void MeshBuffer::reset() {
	if (this->arrayBuffer) glBufferPool.release(this->arrayBuffer);
	if (this->arrayElementsBuffer) glBufferPool.release(this->arrayElementsBuffer);

	this->arrayBuffer = 0;
	this->arraysCount = 0;
	this->arrayElementsBuffer = 0;
	this->elementsCount = 0;
	this->drawMode = 0;
	this->drawType = 0;
	this->vertexFormat = 0;
}

MeshBuffer::~MeshBuffer() {
	this->reset();
}

MeshBuffer::VertexFormat::VertexFormat() {
	this->stride = 0;
	for (int32_t i = 0; i < 4; ++i) this->offsets[i] = 255;
}

void MeshBuffer::VertexFormat::bindArrays() const {
	glVertexPointer2(3, GL_FLOAT, this->stride, (void*)(size_t)this->offsets[0]);
	uint8_t texOffset = this->offsets[1];
	if (texOffset != 255) glTexCoordPointer2(2, GL_FLOAT, this->stride, (void*)(size_t)texOffset);
	uint8_t colOffset = this->offsets[2];
	if (colOffset != 255) glColorPointer2(4, GL_UNSIGNED_BYTE, this->stride, (void*)(size_t)colOffset);
	uint8_t normOffset = this->offsets[3];
	if (normOffset != 255) glNormalPointer(GL_BYTE, this->stride, (void*)(size_t)normOffset);
}

void MeshBuffer::VertexFormat::enableField(MeshBuffer::VertexFormat::Field f) {
	static int32_t FieldSize[] = { 12, 8, 4, 4 };
	if (this->offsets[f] == 255) {
		this->offsets[f] = stride;
		this->stride += FieldSize[f];
	}
}
