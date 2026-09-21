#ifndef MAIN_ANDROID_H__
#define MAIN_ANDROID_H__

/*
 * Android 入口 / 主循环（对应 Win 版的 main_win32.h）。
 *
 * 结构完全照搬 Win 版的骨架，只是把「Win32 消息循环 + EGL(PowerVR)」换成
 * 「android_native_app_glue 事件循环 + 原生 EGL(GLES1)」：
 *
 *   main_win32.h                         main_android.h
 *   ─────────────────────────────        ─────────────────────────────
 *   main() + PeekMessage 循环             android_main() + ALooper_pollAll
 *   windowProc(WM_*)                     onInputEvent(AInputEvent*)
 *     → Mouse/Keyboard/Multitouch::feed     → Multitouch/Keyboard::feed
 *   AppPlatform_win32                    AppPlatform_android
 *
 * 输入事件同样喂给平台无关的 Mouse/Keyboard/Multitouch 静态队列，所以
 * 游戏逻辑一行不用改；触摸（Multitouch::feed 的 ANDROID 分支）会自动镜像
 * 到 Mouse，GUI 的 mouseClicked/Released 照常工作。
 */

#include "client/renderer/gles.h"

#include <EGL/egl.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/configuration.h>
#include <android/input.h>
#include <android/keycodes.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>
#include <jni.h>

#include <cstdarg>
#include <cstdio>
#include <string>

#include "platform/input/Keyboard.h"
#include "platform/input/Mouse.h"
#include "platform/input/Multitouch.h"
#include "platform/time.h"
#include "util/FrameProf.h"
#include "util/Mth.h"

#include "AndroidGlobals.h"
#include "AppPlatform_android.h"
#include "client/Minecraft.h"
#include "mod/ModEngine.h"

// ── 全局（AndroidGlobals.h 里 extern 声明） ──────────────────────────────
AAssetManager* g_androidAssetManager = NULL;
JavaVM*        g_androidVM = NULL;
jobject        g_androidActivity = NULL;
ANativeWindow* g_androidWindow = NULL;
int            g_androidWidth = 0;
int            g_androidHeight = 0;
struct android_app* g_androidApp = NULL;

static App* g_app = NULL;
static bool g_inited = false;
static bool g_animating = false;

// 帧耗时诊断（F3 覆盖层读，PerfRenderer/App.h 引用）。
// g_diagUpdateMs 在非 Win32 平台由 LevelRenderer.cpp 定义，这里只做 extern 声明；
// g_diagSwapMs 只有 App.h 的 extern 声明，需要在这一处给出定义。
extern double g_diagUpdateMs;
double g_diagSwapMs = 0.0;
extern int g_frameStamp;   // 定义在 LevelRenderer.cpp

// ── EGL ──────────────────────────────────────────────────────────────────
static EGLDisplay g_display = EGL_NO_DISPLAY;
static EGLSurface g_surface = EGL_NO_SURFACE;
static EGLContext g_context = EGL_NO_CONTEXT;
static AppContext  g_appContext;

// ── 键位映射：Android AKEYCODE → 游戏内部码（Win32 版直接用 VK/ASCII） ──
static unsigned char mapAndroidKey(int32_t kc)
{
	if (kc >= AKEYCODE_A && kc <= AKEYCODE_Z)
		return (unsigned char)('A' + (kc - AKEYCODE_A));
	if (kc >= AKEYCODE_0 && kc <= AKEYCODE_9)
		return (unsigned char)('0' + (kc - AKEYCODE_0));
	switch (kc) {
	case AKEYCODE_SPACE:       return Keyboard::KEY_SPACE;      // 32
	case AKEYCODE_ENTER:       return Keyboard::KEY_RETURN;     // 13
	case AKEYCODE_DEL:         return Keyboard::KEY_BACKSPACE;  // 8
	case AKEYCODE_ESCAPE:      return Keyboard::KEY_ESCAPE;     // 27
	case AKEYCODE_TAB:         return 250;                      // 与 macOS/Linux 一致的内部 Tab
	case AKEYCODE_SHIFT_LEFT:
	case AKEYCODE_SHIFT_RIGHT: return Keyboard::KEY_LSHIFT;     // 10
	case AKEYCODE_DPAD_UP:     return 38;   // VK_UP
	case AKEYCODE_DPAD_DOWN:   return 40;   // VK_DOWN
	case AKEYCODE_DPAD_LEFT:   return 37;   // VK_LEFT
	case AKEYCODE_DPAD_RIGHT:  return 39;   // VK_RIGHT
	case AKEYCODE_F1:          return Keyboard::KEY_F1;
	case AKEYCODE_F2:          return Keyboard::KEY_F2;
	case AKEYCODE_F3:          return Keyboard::KEY_F3;
	case AKEYCODE_F4:          return Keyboard::KEY_F4;
	case AKEYCODE_F5:          return Keyboard::KEY_F5;
	default:                   return 0;
	}
}

