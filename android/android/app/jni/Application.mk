# MCPE 0.8.1 Android 版 —— ndk-build 应用配置
#
# 面向 GLES 1.1 固定管线后端（见 handheld/src/client/renderer/gles.h 的 ANDROID 分支）。

APP_PLATFORM := android-21
APP_ABI := arm64-v8a armeabi-v7a
# 静态链接 libc++，APK 里只多一个 libminecraftpe.so，避免 libc++_shared.so 的加载顺序坑
APP_STL := c++_static

# 让 NDK 工具链带 -fno-exceptions/-fno-rtti 以外的默认行为即可；
# 具体编译宏在 Android.mk 里按需追加（ANDROID / __ANDROID__ / NO_EGL 等自动由 NDK 定义）。
APP_CPPFLAGS := -fno-exceptions -fno-rtti -std=gnu++14

# 老项目代码风格：放宽一些警告
APP_CFLAGS := -w
APP_LDFLAGS := 
