#ifndef APPPLATFORM_ANDROID_H__
#define APPPLATFORM_ANDROID_H__

#include "AppPlatform.h"
#include <string>

/*
 * Android 平台实现（对应 Win 版的 AppPlatform_win32）。
 *
 * 资源读取走 AAssetManager（assets 根 = handheld/data 的内容，
 * 所以 readAssetFile("lang/en_US.lang") 直接命中 assets/lang/en_US.lang）。
 * 贴图解码用 libpng，读路径由 assets 提供。
 */
class AppPlatform_android : public AppPlatform
{
public:
	AppPlatform_android();
	virtual ~AppPlatform_android();

	// 由 main_android.h 在 surface 创建/尺寸变化时调用
	static void setWindowSize(int w, int h);

	// 系统接管、不派发触摸的右侧宽度（物理像素）：由 Java 侧按窗口 insets 推入，
	// UI 布局把它当成“屏幕外”。
	static void setUiRightInset(int px);
	virtual int getUiRightInset();

	// 软键盘文本回调（MainActivity 的 nativeOnTextChanged → 这里）
	static void onTextChanged(const std::string& text);

	virtual BinaryBlob readAssetFile(const std::string& filename);
	virtual TextureData loadTexture(const std::string& filename_, bool textureFolder);
	virtual void saveScreenshot(const std::string& filename, int glWidth, int glHeight);
	virtual std::string getDateString(int s);
	virtual int checkLicense();
	virtual bool hasBuyButtonWhenInvalidLicense();
	virtual StringVector listLanguageCodes();
	virtual bool isNetworkEnabled(bool onlyWifiAllowed) { return true; }

	virtual int getScreenWidth();
	virtual int getScreenHeight();
	virtual float getPixelsPerMillimeter();
	virtual bool supportsTouchscreen();
	virtual std::string getPlatformStringVar(int stringId);

	virtual void vibrate(int milliSeconds);
	virtual void statsTrackData(const std::string& name, const std::string& value) {}
	virtual void buyGame() {}

	// 软键盘：Android 无硬件键盘，走系统 IME
	virtual void showKeyboard(std::string* text, int32_t maxLength, bool flag);
	virtual void hideKeyboard();
	virtual void reshowKeyboard();
	virtual bool isKeyboardVisible() { return keyboardVisible; }

private:
	static std::string* _editText;
	static int          _editTextMaxLen;
};

#endif /*APPPLATFORM_ANDROID_H__*/
