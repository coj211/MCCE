#include "Font.h"

//#include "SharedConstants.h"
#include "../Options.h"
#include "../renderer/Textures.h"
#include "../renderer/Tesselator.h"
#include "../../util/Mth.h"
#include "../../AppPlatform.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif
#if defined(_WIN32) || defined(ANDROID)
#include <map>
#include "cjk_gb2312.h"
#endif

Font::Font( Options* options, const std::string& name, Textures* textures )
:	options(options),
	fontTexture(0),
	fontName(name),
	index(0),
	count(0),
	_textures(textures),
	_x(0), _y(0),
	_cols(16), _rows(16),
	_charOffset(0),
	lineHeight(DefaultLineHeight)
#if defined(_WIN32) || defined(ANDROID)
	, _cjkReady(false), _cjkTexture(0),
	_cjkTexW(0), _cjkTexH(0), _cjkCols(0), _cjkRows(0),
	_cjkGlyph(0), _cjkDraw(0)
#endif
#ifdef _WIN32
	, _latinTexture(0)
#endif
{
	init(options);
}

Font::~Font()
{
#ifdef _WIN32
	if (_latinTexture) {
		glDeleteTextures(1, (GLuint*)&_latinTexture);
		_latinTexture = 0;
	}
	std::string p = findCustomFontPath();
	if (!p.empty()) {
		int plen = MultiByteToWideChar(CP_UTF8, 0, p.c_str(), -1, NULL, 0);
		if (plen > 0) {
			std::vector<wchar_t> pw(plen);
			MultiByteToWideChar(CP_UTF8, 0, p.c_str(), -1, &pw[0], plen);
			RemoveFontResourceExW(&pw[0], FR_PRIVATE, 0);
		}
	}
#endif
}


//Font::Font( Options* options, const std::string& name, Textures* textures, int imgW, int imgH, int x, int y, int cols, int rows, unsigned char charOffset )
//:	options(options),
//	fontTexture(0),
//	fontName(name),
//	index(0),
//	count(0),
//	_textures(textures),
//	_x(x), _y(y),
//	_cols(cols), _rows(rows),
//	_charOffset(charOffset)
//{
//	init(options);
//}

void Font::onGraphicsReset()
{
	init(options);
}

void Font::init( Options* options )
{
	TextureId fontTexture = _textures->loadTexture(fontName);
	const TextureData* tex = _textures->getTemporaryTextureData(fontTexture);

#if defined(_WIN32) || defined(ANDROID)
	// Custom font (data/fonts/custom.ttf): 只用于 CJK 图集 —— initCjkFont()
	// 内部会检测 custom.ttf 并用来渲染中文。英文/数字保持原版 default8.png
	// 小字(原版英文观感已足够, 自定义拉丁图集易出现间距/字形问题)。
	// 因此这里不再生成自定义拉丁图集, _latinTexture 恒为 0。
	//  Windows: GDI 现渲染中文图集；Android: 读 assets/fonts/cjk_atlas.bmp。
	initCjkFont();
#endif
#ifdef _WIN32
	unsigned char* customBits = NULL;
#endif

	const unsigned char* rawPixels = NULL;
	int texW = 128;
#ifdef _WIN32
	if (customBits) {
		rawPixels = customBits;
		texW = 256;
	}
#endif
	if (!rawPixels && tex) {
		rawPixels = tex->data;
		texW = tex->w;
	}
	if (!rawPixels) {
#ifdef _WIN32
		if (customBits) free(customBits);
#endif
		return;
	}

	const int numChars = _rows * _cols;
	// Custom font atlas is supersampled 256x256 with 16px cells (glyphs drawn
	// at 8px on screen), matching buildChar. Measure advances on the same
	// grid; divide by 2 to convert back to on-screen 8px-space.
#ifdef _WIN32
	const int measCell = _latinTexture ? 16 : 8;
	const int measAtlas = _latinTexture ? 256 : 128;
#else
	const int measCell = 8;
	const int measAtlas = 128;
#endif
	for (int i = 0; i < numChars; i++) {
		int xt = i % _cols;
		int yt = i / _cols;

#ifdef _WIN32
		unsigned char gl = 0, gw = 0;
		if (measCell > 8) {
			// Custom supersampled atlas: glyph is centered in the 16px cell.
			// Measure its left padding + width so sampling can be tight
			// (otherwise narrow letters like 'i' get huge gaps).
			int leftEmpty = 0;
			bool any = false;
			while (leftEmpty < measCell) {
				int xPixel = _x + xt * measCell + leftEmpty;
				bool colEmpty = true;
				for (int y = 0; y < measCell && colEmpty; y++) {
					int yPixel = (_y + yt * measCell + y) * measAtlas;
					if (rawPixels[(xPixel + yPixel) << 2] > 0) colEmpty = false;
				}
				if (!colEmpty) { any = true; break; }
				leftEmpty++;
			}
			int rightEmpty = 0;
			while (rightEmpty < measCell) {
				int xPixel = _x + xt * measCell + (measCell - 1 - rightEmpty);
				bool colEmpty = true;
				for (int y = 0; y < measCell && colEmpty; y++) {
					int yPixel = (_y + yt * measCell + y) * measAtlas;
					if (rawPixels[(xPixel + yPixel) << 2] > 0) colEmpty = false;
				}
				if (!colEmpty) { any = true; break; }
				rightEmpty++;
			}
			if (any) {
				gl = (unsigned char)leftEmpty;
				gw = (unsigned char)(measCell - leftEmpty - rightEmpty);
			}
			_glyphL[i] = gl;
			_glyphW[i] = gw;
		}
#endif
		int x;
		// 非 Win32（含 Android）：measCell 恒为 8，上面 if(measCell==8) 分支
		// 已经 continue，这里定义 gw 只是为了让下面的宽度计算能通过编译。
#ifndef _WIN32
		unsigned char gl = 0, gw = 0;
		(void)gl;
#endif
		if (measCell == 8) {
			// Classic 8px font: original advance logic.
			int rx = 7;
			for (; rx >= 0; rx--) {
				int xPixel = _x + xt * 8 + rx;
				bool emptyColumn = true;
				for (int y = 0; y < 8 && emptyColumn; y++) {
					int yPixel = _y + (yt * 8 + y) * measAtlas;
					if (rawPixels[(xPixel + yPixel) << 2] > 0) emptyColumn = false;
				}
				if (!emptyColumn) break;
			}
			if (i == ' ') rx = 4 - 2;
			// 字形在屏幕上是从 8px 格画成 9px 的（见 buildChar），推进宽度必须
			// 同步放大，否则字母会挤在一起。
			x = (int)((rx + 2) * 9.0f / 8.0f + 0.5f);
			charWidths[i] = x;
			fcharWidths[i] = (float)x;
			continue;
		}
		if (i == ' ') {
			x = 4;  // 空格固定 4px 屏幕宽
		} else if (gw == 0) {
			x = 4;  // 空字形/控制字符默认
		} else {
			x = (int)gw / 2 + 1;  // 超采样字形宽 ÷2 = 屏幕宽 + 1px 字距
			if (x < 2) x = 2;
		}
		charWidths[i] = x;
		fcharWidths[i] = (float)charWidths[i];
	}

#ifdef _WIN32
	if (customBits) free(customBits);
#endif

#ifdef USE_VBO
	return; // this <1
#endif

#ifndef USE_VBO
	listPos = glGenLists(256 + 32);

	Tesselator& t = Tesselator::instance;
	for (int i = 0; i < 256; i++) {
		glNewList(listPos + i, GL_COMPILE);
		// @attn @huge @note: This is some dangerous code right here / Aron, added ^1
		t.begin();
		buildChar(i);
		t.end(false, -1);

		glTranslatef2((GLfloat)charWidths[i], 0.0f, 0.0f);
		glEndList();
	}

	for (int i = 0; i < 32; i++) {
		int br = ((i >> 3) & 1) * 0x55;
		int r = ((i >> 2) & 1) * 0xaa + br;
		int g = ((i >> 1) & 1) * 0xaa + br;
		int b = ((i >> 0) & 1) * 0xaa + br;
		if (i == 6) {
			r += 0x55;
		}
		bool darken = i >= 16;

		if (options->anaglyph3d) {
			int cr = (r * 30 + g * 59 + b * 11) / 100;
			int cg = (r * 30 + g * 70) / (100);
			int cb = (r * 30 + b * 70) / (100);

			r = cr;
			g = cg;
			b = cb;
		}

		// color = r << 16 | g << 8 | b;
		if (darken) {
			r /= 4;
			g /= 4;
			b /= 4;
		}

		glNewList(listPos + 256 + i, GL_COMPILE);
		glColor3f(r / 255.0f, g / 255.0f, b / 255.0f);
		glEndList();
	}
#endif
}

