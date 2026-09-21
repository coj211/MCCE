#include "TouchscreenInput.h"
#include "../../../../mod/ModEngine.h"   // 模组注册的 HUD 按钮也要排除出转向区
#include "../../../Options.h"
#include "../../../../platform/input/Multitouch.h"
#include "../../../gui/Gui.h"
#include "../../../renderer/Tesselator.h"
#include "../../../../world/entity/player/Player.h"

#include "../../../Minecraft.h"
#include "../../../../platform/log.h"
#include "../../../renderer/Textures.h"
#include "../../../sound/SoundEngine.h"

static const int AREA_DPAD_FIRST = 100;
static const int AREA_DPAD_N = 100;
static const int AREA_DPAD_S = 101;
static const int AREA_DPAD_W = 102;
static const int AREA_DPAD_E = 103;
static const int AREA_DPAD_C = 104;
static const int AREA_PAUSE = 105;
// 飞行时贴在右侧跳跃键上/下方的**独立**升降键。
// 用它们就不再需要原来那套「先按住跳跃键、再按前进/后退来升降」的组合键 ——
// 那套组合键会让方向键在飞行中改图标、改作用（走路就变成升降），很难接受。
static const int AREA_FLY_UP = 106;
static const int AREA_FLY_DOWN = 107;
// D-pad 正中间的潜行键（跳跃键搬到右手边之后空出来的那格）
static const int AREA_SNEAK = 108;
// 摇杆模式下的摇杆命中区（代替 D-pad 四向）
static const int AREA_JOYSTICK = 109;

// 摇杆双击（切换潜行）的判定窗口，秒
static const float TapWindow = 0.30f;

static int cPressed = 0;
static int cReleased = 0;
static int cDiscreet = 0;
static int cPressedPause = 0;
static int cReleasedPause = 0;
//static const int AREA_DPAD_N_JUMP = 105;

//
// TouchscreenInput_TestFps
//

static void Copy(int n, float* x, float* y, float* dx, float* dy) {
	for (int i = 0; i < n; ++i) {
		dx[i] = x[i];
		dy[i] = y[i];
	}
}

static void Translate(int n, float* x, float* y, float xt, float yt) {
	for (int i = 0; i < n; ++i) {
		x[i] += xt;
		y[i] += yt;
	}
}

static void Scale(int n, float* x, float* y, float xt, float yt) {
	for (int i = 0; i < n; ++i) {
		x[i] *= xt;
		y[i] *= yt;
	}
}

static void Transformed(int n, float* x, float* y, float* dx, float* dy, float xt, float yt, float sx=1.0f, float sy=1.0f) {
	Copy(n, x, y, dx, dy);
	Scale(n, dx, dy, sx, sy);
	Translate(n, dx, dy, xt, yt);

	//for (int i = 0; i < n; ++i) {
	//	LOGI("%d. (%f, %f)\n", i, dx[i], dy[i]);
	//}
}

TouchscreenInput_TestFps::TouchscreenInput_TestFps( Minecraft* mc, Options* options )
:	_minecraft(mc),
	_options(options),
	_northJump(false),
	_forward(false),
	_boundingRectangle(0, 0, 1, 1),
	_pressedJump(false),
	_pauseIsDown(false),
	_sneakTapTime(-999),
	aLeft(0),
	aRight(0),
	aUp(0),
	aDown(0),
	aJump(0),
	aFlyUp(0),
	aFlyDown(0),
	aSneak(0),
	aUpLeft(0),
	aUpRight(0),
	_allowHeightChange(false),
	_useJoystick(false),
	_joystickArea(0, 0, 0, 0),
	_joyCX(0), _joyCY(0), _joyRadius(0),
	_joyX(0), _joyY(0),
	_joyPointer(-1),
	_joySneak(false),
	_joyTapTime(-1.0f),
	_joyWasDown(false)
{
	releaseAllKeys();
	onConfigChanged( createConfig(mc) );

	Tesselator& t = Tesselator::instance;
	const int alpha = 128;
	t.color( 0xc0c0c0, alpha); cPressed  = t.getColor();
	t.color( 0xffffff, alpha); cReleased = t.getColor();
	t.color( 0xffffff, alpha / 4); cDiscreet = t.getColor();
    t.color( 0xc0c0c0, 80); cPressedPause=t.getColor();
    t.color( 0xffffff, 80); cReleasedPause=t.getColor();
}

TouchscreenInput_TestFps::~TouchscreenInput_TestFps() {
	clear();
}

void TouchscreenInput_TestFps::clear() {
	_model.clear();

	// 区域对象由 _model 拥有（上一步 clear 已释放），这些指针必须一并置空：
	// 否则某次 onConfigChanged 没重建到的指针会变成悬垂指针，
	// getControlAreas() 会把已释放的内存当区域拷出去。
	aUp = aDown = aLeft = aRight = NULL;
	aJump = aFlyUp = aFlyDown = aSneak = NULL;

	delete aUpLeft; aUpLeft = NULL; // @todo: SAFEDEL
	delete aUpRight; aUpRight = NULL;
	// 模组控件表里的指针也跟着 _model 一起没了，别留着悬垂的 key
	_areaKeys.clear();
	_keyAreas.clear();
}

bool TouchscreenInput_TestFps::isButtonDown(int areaId) {
	return _buttons[areaId - AREA_DPAD_FIRST];
}


