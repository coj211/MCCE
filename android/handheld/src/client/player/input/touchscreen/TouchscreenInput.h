#ifndef NET_MINECRAFT_CLIENT_PLAYER_INPUT_TOUCHSCREEN_TouchscreenInput_H__
#define NET_MINECRAFT_CLIENT_PLAYER_INPUT_TOUCHSCREEN_TouchscreenInput_H__

//package net.minecraft.client.player;

#include "../IMoveInput.h"
#include "../../../gui/GuiComponent.h"
#include "TouchAreaModel.h"
#include "../../../renderer/RenderChunk.h"
#include <map>
#include <string>

class Options;
class Player;
class Minecraft;
class PolygonArea;
class Tesselator;

// @todo: extract a separate MoveInput (-> merge XperiaPlayInput)
class TouchscreenInput_TestFps:	public IMoveInput,
								public GuiComponent
{
public:
    static const int KEY_UP = 0;
    static const int KEY_DOWN = 1;
    static const int KEY_LEFT = 2;
    static const int KEY_RIGHT = 3;
    static const int KEY_JUMP = 4;
    static const int KEY_SNEAK = 5;
	static const int KEY_CRAFT = 6;
	static const int NumKeys = 7;

    TouchscreenInput_TestFps(Minecraft* mc, Options* options);
	~TouchscreenInput_TestFps();

	void onConfigChanged(const Config& c);

	void tick(Player* player);
	void render(float a);

	void setKey(int key, bool state);
    void releaseAllKeys();

	const RectangleArea& getRectangleArea();
    const RectangleArea& getPauseRectangleArea();

	// 所有可点击 HUD 按钮的矩形（返回实际写入 out 的个数）。
	// TouchInputHolder 拿它把按钮区从 UnifiedTurnBuild 的「转向区」里排除，
	// 否则按住按钮不动会被当成「按住转向区」→ 开始破坏方块。
	// 见 UnifiedTurnBuild::setControlAreas。
	int getControlAreas(RectangleArea* out, int maxCount) const;

	// 移动版专属摇杆：开启 Options::USE_TOUCH_JOYSTICK 时用它代替十字键。
	// 底座（空心圆）位置 = 原十字键的 3x3 区域，中心固定，手柄（实心圆）可拖、
	// 松手回中。
	bool isJoystickMode() const { return _useJoystick; }
	const RectangleArea& getJoystickArea() const { return _joystickArea; }
	int getJoystickPointer() const { return _useJoystick ? _joyPointer : -1; }

	// —— 模组自定义控件外观（UI.setControlStyle / UI.getControlRect）——
	// 控件用字符串 key 标识："jump" "sneak" "fly_up" "fly_down"
	// "dpad_up/down/left/right" "joystick"。模组可以改它们的位置/大小/隐藏。
	struct AreaInfo {
		std::string key;
		int areaId;
		AreaInfo() : areaId(0) {}
		AreaInfo(const char* k, int id) : key(k), areaId(id) {}
	};
	// 按 key 登记一个控件：位置/大小可能被模组覆盖；被隐藏时返回 NULL。
	RectangleArea* makeControl(const char* key, int areaId, float x0, float y0, float x1, float y1);
	// 把 key → 当前矩形（GUI 坐标）推给 ModEngine，供模组 UI.getControlRect 读。
	void publishControlRects();
	// 代替 drawRectangleArea：模组给这个控件设过样式就按样式画（圆/矩形/透明度）。
	void drawArea(Tesselator& t, RectangleArea* a, int ux, int vy, float ssz = 64.0f, bool flipV = false);
	std::map<const RectangleArea*, AreaInfo> _areaKeys;   // 区域 → (key, areaId)
	std::map<std::string, RectangleArea*>    _keyAreas;   // key → 区域

private:
	void clear();

	RectangleArea _boundingRectangle;

	bool _keys[NumKeys];
	Options* _options;

	bool _pressedJump;
	bool _forward;
	bool _northJump;
	bool _renderFlightImage;
	TouchAreaModel _model;
	Minecraft* _minecraft;

	RectangleArea* aLeft;
	RectangleArea* aRight;
	RectangleArea* aUp;
	RectangleArea* aDown;
	RectangleArea* aPause;
	//RectangleArea* aUpJump;
	RectangleArea* aJump;
	RectangleArea* aFlyUp;    // 飞行时：跳跃键正上方的上升键
	RectangleArea* aFlyDown;  // 飞行时：跳跃键正下方的下降键
	RectangleArea* aSneak;    // D-pad 正中间的潜行键
	RectangleArea* aUpLeft;
	RectangleArea* aUpRight;
	bool _pauseIsDown;

	RenderChunk _render;
	bool _allowHeightChange;
	float _sneakTapTime;

	bool _buttons[10];   // 索引 = areaId - AREA_DPAD_FIRST（升降键占 106/107，也要放得下）
	bool isButtonDown(int areaId);
	void rebuild();

	// —— 移动版专属：摇杆 ——
	void updateJoystick();
	void drawJoystick(Tesselator& t);

	bool _useJoystick;            // 本帧是否用摇杆（来自 Options）
	RectangleArea _joystickArea;  // 底座外接矩形
	float _joyCX, _joyCY;         // 底座中心
	float _joyRadius;             // 底座半径
	float _joyX, _joyY;           // 手柄偏移（归一化 -1..1，松手为 0）
	int   _joyPointer;            // 正在拖摇杆的指针 id（-1 = 未按）

	// 摇杆模式：双击摇杆切换潜行（该模式下没有潜行键）
	bool  _joySneak;              // 双击切出来的潜行状态
	float _joyTapTime;            // 上一次「点摇杆」的时刻（秒）；<=0 = 没在计时
	bool  _joyWasDown;            // 上一帧摇杆是否被按住（用来取按下沿）
};

#endif /*NET_MINECRAFT_CLIENT_PLAYER_INPUT_TOUCHSCREEN_TouchscreenInput_H__*/
