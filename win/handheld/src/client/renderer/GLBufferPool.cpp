#include "GLBufferPool.h"

GLBufferPool glBufferPool(10);

GLBufferPool::GLBufferPool(unsigned int reserveCnt) {
	this->reserveCnt = reserveCnt;
}

GLBufferPool::~GLBufferPool() {
	this->trim();
}

bool GLBufferPool::trim() {
	if (this->unusedBuffers.size() == 0) {
		return 0;
	}
	while (!this->unusedBuffers.empty()) {
		GLuint b = this->unusedBuffers.front();
		glDeleteBuffers(1, &b);
		this->unusedBuffers.pop_front();
	}
	return 1;
}

void GLBufferPool::release(unsigned int n) {
	this->unusedBuffers.push_back(n);
	this->usedBuffers.erase(n);
}

GLuint GLBufferPool::get() {
	if (this->unusedBuffers.size() < (size_t)this->reserveCnt) {
		while (this->unusedBuffers.size() < (size_t)this->reserveCnt) {
			unsigned int bf;
			glGenBuffers2(1, &bf);
			if (glGetError()) break;
			this->unusedBuffers.push_back(bf);
		}
	}
	if (this->unusedBuffers.empty()) {
		return 0;
	}
	unsigned int bf = this->unusedBuffers.front();
	this->unusedBuffers.pop_front();
	this->usedBuffers.insert(bf);
	return bf;
}