void TouchscreenInput_TestFps::onConfigChanged(const Config& c) {
	clear();

	const float w = (float)c.width;
	const float h = (float)c.height;

	/*
	// Code for "Move when touching left side of the screen"
	float x0[] = {  0,  w * 0.3f,  w * 0.3f,     0 };
	float y0[] = {	0,	       0,      h-32,  h-32 };

	_model.addArea(AREA_MOVE, new RectangleArea(0, 0, w*0.3f, h-32));
	*/

	// Code for "D-pad with jump in center"
	float Bw = w * 0.11f;//0.08f;
	float Bh = Bw;//0.15f;
    
    // If too large (like playing on Tablet)
    PixelCalc& pc = _minecraft->pixelCalc;
    if (pc.pixelsToMillimeters(Bw) > 14) {
        Bw = Bh = pc.millimetersToPixels(14);
    }
	// temp data
	float xx;
	float yy;

	const float BaseY = -8 + h - 3.0f * Bh;
	const float BaseX = _options->isLeftHanded? -8 + w - 3 * Bw
											:	8 + 0;
	// Setup the bounding rectangle
	_boundingRectangle = RectangleArea(BaseX, BaseY, BaseX + 3 * Bw, BaseY + 3 * Bh);

	// 移动版专属：摇杆模式 —— 底座占掉原来整个 3x3 十字键的位置，D-pad 四向不创建。
	// 跳跃 / 飞行升降键位置不变；潜行键原本在底座中心，改挪到跳跃键左边一格。
	_useJoystick = _options->useTouchJoystick;
	if (_useJoystick) {
		// 底座圆心 = 原十字键 3x3 的中心（固定不动），但半径比原来小很多：
		// 原来用 1.5*Bw ≈ 378px，直径占了全屏高的 70%，太夸张。
		// 命中区跟着图走（手指要落在圆上才接管摇杆）；_boundingRectangle 仍是
		// 整块 3x3，继续作为「排除转向区」的范围（比圆大一点更安全）。
		const float backR = 0.6f * Bw;   // ≈ 151px，直径≈屏高的 28%
		_joyCX = BaseX + 1.5f * Bw;
		_joyCY = BaseY + 1.5f * Bh;
		_joyRadius = backR;
		_joystickArea = RectangleArea(_joyCX - backR, _joyCY - backR,
		                              _joyCX + backR, _joyCY + backR);
		_joyX = _joyY = 0;
		_joyPointer = -1;
		_model.addArea(AREA_JOYSTICK, new RectangleArea(_joystickArea));

		xx = w - BaseX - 2 * Bw; yy = BaseY + Bh;
		aJump = makeControl("jump", AREA_DPAD_C, xx, yy, xx + Bw, yy + Bh);
		xx = w - BaseX - 2 * Bw; yy = BaseY;
		aFlyUp = makeControl("fly_up", AREA_FLY_UP, xx, yy, xx + Bw, yy + Bh);
		xx = w - BaseX - 2 * Bw; yy = BaseY + 2 * Bh;
		aFlyDown = makeControl("fly_down", AREA_FLY_DOWN, xx, yy, xx + Bw, yy + Bh);
		publishControlRects();
		// 潜行：摇杆模式不放潜行键，改成双击摇杆切换（见 tick 里的 _joySneak）
		return;
	}

	xx = BaseX + Bw; yy = BaseY;
	aUp = makeControl("dpad_up", AREA_DPAD_N, xx, yy, xx + Bw, yy + Bh);
	xx = BaseX;
	aUpLeft = new RectangleArea(xx, yy, xx+Bw, yy+Bh);
	xx = BaseX + 2 * Bw;
	aUpRight = new RectangleArea(xx, yy, xx+Bw, yy+Bh);

	xx = BaseX + Bw; yy = BaseY + Bh;
	// 跳跃键（飞行时同一格换成飞行图标，用的还是这个 aJump）不再放 D-pad 中心，
	// 而是挪到屏幕另一侧、与原先位置**左右对称**的地方：左手管方向、右手跳。
	// 右手模式（isLeftHanded）会自动镜像到左边。
	// 必须保持注册在 AREA_TURN（全屏减去各控件区）之前 —— TouchAreaModel::
	// getPointerId 按注册顺序取第一个命中，D-pad 这几个区先注册才不会被转向区吃掉。
	xx = w - BaseX - 2 * Bw; yy = BaseY + Bh;
	aJump = makeControl("jump", AREA_DPAD_C, xx, yy, xx + Bw, yy + Bh);

	// 潜行键：D-pad 正中间（跳跃键搬到右手边之后空出来的那格）
	xx = BaseX + Bw; yy = BaseY + Bh;
	aSneak = makeControl("sneak", AREA_SNEAK, xx, yy, xx + Bw, yy + Bh);

	// 飞行时用的独立升降键：贴在跳跃键正上/正下方，右手拇指不用移开就能升降。
	// 注册顺序同样要排在 AREA_TURN（TurnBuild 的全屏转向区）之前。
	xx = w - BaseX - 2 * Bw; yy = BaseY;
	aFlyUp = makeControl("fly_up", AREA_FLY_UP, xx, yy, xx + Bw, yy + Bh);
	xx = w - BaseX - 2 * Bw; yy = BaseY + 2 * Bh;
	aFlyDown = makeControl("fly_down", AREA_FLY_DOWN, xx, yy, xx + Bw, yy + Bh);

	LOGI("[touch] layout: w=%g baseX=%g Bw=%g Bh=%g | jump.x=%g flyUp.y=%g flyDown.y=%g\n",
	     (double)w, (double)BaseX, (double)Bw, (double)Bh,
	     (double)(w - BaseX - 2 * Bw), (double)BaseY, (double)(BaseY + 2 * Bh));

	xx = BaseX + Bw; yy = BaseY + 2 * Bh;
	aDown = makeControl("dpad_down", AREA_DPAD_S, xx, yy, xx + Bw, yy + Bh);

	xx = BaseX; yy = BaseY + Bh;
	aLeft = makeControl("dpad_left", AREA_DPAD_W, xx, yy, xx + Bw, yy + Bh);

	xx = BaseX + 2 * Bw; yy = BaseY + Bh;
	aRight = makeControl("dpad_right", AREA_DPAD_E, xx, yy, xx + Bw, yy + Bh);

#ifdef __APPLE__
    float maxPixels = _minecraft->pixelCalc.millimetersToPixels(10);
    float btnSize = Mth::Min(18 * Gui::GuiScale, maxPixels);
	_model.addArea(AREA_PAUSE, aPause = new RectangleArea(w - 4 - btnSize,
                                                          4,
                                                          w - 4,
                                                          4 + btnSize));
#endif /* __APPLE__ */

	// 把控件当前矩形告诉模组（UI.getControlRect）
	publishControlRects();

	//rebuild();
}