void Font::drawShadow( const std::string& str, float x, float y, int color )
{
	draw(str, x + 1, y + 1, color, true);
	draw(str, x, y, color);
}
void Font::drawShadow( const char* str, float x, float y, int color )
{
	draw(str, x + 1, y + 1, color, true);
	draw(str, x, y, color);
}

void Font::draw( const std::string& str, float x, float y, int color )
{
	draw(str, x, y, color, false);
}

void Font::draw( const char* str, float x, float y, int color )
{
	draw(str, x, y, color, false);
}

void Font::draw( const char* str, float x, float y, int color, bool darken )
{
#ifdef USE_VBO
	drawSlow(str, x, y, color, darken);
#endif
}

void Font::draw( const std::string& str, float x, float y, int color, bool darken )
{
#ifdef USE_VBO
	drawSlow(str, x, y, color, darken);
	return;
#endif

	if (str.empty()) return;

	if (darken) {
		int oldAlpha = color & 0xff000000;
		color = (color & 0xfcfcfc) >> 2;
		color += oldAlpha;
	}

	// Custom font: the display-list glyph UVs were built for the 256x256
	// GDI atlas (_latinTexture).  Bind it instead of the classic fontName.
#ifdef _WIN32
	if (_latinTexture)
		glBindTexture(GL_TEXTURE_2D, (GLuint)_latinTexture);
	else
#endif
	_textures->loadAndBindTexture(fontName);
	float r = ((color >> 16) & 0xff) / 255.0f;
	float g = ((color >> 8) & 0xff) / 255.0f;
	float b = ((color) & 0xff) / 255.0f;
	float a = ((color >> 24) & 0xff) / 255.0f;
	if (a == 0) a = 1;
	glColor4f2(r, g, b, a);

	static const std::string hex("0123456789abcdef");

	index = 0;
	glPushMatrix2();
	glTranslatef2((GLfloat)x, (GLfloat)y, 0.0f);
	for (unsigned int i = 0; i < str.length(); i++) {
		while (str.length() > i + 1 && str[i] == '\xA7') {
			int cc = hex.find((char)tolower(str[i + 1]));
			if (cc < 0 || cc > 15) cc = 15;
			lists[index++] = listPos + 256 + cc + (darken ? 16 : 0);

			if (index == 1024) {
				count = index;
				index = 0;
#ifndef USE_VBO
				glCallLists(count, GL_UNSIGNED_INT, lists);
#endif
				count = 1024;
			}

			i += 2;
		}

		if (i < str.length()) {
			//int ch = SharedConstants.acceptableLetters.indexOf(str.charAt(i));
			char ch = str[i];
			if (ch >= 0) {
				//ib.put(listPos + ch + 32);
				lists[index++] = listPos + ch;
			}
		}

		if (index == 1024) {
			count = index;
			index = 0;
#ifndef USE_VBO
			glCallLists(count, GL_UNSIGNED_INT, lists);
#endif
			count = 1024;
		}
	}
	count = index;
	index = 0;
#ifndef USE_VBO
	glCallLists(count, GL_UNSIGNED_INT, lists);
#endif
	glPopMatrix2();
}

int Font::width( const std::string& str )
{
	int maxLen = 0;
	int len = 0;

	for (unsigned int i = 0; i < str.length(); i++) {
		if ((unsigned char)str[i] == '\xA7') {
			i++;
		} else {
			if (str[i] == '\n') {
				if (len > maxLen) maxLen = len;
				len = 0;
			}
#if defined(_WIN32) || defined(ANDROID)
			else if ((unsigned char)str[i] >= 0x80) {
				// UTF-8 multi-byte -> CJK glyph width
				unsigned char ch = (unsigned char)str[i];
				int consumed = 1;
				if ((ch & 0xE0) == 0xC0 && i + 1 < str.length()) consumed = 2;
				else if ((ch & 0xF0) == 0xE0 && i + 2 < str.length()) consumed = 3;
				else if ((ch & 0xF8) == 0xF0 && i + 3 < str.length()) consumed = 4;
				len += _cjkDraw;
				i += consumed - 1;
			}
#endif
			else {
				int charWidth = charWidths[ (unsigned char) str[i] ];
				len += charWidth;
			}
		}
	}
	return maxLen>len? maxLen : len;
}

