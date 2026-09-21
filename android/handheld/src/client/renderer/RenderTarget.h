#ifndef NET_MINECRAFT_CLIENT_RENDERER__RenderTarget_H__
#define NET_MINECRAFT_CLIENT_RENDERER__RenderTarget_H__

// Offscreen color+depth render target (FBO) for the mod render API.
//
// Desktop OpenGL (WIN32 GLEW / macOS / Linux): real FBO with an RGBA8 color
// texture (+ optional second color for the water-mask channel) and a D24
// DEPTH TEXTURE, so JS mods can sample the scene depth in their composite
// shaders. The window surface only has a 16-bit EGL depth buffer; offscreen
// targets always use D24.
//
// GLES / dedicated-server builds: no FBO support — the class degrades to
// no-ops so shared code still compiles (the render-mod feature simply stays
// off there).
#include "gles.h"

class RenderTarget
{
public:
	RenderTarget()
		: _fbo(0), _color(0), _color1(0), _color2(0), _color3(0), _depthTex(0), _ok(false), _oldFbo(0),
		  _hasSecond(false), _hasThird(false), _hasForth(false), _oldDraw(0),
		  width(0), height(0)
	{
		_oldViewport[0] = _oldViewport[1] = _oldViewport[2] = _oldViewport[3] = 0;
	}

	~RenderTarget() { destroy(); }

	// (Re)create the target at w×h. Frees any previous target first.
	// When second is true, a second color attachment (COLOR_ATTACHMENT1) is
	// added — used for the water-mask channel that shares the scene depth
	// (MRT): the same depth buffer then correctly occludes the mask.
	// When third is true, a third attachment (COLOR_ATTACHMENT2) is added —
	// the future gbuffer "brightness/AO" channel (M0: reserved, not yet
	// written; M1 will render it with a shader pass).
	bool init(int w, int h, bool second = false, bool third = false, bool forth = false)
	{
		destroy();
		if (w <= 0 || h <= 0)
			return false;
#if !defined(OPENGL_ES) && !defined(STANDALONE_SERVER)
		glGenFramebuffers(1, &_fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, _fbo);

		glGenTextures(1, &_color);
		glBindTexture(GL_TEXTURE_2D, _color);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _color, 0);

		if (second) {
			glGenTextures(1, &_color1);
			glBindTexture(GL_TEXTURE_2D, _color1);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, _color1, 0);
			_hasSecond = true;
		}
		if (third) {
			glGenTextures(1, &_color2);
			glBindTexture(GL_TEXTURE_2D, _color2);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, _color2, 0);
			_hasThird = true;
		}
		if (forth) {
			glGenTextures(1, &_color3);
			glBindTexture(GL_TEXTURE_2D, _color3);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, _color3, 0);
			_hasForth = true;
		}

		// Depth as a texture (samplable by the composite shaders).
		glGenTextures(1, &_depthTex);
		glBindTexture(GL_TEXTURE_2D, _depthTex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0,
			GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _depthTex, 0);

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, 0);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			destroy();
			return false;
		}
		width = w;
		height = h;
		_ok = true;
		return true;
#else
		return false;
#endif
	}

	void destroy()
	{
#if !defined(OPENGL_ES) && !defined(STANDALONE_SERVER)
		if (_color) { glDeleteTextures(1, &_color); _color = 0; }
		if (_color1) { glDeleteTextures(1, &_color1); _color1 = 0; }
		if (_color2) { glDeleteTextures(1, &_color2); _color2 = 0; }
		if (_color3) { glDeleteTextures(1, &_color3); _color3 = 0; }
		if (_depthTex) { glDeleteTextures(1, &_depthTex); _depthTex = 0; }
		if (_fbo)   { glDeleteFramebuffers(1, &_fbo);   _fbo = 0; }
#endif
		_hasSecond = false;
		_hasThird = false;
		_hasForth = false;
		_ok = false;
		width = height = 0;
	}

	bool ok() const { return _ok; }
	bool hasSecond() const { return _hasSecond; }
	bool hasThird() const { return _hasThird; }
	bool hasForth() const { return _hasForth; }

	unsigned int colorTexture(int index = 0) const {
		if (index == 0) return _color;
		if (index == 1) return _hasSecond ? _color1 : 0;
		if (index == 2) return _hasThird ? _color2 : 0;
		return _hasForth ? _color3 : 0;
	}

	// Samplable depth texture (window-space depth 0..1, GL depth convention:
	// 1.0 = far).
	unsigned int depthTexture() const { return _depthTex; }

	// Bind as the current GL framebuffer (saving FBO + viewport + draw buffer).
	void bind()
	{
#if !defined(OPENGL_ES) && !defined(STANDALONE_SERVER)
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &_oldFbo);
		glGetIntegerv(GL_VIEWPORT, _oldViewport);
		glGetIntegerv(GL_DRAW_BUFFER, &_oldDraw);
		glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
		glViewport(0, 0, width, height);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
#endif
	}

	// Restore the framebuffer + viewport + draw buffer active before bind().
	void unbind()
	{
#if !defined(OPENGL_ES) && !defined(STANDALONE_SERVER)
		glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)_oldFbo);
		glDrawBuffer((GLenum)_oldDraw);
		glViewport(_oldViewport[0], _oldViewport[1], _oldViewport[2], _oldViewport[3]);
#endif
	}

	// MRT helpers (only meaningful when hasSecond()).
	// 桌面 FBO/MRT 专用：GLES 1.1 没有 MRT，也没有 GL_COLOR_ATTACHMENT* 常量，
	// 所以这些 helper 在 GLES 平台只留空实现（对应路径在 Android 下整段不编译）。
#if !defined(OPENGL_ES) && !defined(STANDALONE_SERVER)
	void drawToFirst() { if (_hasSecond) glDrawBuffer(GL_COLOR_ATTACHMENT0); }
	void drawToSecond() { if (_hasSecond) glDrawBuffer(GL_COLOR_ATTACHMENT1); }
	void drawToBoth() { if (_hasSecond) glDrawBuffer(GL_COLOR_ATTACHMENT0 | GL_COLOR_ATTACHMENT1); }
	// gbuffer color2 (brightness/AO) — only meaningful when hasThird().
	void drawToThird() { if (_hasThird) glDrawBuffer(GL_COLOR_ATTACHMENT2); }
	// gbuffer color3 (normals) — only meaningful when hasForth().
	void drawToFourth() { if (_hasForth) glDrawBuffer(GL_COLOR_ATTACHMENT3); }
#else
	void drawToFirst() {}
	void drawToSecond() {}
	void drawToBoth() {}
	void drawToThird() {}
	void drawToFourth() {}
#endif

	int width;
	int height;

private:
	unsigned int _fbo;
	unsigned int _color;
	unsigned int _color1;
	unsigned int _color2;
	unsigned int _color3;
	unsigned int _depthTex;
	bool _ok;
	bool _hasSecond;
	bool _hasThird;
	bool _hasForth;
	GLint _oldFbo;
	GLint _oldDraw;
	GLint _oldViewport[4];
};

#endif /*NET_MINECRAFT_CLIENT_RENDERER__RenderTarget_H__*/
