/*
 * Android 平台实现 —— 对应 Win 版的 AppPlatform_win32。
 *
 * 资源：Android 不能用 fopen 读打包进 APK 的 assets，必须走 AAssetManager。
 *       Gradle 把 handheld/data 的内容直接映射为 assets 根，所以
 *       readAssetFile("lang/en_US.lang") == assets/lang/en_US.lang。
 * 贴图：libpng 从内存解码（与 Win 版同一条路径，保证贴图一致）。
 * 截图：glReadPixels + zlib 手写 PNG（沿用 Win 版做法）。
 */

#include "AppPlatform_android.h"
#include "AndroidGlobals.h"
#include "client/renderer/gles.h"
#include "platform/input/Keyboard.h"   // 软键盘的「完成」要当回车喂给输入系统
#include "platform/log.h"
#include "util/Mth.h"

#include <android/configuration.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>
#include <png.h>
#include <zlib.h>

#include <dirent.h>
#include <sys/stat.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

// 基类的静态单例（Win 版定义在 AppPlatform_win32.cpp，Android 版在这里定义）
AppPlatform* AppPlatform::_singleton = NULL;

std::string* AppPlatform_android::_editText = NULL;
int          AppPlatform_android::_editTextMaxLen = 0;

static int s_screenW = 480;
static int s_screenH = 854;
static int s_uiRightInset = 0;   // 系统接管、不派发触摸的右侧宽度（物理像素）

AppPlatform_android::AppPlatform_android()
{
}

AppPlatform_android::~AppPlatform_android()
{
}

void AppPlatform_android::setWindowSize(int w, int h)
{
	if (w > 0) s_screenW = w;
	if (h > 0) s_screenH = h;
}

int AppPlatform_android::getScreenWidth()  { return s_screenW; }
int AppPlatform_android::getScreenHeight() { return s_screenH; }

// 右侧 inset：Java 侧按窗口 insets 推过来（沉浸式全屏下是 0）。
void AppPlatform_android::setUiRightInset(int px) { s_uiRightInset = (px > 0) ? px : 0; }
int  AppPlatform_android::getUiRightInset()       { return s_uiRightInset; }

float AppPlatform_android::getPixelsPerMillimeter()
{
	// 从 AConfiguration 的 density(dpi) 换算 px/mm：dpi / 25.4
	if (g_androidApp && g_androidApp->config) {
		int density = AConfiguration_getDensity(g_androidApp->config);
		if (density > 0)
			return density / 25.4f;
	}
	return 10.0f;
}

bool AppPlatform_android::supportsTouchscreen() { return true; }

bool AppPlatform_android::hasBuyButtonWhenInvalidLicense() { return false; }

std::string AppPlatform_android::getPlatformStringVar(int stringId)
{
	if (stringId == PlatformStringVars::DEVICE_BUILD_MODEL)
		return androidDeviceModel();
	return "<undefined>";
}

// ── 资源 ─────────────────────────────────────────────────────────────────

BinaryBlob AppPlatform_android::readAssetFile(const std::string& filename)
{
	BinaryBlob blob;
	AAssetManager* mgr = g_androidAssetManager;
	if (!mgr)
		return blob;

	AAsset* asset = AAssetManager_open(mgr, filename.c_str(), AASSET_MODE_BUFFER);
	if (!asset) {
		LOGI("readAssetFile: not found: %s\n", filename.c_str());
		return blob;
	}

	off_t len = AAsset_getLength(asset);
	if (len <= 0) {
		AAsset_close(asset);
		return blob;
	}

	blob.size = (int)len;
	blob.data = new unsigned char[len];
	int rd = AAsset_read(asset, blob.data, len);
	AAsset_close(asset);

	if (rd != len) {
		delete[] blob.data;
		blob.data = NULL;
		blob.size = -1;
	}
	return blob;
}

StringVector AppPlatform_android::listLanguageCodes()
{
	StringVector codes;
	AAssetManager* mgr = g_androidAssetManager;
	if (!mgr)
		return codes;

	AAssetDir* dir = AAssetManager_openDir(mgr, "lang");
	if (!dir)
		return codes;

	const char* name;
	while ((name = AAssetDir_getNextFileName(dir)) != NULL) {
		std::string n = name;
		if (n.length() > 5 && n.compare(n.length() - 5, 5, ".lang") == 0)
			codes.push_back(n.substr(0, n.length() - 5));
	}
	AAssetDir_close(dir);
	return codes;
}