int Font::height( const std::string& str ) {
	int h = 0;
	bool hasLine = false;
	for (unsigned int i = 0; i < str.length(); ++i) {
		if (str[i] == '\n') hasLine = true;
		else {
			if (hasLine) h += lineHeight;
			hasLine = false;
		}
	}
	return h;
}

int Font::height( const std::string& str, int wrapWidth ) {
	// 0.8.1 GUI 移植：按 drawWordWrap 的断行规则统计总高度（每行计一次行高）。
	if (wrapWidth <= 0) wrapWidth = 1;
	int h = 0;
	int cur = 0;
	for (unsigned int i = 0; i < str.length(); ++i) {
		char c = str[i];
		if (c == '\n') { h += lineHeight; cur = 0; continue; }
		if (c == ' ' && cur == 0) continue;
		int w = charWidths[(unsigned char)c];
		if (cur + w > wrapWidth && cur > 0) { h += lineHeight; cur = 0; }
		cur += w;
	}
	if (cur > 0 || str.length() == 0) h += lineHeight;
	return h;
}

float Font::getPixelLength( const std::string& str ) {
	// 0.8.1 GUI 移植：逐字符宽度求和。
	float len = 0;
	for (unsigned int i = 0; i < str.length(); ++i)
		len += (float)charWidths[(unsigned char)str[i]];
	return len;
}

void Font::drawTransformed( const std::string& str, float x, float y, int color, float scaleX, float scaleY, bool darken, float unk ) {
	// 0.8.1 GUI 移植：带缩放/旋转的绘制（splash 动画用）。
	// 0.8.1 签名 (str, x, y, col, rot, scale, center, maxWidth)
	float rot = scaleX;        // 旋转角度
	float scale = scaleY;      // 缩放
	bool center = darken;      // 是否水平居中
	float maxWidth = unk;      // 最大宽度
	float pixelLength = (center || maxWidth < 3.4028e38f) ? getPixelLength(str) : 0;
	if (pixelLength > maxWidth)
		maxWidth = scale * (maxWidth / pixelLength);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glTranslatef(x, y, 0.0f);
	glScalef(scale, scale, scale);
	glRotatef(rot, 0.0f, 0.0f, 1.0f);
	if (center)
		glTranslatef(-(pixelLength * 0.5f), 0.0f, 0.0f);
	draw(str, 1.0f, 1.0f, color, true);  // 阴影
	draw(str, 0.0f, 0.0f, color);
	glPopMatrix();
}

std::string Font::sanitize( const std::string& str )
{
	std::string sanitized(str.length() + 1, 0);
	int j = 0;

	for (unsigned int i = 0; i < str.length(); i++) {
		if (str[i] == '\xA7') {
			i++;
			//} else if (SharedConstants.acceptableLetters.indexOf(str.charAt(i)) >= 0) {
		} else {
			sanitized[j++] = str[i];
		}
	}
	return sanitized.erase(j);
}

void Font::drawWordWrap( const std::string& str, float x, float y, float w, int col )
{
	char* cstr = new char[str.length() + 1];
	strncpy(cstr, str.c_str(), str.length());
	cstr[str.length()] = 0;

	const char* lims = " \n\t\r";
	char* ptok = strtok(cstr, lims);

	std::vector<std::string> words;
	while (ptok != NULL) {
		words.push_back( ptok );
		ptok = strtok(NULL, lims);
	}

	delete[] cstr;

	int pos = 0;
	while (pos < (int)words.size()) {
		std::string line = words[pos++] + " ";
		while (pos < (int)words.size() && width(line + words[pos]) < w) {
			line += words[pos++] + " ";
		}
		drawShadow(line, x, y, col);
		y += lineHeight;
	}
}

void Font::drawWordWrap( const std::string& str, float x, float y, float w, int col, bool shadow, bool center )
{
	// 0.8.1 GUI 移植：7 参版（shadow / center 选项），复用 5 参的断行逻辑。
	char* cstr = new char[str.length() + 1];
	strncpy(cstr, str.c_str(), str.length());
	cstr[str.length()] = 0;

	const char* lims = " \n\t\r";
	char* ptok = strtok(cstr, lims);

	std::vector<std::string> words;
	while (ptok != NULL) {
		words.push_back( ptok );
		ptok = strtok(NULL, lims);
	}
	delete[] cstr;

	int pos = 0;
	while (pos < (int)words.size()) {
		std::string line = words[pos++] + " ";
		while (pos < (int)words.size() && width(line + words[pos]) < w) {
			line += words[pos++] + " ";
		}
		float drawX = x;
		if (center) drawX += width(line) * 0.5f;
		if (shadow) drawShadow(line, drawX, y, col);
		else        draw(line, drawX, y, col);
		y += lineHeight;
	}
}