void TouchscreenInput_TestFps::setKey( int key, bool state )
{
	#ifdef WIN32
        //LOGI("key: %d, %d\n", key, state);

		int id = -1;
		if (key == _options->keyUp.key) id = KEY_UP;
		if (key == _options->keyDown.key) id = KEY_DOWN;
		if (key == _options->keyLeft.key) id = KEY_LEFT;
		if (key == _options->keyRight.key) id = KEY_RIGHT;
		if (key == _options->keyJump.key) id = KEY_JUMP;
		if (key == _options->keySneak.key) id = KEY_SNEAK;
		if (key == _options->keyCraft.key) id = KEY_CRAFT;
		if (id >= 0) {
			_keys[id] = state;
		}
	#endif
}

void TouchscreenInput_TestFps::releaseAllKeys()
{
	xa = 0;
	ya = 0;

	for (int i = 0; i<10; ++i)
		_buttons[i] = false;
#ifdef WIN32
	for (int i = 0; i<NumKeys; ++i)
		_keys[i] = false;
#endif
	_pressedJump = false;
	_allowHeightChange = false;
	// 摇杆状态一并复位（换世界/重生后不要残留拆杆和潜行）
	_joyPointer = -1;
	_joyX = _joyY = 0;
	_joySneak = false;
	_joyTapTime = -1.0f;
	_joyWasDown = false;
	_areaKeys.clear();
	_keyAreas.clear();
}