// libpng 内存读回调
namespace {
struct PngMemReader {
	const unsigned char* data;
	size_t size;
	size_t pos;
};

void pngMemRead(png_structp pngPtr, png_bytep out, png_size_t length)
{
	PngMemReader* r = (PngMemReader*)png_get_io_ptr(pngPtr);
	if (r->pos + length > r->size) {
		png_error(pngPtr, "read past end of buffer");
		return;
	}
	memcpy(out, r->data + r->pos, length);
	r->pos += length;
}
} // namespace

TextureData AppPlatform_android::loadTexture(const std::string& filename_, bool textureFolder)
{
	TextureData out;

	std::string filename = textureFolder ? "images/" + filename_ : filename_;
	BinaryBlob blob = readAssetFile(filename);
	if (!blob.data || blob.size <= 0)
		return out;

	png_structp pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!pngPtr) {
		delete[] blob.data;
		return out;
	}

	png_infop infoPtr = png_create_info_struct(pngPtr);
	if (!infoPtr) {
		png_destroy_read_struct(&pngPtr, NULL, NULL);
		delete[] blob.data;
		return out;
	}

	if (setjmp(png_jmpbuf(pngPtr))) {
		png_destroy_read_struct(&pngPtr, &infoPtr, (png_infopp)0);
		delete[] blob.data;
		return out;
	}

	PngMemReader reader = { blob.data, (size_t)blob.size, 0 };
	png_set_read_fn(pngPtr, (voidp)&reader, pngMemRead);
	png_read_info(pngPtr, infoPtr);

	// 与 Win 版一致：统一补齐为 RGBA（RGB 补 alpha=255）
	png_set_filler(pngPtr, 0xff, PNG_FILLER_AFTER);

	out.w = png_get_image_width(pngPtr, infoPtr);
	out.h = png_get_image_height(pngPtr, infoPtr);

	png_bytep* rowPtrs = new png_bytep[out.h];
	out.data = new unsigned char[4 * out.w * out.h];
	out.numBytes = 4 * out.w * out.h;
	out.memoryHandledExternally = false;

	int rowStrideBytes = 4 * out.w;
	for (int i = 0; i < out.h; i++)
		rowPtrs[i] = (png_bytep)&out.data[i * rowStrideBytes];

	png_read_image(pngPtr, rowPtrs);

	png_destroy_read_struct(&pngPtr, &infoPtr, (png_infopp)0);
	delete[] (png_bytep)rowPtrs;
	delete[] blob.data;

	return out;
}

// ── 截图 ─────────────────────────────────────────────────────────────────

