// Raw desktop-OpenGL bindings for Duktape mods + the Render camera API.
// New engine implementation (nothing copied from the deprecated stage-10
// bindings); GL names follow the official OpenGL spelling so mods write
// familiar GLSL-style code.
#include "GlJsBindings.h"
#include "ModEngine.h"

#if defined(_WIN32) || defined(MACOS) || defined(LINUX)

#include "../client/renderer/gles.h"
#include "../client/renderer/GameRenderer.h"
#include "../client/renderer/Chunk.h"
#include "../client/Minecraft.h"

#define MCMODR() (ModEngine::instance ? ModEngine::instance->minecraft() : NULL)
#define MCGR()   (MCMODR() ? MCMODR()->gameRenderer : NULL)

// ─────────────────────────── helpers ───────────────────────────

// glBufferData-style payload: accept typed arrays (raw bytes) or a plain
// JS number array (converted to little-endian float32 bytes).
static bool jsReadBytes(duk_context* ctx, duk_idx_t idx, std::vector<unsigned char>& out) {
	if (duk_is_null_or_undefined(ctx, idx))
		return true; // sized allocation (textures)
	if (duk_is_buffer_data(ctx, idx)) {
		duk_size_t n = 0;
		void* p = duk_get_buffer_data(ctx, idx, &n);
		if (p && n) {
			out.assign((unsigned char*)p, (unsigned char*)p + n);
			return true;
		}
		if (n == 0) return true;
		return false;
	}
	if (duk_is_array(ctx, idx)) {
		duk_idx_t len = (duk_idx_t)duk_get_length(ctx, idx);
		out.resize((size_t)len * 4);
		for (duk_idx_t i = 0; i < len; ++i) {
			duk_get_prop_index(ctx, idx, i);
			float f = (float)duk_get_number(ctx, -1);
			duk_pop(ctx);
			memcpy(&out[(size_t)i * 4], &f, 4);
		}
		return true;
	}
	return false;
}

static const char* shaderInfoLog(duk_context* ctx, GLuint obj, bool isProgram) {
	GLint ok = 0;
	GLint len = 0;
	if (isProgram) { glGetProgramiv(obj, GL_LINK_STATUS, &ok); glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &len); }
	else           { glGetShaderiv(obj, GL_COMPILE_STATUS, &ok);  glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &len); }
	if (ok) {
		duk_push_string(ctx, "OK");
		return NULL; // "OK" already pushed
	}
	if (len <= 1) {
		duk_push_string(ctx, "compile/link failed");
		return NULL;
	}
	std::vector<char> buf((size_t)len + 1, 0);
	if (isProgram) glGetProgramInfoLog(obj, len, NULL, buf.data());
	else           glGetShaderInfoLog(obj, len, NULL, buf.data());
	duk_push_lstring(ctx, buf.data(), strlen(buf.data()));
	return NULL;
}

// ─────────────────────────── GL functions ───────────────────────────
#define GLF(name) static duk_ret_t js##name(duk_context* ctx)

