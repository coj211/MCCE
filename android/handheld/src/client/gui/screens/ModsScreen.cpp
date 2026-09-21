#include "ModsScreen.h"

#include "ScreenChooser.h"
#include "../../Minecraft.h"
#include "../../../mod/ModEngine.h"
#include "../../../locale/I18n.h"
#include "../../renderer/gles.h"
#include "../../renderer/Tesselator.h"
#include "../../renderer/Textures.h"
#include "../../renderer/TextureData.h"
#include <gui/NinePatchFactory.hpp>
#include <gui/NinePatchLayer.hpp>
#include <gui/PackedScrollContainer.hpp>
#include <gui/pane/ScrollingPane.hpp>
#include <gui/buttons/Touch_TButton.hpp>
#include <gui/buttons/Touch_THeader.hpp>
#include <gui/elements/ModListItemElement.hpp>
#include <util/IntRectangle.hpp>
#include <input/Mouse.hpp>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <commdlg.h>
extern HWND g_win32Hwnd;
#elif defined(__ANDROID__)
extern "C" void androidPickModFile();
#endif

static const int kIdBack = 1;
static const int kIdLoad = 2;

// 左侧底部信息区布局：封面占 panelH-kDescH，底部 kDescH 高放名字+简介
static const int kDescH = 76;      // 信息区高度
static const int kDescVisH = 56;   // 简介可视高度
static const int kDescLineH = 12;  // 简介行高

class Font;
// 按像素宽度把文本切成多行（UTF-8 感知，中文按 CJK 宽字符计）
static void splitLines(Font* f, const std::string& text, int maxW, std::vector<std::string>& out);

ModsScreen::ModsScreen()
:	bHeader(NULL), bBack(NULL), bLoad(NULL),
	frame(NULL), list(NULL),
	_selected(-1),
	_listLeft(0), _listTop(0), _listW(0), _listH(0),
	_dirty(false),
	_descScroll(0), _descDragging(false), _descDragged(false),
	_descDragStartY(0), _descScrollAtStart(0)
{
}

ModsScreen::~ModsScreen() {
	// 先删列表项（析构会 delete 各自的 ModInfo），再清容器避免悬垂/双删
	for (size_t i = 0; i < items.size(); ++i)
		delete items[i];
	items.clear();
	if (list) {
		list->clearAll();
		delete list;
		list = NULL;
	}
	if (frame) delete frame;
	frame = NULL;
	delete bHeader; bHeader = NULL;
	delete bBack;   bBack   = NULL;
	delete bLoad;   bLoad   = NULL;
}

void ModsScreen::renderBackground(int vo) {
}

void ModsScreen::init() {
	bHeader = new Touch::THeader(0, I18n::get("mods.title"));
	bBack   = new Touch::TButton(kIdBack, I18n::get("gui.back"));
	bLoad   = new Touch::TButton(kIdLoad, I18n::get("mods.load"));
	bBack->width = 38;
	bBack->height = 18;
	((Touch::TButton*)bBack)->init(minecraft);
	((Touch::TButton*)bLoad)->init(minecraft);
	bHeader->width = 200;

	buttons.push_back(bHeader);
	buttons.push_back(bBack);
	buttons.push_back(bLoad);

	NinePatchFactory a1(this->minecraft->textures, "gui/spritesheet.png");
	this->frame = a1.createSymmetrical({34, 43, 14, 14}, 3, 3, 32, 32);

	// 游戏页面(PlayScreen)同款：右侧 PackedScrollContainer 竖排列表（元素树，Screen 分发鼠标/渲染）
	this->list = new PackedScrollContainer(0, 0, 0);
	elements.push_back(this->list);

	refreshMods();
}

