#ifndef APPPLATFORM_H__
#define APPPLATFORM_H__

#include <vector>
#include <string>
#include <cstring>
#include "client/renderer/TextureData.h"

typedef std::vector<std::string> StringVector;

/*
typedef struct UserInput
{
    static const int STATUS_INVALID = -1;
    static const int STATUS_NOTINITED = -2;
    static const int STATUS_OK = 1;
    static const int STATUS_CANCEL = 0;

    UserInput(int id)
    :   _id(id),
        status(STATUS_NOTINITED)
    {}
    UserInput(int id, int status)
    :   _id(id),
        status(status)
    {}
    int getId() { return _id; }

    int status;
private:
    int _id;
} UserInput;


class UserInputStatus {
	int _status;
public:
	UserInputStatus(int status)
	:	_status(status)
	{}
	bool isAnswered() { return _status >= 0; }
	bool isOk() { return _status == UserInput::STATUS_OK; }
	bool isCancel() { return _status == UserInput::STATUS_CANCEL; }
};
*/

class BinaryBlob {
public:
	BinaryBlob()
	:	data(NULL),
		size(-1) {}

	BinaryBlob(unsigned char* data, unsigned int size)
	:	data(data),
		size(size) {}

	unsigned char* data;
	int size;
};

class PlatformStringVars {
public:
	static const int DEVICE_BUILD_MODEL = 0;
};

class AppPlatform
{
public:
	// 0.8.1 GUI 移植：suspend/resume 生命周期监听（Touch::InventoryPane 等
	// 需要在 app 挂起/恢复时释放/重建 GL 缓冲）。win32 下挂起/恢复事件
	// 不常见，默认空实现 + 静态分发接口，与 0.8.1 行为兼容。
	struct Listener {
		virtual ~Listener() {}
		virtual void onLowMemory() {}
		virtual void onAppSuspended() = 0;
		virtual void onAppResumed() {}
		virtual void onAppFocusLost() {}
		virtual void onAppFocusGained() {}
	};
	void addListener(Listener* l, float priority) { _listeners.push_back(std::make_pair(priority, l)); }
	void removeListener(Listener* l) {
		for (auto it = _listeners.begin(); it != _listeners.end(); ++it) {
			if (it->second == l) { _listeners.erase(it); break; }
		}
	}
	void dispatchSuspended() {
		for (auto& p : _listeners) if (p.second) p.second->onAppSuspended();
	}
	void dispatchResumed() {
		for (auto& p : _listeners) if (p.second) p.second->onAppResumed();
	}

	// 0.8.1 语义：全局单例，main 里创建后赋值（_singleton = new AppPlatform_win32()）
	static AppPlatform* _singleton;

	AppPlatform() : keyboardVisible(false) { AppPlatform::_singleton = this; }
    virtual ~AppPlatform() {}

	virtual void saveScreenshot(const std::string& filename, int glWidth, int glHeight) {}
	virtual TextureData loadTexture(const std::string& filename_, bool textureFolder) { return TextureData(); }

    virtual void playSound(const std::string& fn, float volume, float pitch) {}

	virtual void showDialog(int dialogId) {}
    virtual void createUserInput() {}
	
	bool is_big_endian(void)  {
		union {
			unsigned int i;
			char c[4];
		} bint = {0x01020304};
		return bint.c[0] == 1;
	} 

	void createUserInput(int dialogId)
	{
		showDialog(dialogId);
		createUserInput();
	}
	virtual int getUserInputStatus() { return 0; }
	virtual StringVector getUserInput() { return StringVector(); }

	virtual std::string getDateString(int s) { return ""; }
	//virtual void createUserInputScreen(const char* types) {}

    virtual int checkLicense() { return 0; }
	virtual bool hasBuyButtonWhenInvalidLicense() { return false; }

	virtual void uploadPlatformDependentData(int id, void* data) {}
	virtual BinaryBlob readAssetFile(const std::string& filename) { return BinaryBlob(); }
	virtual void _tick() {}

	// 0.8.1 语言：枚举可用语言（data/lang/*.lang 里的语言码）。默认空 =
	// 平台不支持枚举（调用方回退到内置 en_US/zh_CN）。
	virtual StringVector listLanguageCodes() { return StringVector(); }

	virtual int getScreenWidth() { return 854; }
	virtual int getScreenHeight() { return 480; }
    virtual float getPixelsPerMillimeter() { return 10; }

	virtual bool isNetworkEnabled(bool onlyWifiAllowed) { return true; }
	// 0.8.1 GUI 移植：统计埋点（win32 本地 stub）
	virtual void statsTrackData(const std::string& name, const std::string& value) {}

	virtual bool isPowerVR() {
		return false;
	}
	virtual int getKeyFromKeyCode(int keyCode, int metaState, int deviceId) {return 0;}
	virtual bool isSuperFast() { return false; }

	virtual void buyGame() {}

	virtual void finish() {}
	
	virtual bool supportsTouchscreen() { return true; }
	
	virtual void vibrate(int milliSeconds) {}

	virtual std::string getPlatformStringVar(int stringId) {
		return "<getPlatformStringVar NotImplemented>";
	}

	// 0.8.1 GUI 移植：TextBox 用（win32 无软键盘，空实现）
	virtual void showKeyboard(std::string* /*text*/, int32_t /*maxLength*/, bool /*flag*/) {}
	virtual void showKeyboard() {
		keyboardVisible = true;
	}
	virtual void hideKeyboard() {
		keyboardVisible = false;
	}
	virtual bool isKeyboardVisible() {return keyboardVisible;}
protected:
	bool keyboardVisible;
	std::vector<std::pair<float, AppPlatform::Listener*> > _listeners;
};

#endif /*APPPLATFORM_H__*/
