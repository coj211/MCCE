#include "LanguageScreen.h"

#include "ScreenChooser.h"
#include "../../Minecraft.h"
#include "../components/OptionsPane.h"
#include "../components/OptionsItem.h"
#include "../components/ImageButton.h"
#include "../../../locale/I18n.h"
#include "../../renderer/gles.h"
#include "../../../mod/ModEngine.h"
#include "../../../mod/ModZip.h"
#include <cstdint>
#include <cstdio>
#include <ctype.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <commdlg.h>
extern HWND g_win32Hwnd;
#endif

// 语言开关按钮 —— 与设置页开关项外观完全一致（gui/touchgui.png 两帧开关图，
// 同 OptionsGroup::createToggle 的 OptionButton）。on/off 帧按当前语言动态取：
// isSecondImage 返回 true 画 on 帧。
class LanguageToggleButton : public ImageButton
{
	typedef ImageButton super;
public:
	LanguageToggleButton(int id, LanguageScreen* owner, int index)
		: ImageButton(id, ""), _owner(owner), _index(index) {
		ImageDef def;
		def.setSrc(IntRectangle(160, 206, 39, 20));
		def.name = "gui/touchgui.png";
		def.width = 39 * 0.7f;
		def.height = 20 * 0.7f;
		this->setImageDef(def, true);
	}
	virtual void mouseClicked(Minecraft* mc, int x, int y, int buttonNum) {
		if (buttonNum == 1 /*ACTION_LEFT*/) {
			if (this->clicked(mc, x, y) && _owner)
				_owner->selectLanguage(_index);
		}
	}
protected:
	virtual bool isSecondImage(bool hovered) {
		return _owner && _owner->isSelected(_index);
	}
private:
	LanguageScreen* _owner;
	int _index;
};

LanguageScreen::LanguageScreen() {
	this->bHeader = 0;
	this->bBack = 0;
	this->bAdd = 0;
	this->pane = 0;
}

LanguageScreen::~LanguageScreen() {
	if (this->pane) {
		delete this->pane;
		this->pane = 0;
	}
	if (this->bAdd) {
		delete this->bAdd;
		this->bAdd = 0;
	}
	if (this->bHeader) {
		delete this->bHeader;
		this->bHeader = 0;
	}
	if (this->bBack) {
		delete this->bBack;
		this->bBack = 0;
	}
}

void LanguageScreen::init() {
	// 可用语言 = 平台枚举 data/lang/*.lang；玩家放一个 .lang 文件即出现。
	langCodes.clear();
	langNames.clear();
	StringVector codes = I18n::availableLanguages(this->minecraft->platform());
	for (size_t i = 0; i < codes.size(); ++i) {
		langCodes.push_back(codes[i]);
		langNames.push_back(I18n::languageDisplayName(this->minecraft->platform(), codes[i]));
	}

	this->bHeader = new Touch::THeader(0, I18n::get("mainmenu.language"));
	this->bBack = new Touch::TButton(1, I18n::get("gui.back"));
	this->bBack->width = 38;
	this->bBack->height = 18;
	this->bBack->init(this->minecraft);
	// 右上角"导入语言"
	this->bAdd = new Touch::TButton(2, I18n::get("mainmenu.language.import"));
	this->bAdd->width = 70;
	this->bAdd->height = 18;
	this->bAdd->init(this->minecraft);
	this->buttons.push_back(this->bHeader);
	this->buttons.push_back(this->bBack);
	this->buttons.push_back(this->bAdd);

	// 设置页右侧同款内容容器：每个语言 = 一个 OptionsItem 行
	// （左语言名 + 右侧开关按钮，与 OptionsScreen 的开关项完全一致）。
	this->pane = new OptionsPane();
	for (int i = 0; i < (int)langCodes.size(); ++i) {
		LanguageToggleButton* toggle = new LanguageToggleButton(10 + i, this, i);
		OptionsItem* item = new OptionsItem(langNames[i], toggle);
		item->setupPositions();
		this->pane->addChild(item);
		this->items.push_back(item);
	}
}