void Font::drawSlow( const std::string& str, float x, float y, int color, bool darken /*= false*/ ) {
	drawSlow(str.c_str(), x, y, color, darken);
}
void Font::drawSlow( const char* str, float x, float y, int color, bool darken /*= false*/ )
{
	if (!str) return;

	if (darken) {
		int oldAlpha = color & 0xff000000;
		color = (color & 0xfcfcfc) >> 2;
		color += oldAlpha;
	}

	// Bind the latin atlas: the custom GDI-rendered one when available,
	// otherwise the classic default8.png asset.
#ifdef _WIN32
	if (_latinTexture)
		glBindTexture(GL_TEXTURE_2D, (GLuint)_latinTexture);
	else
#endif
		_textures->loadAndBindTexture(fontName);

	// Font glyphs use antialiased alpha — draw with blending instead of
	// ALPHA_TEST so supersampled edges stay smooth.
	glDisable2(GL_ALPHA_TEST);
	glEnable2(GL_BLEND);
	glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	Tesselator& t = Tesselator::instance;
	t.begin();
	int alpha = (0xff000000 & color) >> 24;
	if (!alpha) alpha = 0xff;
	t.color((color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff, alpha);

#if defined(_WIN32) || defined(ANDROID)
	const bool overridden = t.isOverridden();
	// 当前文字颜色。原来是 const —— 现在要能被 §颜色码 改，所以是可变的。
	int cr = (color >> 16) & 0xff;
	int cg = (color >> 8) & 0xff;
	int cb = color & 0xff;
	// §r 回到调用方给的颜色；§0-§f 用与原版一致的 16 色。
	const int baseCr = cr, baseCg = cg, baseCb = cb;
	static const int kCodeColor[16] = {
		0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xFFAA00, 0xAAAAAA,
		0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
	};
#endif
	
	t.addOffset((float)x, (float)y, 0);
	float xOffset = 0;
	float yOffset = 0;

#if defined(_WIN32) || defined(ANDROID)
	bool cjkBound = false;
	// When the texture changes (ASCII <-> CJK) we must submit the current
	// Tesselator batch and start a new one: a single batch is drawn with ONE
	// texture, so mixing glyphs of both fonts would render with the wrong
	// texture (e.g. CJK glyphs sampled from the 8x8 font -> gray blobs).
#define FLUSH_BATCH() \
	do { \
		t.draw(); \
		t.addOffset(-(float)x, -(float)y, 0); \
		t.begin(); \
		t.color(cr, cg, cb, alpha); \
		t.addOffset((float)x, (float)y, 0); \
	} while (0)

	// 应用一个颜色/样式码：§r 回到调用方颜色，§0-§f 切 16 色，
	// 其余（§l/§o/§n/§m/§k）本引擎不支持、直接忽略。
	// 顶点颜色是随批次走的，所以颜色真的变了要先把当前批次提交掉（FLUSH_BATCH），
	// 再用新颜色开新批次 —— 否则整段文字都会用旧颜色。
	static const std::string hex("0123456789abcdef");
#define APPLY_FORMAT_CODE(fc) \
	do { \
		const char _f = (char)tolower((fc)); \
		int _nr = cr, _ng = cg, _nb = cb; \
		if (_f == 'r') { _nr = baseCr; _ng = baseCg; _nb = baseCb; } \
		else { \
			const int _ci = (int)hex.find(_f); \
			if (_ci >= 0 && _ci <= 15) { \
				_nr = (kCodeColor[_ci] >> 16) & 0xff; \
				_ng = (kCodeColor[_ci] >> 8) & 0xff; \
				_nb = kCodeColor[_ci] & 0xff; \
				/* 颜色码取的是原色，但 drawShadow 会把同一段文字再画一遍阴影， */ \
				/* 那一遍的 darken=true —— 这里必须同样压暗，否则阴影也画成亮色， */ \
				/* 两层错开 1 像素叠在一起就是“重影”。 */ \
				if (darken) { \
					_nr = (_nr & 0xfc) >> 2; \
					_ng = (_ng & 0xfc) >> 2; \
					_nb = (_nb & 0xfc) >> 2; \
				} \
			} \
		} \
		if (_nr != cr || _ng != cg || _nb != cb) { \
			cr = _nr; cg = _ng; cb = _nb; \
			FLUSH_BATCH(); \
		} \
	} while (0)

	while (unsigned char ch = *(str++)) {
		if (ch == '\n') {
			xOffset = 0;
			yOffset += lineHeight;
			continue;
		}
		// 颜色/样式码：跳过它和后面那个格式字符，别让它们被当成要画的字形。
		// 引擎自带的文案是单字节 0xA7，而模组脚本是 UTF-8 -> '§' 是两个字节 C2 A7。
		// 不处理的话 C2 会被当多字节字符落到“未光栅化”分支、画成 atlas 第 0 格
		// （一条横杠），后面那个格式字符（b/7/r…）还会原样显示出来。
		if (ch == 0xC2 && (unsigned char)*str == 0xA7) {
			str++;                        // 吃掉 0xA7
			const char fc = *str;         // 格式字符（颜色/样式）
			if (*str) str++;
			APPLY_FORMAT_CODE(fc);
			continue;
		}
		if (ch == 0xA7) {
			const char fc = *str;
			if (*str) str++;
			APPLY_FORMAT_CODE(fc);
			continue;
		}
		if (ch >= 0x80) {
			// UTF-8 multi-byte sequence -> CJK glyph
			unsigned int u;
			int consumed;
			if ((ch & 0xE0) == 0xC0 && str[0]) {
				u = ((ch & 0x1F) << 6) | ((unsigned char)str[0] & 0x3F); consumed = 2;
			} else if ((ch & 0xF0) == 0xE0 && str[0] && str[1]) {
				u = ((ch & 0x0F) << 12) | (((unsigned char)str[0] & 0x3F) << 6) | ((unsigned char)str[1] & 0x3F); consumed = 3;
			} else if ((ch & 0xF8) == 0xF0 && str[0] && str[1] && str[2]) {
				u = ((ch & 0x07) << 18) | (((unsigned char)str[0] & 0x3F) << 12) | (((unsigned char)str[1] & 0x3F) << 6) | ((unsigned char)str[2] & 0x3F); consumed = 4;
			} else {
				u = ch; consumed = 1;
			}
			if (!_cjkReady) { str += consumed - 1; continue; }
			std::map<unsigned int, int>::const_iterator it = _cjkIndex.find(u);
			if (it != _cjkIndex.end()) {
				if (overridden) {
					drawCjkCharImmediate(it->second, x + xOffset, y + yOffset, cr, cg, cb, alpha);
				} else
				{
					if (!cjkBound) {
						FLUSH_BATCH();
						glBindTexture(GL_TEXTURE_2D, (GLuint)_cjkTexture);
						cjkBound = true;
					}
					buildCjkChar(it->second, xOffset, yOffset);
				}
				xOffset += (float)_cjkDraw;
			} else {
				// Unrasterized character: draw a placeholder box.
				if (overridden) {
					drawCjkCharImmediate(0, x + xOffset, y + yOffset, cr, cg, cb, alpha);
				} else
				{
					if (!cjkBound) {
						FLUSH_BATCH();
						glBindTexture(GL_TEXTURE_2D, (GLuint)_cjkTexture);
						cjkBound = true;
					}
					buildCjkChar(0, xOffset, yOffset);
				}
				xOffset += (float)_cjkDraw;
			}
			str += consumed - 1;
			continue;
		}
		if (cjkBound) {
			FLUSH_BATCH();
#ifdef _WIN32
			if (_latinTexture)
				glBindTexture(GL_TEXTURE_2D, (GLuint)_latinTexture);
			else
#endif
				_textures->loadAndBindTexture(fontName);
			cjkBound = false;
		}
		buildChar(ch, xOffset, yOffset);
		xOffset += fcharWidths[ch];
	}
#undef FLUSH_BATCH
#else
	while (unsigned char ch = *(str++)) {
		if (ch == '\n') {
			xOffset = 0;
			yOffset += lineHeight;
		} else {
			buildChar(ch, xOffset, yOffset);
			xOffset += fcharWidths[ch];
		}
	}
#endif
	t.draw();
	t.addOffset(-(float)x, -(float)y, 0);

	// Restore the GUI's default fragment state.
	glEnable2(GL_ALPHA_TEST);
	glDisable2(GL_BLEND);
}

void Font::buildChar( unsigned char i, float x /*= 0*/, float y /*=0*/ )
{
	Tesselator& t = Tesselator::instance;

#ifdef _WIN32
	// Custom atlas is supersampled 256x256 with 16px cells (drawn at 8px).
	if (_latinTexture) {
		const int atlas = 256;
		// Sample only the glyph box (not the empty padding) so narrow
		// letters keep tight spacing; on-screen width = glyphW / 2.
		unsigned char gl = _glyphL[i];
		unsigned char gw = _glyphW[i];
		if (gw == 0) { gw = 8; gl = 4; }   // empty glyph fallback
		float sw = (float)gw * 0.5f;        // screen quad width
		float sh = 7.99f;                   // screen quad height (fixed 8px)
		float ix = (float)((i & 15) * 16 + gl);
		float iy = (float)((i >> 4) * 16);
		float u0 = ix / (float)atlas;
		float v0 = iy / (float)atlas;
		float u1 = (ix + gw) / (float)atlas;
		float v1 = (iy + 15.99f) / (float)atlas;
		t.vertexUV(x, y + sh, 0, u0, v1);
		t.vertexUV(x + sw, y + sh, 0, u1, v1);
		t.vertexUV(x + sw, y, 0, u1, v0);
		t.vertexUV(x, y, 0, u0, v0);
		return;
	}
	const int cell = 8;
	const int atlas = 128;
	float ix = (float)((i & 15) * cell);
	float iy = (float)((i >> 4) * cell);
#else
	//i -= _charOffset;
	//int ix = (i % _cols) * 8 + _x;
	//int iy = (i / _cols) * 8 + _y;
	float ix = (float)((i & 15) * 8);
	float iy = (float)((i >> 4) * 8);
	const int atlas = 128;
#endif
	// 图集每格 8x8：UV 取满一格（7.99 是为了不采到右边邻格），而屏幕上的字形
	// 尺寸单独给 —— 英文也画成 9px，所以这两个尺寸必须分开。
	const float uvW = 7.99f;
	float s = 9.0f;   // on-screen glyph size

	t.vertexUV(x, y + s, 0, ix / (float)atlas, (iy + uvW) / (float)atlas);
	t.vertexUV(x + s, y + s, 0, (ix + uvW) / (float)atlas, (iy + uvW) / (float)atlas);
	t.vertexUV(x + s, y, 0, (ix + uvW) / (float)atlas, iy / (float)atlas);
	t.vertexUV(x, y, 0, ix / (float)atlas, iy / (float)atlas);
}

#ifdef _WIN32
std::string Font::findCustomFontPath()
{
	// Look for data/fonts/custom.ttf next to the executable.
	char buf[MAX_PATH];
	if (!GetModuleFileNameA(NULL, buf, MAX_PATH))
		return "";
	std::string path(buf);
	size_t slash = path.find_last_of("\\/");
	std::string dir = (slash == std::string::npos)? "." : path.substr(0, slash);
	std::string p = dir + "\\data\\fonts\\custom.ttf";
	if (GetFileAttributesA(p.c_str()) == INVALID_FILE_ATTRIBUTES)
		return "";
	return p;
}

std::string Font::ttfFamilyName(const std::string& path)
{
	// Parse the 'name' table of a TTF/OTF file and return the Windows
	// Unicode family name (platform 3, encoding 1/10, nameID 1).
	int plen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, NULL, 0);
	if (plen <= 0) return "";
	std::vector<wchar_t> pw(plen);
	MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &pw[0], plen);
	FILE* f = _wfopen(&pw[0], L"rb");
	if (!f) return "";

	unsigned char hdr[12];
	if (fread(hdr, 1, 12, f) != 12) { fclose(f); return ""; }
	unsigned int numTables = (hdr[4] << 8) | hdr[5];
	unsigned int nameOff = 0;
	for (unsigned int i = 0; i < numTables; ++i) {
		unsigned char rec[16];
		if (fread(rec, 1, 16, f) != 16) break;
		if (rec[0]=='n' && rec[1]=='a' && rec[2]=='m' && rec[3]=='e') {
			nameOff = (rec[8]<<24)|(rec[9]<<16)|(rec[10]<<8)|rec[11];
		}
	}
	if (!nameOff) { fclose(f); return ""; }

	fseek(f, nameOff, SEEK_SET);
	unsigned char nh[6];
	if (fread(nh, 1, 6, f) != 6) { fclose(f); return ""; }
	unsigned int count = (nh[2] << 8) | nh[3];
	unsigned int strOff = (nh[4] << 8) | nh[5];

	for (unsigned int i = 0; i < count; ++i) {
		unsigned char rec[12];
		if (fread(rec, 1, 12, f) != 12) break;
		unsigned int platformID = (rec[0] << 8) | rec[1];
		unsigned int encodingID = (rec[2] << 8) | rec[3];
		unsigned int nameID = (rec[6] << 8) | rec[7];
		unsigned int len = (rec[8] << 8) | rec[9];
		unsigned int off = (rec[10] << 8) | rec[11];
		if (nameID == 1 && platformID == 3 && (encodingID == 1 || encodingID == 10)) {
			fseek(f, nameOff + strOff + off, SEEK_SET);
			std::vector<unsigned char> buf(len);
			if (fread(&buf[0], 1, len, f) != len) break;
			fclose(f);
			// UTF-16BE -> UTF-8
			std::string utf8;
			for (unsigned int k = 0; k + 1 < len; k += 2) {
				unsigned int cp = (buf[k] << 8) | buf[k+1];
				if (cp < 0x80) utf8 += (char)cp;
				else if (cp < 0x800) {
					utf8 += (char)(0xC0 | (cp >> 6));
					utf8 += (char)(0x80 | (cp & 0x3F));
				} else {
					utf8 += (char)(0xE0 | (cp >> 12));
					utf8 += (char)(0x80 | ((cp >> 6) & 0x3F));
					utf8 += (char)(0x80 | (cp & 0x3F));
				}
			}
			return utf8;
		}
	}
	fclose(f);
	return "";
}