// ── 触摸事件 ─────────────────────────────────────────────────────────────
static int32_t handleMotionEvent(AInputEvent* event)
{
	const int32_t action = AMotionEvent_getAction(event);
	const int32_t masked = action & AMOTION_EVENT_ACTION_MASK;
	const size_t  idx    = (size_t)((action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
	                                >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);

	switch (masked) {
	case AMOTION_EVENT_ACTION_DOWN:
	case AMOTION_EVENT_ACTION_POINTER_DOWN: {
		int32_t pid = AMotionEvent_getPointerId(event, idx);
		float x = AMotionEvent_getX(event, idx);
		float y = AMotionEvent_getY(event, idx);
		Multitouch::feed(MouseAction::ACTION_LEFT, MouseAction::DATA_DOWN,
		                 (short)x, (short)y, (char)pid);
		break;
	}
	case AMOTION_EVENT_ACTION_MOVE: {
		const size_t n = AMotionEvent_getPointerCount(event);
		for (size_t i = 0; i < n; ++i) {
			int32_t pid = AMotionEvent_getPointerId(event, i);
			float x = AMotionEvent_getX(event, i);
			float y = AMotionEvent_getY(event, i);
			Multitouch::feed(MouseAction::ACTION_MOVE, 0,
			                 (short)x, (short)y, (char)pid);
		}
		break;
	}
	case AMOTION_EVENT_ACTION_UP:
	case AMOTION_EVENT_ACTION_POINTER_UP: {
		int32_t pid = AMotionEvent_getPointerId(event, idx);
		float x = AMotionEvent_getX(event, idx);
		float y = AMotionEvent_getY(event, idx);
		Multitouch::feed(MouseAction::ACTION_LEFT, MouseAction::DATA_UP,
		                 (short)x, (short)y, (char)pid);
		break;
	}
	case AMOTION_EVENT_ACTION_CANCEL: {
		for (int i = 0; i < Multitouch::MAX_POINTERS; ++i) {
			if (Multitouch::isPointerDown(i))
				Multitouch::feed(MouseAction::ACTION_LEFT, MouseAction::DATA_UP,
				                 Multitouch::getX(i), Multitouch::getY(i), (char)i);
		}
		break;
	}
	default:
		break;
	}
	return 1;
}

// ── 按键事件 ─────────────────────────────────────────────────────────────
// 软键盘是不是正被游戏唤起（输入法里的退格/回车要不要交回系统，靠它判断）
static bool imeVisible()
{
	if (!g_app)
		return false;
	Minecraft* mc = (Minecraft*)g_app;
	return (mc->platform() != NULL) && mc->platform()->isKeyboardVisible();
}

static int32_t handleKeyEvent(AInputEvent* event)
{
	const int32_t kc = AKeyEvent_getKeyCode(event);
	const int32_t act = AKeyEvent_getAction(event);
	const bool down = (act == AKEY_EVENT_ACTION_DOWN);

	// 音量键交回系统
	if (kc == AKEYCODE_VOLUME_UP || kc == AKEYCODE_VOLUME_DOWN ||
	    kc == AKEYCODE_VOLUME_MUTE)
		return 0;

	// 软键盘弹出时，「输入类」按键必须先给系统/IME：
	// 输入法的退格是发一个 AKEYCODE_DEL 进来的，native 一旦消费掉（返回 1），
	// 隐藏 EditText 就永远收不到退格 —— 表现就是「键盘能打字、但退格没反应」。
	// 回车同理（让它走 EditText 的 EditorAction，再经 onTextChanged 回流）。
	// 注意：BACK / 方向键 / 游戏快捷键仍由游戏处理，不在这里拦。
	if (kc == AKEYCODE_DEL || kc == AKEYCODE_FORWARD_DEL ||
	    kc == AKEYCODE_ENTER || kc == AKEYCODE_NUMPAD_ENTER) {
		if (imeVisible())
			return 0;
	}

	// 返回键 = 游戏内 handleBack（不开先例给系统去 finish Activity）
	if (kc == AKEYCODE_BACK) {
		if (g_app)
			g_app->handleBack(down);
		return 1;
	}

	unsigned char gk = mapAndroidKey(kc);
	if (gk)
		Keyboard::feed(gk, down ? 1 : 0);

	return gk ? 1 : 0;
}

static int32_t handleInputEvent(struct android_app* app, AInputEvent* event)
{
	const int32_t type = AInputEvent_getType(event);
	if (type == AINPUT_EVENT_TYPE_MOTION)
		return handleMotionEvent(event);
	if (type == AINPUT_EVENT_TYPE_KEY)
		return handleKeyEvent(event);
	return 0;
}

// ── EGL 生命周期 ─────────────────────────────────────────────────────────
static bool initEGL()
{
	const EGLint configAttribs[] = {
		EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
		EGL_RED_SIZE,   8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE,  8,
		EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 16,
		EGL_NONE
	};
	const EGLint contextAttribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 1,   // GLES 1.1
		EGL_NONE
	};

	g_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (g_display == EGL_NO_DISPLAY) {
		LOGE("eglGetDisplay failed\n");
		return false;
	}
	if (!eglInitialize(g_display, 0, 0)) {
		LOGE("eglInitialize failed\n");
		g_display = EGL_NO_DISPLAY;
		return false;
	}

	EGLConfig config;
	EGLint numConfigs = 0;
	if (!eglChooseConfig(g_display, configAttribs, &config, 1, &numConfigs) || numConfigs < 1) {
		LOGE("eglChooseConfig failed\n");
		return false;
	}

	EGLint nativeVisual = 0;
	eglGetConfigAttrib(g_display, config, EGL_NATIVE_VISUAL_ID, &nativeVisual);
	ANativeWindow_setBuffersGeometry(g_androidWindow, 0, 0, nativeVisual);

	g_surface = eglCreateWindowSurface(g_display, config, g_androidWindow, NULL);
	if (g_surface == EGL_NO_SURFACE) {
		LOGE("eglCreateWindowSurface failed (0x%x)\n", eglGetError());
		return false;
	}

	g_context = eglCreateContext(g_display, config, EGL_NO_CONTEXT, contextAttribs);
	if (g_context == EGL_NO_CONTEXT) {
		LOGE("eglCreateContext failed (0x%x)\n", eglGetError());
		return false;
	}

	if (!eglMakeCurrent(g_display, g_surface, g_surface, g_context)) {
		LOGE("eglMakeCurrent failed (0x%x)\n", eglGetError());
		return false;
	}

	// vsync：GLES1 + 这个老固定管线渲染器，让 EGL 自己对齐刷新率即可
	eglSwapInterval(g_display, 1);

	LOGI("EGL ready: GLES1 %dx%d\n", g_androidWidth, g_androidHeight);
	return true;
}