void LanguageScreen::setupPositions() {
	this->bHeader->x = 0;
	this->bHeader->y = 0;
	this->bHeader->width = this->width;
	this->bHeader->height = this->bBack->height + 8;
	this->bBack->x = 4;
	this->bBack->y = 4;
	this->bAdd->x = this->width - this->bAdd->width - 4;
	this->bAdd->y = 4;

	if (this->pane) {
		this->pane->x = 8;
		this->pane->y = this->bHeader->height + 3;
		this->pane->width = this->width - 16;
		this->pane->setupPositions();
	}
}

void LanguageScreen::render(int xm, int ym, float a) {
	glEnable2(GL_BLEND);
	glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable2(GL_TEXTURE_2D);

	// 延迟应用导入文件：对话框返回后这里才做文件 IO/重载，避免在
	// GL 帧中间写文件造成花屏（与皮肤选图同一策略）。
	if (!_pendingImport.empty()) {
		std::string path = _pendingImport;
		_pendingImport.clear();
		if (installLanguageFile(path)) {
			// 已 setScreen(new LanguageScreen)：本屏将在一帧后被销毁替换，
			// 仍把当前帧画完（文件 IO 在帧开头已避开 GL 中间态）。
			this->renderMenuBackground(a);
			this->renderBackground(0);
			Screen::render(xm, ym, a);
			if (this->pane)
				this->pane->render(this->minecraft, xm, ym - 1);
			return;
		}
	}

	this->renderMenuBackground(a);
	this->renderBackground(0);

	// 顶栏按钮（标题/返回/导入）
	Screen::render(xm, ym, a);

	if (this->pane)
		this->pane->render(this->minecraft, xm, ym - 1);
}

void LanguageScreen::buttonClicked(Button* button) {
	if (button == this->bBack) {
		this->minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
	} else if (button == this->bAdd) {
		this->pickLanguageFile();
	}
}

// 解析 lang 文本里的 language.code= 行（找不到返回空）
static std::string langCodeFromText(const std::string& text, const std::string& fallback) {
	size_t pos = text.find("language.code=");
	if (pos != std::string::npos) {
		size_t e = text.find('\n', pos);
		std::string v = text.substr(pos + 14, (e == std::string::npos ? text.size() : e) - (pos + 14));
		// 去掉 \r
		while (!v.empty() && (v[v.size() - 1] == '\r' || v[v.size() - 1] == ' '))
			v.erase(v.size() - 1);
		if (!v.empty())
			return v;
	}
	return fallback;
}