GLF(glCreateShader) {
	int type = (int)duk_require_int(ctx, 0);
	duk_push_int(ctx, (int)glCreateShader((GLenum)type));
	return 1;
}
GLF(glShaderSource) {
	GLuint s = (GLuint)duk_require_int(ctx, 0);
	const char* src = duk_require_string(ctx, 1);
	glShaderSource(s, 1, &src, NULL);
	return 0;
}
GLF(glCompileShader) {
	glCompileShader((GLuint)duk_require_int(ctx, 0));
	return 0;
}
GLF(glGetShaderLog) {
	GLuint s = (GLuint)duk_require_int(ctx, 0);
	shaderInfoLog(ctx, s, false);
	return 1;
}
GLF(glDeleteShader) {
	glDeleteShader((GLuint)duk_require_int(ctx, 0));
	return 0;
}
GLF(glCreateProgram) {
	duk_push_int(ctx, (int)glCreateProgram());
	return 1;
}
GLF(glAttachShader) {
	GLuint p = (GLuint)duk_require_int(ctx, 0);
	GLuint s = (GLuint)duk_require_int(ctx, 1);
	glAttachShader(p, s);
	return 0;
}
GLF(glLinkProgram) {
	glLinkProgram((GLuint)duk_require_int(ctx, 0));
	return 0;
}
GLF(glGetProgramLog) {
	GLuint p = (GLuint)duk_require_int(ctx, 0);
	shaderInfoLog(ctx, p, true);
	return 1;
}
GLF(glDeleteProgram) {
	glDeleteProgram((GLuint)duk_require_int(ctx, 0));
	return 0;
}
GLF(glUseProgram) {
	glUseProgram((GLuint)duk_require_int(ctx, 0));
	return 0;
}
GLF(glGetUniformLocation) {
	GLuint p = (GLuint)duk_require_int(ctx, 0);
	const char* name = duk_require_string(ctx, 1);
	duk_push_int(ctx, (int)glGetUniformLocation(p, name));
	return 1;
}
GLF(glUniform1i) {
	glUniform1i(duk_require_int(ctx, 0), duk_require_int(ctx, 1));
	return 0;
}
GLF(glUniform1f) {
	glUniform1f(duk_require_int(ctx, 0), (GLfloat)duk_require_number(ctx, 1));
	return 0;
}
GLF(glUniform2f) {
	glUniform2f(duk_require_int(ctx, 0), (GLfloat)duk_require_number(ctx, 1), (GLfloat)duk_require_number(ctx, 2));
	return 0;
}
GLF(glUniform3f) {
	glUniform3f(duk_require_int(ctx, 0), (GLfloat)duk_require_number(ctx, 1),
		(GLfloat)duk_require_number(ctx, 2), (GLfloat)duk_require_number(ctx, 3));
	return 0;
}
GLF(glUniform4f) {
	glUniform4f(duk_require_int(ctx, 0), (GLfloat)duk_require_number(ctx, 1),
		(GLfloat)duk_require_number(ctx, 2), (GLfloat)duk_require_number(ctx, 3), (GLfloat)duk_require_number(ctx, 4));
	return 0;
}
GLF(glUniformMatrix4fv) {
	// glUniformMatrix4fv(location, values) — values is a 16-element JS
	// number array in column-major order (no transpose, like the engine's
	// glGetFloatv matrices). Feeds mat4 uniforms (inverse camera matrices,
	// light-space matrices, ...) to mod GLSL.
	GLint loc = duk_require_int(ctx, 0);
	duk_size_t n = duk_get_length(ctx, 1);
	if (n < 16) {
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "glUniformMatrix4fv needs 16 values");
		return 0;
	}
	GLfloat m[16];
	for (int i = 0; i < 16; ++i) {
		duk_get_prop_index(ctx, 1, (duk_idx_t)i);
		m[i] = (GLfloat)duk_get_number(ctx, -1);
		duk_pop(ctx);
	}
	glUniformMatrix4fv(loc, 1, GL_FALSE, m);
	return 0;
}
GLF(glActiveTexture) {
	glActiveTexture((GLenum)duk_require_int(ctx, 0));
	return 0;
}
GLF(glGenTextures) {
	GLuint t = 0;
	glGenTextures(1, &t);
	duk_push_int(ctx, (int)t);
	return 1;
}
GLF(glBindTexture) {
	glBindTexture((GLenum)duk_require_int(ctx, 0), (GLuint)duk_require_int(ctx, 1));
	return 0;
}
GLF(glDeleteTextures) {
	GLuint t = (GLuint)duk_require_int(ctx, 0);
	glDeleteTextures(1, &t);
	return 0;
}
GLF(glTexParameteri) {
	glTexParameteri((GLenum)duk_require_int(ctx, 0), (GLenum)duk_require_int(ctx, 1), duk_require_int(ctx, 2));
	return 0;
}
GLF(glTexImage2D) {
	int argc = duk_get_top(ctx);
	GLenum target = (GLenum)duk_require_int(ctx, 0);
	GLint level = duk_require_int(ctx, 1);
	GLint internal = duk_require_int(ctx, 2);
	GLsizei w = duk_require_int(ctx, 3);
	GLsizei h = duk_require_int(ctx, 4);
	GLint border = 0;
	int fmtIdx = 5, typeIdx = 6, dataIdx = 7;
	if (argc >= 9) { border = duk_require_int(ctx, 5); fmtIdx = 6; typeIdx = 7; dataIdx = 8; }
	GLenum format = (GLenum)duk_require_int(ctx, fmtIdx);
	GLenum type = (GLenum)duk_require_int(ctx, typeIdx);
	std::vector<unsigned char> bytes;
	if (!jsReadBytes(ctx, dataIdx, bytes)) {
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "glTexImage2D: bad pixel data");
		return 0;
	}
	const void* ptr = bytes.empty() ? NULL : (const void*)bytes.data();
	glTexImage2D(target, level, internal, w, h, border, format, type, ptr);
	return 0;
}
GLF(glGenBuffers) {
	GLuint b = 0;
	glGenBuffers(1, &b);
	duk_push_int(ctx, (int)b);
	return 1;
}
GLF(glBindBuffer) {
	glBindBuffer((GLenum)duk_require_int(ctx, 0), (GLuint)duk_require_int(ctx, 1));
	return 0;
}
GLF(glBufferData) {
	GLenum target = (GLenum)duk_require_int(ctx, 0);
	GLenum usage = (GLenum)duk_require_int(ctx, 2);
	std::vector<unsigned char> bytes;
	if (!jsReadBytes(ctx, 1, bytes)) {
		duk_error(ctx, DUK_ERR_TYPE_ERROR, "glBufferData: bad data");
		return 0;
	}
	glBufferData(target, bytes.empty() ? 0 : (GLsizeiptr)bytes.size(),
		bytes.empty() ? NULL : (const void*)bytes.data(), usage);
	return 0;
}
GLF(glDeleteBuffers) {
	GLuint b = (GLuint)duk_require_int(ctx, 0);
	glDeleteBuffers(1, &b);
	return 0;
}
GLF(glEnableVertexAttribArray) {
	glEnableVertexAttribArray((GLuint)duk_require_int(ctx, 0));
	return 0;
}
GLF(glDisableVertexAttribArray) {
	glDisableVertexAttribArray((GLuint)duk_require_int(ctx, 0));
	return 0;
}
GLF(glVertexAttribPointer) {
	int loc = duk_require_int(ctx, 0);
	int size = duk_require_int(ctx, 1);
	GLenum type = (GLenum)duk_require_int(ctx, 2);
	bool norm = duk_require_boolean(ctx, 3) != 0;
	int stride = duk_require_int(ctx, 4);
	duk_size_t off = (duk_size_t)duk_require_number(ctx, 5);
	glVertexAttribPointer((GLuint)loc, size, type, norm ? GL_TRUE : GL_FALSE, stride, (const void*)off);
	return 0;
}
GLF(glDrawArrays) {
	glDrawArrays((GLenum)duk_require_int(ctx, 0), duk_require_int(ctx, 1), duk_require_int(ctx, 2));
	return 0;
}
GLF(glGenFramebuffers) {
	GLuint f = 0;
	glGenFramebuffers(1, &f);
	duk_push_int(ctx, (int)f);
	return 1;
}
GLF(glBindFramebuffer) {
	glBindFramebuffer((GLenum)duk_require_int(ctx, 0), (GLuint)duk_require_int(ctx, 1));
	return 0;
}
GLF(glFramebufferTexture2D) {
	glFramebufferTexture2D((GLenum)duk_require_int(ctx, 0), (GLenum)duk_require_int(ctx, 1),
		(GLenum)duk_require_int(ctx, 2), (GLuint)duk_require_int(ctx, 3), duk_require_int(ctx, 4));
	return 0;
}
GLF(glGenRenderbuffers) {
	GLuint r = 0;
	glGenRenderbuffers(1, &r);
	duk_push_int(ctx, (int)r);
	return 1;
}
GLF(glBindRenderbuffer) {
	glBindRenderbuffer((GLenum)duk_require_int(ctx, 0), (GLuint)duk_require_int(ctx, 1));
	return 0;
}
GLF(glRenderbufferStorage) {
	glRenderbufferStorage((GLenum)duk_require_int(ctx, 0), (GLenum)duk_require_int(ctx, 1),
		duk_require_int(ctx, 2), duk_require_int(ctx, 3));
	return 0;
}
GLF(glFramebufferRenderbuffer) {
	glFramebufferRenderbuffer((GLenum)duk_require_int(ctx, 0), (GLenum)duk_require_int(ctx, 1),
		(GLenum)duk_require_int(ctx, 2), (GLuint)duk_require_int(ctx, 3));
	return 0;
}
GLF(glCheckFramebufferStatus) {
	duk_push_int(ctx, (int)glCheckFramebufferStatus((GLenum)duk_require_int(ctx, 0)));
	return 1;
}
GLF(glDeleteFramebuffers) {
	GLuint f = (GLuint)duk_require_int(ctx, 0);
	glDeleteFramebuffers(1, &f);
	return 0;
}
GLF(glDeleteRenderbuffers) {
	GLuint r = (GLuint)duk_require_int(ctx, 0);
	glDeleteRenderbuffers(1, &r);
	return 0;
}
GLF(glClearColor) {
	glClearColor((GLfloat)duk_require_number(ctx, 0), (GLfloat)duk_require_number(ctx, 1),
		(GLfloat)duk_require_number(ctx, 2), (GLfloat)duk_require_number(ctx, 3));
	return 0;
}
GLF(glClear) {
	glClear((GLbitfield)duk_require_int(ctx, 0));
	return 0;
}
GLF(glViewport) {
	glViewport(duk_require_int(ctx, 0), duk_require_int(ctx, 1), duk_require_int(ctx, 2), duk_require_int(ctx, 3));
	return 0;
}
GLF(glEnable) {
	glEnable((GLenum)duk_require_int(ctx, 0));
	return 0;
}
GLF(glDisable) {
	glDisable((GLenum)duk_require_int(ctx, 0));
	return 0;
}
GLF(glBlendFunc) {
	glBlendFunc((GLenum)duk_require_int(ctx, 0), (GLenum)duk_require_int(ctx, 1));
	return 0;
}
GLF(glDepthMask) {
	glDepthMask(duk_require_boolean(ctx, 0) ? GL_TRUE : GL_FALSE);
	return 0;
}
GLF(glColorMask) {
	glColorMask(duk_require_boolean(ctx, 0) ? GL_TRUE : GL_FALSE,
		duk_require_boolean(ctx, 1) ? GL_TRUE : GL_FALSE,
		duk_require_boolean(ctx, 2) ? GL_TRUE : GL_FALSE,
		duk_require_boolean(ctx, 3) ? GL_TRUE : GL_FALSE);
	return 0;
}
GLF(glDepthFunc) {
	glDepthFunc((GLenum)duk_require_int(ctx, 0));
	return 0;
}
GLF(glCullFace) {
	glCullFace((GLenum)duk_require_int(ctx, 0));
	return 0;
}
GLF(glFrontFace) {
	glFrontFace((GLenum)duk_require_int(ctx, 0));
	return 0;
}
GLF(glGetError) {
	duk_push_int(ctx, (int)glGetError());
	return 1;
}
GLF(glDrawBuffer) {
	glDrawBuffer((GLenum)duk_require_int(ctx, 0));
	return 0;
}
GLF(glGetIntegerv) {
	GLenum pname = (GLenum)duk_require_int(ctx, 0);
	switch (pname) {
		case GL_VIEWPORT: {
			// Single-value returns only in Duktape: pack [x,y,w,h] array.
			GLint v[4];
			glGetIntegerv(pname, v);
			duk_push_array(ctx);
			for (int i = 0; i < 4; ++i) {
				duk_push_int(ctx, v[i]);
				duk_put_prop_index(ctx, -2, (duk_idx_t)i);
			}
			return 1;
		}
		case GL_FRAMEBUFFER_BINDING: case GL_DRAW_BUFFER: case GL_MAX_TEXTURE_SIZE:
		case GL_MAX_TEXTURE_IMAGE_UNITS: {
			GLint v = 0;
			glGetIntegerv(pname, &v);
			duk_push_int(ctx, v);
			return 1;
		}
		default: {
			GLint v[4] = { 0, 0, 0, 0 };
			glGetIntegerv(pname, v);
			duk_push_int(ctx, v[0]);
			return 1;
		}
	}
}
#undef GLF