unsigned char* Font::renderCustomLatinFont()
{
	std::string path = findCustomFontPath();
	if (path.empty()) return NULL;
	std::string fam = ttfFamilyName(path);
	if (fam.empty()) return NULL;
	int fn = MultiByteToWideChar(CP_UTF8, 0, fam.c_str(), -1, NULL, 0);
	if (fn <= 0) return NULL;
	std::vector<wchar_t> fw(fn);
	MultiByteToWideChar(CP_UTF8, 0, fam.c_str(), -1, &fw[0], fn);

	// Supersampling: render the glyphs at 16px into a 256x256 atlas, they
	// are drawn at 8px on screen — LINEAR filtering smooths the edges.
	HDC hdc = CreateCompatibleDC(NULL);
	BITMAPINFO bmi;
	memset(&bmi, 0, sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = 256;
	bmi.bmiHeader.biHeight = -256; // top-down DIB
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	void* bits = NULL;
	HBITMAP hbmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
	if (!hbmp || !bits) {
		if (hbmp) DeleteObject(hbmp);
		DeleteDC(hdc);
		return NULL;
	}
	HGDIOBJ oldBmp = SelectObject(hdc, hbmp);
	memset(bits, 0, 256 * 256 * 4);

	HFONT hfont = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, &fw[0]);
	if (!hfont) {
		SelectObject(hdc, oldBmp);
		DeleteObject(hbmp);
		DeleteDC(hdc);
		return NULL;
	}
	HGDIOBJ oldFont = SelectObject(hdc, hfont);
	SetBkMode(hdc, TRANSPARENT);
	SetTextColor(hdc, RGB(255, 255, 255));

	// ASCII 0x20..0x7F -> atlas cells (i&15, i>>4), same layout as default8.png
	for (int i = 0x20; i <= 0x7F; ++i) {
		wchar_t wc = (wchar_t)i;
		const int col = i & 15;
		const int row = i >> 4;
		RECT rc = { col * 16, row * 16, col * 16 + 16, row * 16 + 16 };
		DrawTextW(hdc, &wc, 1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);
	}

	// BGRA -> RGBA, keep the antialiased alpha gradient (we draw with
	// blending, not ALPHA_TEST, so soft edges survive the downscale).
	unsigned char* px = (unsigned char*)bits;
	for (int p = 0; p < 256 * 256; ++p) {
		unsigned char* q = px + p * 4;
		const int lum = (q[2] * 77 + q[1] * 150 + q[0] * 29) >> 8;
		q[0] = 255; q[1] = 255; q[2] = 255;
		q[3] = (unsigned char)lum;
	}

	unsigned char* out = (unsigned char*)malloc(256 * 256 * 4);
	memcpy(out, bits, 256 * 256 * 4);

	SelectObject(hdc, oldFont);
	SelectObject(hdc, oldBmp);
	DeleteObject(hfont);
	DeleteObject(hbmp);
	DeleteDC(hdc);
	return out;
}
#endif // _WIN32 —— 上面三个 helper 用 GDI / Win32 API，只有 Windows 版需要