// 安装一个语言包文件到 data/lang/<code>.lang；成功返回 true。
// 支持：.lang/.txt 文本；.zip（内含首个 .lang 条目）。
bool LanguageScreen::installLanguageFile(const std::string& path) {
	std::string lower = path;
	for (size_t i = 0; i < lower.size(); ++i)
		lower[i] = (char)tolower((unsigned char)lower[i]);

	std::vector<unsigned char> data;
	std::string text;
	std::string fileStem;

	if (lower.size() > 4 && lower.compare(lower.size() - 4, 4, ".zip") == 0) {
		// zip：找 .lang 条目解出文本
		std::vector<ModZipEntry> entries;
		if (!modZipRead(path, entries)) {
			minecraft->modEngine->log("import lang: not a zip: " + path);
			return false;
		}
		const ModZipEntry* found = NULL;
		for (size_t i = 0; i < entries.size(); ++i) {
			std::string n = entries[i].name;
			for (size_t j = 0; j < n.size(); ++j)
				n[j] = (char)tolower((unsigned char)n[j]);
			if (n.size() > 5 && n.compare(n.size() - 5, 5, ".lang") == 0) {
				found = &entries[i];
				break;
			}
		}
		if (!found) {
			minecraft->modEngine->log("import lang: no .lang inside zip");
			return false;
		}
		if (!modZipExtract(path, *found, data))
			return false;
		text.assign((const char*)data.data(), data.size());
		// 文件名 stem 作 code 后备
		std::string en = found->name;
		size_t slash = en.find_last_of("\\/");
		if (slash != std::string::npos) en = en.substr(slash + 1);
		size_t dot = en.find_last_of('.');
		fileStem = (dot == std::string::npos) ? en : en.substr(0, dot);
	} else {
		// 普通文本文件：读整个文件
		FILE* f = fopen(path.c_str(), "rb");
		if (!f)
			return false;
		fseek(f, 0, SEEK_END);
		long sz = ftell(f);
		fseek(f, 0, SEEK_SET);
		if (sz <= 0) { fclose(f); return false; }
		std::vector<char> buf((size_t)sz);
		fread(buf.data(), 1, (size_t)sz, f);
		fclose(f);
		text.assign(buf.data(), buf.size());
		// 文件 stem 后备
		std::string base = path;
		size_t slash = base.find_last_of("\\/");
		if (slash != std::string::npos) base = base.substr(slash + 1);
		size_t dot = base.find_last_of('.');
		fileStem = (dot == std::string::npos) ? base : base.substr(0, dot);
	}

	std::string code = langCodeFromText(text, fileStem);
	if (code.empty()) {
		minecraft->modEngine->log("import lang: no language.code, file: " + path);
		return false;
	}
	// 目标路径（运行时 CWD = exe 目录；data/lang 必存在）
	std::string outPath = "data/lang/" + code + ".lang";
	FILE* fo = fopen(outPath.c_str(), "wb");
	if (!fo) {
		minecraft->modEngine->log("import lang: cannot write " + outPath);
		return false;
	}
	fwrite(text.data(), 1, text.size(), fo);
	fclose(fo);
	minecraft->modEngine->log("import lang: " + code + " <- " + path);

	// 切到新语言并重建本屏（列表现在包含它）
	minecraft->options.language = code;
	I18n::loadLanguage(minecraft->platform(), code);
	minecraft->options.save();
	minecraft->setScreen(new LanguageScreen());
	return true;
}

void LanguageScreen::pickLanguageFile() {
#ifdef _WIN32
	char file[MAX_PATH] = {0};
	OPENFILENAMEA ofn;
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = g_win32Hwnd;
	ofn.lpstrFilter =
		"Language (*.lang;*.txt;*.zip)\0*.lang;*.txt;*.zip\0"
		"Text (*.lang;*.txt)\0*.lang;*.txt\0"
		"Zip (*.zip)\0*.zip\0"
		"All Files (*.*)\0*.*\0\0";
	ofn.lpstrFile = file;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
	if (GetOpenFileNameA(&ofn))
		_pendingImport = file;   // 延迟到 render 开头处理
#endif
}

void LanguageScreen::mouseClicked(int x, int y, int buttonNum) {
	if (this->pane)
		this->pane->mouseClicked(this->minecraft, x, y, buttonNum);
	Screen::mouseClicked(x, y, buttonNum);
}

void LanguageScreen::mouseReleased(int x, int y, int buttonNum) {
	if (this->pane)
		this->pane->mouseReleased(this->minecraft, x, y, buttonNum);
	Screen::mouseReleased(x, y, buttonNum);
}

bool LanguageScreen::isSelected(int index) const {
	if (index < 0 || index >= (int)langCodes.size())
		return false;
	return this->minecraft->options.language == langCodes[index];
}

void LanguageScreen::selectLanguage(int index) {
	if (index < 0 || index >= (int)langCodes.size())
		return;
	if (this->minecraft->options.language == langCodes[index])
		return;
	this->minecraft->options.language = langCodes[index];
	I18n::loadLanguage(this->minecraft->platform(), langCodes[index]);
	this->minecraft->options.save();
	if (this->bHeader)
		this->bHeader->msg = I18n::get("mainmenu.language");
	if (this->bBack)
		this->bBack->msg = I18n::get("gui.back");
	if (this->bAdd)
		this->bAdd->msg = I18n::get("mainmenu.language.import");
}