void ModsScreen::refreshMods() {
	if (list) {
		// 释放旧列表项（ModListItemElement 析构会 delete 自己的 ModInfo 副本）
		for (size_t i = 0; i < items.size(); ++i)
			delete items[i];
		items.clear();
		list->clearAll();
	}
	mods = minecraft->modEngine->scanMods();
	if (_selected >= (int)mods.size())
		_selected = mods.empty() ? -1 : 0;
	if (_selected < 0 && !mods.empty())
		_selected = 0;
	// 每个模组一个列表项：结构照抄世界列表项 LocalServerListItemElement
	// （整行 TButton 九宫格背景 + 右侧 on/off 开关，替代世界的编辑/删除按钮）
	for (size_t i = 0; i < mods.size(); ++i) {
		ModListItemElement* item = new ModListItemElement(minecraft, new ModInfo(mods[i]), this);
		item->init(minecraft);
		items.push_back(item);
		list->addChild(item);
	}
	// 注意：这里不要调 list->setupPositions()！Screen::init 之后会调
	// ModsScreen::setupPositions → list->setupPositions（此时尺寸已正确）。
	// 若在尺寸仍为 0 时提前创建 ScrollingPane，field_38 缓存会让它永不重建，
	// 导致 area=0 → 拖动失效（滚轮仍有效，因为直接改 contentOffset）。
}

void ModsScreen::setupPositions() {
	int headerH = (bHeader != NULL) ? bHeader->height : 26;

	bHeader->x = 0; bHeader->y = 0;
	bHeader->width = width; bHeader->height = headerH;

	bBack->x = 4; bBack->y = 4;
	bLoad->x = width - 4 - 70; bLoad->y = 4; bLoad->width = 70; bLoad->height = 18;

	// 与游戏页面(PlayScreen)一致：列表缩到右半边
	_listW = (width - 20) / 2;
	_listLeft = width - _listW - 10;
	_listTop = headerH + 6;
	_listH = height - (headerH + 6) - 6;
	if (list) {
		list->x = _listLeft;
		list->y = _listTop;
		list->width = _listW;
		list->height = _listH;
		list->setupPositions();
	}
	if (frame)
		frame->setSize((float)_listW + 6.0f, (float)_listH + 6.0f);
}

void ModsScreen::buttonClicked(Button* button) {
	if (button == bBack) {
		minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
	} else if (button == bLoad) {
		pickAndInstall();
	}
}

void ModsScreen::pickAndInstall() {
#ifdef _WIN32
	char file[MAX_PATH] = {0};
	OPENFILENAMEA ofn;
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = g_win32Hwnd;
	ofn.lpstrFilter = "Mod (*.zip)\0*.zip\0All Files (*.*)\0*.*\0\0";
	ofn.lpstrFile = file;
	ofn.nMaxFile = MAX_PATH;
	// OFN_NOCHANGEDIR: 防 Windows 改 CWD(world/data 为相对 exe 路径)
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
	if (GetOpenFileNameA(&ofn)) {
		std::string name = minecraft->modEngine->installModFile(file);
		if (!name.empty())
			minecraft->modEngine->setEnabled(name, true);
		_dirty = true;
	}
#elif defined(__ANDROID__)
	androidPickModFile();
	_dirty = true;
#endif
}

void ModsScreen::selectMod(const ModInfo* info) {
	if (!info) return;
	for (size_t i = 0; i < mods.size(); ++i) {
		if (mods[i].file == info->file) {
			_selected = (int)i;
			_descScroll = 0;          // 切换模组时简介回到顶部
			_descDragging = false;
			break;
		}
	}
}

bool ModsScreen::isSelected(const ModInfo* info) const {
	if (!info) return false;
	return _selected >= 0 && _selected < (int)mods.size() && mods[_selected].file == info->file;
}

void ModsScreen::onModToggled() {
	_dirty = true;
}

void ModsScreen::mouseClicked(int x, int y, int buttonNum) {
	super::mouseClicked(x, y, buttonNum);
	if (buttonNum != MouseAction::ACTION_LEFT)
		return;
	// 左侧简介区按下 → 准备拖动查看完整简介
	int ty = _listTop + _listH - kDescH;
	if (x < _listLeft && y >= ty + 18 && y <= _listTop + _listH) {
		_descDragging = true;
		_descDragged = false;
		_descDragStartY = y;
		_descScrollAtStart = _descScroll;
	}
}