#if defined(_WIN32) || defined(ANDROID)
void Font::initCjkFont()
{
	// (Re)build the CJK glyph atlas. Called on every font init / graphics
	// reset, so the GL texture is always fresh.
	//   Windows：用 GDI 现渲染（同一函数下面的 _WIN32 分支）。
	//   Android：没有 GDI，读 assets/fonts/cjk_atlas.bmp —— 那份图集是
	//   gen_cjk_atlas.py 走**同一套 GDI 调用**离线渲染的，字形与 Windows 版一致。
	if (_cjkTexture) {
		glDeleteTextures(1, (GLuint*)&_cjkTexture);
		_cjkTexture = 0;
	}
	_cjkReady = false;
	_cjkIndex.clear();

	_cjkCols = 128;
	_cjkRows = 64;
	// custom.ttf（Minecraft AE）是 16x16 像素网格的字体（汉字轮廓坐标全是 64 的
	// 倍数，1024/64 = 16）。两个值含义不同、分开设：
	//   _cjkGlyph = 字形材质的**分辨率**（图集每格的像素数）= 16，保住原生点阵细节
	//   _cjkDraw  = 字形在屏幕上的**占地面积**（跟英文的 9px 一样大）
	// 光栅化关抗锯齿 + 采样用 NEAREST：16px 的硬像素材质缩到 9 格占地，边缘仍是
	// 硬像素（不会变回灰度平滑），代价是 16→9 会抽掉部分像素。
	_cjkGlyph = 16;   // atlas cell size = glyph texture resolution
	_cjkDraw  = 11;   // on-screen footprint (latin stays at 9px)
	_cjkTexW  = _cjkCols * _cjkGlyph; // 2048
	_cjkTexH  = _cjkRows * _cjkGlyph; // 1024

#if defined(ANDROID)
	// ── Android：从 assets 读预生成图集 ──────────────────────────────────
	// 文件 = 54 字节 BMP 头 + 2048x1024 的 BGRA 像素（gen_cjk_atlas.py 产出）。
	// R=G=B=覆盖率、第 4 字节=alpha，所以这里只取第 4 字节，RGB 一律填白。
	AppPlatform* plat = AppPlatform::_singleton;
	if (!plat) {
		LOGI("[Font] initCjkFont: no AppPlatform, CJK disabled\n");
		return;
	}
	const int expected = 54 + _cjkTexW * _cjkTexH * 4;
	BinaryBlob blob = plat->readAssetFile("fonts/cjk_atlas.bmp");
	if (!blob.data || blob.size < expected) {
		LOGI("[Font] initCjkFont: CJK atlas missing/short (got %d, need %d)\n",
		     blob.size, expected);
		delete[] blob.data;
		return;
	}
	{
		const unsigned char* src = blob.data + 54;
		const int pixels = _cjkTexW * _cjkTexH;
		std::vector<unsigned char> rgba((size_t)pixels * 4);
		for (int p = 0; p < pixels; p++) {
			rgba[p * 4 + 0] = 255;
			rgba[p * 4 + 1] = 255;
			rgba[p * 4 + 2] = 255;
			rgba[p * 4 + 3] = src[p * 4 + 3];
		}
		delete[] blob.data;

		glGenTextures(1, (GLuint*)&_cjkTexture);
		glBindTexture(GL_TEXTURE_2D, (GLuint)_cjkTexture);
		// 像素风：字形 1:1 贴上去，不做插值，硬像素边才保得住
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _cjkTexW, _cjkTexH, 0,
		             GL_RGBA, GL_UNSIGNED_BYTE, &rgba[0]);
		LOGI("[Font] CJK atlas %dx%d uploaded (tex=%d)\n",
		     _cjkTexW, _cjkTexH, (int)_cjkTexture);
	}

	// unicode -> 图集格子。顺序必须与 gen_cjk_atlas.py 一致：
	//   [0 .. n-1]        cjk_gb2312.h 的 g_cjkChars（6763 个 GB2312 汉字）
	//   [n .. n+extra-1]  kExtraChars（中文标点 / 常用符号）
	{
		const int cells = _cjkCols * _cjkRows;
		const int n = (g_cjkCharCount < cells) ? g_cjkCharCount : cells;
		for (int i = 0; i < n; i++)
			_cjkIndex[g_cjkChars[i]] = i;

		// ⚠ 与下面 _WIN32 分支的同名表、以及 gen_cjk_atlas.py 必须完全一致
		//   （gen_cjk_atlas.py 会直接解析本文件里的表，不一致它会报错）
		static const unsigned short kExtraChars[] = {
			0xFF0C, 0x3002, 0x3001, 0xFF1A, 0xFF1B, 0xFF01, 0xFF1F, 0xFF08, 0xFF09,
			0x3010, 0x3011, 0x300A, 0x300B, 0x201C, 0x201D, 0x2018, 0x2019,
			0x2014, 0x2026, 0x00B7, 0xFF5E, 0xFF05, 0xFF03, 0xFFE5,
			0x2192, 0x2190, 0x2191, 0x2193, 0x00D7, 0x00F7, 0x00B1, 0x00B0,
			0x2605, 0x2606, 0x2665, 0x2713, 0x2717, 0x26A0, 0x25CF, 0x25A1
		};
		const int extraCount = (int)(sizeof(kExtraChars) / sizeof(kExtraChars[0]));
		for (int e = 0, slot = n; e < extraCount && slot < cells; e++, slot++)
			_cjkIndex[kExtraChars[e]] = slot;
	}

	_cjkReady = true;
	return;
