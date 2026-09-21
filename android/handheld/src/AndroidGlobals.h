#ifndef ANDROID_GLOBALS_H__
#define ANDROID_GLOBALS_H__

/*
 * Android 平台全局桥接。
 *
 * main_android.h 在窗口/上下文就绪后填充这些变量；AppPlatform_android 与
 * ModEngine 等模块通过本头文件取用，避免互相 include 平台主循环。
 */

#include <android/asset_manager.h>
#include <android/native_window.h>
#include <jni.h>
#include <string>

// 由 main_android.h 定义
extern AAssetManager* g_androidAssetManager;
extern JavaVM*        g_androidVM;
extern jobject        g_androidActivity;   // MainActivity（全局引用）
extern ANativeWindow* g_androidWindow;
extern int            g_androidWidth;
extern int            g_androidHeight;

// 当前 Android 应用对象（native_app_glue），供需要 looper/输入队列处取用
struct android_app;
extern struct android_app* g_androidApp;

// JNI 小工具（实现在 AppPlatform_android.cpp）
JNIEnv*     androidGetEnv();
std::string androidCallString(const char* method);          // 无参、返回 String
void        androidCallVoidInt(const char* method, int v);  // 单 int 参、void
std::string androidDeviceModel();

// 软键盘（实现在 AppPlatform_android.cpp，经 JNI 调 MainActivity）
void        androidShowSoftKeyboard(const std::string& text, int maxLength);
std::string androidHideSoftKeyboard();
// 只把输入法重新叫出来，不碰 EditText 里的文本（安卓点“输入框以外”的地方
// 会自动收起 IME，聊天屏里点一下画面就没法接着打字了）
void        androidReshowSoftKeyboard();

// 打开系统文件选择器挑一个模组 zip（异步；结果经 nativeOnModFilePicked 回传）
// 注意：client/gui/screens/ModsScreen.cpp 以 extern "C" 声明它，所以这里必须
// 保持 C 链接，否则链接期会找不到符号。
extern "C" void androidPickModFile();

// 首次启动时把打包进 assets/mods 的模组（zip + modlist.json）释放到可写的
// 外部 mods 目录 —— ModEngine 在 Android 上从
// /sdcard/Android/data/<pkg>/files/mods 读取模组。已存在的文件不覆盖。
void androidInstallBundledMods(const char* externalFilesDir);

// 供 MainActivity 回调的 C++ 入口（定义在 main_android.h / AppPlatform_android.cpp）
void androidOnTextChanged(const std::string& text);

#endif // ANDROID_GLOBALS_H__
