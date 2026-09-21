#ifndef NET_MINECRAFT_CLIENT_RENDERER__GLBufferPool_H__
#define NET_MINECRAFT_CLIENT_RENDERER__GLBufferPool_H__

// 0.8.1 背包渲染管线：VBO 缓冲池。MeshBuffer 从这里取/还 GL buffer，
// 避免反复 glGenBuffers/glDeleteBuffers。

#include <set>
#include <deque>
#include "gles.h"

struct GLBufferPool {
	std::set<unsigned int> usedBuffers;
	std::deque<unsigned int> unusedBuffers;
	int reserveCnt;

	GLBufferPool(unsigned int reserveCnt);
	~GLBufferPool();
	bool trim();
	void release(unsigned int n);
	GLuint get();
};

extern struct GLBufferPool glBufferPool;

#endif /*NET_MINECRAFT_CLIENT_RENDERER__GLBufferPool_H__*/