void TouchscreenInput_TestFps::tick( Player* player )
{
	xa = 0;
	ya = 0;
	jumping = false;

	// 每帧先清空按钮状态。
	// 原来的写法只对「当前还有指针的区域」赋值，而手指抬起那一帧指针已经不在
	// Multitouch 的活动列表里了，于是再没人把它写回 false —— 按钮就永远卡在
	// 按下状态：按一次右侧的升降键会一直往上/下飞，方向键也一直被当成「正在
	// 升降」而变暗/换图标。这里每帧清零，按住的指针随后会重新置位。
	for (int i = 0; i < 10; ++i)
		_buttons[i] = false;

	//bool gotEvent = false;
	bool heldJump = false;
	bool tmpForward = false;
	bool tmpNorthJump = false;

	for (int i = 0; i < 6; ++i)
		_buttons[i] = false;

	// 移动版专属：摇杆模式先更新摇杆，再把偏移变成移动量。右侧的跳跃/升降/
	// 潜行键在两种模式下都存在，仍由下面同一个指针循环处理。
	if (_useJoystick) {
		updateJoystick();

		// 双击摇杆 = 切换潜行（摇杆模式下没有潜行键）。只认「按下沿」，
		// 两次点击间隔必须在 TapWindow 秒内，否则重新计时。
		const bool joyDown = (_joyPointer >= 0);
		if (joyDown && !_joyWasDown) {
			const float now = getTimeS();
			if (_joyTapTime > 0.0f && (now - _joyTapTime) <= TapWindow) {
				_joySneak = !_joySneak;
				_joyTapTime = -1.0f;
			} else {
				_joyTapTime = now;
			}
		}
		_joyWasDown = joyDown;

		const float dead = 0.18f;   // 死区：手指犄动不走路
		const float jx = (_joyX > -dead && _joyX < dead) ? 0.0f : _joyX;
		const float jy = (_joyY > -dead && _joyY < dead) ? 0.0f : _joyY;
		// 符号与 D-pad 保持一致：屏幕上方 = 前进（ya 正）、屏幕右侧 = 右移（xa 负）
		ya = -jy;
		xa = -jx;
	}

	const int* pointerIds;
	int pointerCount = Multitouch::getActivePointerIdsThisUpdate(&pointerIds);
	for (int i = 0; i < pointerCount; ++i) {
		int p = pointerIds[i];
		int x = Multitouch::getX(p);
		int y = Multitouch::getY(p);

		if (_boundingRectangle.isInside((float)x, (float)y) && _forward && !isChangingFlightHeight)
		{
			float angle = Mth::PI + Mth::atan2(y - _boundingRectangle.centerY(), x - _boundingRectangle.centerX());
			ya = Mth::sin(angle);
			xa = Mth::cos(angle);
			tmpForward = true;
		}

		int areaId = _model.getPointerId(x, y, p);
		if (areaId < AREA_DPAD_FIRST)
		{
			continue;
		}

		// 非飞行时右侧那对升降键根本不存在（也不显示）：直接跳过这次命中，
		// 当作“屏幕上这里没有按钮”。UnifiedTurnBuild 的转向区覆盖这一带，
		// 所以手指落在这里会正常转视角 —— 而不是去触发跳跃。
		if (!player->abilities.flying &&
		    (areaId == AREA_FLY_UP || areaId == AREA_FLY_DOWN))
		{
			continue;
		}

		bool setButton = false;

		if (Multitouch::isPressed(p))
			_allowHeightChange = (areaId == AREA_DPAD_C);

        if (areaId == AREA_DPAD_C)
		{
			setButton = true;
			heldJump = true;
			// If we're in water or pressed down on the button: jump
			if (player->isInWater()) {
				jumping = true;
			}
			else if (Multitouch::isPressed(p)) {
				jumping = true;
			}
			// （这里原来还有一条：按住跳跃键 + 前进就把 areaId 改成 AREA_DPAD_N，
			//  让跳跃键“变成”前进键。跳跃键搬到右侧后这会让右手按的跳跃键去点
			//  亮左侧的上键，已去掉 —— 跳跃归跳跃，前进靠方向键。）
		}

		if (areaId == AREA_FLY_UP || areaId == AREA_FLY_DOWN)
		{
			// 右侧那对独立升降键（只有飞行时才会走到这里）：按住就置位
			setButton = true;
		}

		if (areaId == AREA_SNEAK)
		{
			// D-pad 中间的潜行键：按住即潜行
			setButton = true;
		}

		if	(areaId == AREA_DPAD_N)
		{
			setButton = true;
			if (player->isInWater())
				jumping = true;
			else if (!isChangingFlightHeight)
				tmpForward = true;
			ya += 1;
		}
		else if (areaId == AREA_DPAD_S && !_forward)
		{
			setButton = true;
            ya -= 1;
			/*
            if (Multitouch::isReleased(p)) {
                float now = getTimeS();
                if (now - _sneakTapTime < 0.4f) {
                    ya += 1;
                    sneaking = !sneaking;
                    player->setSneaking(sneaking);
                    _sneakTapTime = -1;
                } else {
                    _sneakTapTime = now;
                }
            }
			*/
        }
		else if (areaId == AREA_DPAD_W && !_forward)
		{
			setButton = true;
			xa += 1;
		}
		else if (areaId == AREA_DPAD_E && !_forward)
		{
			setButton = true;
			xa -= 1;
		}
#ifdef __APPLE__
		else if (areaId == AREA_PAUSE) {
			if (Multitouch::isReleased(p)) {
                _minecraft->soundEngine->playUI("random.click", 1, 1);
				_minecraft->screenChooser.setScreen(SCREEN_PAUSE);
            }
		}
#endif /*__APPLE__*/
		_buttons[areaId - AREA_DPAD_FIRST] = setButton;
	}

	_forward = tmpForward;

	// Only jump once at a time
	if (tmpNorthJump) {
		if (!_northJump)
			jumping = true;
		_northJump = true;
	}
	else _northJump = false;

	isChangingFlightHeight = false;
	// 飞行升降**只认**右侧那对独立升降键（方向键在飞行中始终是前进/后退）。
	// 注意：这里**不再**拿 wantUp/wantDown 去设置 isChangingFlightHeight —— 那个
	// 标志会让方向键换图标/变暗，还会把 ya 清零（按着升降键就走不了路）。
	// 现在升降键纯粋只升降，不碰其它任何东西。
	wantUp   = isButtonDown(AREA_FLY_UP);
	wantDown = isButtonDown(AREA_FLY_DOWN);
	_renderFlightImage = player->abilities.flying;

	// 临时诊断：只在状态变化时打印，不刷屏
	{
		static int sPU = -1, sPD = -1, sPF = -1, sPMode = -1;
		const int cU = (int)isButtonDown(AREA_FLY_UP);
		const int cD = (int)isButtonDown(AREA_FLY_DOWN);
		const int cF = (int)player->abilities.flying;
		const int cM = (int)isChangingFlightHeight;
		if (cU != sPU || cD != sPD || cF != sPF || cM != sPMode) {
			LOGI("[touch] FLYUP=%d FLYDOWN=%d flying=%d heightMode=%d | wantUp=%d wantDown=%d\n",
			     cU, cD, cF, cM, (int)wantUp, (int)wantDown);
			sPU = cU; sPD = cD; sPF = cF; sPMode = cM;
		}
	}

#ifdef WIN32
	if (_keys[KEY_UP]) ya++;
	if (_keys[KEY_DOWN]) ya--;
	if (_keys[KEY_LEFT]) xa++;
	if (_keys[KEY_RIGHT]) xa--;
	if (_keys[KEY_JUMP]) jumping = true;
	//sneaking = _keys[KEY_SNEAK];
	if (_keys[KEY_CRAFT])
		player->startCrafting((int)player->x, (int)player->y, (int)player->z, Recipe::SIZE_2X2);
#endif

	// 潜行：十字键模式按住潜行键；摇杆模式双击摇杆切换
	{
		const bool wantSneak = _useJoystick ? _joySneak : isButtonDown(AREA_SNEAK);
		if (wantSneak != sneaking) {
			sneaking = wantSneak;
			if (player) player->setSneaking(sneaking);
		}
	}

	if (sneaking) {
		xa *= 0.3f;
		ya *= 0.3f;
	}
	//printf("\n>- %f %f\n", xa, ya);
	_pressedJump = heldJump;
}

static void drawRectangleArea(Tesselator& t, RectangleArea* a, int ux, int vy, float ssz = 64.0f, bool flipV = false) {
	const float pm = 1.0f / 256.0f;
	const float sz = ssz * pm;
	const float uu = (float)(ux) * pm;
	const float vv = (float)(vy) * pm;
	// flipV：上下翻转这个图集格。图集里没有独立的“上升”图标，就用“下降”翻转。
	const float vTop = flipV ? vv + sz : vv;
	const float vBot = flipV ? vv : vv + sz;
	const float x0 = a->_x0 * Gui::InvGuiScale;
	const float x1 = a->_x1 * Gui::InvGuiScale;
	const float y0 = a->_y0 * Gui::InvGuiScale;
	const float y1 = a->_y1 * Gui::InvGuiScale;

	t.vertexUV(x0, y1, 0, uu,	vBot);
	t.vertexUV(x1, y1, 0, uu+sz,vBot);
	t.vertexUV(x1, y0, 0, uu+sz,vTop);
	t.vertexUV(x0, y0, 0, uu,	vTop);
}

