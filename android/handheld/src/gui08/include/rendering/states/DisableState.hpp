#pragma once
// 适配头：0.8.1 的 DisableState 用 0.6.1 的 GL 封装（gles.h，避免 unigl.h 与 0.6.1 渲染冲突）
#include "../../../../client/renderer/gles.h"

struct DisableState{
	GLenum disabled;

	DisableState(GLenum s);
	~DisableState();
};