void AppPlatform_android::saveScreenshot(const std::string& filename, int glWidth, int glHeight)
{
	GLint vp[4] = { 0, 0, 0, 0 };
	glGetIntegerv(GL_VIEWPORT, vp);
	int vpW = vp[2], vpH = vp[3];
	if (vpW <= 0 || vpH <= 0) { vpW = glWidth; vpH = glHeight; }
	if (vpW <= 0 || vpH <= 0)
		return;

	const size_t rowBytes = (size_t)vpW * 4;
	std::vector<unsigned char> pixels(rowBytes * vpH);
	glReadPixels(0, 0, vpW, vpH, GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0]);

	const int outW = vpW, outH = vpH;
	const size_t outRow = (size_t)outW * 4;
	std::vector<unsigned char> raw((outRow + 1) * outH);
	for (int y = 0; y < outH; y++) {
		unsigned char* dst = &raw[(size_t)y * (outRow + 1)];
		dst[0] = 0; // filter None
		const unsigned char* s = &pixels[(size_t)(outH - 1 - y) * rowBytes];
		for (int x = 0; x < outW; x++) {
			dst[1 + x * 4 + 0] = s[x * 4 + 0];
			dst[1 + x * 4 + 1] = s[x * 4 + 1];
			dst[1 + x * 4 + 2] = s[x * 4 + 2];
			dst[1 + x * 4 + 3] = 255;
		}
	}

	uLongf compLen = compressBound((uLong)raw.size());
	std::vector<unsigned char> comp(compLen);
	if (compress2(&comp[0], &compLen, &raw[0], (uLong)raw.size(), 9) != Z_OK)
		return;

	FILE* fp = fopen(filename.c_str(), "wb");
	if (!fp) {
		LOGI("saveScreenshot: cannot open %s\n", filename.c_str());
		return;
	}
	static const unsigned char sig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
	fwrite(sig, 1, 8, fp);

	struct ChunkWriter {
		FILE* fp;
		void write(const char* type, const unsigned char* data, unsigned len) {
			unsigned char hdr[8];
			hdr[0] = (unsigned char)((len >> 24) & 0xff);
			hdr[1] = (unsigned char)((len >> 16) & 0xff);
			hdr[2] = (unsigned char)((len >> 8) & 0xff);
			hdr[3] = (unsigned char)(len & 0xff);
			memcpy(hdr + 4, type, 4);
			fwrite(hdr, 1, 8, fp);
			uLong crc = crc32(0, (const Bytef*)type, 4);
			if (len) {
				crc = crc32(crc, data, len);
				fwrite(data, 1, len, fp);
			}
			unsigned char cb[4];
			cb[0] = (unsigned char)((crc >> 24) & 0xff);
			cb[1] = (unsigned char)((crc >> 16) & 0xff);
			cb[2] = (unsigned char)((crc >> 8) & 0xff);
			cb[3] = (unsigned char)(crc & 0xff);
			fwrite(cb, 1, 4, fp);
		}
	} cw = { fp };

	unsigned char ihdr[13];
	ihdr[0] = (unsigned char)((outW >> 24) & 0xff);
	ihdr[1] = (unsigned char)((outW >> 16) & 0xff);
	ihdr[2] = (unsigned char)((outW >> 8) & 0xff);
	ihdr[3] = (unsigned char)(outW & 0xff);
	ihdr[4] = (unsigned char)((outH >> 24) & 0xff);
	ihdr[5] = (unsigned char)((outH >> 16) & 0xff);
	ihdr[6] = (unsigned char)((outH >> 8) & 0xff);
	ihdr[7] = (unsigned char)(outH & 0xff);
	ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
	cw.write("IHDR", ihdr, 13);
	cw.write("IDAT", &comp[0], (unsigned)compLen);
	cw.write("IEND", NULL, 0);
	fclose(fp);

	LOGI("saveScreenshot -> %s (%dx%d)\n", filename.c_str(), outW, outH);
}

// ── 杂项 ─────────────────────────────────────────────────────────────────

std::string AppPlatform_android::getDateString(int s)
{
	std::stringstream ss;
	ss << s << " s (UTC)";
	return ss.str();
}

int AppPlatform_android::checkLicense()
{
	return 0; // 已拥有
}

void AppPlatform_android::vibrate(int milliSeconds)
{
	if (milliSeconds > 0)
		androidCallVoidInt("vibrate", milliSeconds);
}

// ── 软键盘 ───────────────────────────────────────────────────────────────

void AppPlatform_android::showKeyboard(std::string* text, int32_t maxLength, bool flag)
{
	_editText = text;
	_editTextMaxLen = (int)maxLength;
	keyboardVisible = true;
	LOGI("[kbd] showKeyboard text='%s' max=%d target=%p\n",
	     text ? text->c_str() : "", (int)maxLength, (void*)_editText);
	androidShowSoftKeyboard(text ? *text : std::string(), (int)maxLength);
}

void AppPlatform_android::hideKeyboard()
{
	std::string t = androidHideSoftKeyboard();
	if (_editText)
		*_editText = t;
	_editText = NULL;
	keyboardVisible = false;
}

// 点屏幕后把输入法叫回来：安卓在点击「输入框以外」的地方时会自动收起 IME，
// 聊天屏里点一下画面就没法继续打字了（只能一次打完）。
void AppPlatform_android::reshowKeyboard()
{
	if (!keyboardVisible)
		return;          // 没在该打字（键盘早收了），不要自作主张再弹出来
	androidReshowSoftKeyboard();
}