// ── 模组自定义控件外观（UI.setControlStyle）────────────────
// 按模组样式画控件：shape 1 = 矩形（填充 + 边框），2 = 圆环 + 中心实心。
// 透明度用样式里的 opacity（按下时略微提亮）。画完把贴图状态恢复回去，
// 不然紧接着画的下一个控件会糊成一片。
static void drawStyledShapeT(Tesselator& t, RectangleArea* a, int shape,
                             unsigned int fill, unsigned int border, float borderW, float opacity)
{
	const float inv = Gui::InvGuiScale;
	const float x0 = a->_x0 * inv, y0 = a->_y0 * inv;
	const float x1 = a->_x1 * inv, y1 = a->_y1 * inv;
	const float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f;

	float fa = ((fill   >> 24) & 0xff) / 255.0f;
	float fr = ((fill   >> 16) & 0xff) / 255.0f;
	float fg = ((fill   >>  8) & 0xff) / 255.0f;
	float fb = ( fill          & 0xff) / 255.0f;
	float ba = ((border >> 24) & 0xff) / 255.0f;
	float br = ((border >> 16) & 0xff) / 255.0f;
	float bg = ((border >>  8) & 0xff) / 255.0f;
	float bb = ( border        & 0xff) / 255.0f;
	if (opacity < 0.0f) opacity = 0.0f;
	if (opacity > 1.0f) opacity = 1.0f;
	fa *= opacity;
	ba *= opacity;

	const int seg = 32;
	const float step = 6.2831853f / (float)seg;

	if (shape == 2) {
		const float R = (y1 - y0) * 0.5f;
		if (R <= 0.5f) return;
		float ring = (borderW > 0.0f) ? borderW : 2.0f;
		if (ring > R * 0.5f) ring = R * 0.5f;
		const float inner = R - ring;
		if (ba > 0.002f) {
			t.color(br, bg, bb, ba);
			for (int i = 0; i < seg; ++i) {
				const float s0 = i * step, s1 = s0 + step;
				t.vertex(cx + inner * Mth::cos(s0), cy + inner * Mth::sin(s0), 0);
				t.vertex(cx + inner * Mth::cos(s1), cy + inner * Mth::sin(s1), 0);
				t.vertex(cx + R     * Mth::cos(s1), cy + R     * Mth::sin(s1), 0);
				t.vertex(cx + R     * Mth::cos(s0), cy + R     * Mth::sin(s0), 0);
			}
		}
		if (fa > 0.002f) {
			t.color(fr, fg, fb, fa);
			for (int i = 0; i < seg; ++i) {
				const float s0 = i * step, s1 = s0 + step;
				t.vertex(cx, cy, 0);
				t.vertex(cx + inner * Mth::cos(s1), cy + inner * Mth::sin(s1), 0);
				t.vertex(cx + inner * Mth::cos(s0), cy + inner * Mth::sin(s0), 0);
				t.vertex(cx, cy, 0);
			}
		}
	} else {
		if (fa > 0.002f) {
			t.color(fr, fg, fb, fa);
			t.vertex(x0, y1, 0); t.vertex(x1, y1, 0);
			t.vertex(x1, y0, 0); t.vertex(x0, y0, 0);
		}
		if (ba > 0.002f) {
			float bw = (borderW > 0.0f) ? borderW : 1.5f;
			if (bw > (x1 - x0) * 0.5f) bw = (x1 - x0) * 0.5f;
			t.color(br, bg, bb, ba);
			t.vertex(x0,      y1,      0); t.vertex(x1,      y1,      0);
			t.vertex(x1,      y1 - bw, 0); t.vertex(x0,      y1 - bw, 0);
			t.vertex(x0,      y0 + bw, 0); t.vertex(x1,      y0 + bw, 0);
			t.vertex(x1,      y0,      0); t.vertex(x0,      y0,      0);
			t.vertex(x0,      y1,      0); t.vertex(x0 + bw, y1,      0);
			t.vertex(x0 + bw, y0,      0); t.vertex(x0,      y0,      0);
			t.vertex(x1 - bw, y1,      0); t.vertex(x1,      y1,      0);
			t.vertex(x1,      y0,      0); t.vertex(x1 - bw, y0,      0);
		}
	}
}

