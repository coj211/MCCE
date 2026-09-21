#include "RenderChunk.h"

int RenderChunk::runningId = 0;

RenderChunk::RenderChunk() :
	vboId(-1),
	normalVboId(0),
	vertexCount(0)
{
	id = ++runningId;
}

RenderChunk::RenderChunk( GLuint vboId_, int vertexCount_ )
:	vboId(vboId_),
	normalVboId(0),
	vertexCount(vertexCount_)
{
	id = ++runningId;
}