void AppPlatform_android::onTextChanged(const std::string& text)
{
	// 软键盘上的「完成/回车」会带来一个结尾的换行（Java 侧补的，见
	// MainActivity 的 setOnEditorActionListener）：这里剥掉它，并当成按了一次
	// 回车（KEY_RETURN）。不然手机上文字打完了没有提交的途径 ——
	// 聊天屏、告示牌这些屏都是靠 KEY_RETURN 收尾。
	std::string t = text;
	bool enter = false;
	while (!t.empty() && (t[t.size() - 1] == '\n' || t[t.size() - 1] == '\r')) {
		t.erase(t.size() - 1);
		enter = true;
	}
	if (_editText)
		*_editText = t;
	LOGI("[kbd] onTextChanged len=%d enter=%d target=%p text='%s'\n",
	     (int)t.size(), enter ? 1 : 0, (void*)_editText, t.c_str());
	if (enter) {
		Keyboard::feed(Keyboard::KEY_RETURN, 1);
		Keyboard::feed(Keyboard::KEY_RETURN, 0);
	}
}

// ── JNI 桥接 ─────────────────────────────────────────────────────────────

JNIEnv* androidGetEnv()
{
	if (!g_androidVM)
		return NULL;
	JNIEnv* env = NULL;
	jint status = g_androidVM->GetEnv((void**)&env, JNI_VERSION_1_6);
	if (status == JNI_EDETACHED) {
		if (g_androidVM->AttachCurrentThread(&env, NULL) != JNI_OK)
			return NULL;
	} else if (status != JNI_OK) {
		return NULL;
	}
	return env;
}

static jmethodID findActivityMethod(JNIEnv* env, jclass* outCls, const char* name, const char* sig)
{
	if (!g_androidActivity) {
		LOGE("findActivityMethod: g_androidActivity is NULL (JNI bridge dead)\n");
		return NULL;
	}
	jclass cls = env->GetObjectClass(g_androidActivity);
	if (!cls) {
		LOGE("findActivityMethod: GetObjectClass failed\n");
		return NULL;
	}
	jmethodID mid = env->GetMethodID(cls, name, sig);
	if (!mid) {
		LOGE("MainActivity.%s%s not found\n", name, sig);
		env->DeleteLocalRef(cls);
		return NULL;
	}
	if (outCls)
		*outCls = cls;
	else
		env->DeleteLocalRef(cls);
	return mid;
}

std::string androidCallString(const char* method)
{
	JNIEnv* env = androidGetEnv();
	if (!env)
		return "";
	jclass cls = NULL;
	jmethodID mid = findActivityMethod(env, &cls, method, "()Ljava/lang/String;");
	if (!mid)
		return "";
	jstring js = (jstring)env->CallObjectMethod(g_androidActivity, mid);
	std::string out;
	if (js) {
		const char* s = env->GetStringUTFChars(js, NULL);
		if (s) {
			out = s;
			env->ReleaseStringUTFChars(js, s);
		}
		env->DeleteLocalRef(js);
	}
	env->DeleteLocalRef(cls);
	return out;
}

void androidCallVoidInt(const char* method, int v)
{
	JNIEnv* env = androidGetEnv();
	if (!env)
		return;
	jclass cls = NULL;
	jmethodID mid = findActivityMethod(env, &cls, method, "(I)V");
	if (mid)
		env->CallVoidMethod(g_androidActivity, mid, (jint)v);
	if (cls)
		env->DeleteLocalRef(cls);
}

std::string androidDeviceModel()
{
	return androidCallString("getDeviceModel");
}

// 只把输入法重新叫出来，不碰 EditText 里的文本。
// 不能复用 androidShowSoftKeyboard：它会把 EditText.setText() 重置，
// 会抹掉用户正在拼的拼音和光标位置。
void androidReshowSoftKeyboard()
{
	JNIEnv* env = androidGetEnv();
	if (!env)
		return;
	jclass cls = NULL;
	jmethodID mid = findActivityMethod(env, &cls, "reshowSoftKeyboard", "()V");
	if (mid) {
		env->CallVoidMethod(g_androidActivity, mid);
		LOGI("[kbd] jni reshowSoftKeyboard called\n");
	} else {
		LOGE("[kbd] reshowSoftKeyboard not found on MainActivity!\n");
	}
	if (cls)
		env->DeleteLocalRef(cls);
}