void ModsScreen::mouseReleased(int x, int y, int buttonNum) {
	super::mouseReleased(x, y, buttonNum);
	_descDragging = false;
}

void ModsScreen::onMouseWheel(int dy) {
	int gx = Mouse::getX() * width / minecraft->width;   // GUI x（判定鼠标在哪一侧）
	if (gx < _listLeft) {
		// 左侧：滚简介
		int minS = descMinScroll();
		int ns = _descScroll + dy * kDescLineH;
		if (ns > 0) ns = 0;
		if (ns < minS) ns = minS;
		_descScroll = ns;
		return;
	}
	// 右侧：滚列表（滚轮一格滚一行(32px)，拖动由 ScrollingPane 原生支持）
	if (!list || !list->scrollingPane)
		return;
	if (items.empty())
		return;
	int contentH = 0;
	for (size_t i = 0; i < items.size(); ++i)
		contentH += items[i]->height;
	if (contentH <= list->height) {
		list->scrollingPane->getContentOffset()->y = 0.0f;
		return;
	}
	int minY = list->height - contentH;   // 负值 = 最大可滚距离
	Vec3* off = list->scrollingPane->getContentOffset();
	float ny = off->y + dy * 32.0f;
	if (ny > 0) ny = 0;
	if (ny < minY) ny = (float)minY;
	off->y = ny;
}

int ModsScreen::descMinScroll() const {
	if (_selected < 0 || _selected >= (int)mods.size())
		return 0;
	std::vector<std::string> lines;
	splitLines(minecraft->font,
		mods[_selected].description.empty() ? I18n::get("mods.info.noDescription") : mods[_selected].description,
		_listW - 8, lines);
	int totalH = (int)lines.size() * kDescLineH;
	if (totalH <= kDescVisH) return 0;
	return kDescVisH - totalH;   // 负值
}

void ModsScreen::tick() {
	if (_dirty) {
		_dirty = false;
		refreshMods();
	}
	if (list)
		list->tick(minecraft);
	// 简介拖动（仿 ScrollingPane：按住移动 → 更新滚动偏移）
	if (_descDragging && _selected >= 0 && _selected < (int)mods.size()) {
		int my = Mouse::getY() * height / minecraft->height - 1;   // GUI y
		int delta = my - _descDragStartY;
		if (delta != 0)
			_descDragged = true;
		int ns = _descScrollAtStart + delta;
		int minS = descMinScroll();
		if (ns > 0) ns = 0;
		if (ns < minS) ns = minS;
		_descScroll = ns;
	}
}

// 画一张已知像素尺寸的原生 GL 纹理（contain 缩放居中到目标框内）。
// 注意：封面纹理是 ModEngine::makeGlTexture 直接创建的（不进 Textures 管理表），
// 所以真实尺寸由调用方传入，不能再用 textures->getTemporaryTextureData 查询。
static bool drawTexScaled(Minecraft* mc, unsigned int texId, int tw, int th, int x, int y, int w, int h, int maxW, int maxH) {
	if (!texId) return false;
	if (tw <= 0 || th <= 0) return false;
	float scale = 1.0f;
	if ((float)tw / (float)th > (float)w / (float)h)
		scale = (float)w / (float)tw;
	else
		scale = (float)h / (float)th;
	float dw = (float)tw * scale, dh = (float)th * scale;
	if (dw > maxW || dh > maxH) {
		float s2 = 1.0f;
		if (dw > maxW) s2 = (float)maxW / dw;
		if (dh * s2 > maxH) s2 = (float)maxH / dh;
		dw *= s2; dh *= s2;
	}
	float dx = x + (w - dw) / 2.0f;
	float dy = y + (h - dh) / 2.0f;
	// 与 mod 按钮自绘底图 (drawModTexRect) 同款通路: 直接 glBind 裸 GL 纹理
	// + 显式 enable blend/texture + 顶点白色, 否则纹理乘黑或状态残留导致不可见。
	glEnable2(GL_TEXTURE_2D);
	glEnable2(GL_BLEND);
	glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBindTexture(GL_TEXTURE_2D, texId);
	glColor4f2(1, 1, 1, 1);
	Tesselator::instance.offset(0, 0, 0);
	Tesselator& t = Tesselator::instance;
	t.begin();
	t.color(0xffffffff);
	t.vertexUV(dx, dy + dh, 0, 0, 1);
	t.vertexUV(dx + dw, dy + dh, 0, 1, 1);
	t.vertexUV(dx + dw, dy, 0, 1, 0);
	t.vertexUV(dx, dy, 0, 0, 0);
	t.draw();
	return true;
}