// ─────────────────────────── Render object ───────────────────────────
static duk_ret_t jsRenderEnable(duk_context* ctx) {
	GameRenderer* gr = MCGR();
	if (!gr) return 0;
	bool on = duk_require_boolean(ctx, 0) != 0;
	bool wasOn = gr->isOffscreenSceneEnabled();
	gr->setOffscreenSceneEnabled(on);
	if (on && !wasOn) {
		// 离屏从关→开(首次启用)默认请求一次 gbuffer 通道: 覆盖"先渲染后
		// 查询"的一帧滞后。真正按需由查询 API 每帧置位维持(见 renderLevel
		// 消费后 clearGbufferRequests);从不查询的模组只多渲启用首帧。
		gr->requestGbufferBrightness();
		gr->requestGbufferNormal();
	}
	return 0;
}
static duk_ret_t jsRenderEnabled(duk_context* ctx) {
	GameRenderer* gr = MCGR();
	duk_push_boolean(ctx, gr && gr->isOffscreenSceneEnabled());
	return 1;
}
static duk_ret_t jsRenderSceneTexture(duk_context* ctx) {
	GameRenderer* gr = MCGR();
	duk_push_int(ctx, gr ? (int)gr->sceneColorTexture() : 0);
	return 1;
}
static duk_ret_t jsRenderDepthTexture(duk_context* ctx) {
	GameRenderer* gr = MCGR();
	duk_push_int(ctx, gr ? (int)gr->sceneDepthTexture() : 0);
	return 1;
}
static duk_ret_t jsRenderBrightnessTexture(duk_context* ctx) {
	GameRenderer* gr = MCGR();
	if (gr) gr->requestGbufferBrightness();  // 查询=真会采样 color2 → 引擎按需渲染该通道
	duk_push_int(ctx, gr ? (int)gr->sceneBrightnessTexture() : 0);
	return 1;
}
static duk_ret_t jsRenderNormalTexture(duk_context* ctx) {
	GameRenderer* gr = MCGR();
	if (gr) gr->requestGbufferNormal();      // 查询=真会采样 color3 → 引擎按需渲染该通道
	duk_push_int(ctx, gr ? (int)gr->sceneNormalTexture() : 0);
	return 1;
}
static duk_ret_t jsRenderGbufferInfo(duk_context* ctx) {
	GameRenderer* gr = MCGR();
	if (gr) { gr->requestGbufferBrightness(); gr->requestGbufferNormal(); }
	duk_push_array(ctx);
	if (gr) {
		duk_push_int(ctx, (int)gr->sceneBrightnessTexture()); duk_put_prop_index(ctx, -2, 0);
		duk_push_int(ctx, (int)gr->sceneNormalTexture());     duk_put_prop_index(ctx, -2, 1);
		duk_push_int(ctx, gr->normalProgramState());          duk_put_prop_index(ctx, -2, 2);
	}
	return 1;
}
static void pushMat16(duk_context* ctx, const float* m) {
	duk_push_array(ctx);
	for (int i = 0; i < 16; ++i) {
		duk_push_number(ctx, (double)m[i]);
		duk_put_prop_index(ctx, -2, (duk_idx_t)i);
	}
}
static duk_ret_t jsRenderProjectionMatrix(duk_context* ctx) {
	GameRenderer* gr = MCGR();
	if (gr && gr->lastProjectionMatrix()) pushMat16(ctx, gr->lastProjectionMatrix());
	else duk_push_array(ctx);
	return 1;
}
static duk_ret_t jsRenderModelViewMatrix(duk_context* ctx) {
	GameRenderer* gr = MCGR();
	if (gr && gr->lastModelViewMatrix()) pushMat16(ctx, gr->lastModelViewMatrix());
	else duk_push_array(ctx);
	return 1;
}
static duk_ret_t jsRenderMaskTexture(duk_context* ctx) {
	GameRenderer* gr = MCGR();
	duk_push_int(ctx, gr ? (int)gr->maskColorTexture() : 0);
	return 1;
}
static duk_ret_t jsRenderReflectionTexture(duk_context* ctx) {
	GameRenderer* gr = MCGR();
	duk_push_int(ctx, gr ? (int)gr->reflectionColorTexture() : 0);
	return 1;
}
static duk_ret_t jsRenderSetSway(duk_context* ctx) {
	// Render.setSway(amp, speed): 植被摇摆全局(amp 0=关; 引擎默认关, 由光影模组开启)
	float amp = (float)duk_require_number(ctx, 0);
	float spd = duk_get_top(ctx) > 1 ? (float)duk_require_number(ctx, 1) : 1.0f;
	Chunk::setSwayGlobals(amp, spd);
	return 0;
}
static duk_ret_t jsRenderSetWaterLevel(duk_context* ctx) {
	GameRenderer* gr = MCGR();
	if (gr) gr->setReflectionWaterY((float)duk_require_number(ctx, 0));
	return 0;
}
static duk_ret_t jsRenderWaterLevel(duk_context* ctx) {
	GameRenderer* gr = MCGR();
	duk_push_number(ctx, gr ? gr->reflectionWaterY() : -999.0);
	return 1;
}
static duk_ret_t jsRenderSceneReady(duk_context* ctx) {
	GameRenderer* gr = MCGR();
	duk_push_boolean(ctx, gr && gr->isSceneTargetReady());
	return 1;
}
static duk_ret_t jsRenderGetCamera(duk_context* ctx) {
	// Returns [x, y, z, yaw, pitch] snapshot of the camera used for the
	// last scene render (mirror/water work will derive extra cameras from
	// this engine-side later). NOTE: a native function may only return
	// 0/1 values in Duktape — pack the snapshot into a single array.
	GameRenderer* gr = MCGR();
	double v[5] = { 0, 0, 0, 0, 0 };
	if (gr) {
		v[0] = gr->lastCameraX();
		v[1] = gr->lastCameraY();
		v[2] = gr->lastCameraZ();
		v[3] = gr->lastCameraYaw();
		v[4] = gr->lastCameraPitch();
	}
	duk_push_array(ctx);
	for (int i = 0; i < 5; ++i) {
		duk_push_number(ctx, v[i]);
		duk_put_prop_index(ctx, -2, (duk_idx_t)i);
	}
	return 1;
}
static duk_ret_t jsRenderWorld(duk_context* ctx) {
	// Render.world([x, y, z, yawDeg, pitchDeg, fovDeg, near?, far?, orthoHalf?])
	// Renders the world from an arbitrary camera into the currently bound
	// framebuffer (mod pre-binds its own FBO + viewport, with a depth
	// buffer attached). Opaque terrain only. Returns true on success.
	// orthoHalf > 0 switches to an orthographic ±orthoHalf camera (shadow
	// maps); otherwise a perspective camera with fovDeg is used.
	// The exact matrices used are exposed via Render.getWorldMatrices().
	GameRenderer* gr = MCGR();
	if (!gr) { duk_push_boolean(ctx, 0); return 1; }
	double c[9] = { 0, 0, 0, 0, 0, 70, 0.05, 0, 0 }; // far 0 => engine default; orthoHalf 0 => perspective
	if (duk_is_array(ctx, 0)) {
		duk_size_t len = duk_get_length(ctx, 0);
		if (len > 9) len = 9;
		for (duk_size_t i = 0; i < len; ++i) {
			duk_get_prop_index(ctx, 0, (duk_idx_t)i);
			c[i] = duk_get_number(ctx, -1);
			duk_pop(ctx);
		}
	}
	bool ok = gr->renderWorldScripted((float)c[0], (float)c[1], (float)c[2],
		(float)c[3], (float)c[4], (float)c[5], (float)c[6], (float)c[7], (float)c[8]);
	duk_push_boolean(ctx, ok);
	return 1;
}
static duk_ret_t jsRenderGetWorldMatrices(duk_context* ctx) {
	// Returns the matrices of the most recent Render.world() call:
	// { proj:[16], view:[16], x, y, z } — column-major. World positions map
	// into that render's clip space as proj*view*(world - center), because
	// the chunk renderer draws relative to the camera origin. Null when no
	// scripted render has happened yet.
	GameRenderer* gr = MCGR();
	if (!gr || !gr->lastWorldProjection() || !gr->lastWorldView()) {
		duk_push_null(ctx);
		return 1;
	}
	duk_push_object(ctx);
	duk_push_array(ctx);
	{
		const float* p = gr->lastWorldProjection();
		for (int i = 0; i < 16; ++i) {
			duk_push_number(ctx, (double)p[i]);
			duk_put_prop_index(ctx, -2, (duk_idx_t)i);
		}
	}
	duk_put_prop_string(ctx, -2, "proj");
	duk_push_array(ctx);
	{
		const float* v = gr->lastWorldView();
		for (int i = 0; i < 16; ++i) {
			duk_push_number(ctx, (double)v[i]);
			duk_put_prop_index(ctx, -2, (duk_idx_t)i);
		}
	}
	duk_put_prop_string(ctx, -2, "view");
	duk_push_number(ctx, (double)gr->lastWorldCenterX());
	duk_put_prop_string(ctx, -2, "x");
	duk_push_number(ctx, (double)gr->lastWorldCenterY());
	duk_put_prop_string(ctx, -2, "y");
	duk_push_number(ctx, (double)gr->lastWorldCenterZ());
	duk_put_prop_string(ctx, -2, "z");
	duk_push_int(ctx, gr->lastWorldChunks());
	duk_put_prop_string(ctx, -2, "chunks");
	return 1;
}