#else
	HDC hdc = CreateCompatibleDC(NULL);
	BITMAPINFO bmi;
	memset(&bmi, 0, sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = _cjkTexW;
	bmi.bmiHeader.biHeight = -_cjkTexH; // top-down DIB
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	void* bits = NULL;
	HBITMAP hbmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
	if (!hbmp || !bits) {
		if (hbmp) DeleteObject(hbmp);
		DeleteDC(hdc);
		return;
	}
	HGDIOBJ oldBmp = SelectObject(hdc, hbmp);
	memset(bits, 0, (size_t)_cjkTexW * _cjkTexH * 4);

	// CJK font. Use grayscale antialiasing (ClearType bleeds color on a
	// transparent background). A user-provided data/fonts/custom.ttf takes
	// precedence; otherwise fall back to the classic system fonts.
	std::wstring face = L"Microsoft YaHei";
	std::string customPath = findCustomFontPath();
	if (!customPath.empty()) {
		int plen = MultiByteToWideChar(CP_UTF8, 0, customPath.c_str(), -1, NULL, 0);
		if (plen > 0) {
			std::vector<wchar_t> pw(plen);
			MultiByteToWideChar(CP_UTF8, 0, customPath.c_str(), -1, &pw[0], plen);
			AddFontResourceExW(&pw[0], FR_PRIVATE, 0); // process-private registration
		}
		std::string fam = ttfFamilyName(customPath);
		if (!fam.empty()) {
			int fn = MultiByteToWideChar(CP_UTF8, 0, fam.c_str(), -1, NULL, 0);
			if (fn > 0) {
				std::vector<wchar_t> fw(fn);
				MultiByteToWideChar(CP_UTF8, 0, fam.c_str(), -1, &fw[0], fn);
				face = &fw[0];
			}
		}
	}
	HFONT hfont = CreateFontW(-_cjkGlyph, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face.c_str());
	if (!hfont) {
		hfont = CreateFontW(-_cjkGlyph, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
			NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"SimSun");
	}
	if (!hfont) {
		SelectObject(hdc, oldBmp);
		DeleteObject(hbmp);
		DeleteDC(hdc);
		return;
	}
	HGDIOBJ oldFont = SelectObject(hdc, hfont);
	SetBkMode(hdc, TRANSPARENT);
	SetTextColor(hdc, RGB(255, 255, 255));

	const int cells = _cjkCols * _cjkRows;
	const int n = (g_cjkCharCount < cells) ? g_cjkCharCount : cells;
	for (int i = 0; i < n; i++) {
		wchar_t wc = (wchar_t)g_cjkChars[i];
		const int col = i % _cjkCols;
		const int row = i / _cjkCols;
		RECT rc;
		rc.left   = col * _cjkGlyph;
		rc.top    = row * _cjkGlyph;
		rc.right  = rc.left + _cjkGlyph;
		rc.bottom = rc.top + _cjkGlyph;
		DrawTextW(hdc, &wc, 1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);
		_cjkIndex[g_cjkChars[i]] = i;
	}

	// 常用中文标点 + 符号：字形表原来只有 6763 个 GB2312 汉字，标点不在里面，
	// 于是全角标点也会走“未光栅化”分支被画成一条横杠（模组文案里到处都是）。
	// 这里把它们接在汉字之后编号（cells = 8192，放得下）。
	static const unsigned short kExtraChars[] = {
		0xFF0C, 0x3002, 0x3001, 0xFF1A, 0xFF1B, 0xFF01, 0xFF1F, 0xFF08, 0xFF09,
		0x3010, 0x3011, 0x300A, 0x300B, 0x201C, 0x201D, 0x2018, 0x2019,
		0x2014, 0x2026, 0x00B7, 0xFF5E, 0xFF05, 0xFF03, 0xFFE5,
		0x2192, 0x2190, 0x2191, 0x2193, 0x00D7, 0x00F7, 0x00B1, 0x00B0,
		0x2605, 0x2606, 0x2665, 0x2713, 0x2717, 0x26A0, 0x25CF, 0x25A1
	};
	const int extraCount = (int)(sizeof(kExtraChars) / sizeof(kExtraChars[0]));
	for (int e = 0, slot = n; e < extraCount && slot < cells; e++, slot++) {
		wchar_t wc = (wchar_t)kExtraChars[e];
		const int col = slot % _cjkCols;
		const int row = slot / _cjkCols;
		RECT rc;
		rc.left   = col * _cjkGlyph;
		rc.top    = row * _cjkGlyph;
		rc.right  = rc.left + _cjkGlyph;
		rc.bottom = rc.top + _cjkGlyph;
		DrawTextW(hdc, &wc, 1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);
		_cjkIndex[kExtraChars[e]] = slot;
	}

	// Convert BGRA DIB to RGBA atlas: white glyph, alpha = luminance.
	// Keep the antialiased alpha gradient — fonts are drawn with blending
	// (not ALPHA_TEST), so the soft edges survive the supersampled downscale.
	unsigned char* px = (unsigned char*)bits;
	const int total = _cjkTexW * _cjkTexH;
	for (int p = 0; p < total; p++) {
		unsigned char* q = px + p * 4;
		const int lum = (q[2] * 77 + q[1] * 150 + q[0] * 29) >> 8;
		q[0] = 255; q[1] = 255; q[2] = 255;
		q[3] = (unsigned char)lum;
	}

	glGenTextures(1, (GLuint*)&_cjkTexture);
	glBindTexture(GL_TEXTURE_2D, (GLuint)_cjkTexture);
	// 像素风：字形是 1:1 贴上去的，不做插值，硬像素边才保得住
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _cjkTexW, _cjkTexH, 0, GL_RGBA, GL_UNSIGNED_BYTE, bits);

	SelectObject(hdc, oldFont);
	SelectObject(hdc, oldBmp);
	DeleteObject(hfont);
	DeleteObject(hbmp);
	DeleteDC(hdc);

	_cjkReady = true;
