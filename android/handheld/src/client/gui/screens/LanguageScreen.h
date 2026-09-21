#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS__LanguageScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS__LanguageScreen_H__

#include "../Screen.h"
#include "../components/Button.h"

#include <string>
#include <vector>

class OptionsPane;
class OptionsItem;

// 语言选择屏 —— 设置页(OptionsScreen)同款结构与组件：
// 顶栏(标题/返回) + OptionsPane(设置页右侧的内容容器)；每个语言是
// OptionsPane 里的一个 OptionsItem 行 —— 行内左边语言自名、右边开关按钮
// (选中=开)，与设置页里的开关项外观/交互完全一致。
class LanguageScreen: public Screen
{
	typedef Screen super;
public:
	LanguageScreen();
	virtual ~LanguageScreen();

	virtual void init();
	virtual void setupPositions();
	virtual void render(int xm, int ym, float a);
	virtual void buttonClicked(Button* button);
	virtual void mouseClicked(int x, int y, int buttonNum);
	virtual void mouseReleased(int x, int y, int buttonNum);
	virtual bool renderGameBehind() { return true; }
	// 当前语言行是否为 index（开关按钮每帧查它决定 on/off 帧）
	bool isSelected(int index) const;
	// 点开关 → 切语言并立即生效（radio 单选：旧的自动变 off）
	void selectLanguage(int index);
	// 右上角"导入语言"：打开文件对话框选 .lang/.txt/.zip → 延迟到下一帧安装。
	void pickLanguageFile();
	// 安装一个语言包到 data/lang/<code>.lang（.lang/.txt 文本或内含 .lang 的
	// .zip）。成功后切到新语言并重建本屏。返回 true = 已重建。
	bool installLanguageFile(const std::string& path);

	Touch::THeader* bHeader;
	Touch::TButton* bBack;
	Touch::TButton* bAdd;              // 右上角"导入语言"
	OptionsPane* pane;                 // 设置页同款内容容器（持有行）
	std::vector<OptionsItem*> items;   // 每个语言一行（pane 拥有，仅记录指针供状态查询）
	// 语言码与自名("English"/"简体中文" —— 与 lang 文件 language.name 一致)
	std::vector<std::string> langCodes;
	std::vector<std::string> langNames;
	// 延迟到下一帧 render 开头处理的导入文件路径（对话框回调后立即写文件
	// 可能打断 GL 状态；与皮肤选图同一延迟策略）
	std::string _pendingImport;
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS__LanguageScreen_H__*/