void TouchscreenInput_TestFps::drawArea(Tesselator& t, RectangleArea* a, int ux, int vy, float ssz, bool flipV)
{
	if (!a) return;
	ModEngine* me = ModEngine::instance;
	const ModEngine::ControlStyle* st = NULL;
	bool pressed = false;
	if (me) {
		std::map<const RectangleArea*, AreaInfo>::const_iterator it = _areaKeys.find(a);
		if (it != _areaKeys.end()) {
			st = me->controlStyle(it->second.key);
			pressed = isButtonDown(it->second.areaId);
		}
	}
	if (st && st->used) {
		float op = st->hasOpacity ? st->opacity : 1.0f;
		if (pressed) op = (op > 0.8f) ? 1.0f : (op + 0.25f);
		if (st->shape == 0) {
			// 图案还是原版贴图，只把透明度换掉
			t.color(1.0f, 1.0f, 1.0f, op);
			drawRectangleArea(t, a, ux, vy, ssz, flipV);
			glColor4f2(1, 1, 1, 1);
			return;
		}
		// 纯色图形必须和贴图顶点分开批次：Tesselator 的 hasTexture 是整批生效的，
		// 混在一起会让图形也按 UV=(0,0) 采样 gui.png 左上角的透明像素 ——
		// 结果就是「控件画了但却看不见，只是还能点」（跳跃键那个 bug）。
		// 先用当前状态提交已累积的贴图顶点，再单独开一批画圆/矩形。
		t.draw();
		glDisable2(GL_CULL_FACE);
		glDisable2(GL_DEPTH_TEST);
		glDisable2(GL_TEXTURE_2D);
		glEnable2(GL_BLEND);
		glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		t.begin();
		drawStyledShapeT(t, a, st->shape, st->fill, st->border, st->borderWidth, op);
		t.draw();
		glEnable2(GL_TEXTURE_2D);
		glColor4f2(1, 1, 1, 1);
		t.begin();        // 给后续贴图控件重新开一个批次
		return;
	}
	drawRectangleArea(t, a, ux, vy, ssz, flipV);
}

// 按 key 登记一个控件：模组可能改了位置/大小，或直接隐藏它。
RectangleArea* TouchscreenInput_TestFps::makeControl(const char* key, int areaId,
                                                     float x0, float y0, float x1, float y1)
{
	RectangleArea* a = new RectangleArea(x0, y0, x1, y1);
	bool hidden = false;
	ModEngine* me = ModEngine::instance;
	if (me) {
		const ModEngine::ControlStyle* st = me->controlStyle(key);
		if (st && st->used) {
			hidden = st->hidden;
			if (st->hasRect) {
				// 模组用 GUI 坐标，控件区域是物理像素
				const float s = 1.0f / Gui::InvGuiScale;
				a->_x0 = st->x * s;
				a->_y0 = st->y * s;
				a->_x1 = (st->x + st->w) * s;
				a->_y1 = (st->y + st->h) * s;
			}
		}
	}
	if (hidden) {
		delete a;
		return NULL;      // 隐藏：不注册进命中表，也不绘制、不挡触摸
	}
	_areaKeys[a] = AreaInfo(key, areaId);
	_keyAreas[key] = a;
	_model.addArea(areaId, a);
	return a;
}

// 把控件当前矩形（GUI 坐标）推给 ModEngine，供模组 UI.getControlRect 读。
void TouchscreenInput_TestFps::publishControlRects()
{
	ModEngine* me = ModEngine::instance;
	if (!me) return;
	const float s = Gui::InvGuiScale;      // 物理 → GUI
	for (std::map<std::string, RectangleArea*>::iterator it = _keyAreas.begin(); it != _keyAreas.end(); ++it) {
		RectangleArea* a = it->second;
		if (!a) {
			me->setControlRect(it->first, 0, 0, 0, 0, false);
			continue;
		}
		me->setControlRect(it->first, a->_x0 * s, a->_y0 * s,
		                   (a->_x1 - a->_x0) * s, (a->_y1 - a->_y0) * s, true);
	}
}

static void drawPolygonArea(Tesselator& t, PolygonArea* a, int x, int y) {
	float pm = 1.0f / 256.0f;
	float sz = 64.0f * pm;
	float uu = (float)(x) * pm;
	float vv = (float)(y) * pm;

	float uvs[] = {uu, vv, uu+sz, vv, uu+sz, vv+sz, uu, vv+sz};
	const int o = 0;

	for (int j = 0; j < a->_numPoints; ++j) {
		t.vertexUV(a->_x[j] * Gui::InvGuiScale, a->_y[j] * Gui::InvGuiScale, 0, uvs[(o+j+j)&7], uvs[(o+j+j+1)&7]);
	}
}

void TouchscreenInput_TestFps::render( float a ) {
	//return;

	//static Stopwatch sw;
	//sw.start();


	//glColor4f2(1, 0, 1, 1.0f);
	//glDisable2(GL_CULL_FACE);
	glDisable2(GL_ALPHA_TEST);

	glEnable2(GL_BLEND);
	glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	_minecraft->textures->loadAndBindTexture("gui/gui.png");
	
	//glDisable2(GL_TEXTURE_2D);

	rebuild();
	//drawArrayVTC(_bufferId, 5 * 2 * 3, 24);

	glDisable2(GL_BLEND);
	//glEnable2(GL_TEXTURE_2D);
	//glEnable2(GL_CULL_FACE);

	//sw.stop();
	//sw.printEvery(100, "buttons");
}

const RectangleArea& TouchscreenInput_TestFps::getRectangleArea()
{
	return _boundingRectangle;
}
const RectangleArea& TouchscreenInput_TestFps::getPauseRectangleArea()
{
    return *aPause;
}