#endif // ANDROID / _WIN32
}

void Font::buildCjkChar( int glyphIndex, float x, float y )
{
	if (glyphIndex < 0 || glyphIndex >= _cjkCols * _cjkRows)
		return;
	Tesselator& t = Tesselator::instance;
	const int col = glyphIndex % _cjkCols;
	const int row = glyphIndex / _cjkCols;
	// Glyph data lives in the DIB at exactly (row, col); the texture is
	// uploaded with row 0 at v=0, so use the row index directly - no flip.
	const float s = (float)_cjkDraw;
	const float u0 = (float)(col * _cjkGlyph) / _cjkTexW;
	const float u1 = (float)((col + 1) * _cjkGlyph) / _cjkTexW;
	const float v0 = (float)(row * _cjkGlyph) / _cjkTexH;
	const float v1 = (float)((row + 1) * _cjkGlyph) / _cjkTexH;

	t.vertexUV(x, y + s, 0, u0, v1);
	t.vertexUV(x + s, y + s, 0, u1, v1);
	t.vertexUV(x + s, y, 0, u1, v0);
	t.vertexUV(x, y, 0, u0, v0);
}

void Font::drawCjkCharImmediate(int glyphIndex, float px, float py, int r, int g, int b, int a)
{
	if (glyphIndex < 0 || glyphIndex >= _cjkCols * _cjkRows)
		return;
	if (!_cjkReady || !_cjkTexture)
		return;
	const int col = glyphIndex % _cjkCols;
	const int row = glyphIndex / _cjkCols;
	const float s = (float)_cjkDraw;
	const float u0 = (float)(col * _cjkGlyph) / _cjkTexW;
	const float u1 = (float)((col + 1) * _cjkGlyph) / _cjkTexW;
	const float v0 = (float)(row * _cjkGlyph) / _cjkTexH;
	const float v1 = (float)((row + 1) * _cjkGlyph) / _cjkTexH;

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, (GLuint)_cjkTexture);
	glColor4f(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
	// Android 的 GLES1 头文件里没有 glBegin/glEnd（那是桌面 GL 的立即模式），
	// 所以这里用顶点数组 + TRIANGLE_STRIP 拼出同一个四边形。
	// 顶点顺序：左下 / 右下 / 左上 / 右上（strip 正好两个三角形）。
	const GLfloat verts[8] = {
		px,     py + s,
		px + s, py + s,
		px,     py,
		px + s, py
	};
	const GLfloat uvs[8] = {
		u0, v1,
		u1, v1,
		u0, v0,
		u1, v0
	};
	glEnableClientState2(GL_VERTEX_ARRAY);
	glEnableClientState2(GL_TEXTURE_COORD_ARRAY);
	glVertexPointer2(2, GL_FLOAT, 0, verts);
	glTexCoordPointer2(2, GL_FLOAT, 0, uvs);
	glDrawArrays2(GL_TRIANGLE_STRIP, 0, 4);
	glDisableClientState2(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState2(GL_VERTEX_ARRAY);
	// Restore the font texture so the pending override batch (ASCII glyphs,
	// icons) is submitted by endOverrideAndDraw() with the correct texture.
	glBindTexture(GL_TEXTURE_2D, (GLuint)fontTexture);
}
#endif

