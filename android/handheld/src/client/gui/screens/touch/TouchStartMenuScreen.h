#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS_TOUCH__TouchStartMenuScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS_TOUCH__TouchStartMenuScreen_H__

#include "../../Screen.h"
#include "../../components/Button.h"
// 0.8.1 GUI 移植：0.8.1 触摸主菜单的 iOS 大按钮（BuyButton）
#include <gui/buttons/BuyButton.hpp>
struct ImageWithBackground;

namespace Touch {

// 0.8.1 GUI 移植：0.8.1 风格触摸主菜单（替换 1.7.10 还原版）
// 顶部游戏标题图 + 随机 splash 标语 + 触摸大按钮（Play / 纹理包 / 设置）
class StartMenuScreen: public Screen
{
public:
	static int currentSplash;

	StartMenuScreen();
	virtual ~StartMenuScreen();

	void init();
	void setupPositions();

	void tick();
	void render(int xm, int ym, float a);

	void buttonClicked(Button* button);
	bool handleBackEvent(bool isDown);
	bool isInGameScreen();
	// 09 · UI 覆盖系统：本屏逻辑名（mod 用 "mainmenu.<element>" 覆盖）
	const char* uiScreenId() const { return "mainmenu"; }
	// 皮肤预览纸娃娃：绘制在四按钮组右侧（无实体，仅 HumanoidModel）
	void renderSkinDoll(float mouseX, float mouseY);
	// 点击纸娃娃区域 → 打开文件选择器选皮肤(png/jpg)
	virtual void mouseClicked(int x, int y, int buttonNum);
private:
	void _updateLicense();
	static void chooseRandomSplash();
	void setupPlayButtons(bool realmsVisible);
	// 弹出系统文件对话框选 png/jpg; 选中后只记录路径, 延迟到 render() 开头
	// (渲染准备阶段, 与启动恢复同一时机)再应用 —— 避免在 mouseClicked/事件
	// 处理里做 GL 纹理替换导致 GUI 状态错乱(症状: 存档列表消失/贴图错)。
	void pickSkinFile();

	// 皮肤预览纸娃娃状态(无实体渲染, 不与 minecraft->player 关联)
	int _dollCenterX, _dollCenterY;   // 纸娃娃视觉中心(GUI 坐标)
	int _dollW, _dollH;               // 指针跟随判定区(纸娃娃可视外扩)
	float _dollYaw, _dollPitch;       // 平滑朝向(度)
	float _dollLastTime;
	bool _dollHasPointer;
	// 已选待应用皮肤路径(鼠标点击时记录, 下一帧 render 应用), "" = 无
	std::string _pendingSkinPath;

	TButton playButton;
	TButton playOnRealmsButton;
	TButton marketplaceButton;
	ImageWithBackground* settingsButtonMaybe;
	BuyButton buyButton;
	// 0.8.1 GUI 移植 + 用户定制：Mod 按钮（与 Game 同尺寸，放其下方）+ 语言按钮（与设置同尺寸，放 Mod 旁）
	TButton modButton;
	ImageWithBackground* languageButton;

	std::string field_138; // 版权（右上角）
	std::string field_13C; // 版本（右上角）
	int field_140;         // 标题图绘制矩形
	int field_144;
	int field_148;
	int field_14C;
	double field_150;      // splash 动画时间累计
};

extern char* gSplashes[];

};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS_TOUCH__TouchStartMenuScreen_H__*/