// 把所有已创建的 HUD 按钮矩形拷给调用方（定长数组，避免动态分配）。
// 只列真正注册/生效的按钮；aPause 只有 __APPLE__ 才创建，且不在这里列。
int TouchscreenInput_TestFps::getControlAreas(RectangleArea* out, int maxCount) const
{
	RectangleArea* const list[] = {
		aUp, aDown, aLeft, aRight, aJump, aFlyUp, aFlyDown, aSneak
	};
	const int listCount = (int)(sizeof(list) / sizeof(list[0]));
	int count = 0;
	for (int i = 0; i < listCount && count < maxCount; ++i)
		if (list[i] != NULL)
			out[count++] = *list[i];
	// 摇杆模式：底座区域也排除（它同时就是 moveArea，这里再保险一层）
	if (_useJoystick && count < maxCount)
		out[count++] = _joystickArea;
	// 模组注册的 HUD 按钮 / 按键映射按钮（GUI 坐标 → 物理像素）同样要排除：
	// 不排除的话，按着按钮会被 UnifiedTurnBuild 当成「按住转向区」——
	// 轻则转视角，按住 400ms 还会开始挖方块。模组改动布局时会重建布局。
	if (ModEngine::instance) {
		const float s = 1.0f / Gui::InvGuiScale;
		const std::vector<ModEngine::UiHudKey>& hk = ModEngine::instance->hudKeys();
		for (size_t i = 0; i < hk.size() && count < maxCount; ++i)
			out[count++] = RectangleArea(hk[i].x * s, hk[i].y * s,
			                             (hk[i].x + hk[i].w) * s, (hk[i].y + hk[i].h) * s);
		const std::vector<ModEngine::UiHudButton>& hb = ModEngine::instance->hudButtons();
		for (size_t i = 0; i < hb.size() && count < maxCount; ++i)
			out[count++] = RectangleArea(hb[i].x * s, hb[i].y * s,
			                             (hb[i].x + hb[i].w) * s, (hb[i].y + hb[i].h) * s);
	}
	return count;
}

// ─────────────────────────────────────────────────────────────
// 移动版专属：摇杆
// ─────────────────────────────────────────────────────────────

// 把某根手指的位置换算成摇杆偏移（超出底座半径就钳在边缘）。
static void joystickOffset(float x, float y, float cx, float cy, float radius,
                           float& outX, float& outY)
{
	float dx = x - cx;
	float dy = y - cy;
	const float r = sqrtf(dx * dx + dy * dy);
	if (r > radius && r > 0.0001f) {
		const float s = radius / r;
		dx *= s;
		dy *= s;
	}
	outX = (radius > 0.0f) ? (dx / radius) : 0.0f;
	outY = (radius > 0.0f) ? (dy / radius) : 0.0f;
}

// 每帧更新摇杆状态：已经按住的指针跟着走，松手就回中心。
void TouchscreenInput_TestFps::updateJoystick()
{
	if (_joyRadius <= 0.0f) {
		_joyX = _joyY = 0;
		_joyPointer = -1;
		return;
	}

	const int* ids;
	const int count = Multitouch::getActivePointerIdsThisUpdate(&ids);

	// 已经在拖的那根手指
	if (_joyPointer >= 0) {
		for (int i = 0; i < count; ++i) {
			if (ids[i] != _joyPointer)
				continue;
			joystickOffset((float)Multitouch::getX(_joyPointer),
			               (float)Multitouch::getY(_joyPointer),
			               _joyCX, _joyCY, _joyRadius, _joyX, _joyY);
			return;
		}
		// 手指抬了：手柄弹回底座中心
		_joyPointer = -1;
		_joyX = _joyY = 0;
		return;
	}

	// 找一根落在底座上的手指接管摇杆（按下点就是移动方向 = 手柄位置）
	for (int i = 0; i < count; ++i) {
		const int p = ids[i];
		const float x = (float)Multitouch::getX(p);
		const float y = (float)Multitouch::getY(p);
		if (!_joystickArea.isInside(x, y))
			continue;
		_joyPointer = p;
		joystickOffset(x, y, _joyCX, _joyCY, _joyRadius, _joyX, _joyY);
		return;
	}
}

// 画摇杆：底座是空心圆环（位置固定），手柄是实心圆（跟着手指，松手回中心）。
// 用纯色三角形画（GLES1 下没有画圆的原语），所以要临时关掉贴图。
void TouchscreenInput_TestFps::drawJoystick(Tesselator& t)
{
	const float inv = Gui::InvGuiScale;
	const float cx = _joyCX * inv;
	const float cy = _joyCY * inv;
	const float R  = _joyRadius * inv;

	if (R <= 1.0f)
		return;

	// 线条变细：环厚 ≈ 6% 半径（原来 14%，看着像甜甜圈）
	float ringW = R * 0.06f;
	if (ringW < 2.5f)
		ringW = 2.5f;
	const float ringInner = R - ringW;
	const float handleR   = R * 0.30f;   // 实心手柄半径
	const float hx = cx + _joyX * (R - handleR);
	const float hy = cy + _joyY * (R - handleR);
	const bool active = (_joyPointer >= 0);

	// GUI 这块区域可能还带着 3D 的剔除/深度状态，画 2D 圆先关掉，画完恢复。
	const GLboolean cullWas  = glIsEnabled(GL_CULL_FACE);
	const GLboolean depthWas = glIsEnabled(GL_DEPTH_TEST);
	glDisable2(GL_CULL_FACE);
	glDisable2(GL_DEPTH_TEST);
	glDisable2(GL_TEXTURE_2D);
	glEnable2(GL_BLEND);
	glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4f2(1, 1, 1, 1);   // 顶点色可用时的兼底

	const int seg = 40;
	const float step = 6.2831853f / (float)seg;

	t.begin();
	// 底座：空心圆环（每段一个四边形，绕向与 drawRectangleArea 一致 ——
	// 不然一旦 GUI 阶段开着 GL_CULL_FACE 就会被整块剔掉）
	t.color(0xffffff, active ? 0x78 : 0x50);
	for (int i = 0; i < seg; ++i) {
		const float a0 = i * step;
		const float a1 = a0 + step;
		const float c0 = Mth::cos(a0), s0 = Mth::sin(a0);
		const float c1 = Mth::cos(a1), s1 = Mth::sin(a1);
		t.vertex(cx + ringInner * c0, cy + ringInner * s0, 0);
		t.vertex(cx + ringInner * c1, cy + ringInner * s1, 0);
		t.vertex(cx + R         * c1, cy + R         * s1, 0);
		t.vertex(cx + R         * c0, cy + R         * s0, 0);
	}
	// 手柄：实心圆（每段一个退化四边形：中心 + 弧两点 + 中心）
	t.color(0xffffff, active ? 0xd8 : 0xa8);
	for (int i = 0; i < seg; ++i) {
		const float a0 = i * step;
		const float a1 = a0 + step;
		t.vertex(hx, hy, 0);
		t.vertex(hx + handleR * Mth::cos(a1), hy + handleR * Mth::sin(a1), 0);
		t.vertex(hx + handleR * Mth::cos(a0), hy + handleR * Mth::sin(a0), 0);
		t.vertex(hx, hy, 0);
	}
	t.draw();

	glEnable2(GL_TEXTURE_2D);
	if (cullWas)  glEnable2(GL_CULL_FACE);
	if (depthWas) glEnable2(GL_DEPTH_TEST);
}