// ─────────────────────────── registration ───────────────────────────
static void putGlConst(duk_context* ctx, const char* name, long v) {
	duk_push_number(ctx, (double)v);
	duk_put_prop_string(ctx, -2, name);
}

void registerGlJsBindings(duk_context* ctx) {
	// --- GL object ---
	duk_push_object(ctx);
	#define GLFN(n) duk_push_c_function(ctx, js##n, DUK_VARARGS); duk_put_prop_string(ctx, -2, #n)
	GLFN(glCreateShader); GLFN(glShaderSource); GLFN(glCompileShader); GLFN(glGetShaderLog); GLFN(glDeleteShader);
	GLFN(glCreateProgram); GLFN(glAttachShader); GLFN(glLinkProgram); GLFN(glGetProgramLog); GLFN(glDeleteProgram);
	GLFN(glUseProgram); GLFN(glGetUniformLocation);
	GLFN(glUniform1i); GLFN(glUniform1f); GLFN(glUniform2f); GLFN(glUniform3f); GLFN(glUniform4f);
	GLFN(glUniformMatrix4fv);
	GLFN(glActiveTexture);
	GLFN(glGenTextures); GLFN(glBindTexture); GLFN(glDeleteTextures);
	GLFN(glTexParameteri); GLFN(glTexImage2D);
	GLFN(glGenBuffers); GLFN(glBindBuffer); GLFN(glBufferData); GLFN(glDeleteBuffers);
	GLFN(glEnableVertexAttribArray); GLFN(glDisableVertexAttribArray);
	GLFN(glVertexAttribPointer); GLFN(glDrawArrays);
	GLFN(glGenFramebuffers); GLFN(glBindFramebuffer); GLFN(glFramebufferTexture2D);
	GLFN(glGenRenderbuffers); GLFN(glBindRenderbuffer); GLFN(glRenderbufferStorage);
	GLFN(glFramebufferRenderbuffer); GLFN(glCheckFramebufferStatus);
	GLFN(glDeleteFramebuffers); GLFN(glDeleteRenderbuffers);
	GLFN(glClearColor); GLFN(glClear); GLFN(glViewport);
	GLFN(glEnable); GLFN(glDisable); GLFN(glBlendFunc); GLFN(glDepthMask);
	GLFN(glCullFace); GLFN(glFrontFace); GLFN(glGetError); GLFN(glColorMask); GLFN(glDepthFunc);
	GLFN(glDrawBuffer); GLFN(glGetIntegerv);
	#undef GLFN
	putGlConst(ctx, "VERTEX_SHADER", GL_VERTEX_SHADER);
	putGlConst(ctx, "FRAGMENT_SHADER", GL_FRAGMENT_SHADER);
	putGlConst(ctx, "TEXTURE_2D", GL_TEXTURE_2D);
	putGlConst(ctx, "TEXTURE0", GL_TEXTURE0);
	putGlConst(ctx, "TEXTURE1", GL_TEXTURE1);
	putGlConst(ctx, "TEXTURE2", GL_TEXTURE2);
	putGlConst(ctx, "TEXTURE3", GL_TEXTURE3);
	putGlConst(ctx, "NEAREST", GL_NEAREST);
	putGlConst(ctx, "LINEAR", GL_LINEAR);
	putGlConst(ctx, "CLAMP_TO_EDGE", GL_CLAMP_TO_EDGE);
	putGlConst(ctx, "REPEAT", GL_REPEAT);
	putGlConst(ctx, "RGBA8", GL_RGBA8);
	putGlConst(ctx, "RGBA16F", 0x881A);       // G3: HDR 中间帧 internal format
	putGlConst(ctx, "HALF_FLOAT", 0x140B);    // G3: half-float upload type
	putGlConst(ctx, "RGBA", GL_RGBA);
	putGlConst(ctx, "RGB", GL_RGB);
	putGlConst(ctx, "UNSIGNED_BYTE", GL_UNSIGNED_BYTE);
	putGlConst(ctx, "UNSIGNED_INT", GL_UNSIGNED_INT);
	putGlConst(ctx, "UNSIGNED_SHORT", GL_UNSIGNED_SHORT);
	putGlConst(ctx, "COLOR_BUFFER_BIT", GL_COLOR_BUFFER_BIT);
	putGlConst(ctx, "DEPTH_BUFFER_BIT", GL_DEPTH_BUFFER_BIT);
	putGlConst(ctx, "FLOAT", GL_FLOAT);
	putGlConst(ctx, "DEPTH_COMPONENT", GL_DEPTH_COMPONENT);
	putGlConst(ctx, "DEPTH_COMPONENT16", GL_DEPTH_COMPONENT16);
	putGlConst(ctx, "DEPTH_COMPONENT24", GL_DEPTH_COMPONENT24);
	putGlConst(ctx, "FRAMEBUFFER", GL_FRAMEBUFFER);
	putGlConst(ctx, "RENDERBUFFER", GL_RENDERBUFFER);
	putGlConst(ctx, "DRAW_BUFFER", GL_DRAW_BUFFER);
	putGlConst(ctx, "FRAMEBUFFER_BINDING", GL_FRAMEBUFFER_BINDING);
	putGlConst(ctx, "VIEWPORT", GL_VIEWPORT);
	putGlConst(ctx, "MAX_TEXTURE_SIZE", GL_MAX_TEXTURE_SIZE);
	putGlConst(ctx, "COLOR_ATTACHMENT0", GL_COLOR_ATTACHMENT0);
	putGlConst(ctx, "DEPTH_ATTACHMENT", GL_DEPTH_ATTACHMENT);
	putGlConst(ctx, "FRAMEBUFFER_COMPLETE", GL_FRAMEBUFFER_COMPLETE);
	putGlConst(ctx, "FRAMEBUFFER_BINDING", GL_FRAMEBUFFER_BINDING);
	putGlConst(ctx, "ARRAY_BUFFER", GL_ARRAY_BUFFER);
	putGlConst(ctx, "STATIC_DRAW", GL_STATIC_DRAW);
	putGlConst(ctx, "DYNAMIC_DRAW", GL_DYNAMIC_DRAW);
	putGlConst(ctx, "TRIANGLES", GL_TRIANGLES);
	putGlConst(ctx, "DEPTH_TEST", GL_DEPTH_TEST);
	putGlConst(ctx, "LESS", GL_LESS);
	putGlConst(ctx, "LEQUAL", GL_LEQUAL);
	putGlConst(ctx, "GEQUAL", GL_GEQUAL);
	putGlConst(ctx, "GREATER", GL_GREATER);
	putGlConst(ctx, "ALWAYS", GL_ALWAYS);
	putGlConst(ctx, "BLEND", GL_BLEND);
	putGlConst(ctx, "CULL_FACE", GL_CULL_FACE);
	putGlConst(ctx, "FRONT", GL_FRONT);
	putGlConst(ctx, "BACK", GL_BACK);
	putGlConst(ctx, "CCW", GL_CCW);
	putGlConst(ctx, "CW", GL_CW);
	putGlConst(ctx, "SRC_ALPHA", GL_SRC_ALPHA);
	putGlConst(ctx, "ONE_MINUS_SRC_ALPHA", GL_ONE_MINUS_SRC_ALPHA);
	putGlConst(ctx, "ONE", GL_ONE);
	putGlConst(ctx, "ZERO", GL_ZERO);
	putGlConst(ctx, "TEXTURE_MIN_FILTER", GL_TEXTURE_MIN_FILTER);
	putGlConst(ctx, "TEXTURE_MAG_FILTER", GL_TEXTURE_MAG_FILTER);
	putGlConst(ctx, "TEXTURE_WRAP_S", GL_TEXTURE_WRAP_S);
	putGlConst(ctx, "TEXTURE_WRAP_T", GL_TEXTURE_WRAP_T);
	putGlConst(ctx, "TRUE", 1);
	putGlConst(ctx, "FALSE", 0);
	duk_put_global_string(ctx, "GL");

	// --- Render object ---
	duk_push_object(ctx);
	duk_push_c_function(ctx, jsRenderEnable, 1);       duk_put_prop_string(ctx, -2, "enable");
	duk_push_c_function(ctx, jsRenderEnabled, 0);      duk_put_prop_string(ctx, -2, "enabled");
	duk_push_c_function(ctx, jsRenderSceneTexture, 0); duk_put_prop_string(ctx, -2, "sceneTexture");
	duk_push_c_function(ctx, jsRenderDepthTexture, 0); duk_put_prop_string(ctx, -2, "depthTexture");
	duk_push_c_function(ctx, jsRenderBrightnessTexture, 0); duk_put_prop_string(ctx, -2, "brightnessTexture");
	duk_push_c_function(ctx, jsRenderNormalTexture, 0); duk_put_prop_string(ctx, -2, "normalTexture");
	duk_push_c_function(ctx, jsRenderGbufferInfo, 0);   duk_put_prop_string(ctx, -2, "gbufferInfo");
	duk_push_c_function(ctx, jsRenderMaskTexture, 0);  duk_put_prop_string(ctx, -2, "waterMaskTexture");
	duk_push_c_function(ctx, jsRenderReflectionTexture, 0); duk_put_prop_string(ctx, -2, "reflectionTexture");
	duk_push_c_function(ctx, jsRenderSetSway, 2);       duk_put_prop_string(ctx, -2, "setSway");
	duk_push_c_function(ctx, jsRenderSetWaterLevel, 1); duk_put_prop_string(ctx, -2, "setWaterLevel");
	duk_push_c_function(ctx, jsRenderWaterLevel, 0);   duk_put_prop_string(ctx, -2, "waterLevel");
	duk_push_c_function(ctx, jsRenderSceneReady, 0);   duk_put_prop_string(ctx, -2, "sceneReady");
	duk_push_c_function(ctx, jsRenderGetCamera, 0);    duk_put_prop_string(ctx, -2, "getCamera");
	duk_push_c_function(ctx, jsRenderProjectionMatrix, 0); duk_put_prop_string(ctx, -2, "getProjectionMatrix");
	duk_push_c_function(ctx, jsRenderModelViewMatrix, 0);  duk_put_prop_string(ctx, -2, "getModelViewMatrix");
	duk_push_c_function(ctx, jsRenderWorld, 1);            duk_put_prop_string(ctx, -2, "world");
	duk_push_c_function(ctx, jsRenderGetWorldMatrices, 0); duk_put_prop_string(ctx, -2, "getWorldMatrices");
	duk_put_global_string(ctx, "Render");
}

#endif // desktop GL