void ModsScreen::drawCover(int x, int y, int w, int h, const ModInfo& m) {
	if (m.cover.empty()) return;
	unsigned int texId = minecraft->modEngine->getModCoverTexture(m.file);
	if (!texId) return;
	int tw = 0, th = 0;
	if (!minecraft->modEngine->getModCoverSize(m.file, tw, th) || tw <= 0 || th <= 0)
		return;
	drawTexScaled(minecraft, texId, tw, th, x, y, w, h, w, h);
}

// 按像素宽度把文本切成多行（UTF-8 感知，中文按 CJK 宽字符计）
static void splitLines(Font* f, const std::string& text, int maxW, std::vector<std::string>& out) {
	std::string cur;
	int curW = 0;
	size_t i = 0;
	while (i < text.length()) {
		unsigned char ch = (unsigned char)text[i];
		int consumed = 1;
		if ((ch & 0xF8) == 0xF0 && i + 3 < text.length()) consumed = 4;
		else if ((ch & 0xF0) == 0xE0 && i + 2 < text.length()) consumed = 3;
		else if ((ch & 0xE0) == 0xC0 && i + 1 < text.length()) consumed = 2;
		std::string c = text.substr(i, consumed);
		if (ch == '\n') {
			out.push_back(cur);
			cur.clear(); curW = 0;
			i += consumed;
			continue;
		}
		int cw = f->width(c);
		if (curW + cw > maxW && !cur.empty()) {
			out.push_back(cur);
			cur.clear(); curW = 0;
		}
		cur += c;
		curW += cw;
		i += consumed;
	}
	if (!cur.empty()) out.push_back(cur);
}

void ModsScreen::render(int xm, int ym, float a) {
	glEnable2(GL_BLEND);
	glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable2(GL_TEXTURE_2D);

	// 与游戏页面(PlayScreen)一致：全景背景 → 列表边框(先画, 列表覆盖其内部) → 列表 → 左侧封面
	this->renderMenuBackground(a);
	if (frame)
		frame->draw(Tesselator::instance, _listLeft - 3, _listTop - 3);
	Screen::render(xm, ym, a);

	// 左侧封面区（选中模组的 cover，无封面只显示名称/简介）
	int panelX = 4;
	int panelY = _listTop;
	int panelW = _listW;
	int panelH = _listH;
	if (_selected >= 0 && _selected < (int)mods.size()) {
		const ModInfo& m = mods[_selected];
		int ty = panelY + panelH - kDescH;   // 信息区顶部（名字行，比之前上移）
		drawCover(panelX, panelY, panelW, panelH - kDescH, m);
		Font* f = minecraft->font;
		f->drawShadow(m.name, panelX + 4, ty + 2, 0xffffffff);
		// 简介：不截断，全部按宽度换行；超出可视区可拖动/滚轮滚动查看（_descScroll）
		std::vector<std::string> lines;
		splitLines(f, m.description.empty() ? I18n::get("mods.info.noDescription") : m.description, panelW - 8, lines);
		for (int i = 0; i < (int)lines.size(); ++i) {
			int ly = ty + 18 + i * kDescLineH + _descScroll;
			if (ly < ty + 18 || ly >= ty + 18 + kDescVisH)   // 裁剪到简介可视区
				continue;
			f->drawShadow(lines[i], panelX + 4, ly, 0xffcccccc);
		}
	} else {
		minecraft->font->drawShadow(I18n::get("mods.empty"), panelX + 8, panelY + panelH / 2, 0xff888888);
	}
}

void ModsScreen::removed() {
	super::removed();
}