void TouchscreenInput_TestFps::rebuild() {
    if (_options->hideGui)
        return;
    
	Tesselator& t = Tesselator::instance;
	//LOGI("instance is: %p, %p, %p, %p, %p FOR %d\n", &t, aLeft, aRight, aUp, aDown, aJump, _bufferId);
	//t.setAccessMode(Tesselator::ACCESS_DYNAMIC);
	t.begin();

	const int imageU = 0;
	const int imageV = 107;
	const int imageSize = 26;

	bool northDiagonals = !isChangingFlightHeight && (_northJump || _forward);

	if (!_useJoystick) {

	// render left button
	if (northDiagonals || isChangingFlightHeight) t.colorABGR(cDiscreet);
    else if (isButtonDown(AREA_DPAD_W)) t.colorABGR(cPressed);
	else						   t.colorABGR(cReleased);
	drawArea(t, aLeft, imageU + imageSize, imageV, (float)imageSize);

	// render right button
	if (northDiagonals || isChangingFlightHeight) t.colorABGR(cDiscreet);
	else if (isButtonDown(AREA_DPAD_E)) t.colorABGR(cPressed);
	else						   t.colorABGR(cReleased);
	drawArea(t, aRight, imageU + imageSize * 3, imageV, (float)imageSize);

	// render forward button
	if (isButtonDown(AREA_DPAD_N)) t.colorABGR(cPressed);
	else						   t.colorABGR(cReleased);
	if (isChangingFlightHeight)
	{
		drawArea(t, aUp, imageU + imageSize * 2, imageV + imageSize, (float)imageSize);
	}
	else
	{
		drawArea(t, aUp, imageU, imageV, (float)imageSize);
	}
	
	// render diagonals, if available
	if (northDiagonals)
	{
		t.colorABGR(cReleased);
		drawRectangleArea(t, aUpLeft, imageU, imageV + imageSize, (float)imageSize);
		drawRectangleArea(t, aUpRight, imageU + imageSize, imageV + imageSize, (float)imageSize);
	}

	// render backwards button
	if (northDiagonals) t.colorABGR(cDiscreet);
	else if (isButtonDown(AREA_DPAD_S)) t.colorABGR(cPressed);
	else						   t.colorABGR(cReleased);
	if (isChangingFlightHeight)
	{
		drawArea(t, aDown, imageU + imageSize * 3, imageV + imageSize, (float)imageSize);
	}
	else
	{
		drawArea(t, aDown, imageU + imageSize * 2, imageV, (float)imageSize);
	}

	} // end if (!_useJoystick)（十字键四向与斜向）

	// 潜行键（只有十字键模式有；摇杆模式已改成双击摇杆切换潜行）
	if (aSneak) {
		t.colorABGR(isButtonDown(AREA_SNEAK) ? cPressed : cReleased);
		drawArea(t, aSneak, imageU + imageSize * 4, imageV, (float)imageSize);
	}

	// render jump / flight button
	if (_renderFlightImage && northDiagonals) t.colorABGR(cDiscreet);
	else if (isButtonDown(AREA_DPAD_C)) t.colorABGR(cPressed);
	else						   t.colorABGR(cReleased);
	if (_renderFlightImage)
	{
		drawArea(t, aJump, imageU + imageSize * 4, imageV + imageSize, (float)imageSize);
	}
	else
	{
		drawArea(t, aJump, imageU + imageSize * 4, imageV, (float)imageSize);
	}

	// 飞行时：右侧跳跃键的正上/正下方各画一个独立升降键（上升/下降图标）
	if (_renderFlightImage)
	{
		t.colorABGR(isButtonDown(AREA_FLY_UP)   ? cPressed : cReleased);
		// 上升 = 下降图标上下翻转（图集里那个“上升”格子其实是左上对角箭头）
		drawArea(t, aFlyUp,   imageU + imageSize * 3, imageV + imageSize, (float)imageSize, true);
		t.colorABGR(isButtonDown(AREA_FLY_DOWN) ? cPressed : cReleased);
		drawArea(t, aFlyDown, imageU + imageSize * 3, imageV + imageSize, (float)imageSize);
	}
	

#ifdef __APPLE__
	if (!_minecraft->screen) {
		if (isButtonDown(AREA_PAUSE))  t.colorABGR(cPressedPause);
		else						   t.colorABGR(cReleasedPause);
		
        drawRectangleArea(t, aPause, 200, 64, 18.0f);
	}
#endif /*__APPLE__*/
//t.end(true, _bufferId);
	//return;

	t.draw();

	// 移动版专属：摇杆（底座空心圆 + 手柄实心圆，无贴图）
	if (_useJoystick)
		drawJoystick(t);

	//RenderChunk _render = t.end(true, _bufferId);
	//t.setAccessMode(Tesselator::ACCESS_STATIC);
	//_bufferId = _render.vboId;
}