static void destroyEGL()
{
	if (g_display != EGL_NO_DISPLAY) {
		eglMakeCurrent(g_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if (g_context != EGL_NO_CONTEXT) eglDestroyContext(g_display, g_context);
		if (g_surface != EGL_NO_SURFACE) eglDestroySurface(g_display, g_surface);
		eglTerminate(g_display);
	}
	g_display = EGL_NO_DISPLAY;
	g_context = EGL_NO_CONTEXT;
	g_surface = EGL_NO_SURFACE;
}

// ── AppCmd（Android 生命周期） ───────────────────────────────────────────
static void handleAppCmd(struct android_app* app, int32_t cmd)
{
	switch (cmd) {
	case APP_CMD_INIT_WINDOW:
		if (app->window != NULL) {
			g_androidWindow = app->window;
			g_androidWidth  = ANativeWindow_getWidth(app->window);
			g_androidHeight = ANativeWindow_getHeight(app->window);
			AppPlatform_android::setWindowSize(g_androidWidth, g_androidHeight);

			if (!initEGL())
				break;

			if (!g_inited) {
				g_appContext.display = g_display;
				g_appContext.context = g_context;
				g_appContext.surface = g_surface;
				g_appContext.doRender = true;
				g_appContext.platform = new AppPlatform_android();

				glInit();

				App* app2 = new MAIN_CLASS();
				g_app = app2;

				// 存档/配置根目录：/sdcard/Android/data/com.reconnern.mcpe/files
				// （ANativeActivity 只给 externalDataPath，没有单独的 cache 目录字段）
				const char* ext = app->activity->externalDataPath;
				if (ext) {
					((MAIN_CLASS*)g_app)->externalStoragePath = ext;
					((MAIN_CLASS*)g_app)->externalCacheStoragePath = ext;
				}

				// 把打包进 assets/mods 的内置模组（与 Windows 版 mods/ 同一批）
				// 释放到可写的 mods 目录，保证两端模组一致。
				androidInstallBundledMods(ext);

				g_app->init(g_appContext);
				g_app->setSize(g_androidWidth, g_androidHeight);
				g_inited = true;
				LOGI("MCPE: app initialized (%dx%d)\n", g_androidWidth, g_androidHeight);
			} else {
				// 上下文重建（切回前台/旋转）
				g_appContext.display = g_display;
				g_appContext.context = g_context;
				g_appContext.surface = g_surface;
				g_appContext.doRender = true;
				glInit();
				g_app->onGraphicsReset(g_appContext);
				g_app->setSize(g_androidWidth, g_androidHeight);
			}
			g_animating = true;
		}
		break;

	case APP_CMD_TERM_WINDOW:
		g_animating = false;
		if (g_inited && g_app)
			; // 保留 App，仅销毁 GL 表面
		destroyEGL();
		g_androidWindow = NULL;
		break;

	case APP_CMD_GAINED_FOCUS:
		g_animating = true;
		break;

	case APP_CMD_LOST_FOCUS:
		g_animating = false;
		break;

	case APP_CMD_CONFIG_CHANGED:
		if (app->window) {
			g_androidWidth  = ANativeWindow_getWidth(app->window);
			g_androidHeight = ANativeWindow_getHeight(app->window);
			AppPlatform_android::setWindowSize(g_androidWidth, g_androidHeight);
			if (g_inited && g_app)
				g_app->setSize(g_androidWidth, g_androidHeight);
		}
		break;

	case APP_CMD_LOW_MEMORY:
		LOGI("APP_CMD_LOW_MEMORY\n");
		break;

	default:
		break;
	}
}

// ── JNI：MainActivity 的文本回传 ─────────────────────────────────────────
extern "C" JNIEXPORT void JNICALL
Java_com_reconnern_mcpe_MainActivity_nativeOnTextChanged(JNIEnv* env, jobject thiz, jstring text)
{
	if (!text)
		return;
	const char* s = env->GetStringUTFChars(text, NULL);
	if (s) {
		AppPlatform_android::onTextChanged(std::string(s));
		env->ReleaseStringUTFChars(text, s);
	}
}

// ── JNI：Java 侧推送「系统接管、不派发触摸」的右侧宽度 ───────────────────
// Java 用窗口 insets 算出来（沉浸式全屏生效时为 0，导航栏/手势区露出时是它的
// 宽度），UI 布局把它当成屏幕外，见 AppPlatform::getUiRightInset()。
extern "C" JNIEXPORT void JNICALL
Java_com_reconnern_mcpe_MainActivity_nativeSetRightInset(JNIEnv* env, jobject thiz, jint px)
{
	// Java 侧第一次回调时顺手把 Activity 的全局引用存下来。
	// native 侧从 ANativeActivity::clazz 取有时拿不到（会静默地变成 NULL），
	// 一旦为空，所有 native→Java 的 JNI 调用（软键盘、震动……）就全废了。
	if (!g_androidActivity && thiz) {
		g_androidActivity = env->NewGlobalRef(thiz);
		LOGI("nativeSetRightInset: acquired activity ref %p\n", (void*)g_androidActivity);
	}
	AppPlatform_android::setUiRightInset((int)px);
	LOGI("nativeSetRightInset: %d\n", (int)px);
	// UI 按新宽度重新布局（Screen 尺寸、热键栏位置等都会跟着更新）
	if (g_app && g_inited)
		g_app->setSize(g_androidWidth, g_androidHeight);
}

// ── JNI：MainActivity 选完模组文件后的回调 ───────────────────────────────
// Java 侧已把 content:// 复制到可读路径，这里直接走与 Windows 版同一条
// ModsScreen::pickAndInstall 的安装逻辑（ModEngine::installModFile + 启用）。
extern "C" JNIEXPORT void JNICALL
Java_com_reconnern_mcpe_MainActivity_nativeOnModFilePicked(JNIEnv* env, jobject thiz, jstring path)
{
	LOGI("nativeOnModFilePicked: enter (g_app=%p)\n", (void*)g_app);
	if (!path || !g_app)
		return;
	const char* s = env->GetStringUTFChars(path, NULL);
	if (s) {
		Minecraft* mc = (Minecraft*)g_app;
		if (mc->modEngine) {
			std::string name = mc->modEngine->installModFile(std::string(s));
			if (!name.empty())
				mc->modEngine->setEnabled(name, true);
			LOGI("nativeOnModFilePicked: installed '%s'\n", name.c_str());
		}
		env->ReleaseStringUTFChars(path, s);
	}
}

// ── 入口 ─────────────────────────────────────────────────────────────────
void android_main(struct android_app* state)
{
	g_androidApp = state;
	g_androidVM = state->activity->vm;
	g_androidAssetManager = state->activity->assetManager;

	// MainActivity 的全局引用（JNI 回调要跨线程用，不能是局部引用）
	{
		JNIEnv* env = NULL;
		if (state->activity->vm->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_OK && env) {
			g_androidActivity = env->NewGlobalRef(state->activity->clazz);
		}
	}

	state->onAppCmd = handleAppCmd;
	state->onInputEvent = handleInputEvent;

	LOGI("MCPE: android_main start\n");
	LOGI("MCPE: BUILD TAG = touch-layout-v3\n");   // 验证设备上的 .so 真的是刚编出来的

	while (true) {
		int ident;
		int events;
		struct android_poll_source* source;

		// ALooper_pollAll 在 NDK r27 已标记为废弃（会漏 wake），改用
		// ALooper_pollOnce：内层 while 处理完所有待处理事件，返回负数即无事件。
		while ((ident = ALooper_pollOnce(g_animating ? 0 : -1, NULL, &events,
		                                (void**)&source)) >= 0) {
			if (source != NULL)
				source->process(state, source);

			if (state->destroyRequested) {
				if (g_inited && g_app) {
					g_app->audioEngineOff();
					delete g_app;
					g_app = NULL;
					g_inited = false;
				}
				if (g_appContext.platform) {
					g_appContext.platform->finish();
					delete g_appContext.platform;
					g_appContext.platform = NULL;
				}
				destroyEGL();
				LOGI("MCPE: shutdown\n");
				return;
			}
		}

		if (g_animating && g_inited && g_app) {
			++g_frameStamp;

			const float t0 = getTimeS();
			g_app->update();     // 内部会 swapBuffers()（eglSwapBuffers）
			g_diagUpdateMs = (getTimeS() - t0) * 1000.0f;

			Mouse::reset2();

			FrameProf::endFrame(g_diagUpdateMs, g_diagUpdateMs + g_diagSwapMs);
		}
	}
}

#endif /*MAIN_ANDROID_H__*/