void androidShowSoftKeyboard(const std::string& text, int maxLength)
{
	JNIEnv* env = androidGetEnv();
	if (!env) {
		LOGE("[kbd] androidShowSoftKeyboard: no JNIEnv\n");
		return;
	}
	// 兜底自愈：g_androidActivity 为空时从 android_app 再取一次全局引用
	if (!g_androidActivity && g_androidApp && g_androidApp->activity && g_androidApp->activity->clazz) {
		g_androidActivity = env->NewGlobalRef(g_androidApp->activity->clazz);
		LOGI("[kbd] re-acquired activity ref = %p\n", (void*)g_androidActivity);
	}
	jclass cls = NULL;
	jmethodID mid = findActivityMethod(env, &cls, "showSoftKeyboard", "(Ljava/lang/String;I)V");
	if (mid) {
		jstring js = env->NewStringUTF(text.c_str());
		env->CallVoidMethod(g_androidActivity, mid, js, (jint)maxLength);
		if (js)
			env->DeleteLocalRef(js);
		LOGI("[kbd] jni showSoftKeyboard called (len=%d)\n", (int)text.size());
	} else {
		LOGE("[kbd] showSoftKeyboard method not found on MainActivity!\n");
	}
	if (cls)
		env->DeleteLocalRef(cls);
}

std::string androidHideSoftKeyboard()
{
	JNIEnv* env = androidGetEnv();
	if (!env)
		return "";
	jclass cls = NULL;
	jmethodID mid = findActivityMethod(env, &cls, "hideSoftKeyboard", "()Ljava/lang/String;");
	std::string out;
	if (mid) {
		jstring js = (jstring)env->CallObjectMethod(g_androidActivity, mid);
		if (js) {
			const char* s = env->GetStringUTFChars(js, NULL);
			if (s) {
				out = s;
				env->ReleaseStringUTFChars(js, s);
			}
			env->DeleteLocalRef(js);
		}
	}
	if (cls)
		env->DeleteLocalRef(cls);
	return out;
}

void androidPickModFile()
{
	JNIEnv* env = androidGetEnv();
	if (!env)
		return;
	jclass cls = NULL;
	jmethodID mid = findActivityMethod(env, &cls, "pickModFile", "()V");
	if (mid)
		env->CallVoidMethod(g_androidActivity, mid);
	if (cls)
		env->DeleteLocalRef(cls);
}

// ── 内置模组释放 ─────────────────────────────────────────────────────────

static bool fileExists(const char* path)
{
	struct stat st;
	return stat(path, &st) == 0;
}

static void makeDirs(const std::string& path)
{
	std::string cur;
	for (size_t i = 0; i < path.size(); ++i) {
		cur += path[i];
		if (path[i] == '/' || i + 1 == path.size()) {
			if (cur.size() > 1)
				mkdir(cur.c_str(), 0777);
		}
	}
}

static bool writeAssetToFile(const std::string& assetName, const std::string& dstPath)
{
	AAsset* asset = AAssetManager_open(g_androidAssetManager, assetName.c_str(),
	                                   AASSET_MODE_BUFFER);
	if (!asset)
		return false;

	off_t len = AAsset_getLength(asset);
	FILE* fp = fopen(dstPath.c_str(), "wb");
	if (!fp || len <= 0) {
		AAsset_close(asset);
		if (fp) fclose(fp);
		return false;
	}

	const void* buf = AAsset_getBuffer(asset);
	if (buf) {
		fwrite(buf, 1, (size_t)len, fp);
	} else {
		std::vector<unsigned char> tmp((size_t)len);
		int rd = AAsset_read(asset, &tmp[0], (size_t)len);
		if (rd > 0) fwrite(&tmp[0], 1, (size_t)rd, fp);
	}
	fclose(fp);
	AAsset_close(asset);
	return true;
}

void androidInstallBundledMods(const char* externalFilesDir)
{
	if (!externalFilesDir || !g_androidAssetManager)
		return;

	std::string modsDir = std::string(externalFilesDir) + "/mods";
	makeDirs(modsDir);

	AAssetDir* dir = AAssetManager_openDir(g_androidAssetManager, "mods");
	if (!dir)
		return;

	const char* name;
	int copied = 0;
	while ((name = AAssetDir_getNextFileName(dir)) != NULL) {
		std::string dst = modsDir + "/" + name;
		// 已存在就不动：玩家可能换过版本或自己装过
		if (fileExists(dst.c_str()))
			continue;
		if (writeAssetToFile("mods/" + std::string(name), dst)) {
			LOGI("bundled mod installed: %s\n", dst.c_str());
			++copied;
		}
	}
	AAssetDir_close(dir);

	if (copied)
		LOGI("androidInstallBundledMods: %d file(s) -> %s\n", copied, modsDir.c_str());
}
