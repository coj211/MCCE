#ifndef NET_MINECRAFT_CLIENT_GUI__Font_H__
#define NET_MINECRAFT_CLIENT_GUI__Font_H__

//package net.minecraft.client.gui;

#include <string>
#include <cctype>

#include "../renderer/gles.h"
#if defined(_WIN32) || defined(ANDROID)
#include <map>
#endif

class Textures;
class Options;

class Font
{
public:
    Font(Options* options, const std::string& name, Textures* textures);
	~Font();
	//Font(Options* options, const std::string& name, Textures* textures, int imgW, int imgH, int x, int y, int cols, int rows, unsigned char charOffset);
	
	void init(Options* options);
	void onGraphicsReset();

	void draw(const char* str, float x, float y, int color);
    void draw(const std::string& str, float x, float y, int color);
	void draw(const char* str, float x, float y, int color, bool darken);
    void draw(const std::string& str, float x, float y, int color, bool darken);
	void drawShadow(const std::string& str, float x, float y, int color);
	void drawShadow(const char* str, float x, float y, int color);
	void drawWordWrap(const std::string& str, float x, float y, float w, int col);
	// 0.8.1 GUI 移植补口：
	void drawWordWrap(const std::string& str, float x, float y, float w, int col, bool shadow, bool center);
	int height(const std::string& str, int wrapWidth);
	float getPixelLength(const std::string& str);
	void drawTransformed(const std::string& str, float x, float y, int color, float scaleX, float scaleY, bool darken, float unk);

	int width(const std::string& str);
	int height(const std::string& str);

	static std::string sanitize(const std::string& str);
private:
	void buildChar(unsigned char i, float x = 0, float y = 0);
	void drawSlow(const std::string& str, float x, float y, int color, bool darken = false);
	void drawSlow(const char* str, float x, float y, int color, bool darken = false);
#if defined(_WIN32) || defined(ANDROID)
	// CJK（中文）字形图集。
	//   Windows：initCjkFont() 用 GDI 现渲染（见 Font.cpp）。
	//   Android：没有 GDI，改成读 assets/fonts/cjk_atlas.bmp —— 由
	//   gen_cjk_atlas.py 走同一套 GDI 调用离线渲染，字形与 Windows 版一致。
	void initCjkFont();
	void buildCjkChar(int glyphIndex, float x, float y);
	// Immediate-mode variant used when the Tesselator is in override mode
	// (Touch UI): Tesselator::draw() is a no-op there, so we cannot switch
	// textures between ASCII and CJK glyphs - draw CJK directly instead.
	void drawCjkCharImmediate(int glyphIndex, float px, float py, int r, int g, int b, int a);
#endif
#ifdef _WIN32
	// Custom font support: if data/fonts/custom.ttf exists next to the exe,
	// the latin glyphs are GDI-rendered into a 128x128 atlas at runtime and
	// the CJK atlas uses that font instead of the hard-coded default.
	unsigned char* renderCustomLatinFont();
	static std::string findCustomFontPath();
	static std::string ttfFamilyName(const std::string& path);
#endif
public:
	int fontTexture;
	int lineHeight;
	static const int DefaultLineHeight = 10;
private:
	int charWidths[256];
	float fcharWidths[256];
	int listPos;

	int index;
	int count;
	GLuint lists[1024];

	std::string fontName;
	Textures* _textures;

	Options* options;

	int _x, _y;
	int _cols;
	int _rows;
	unsigned char _charOffset;
#if defined(_WIN32) || defined(ANDROID)
	bool _cjkReady;
	int _cjkTexture;
	int _cjkTexW, _cjkTexH;
	int _cjkCols, _cjkRows;
	int _cjkGlyph;
	int _cjkDraw;          // on-screen glyph size (px)
	std::map<unsigned int, int> _cjkIndex; // unicode -> atlas cell index
#endif
#ifdef _WIN32
	int _latinTexture;     // custom-rendered latin atlas (0 = use default8.png)
	// Per-glyph bounding box inside the 256x256 custom atlas (16px cells),
	// measured at init.  _glyphL = left padding, _glyphW = glyph width.
	// Used to sample only the glyph (not the empty cell padding) so narrow
	// letters like 'i' don't get huge gaps around them.
	unsigned char _glyphL[256];
	unsigned char _glyphW[256];
#endif
};

#endif /*NET_MINECRAFT_CLIENT_GUI__Font_H__*/
