#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
从 Windows 版 vcxproj 的源文件清单生成 Android 的 jni/Android.mk。

用法：
    python gen_android_mk.py <cpp_list.txt> <输出 Android.mk>

要点：
  * 源码在 ../../../handheld/ 下（Android.mk 位于 android/app/jni/）。
  * 393 个源文件如果全塞进一个 shared library，Windows 上最终链接命令会超过
    CreateProcess 的命令行上限（报 make (e=87) 参数错误）。所以按目录拆成若干
    static library，最终 so 只链接这些 .a。
  * libpng 单独一个 static library（ModEngine 需要 png.h）。
"""
import sys

BS = chr(92)   # 反斜杠
NL = chr(10)

# Windows 专属 / 被替换掉的源文件
EXCLUDE = (
    'AppPlatform_win32.cpp',   # → AppPlatform_android.cpp
    'SoundSystemAL.cpp',       # → SoundSystemSL.cpp（OpenSL ES）
)

# Android 专属追加的源文件
EXTRA = [
    '../../../handheld/src/AppPlatform_android.cpp',
    '../../../handheld/src/platform/audio/SoundSystemSL.cpp',
]

# 预留一个源文件给最终 so module（ndk-build 不喜欢空的 LOCAL_SRC_FILES）
SO_OWN_SRC = 'SharedConstants.cpp'

LIBPNG_SRCS = [
    'png.c', 'pngerror.c', 'pngget.c', 'pngmem.c', 'pngpread.c', 'pngread.c',
    'pngrio.c', 'pngrtran.c', 'pngrutil.c', 'pngset.c', 'pngtrans.c',
    'pngwio.c', 'pngwrite.c', 'pngwtran.c', 'pngwutil.c',
    # ARM 优化：libpng 在 aarch64/arm 上会自动启用 NEON，需要这些实现文件
    'arm/arm_init.c', 'arm/filter_neon_intrinsics.c', 'arm/palette_neon_intrinsics.c',
]

# 分组顺序 = 依赖从底层到上层（用 WHOLE_STATIC_LIBRARIES 其实不依赖顺序，
# 但保持可读）
GROUP_ORDER = [
    ('mpe_core',    '核心/平台/模组/网络/服务端'),
    ('mpe_raknet',  'RakNet 网络库'),
    ('mpe_world',   '世界/方块/实体/物品'),
    ('mpe_gui',     'client/gui 界面'),
    ('mpe_render',  'client/renderer 渲染'),
    ('mpe_client',  'client 其余'),
    ('mpe_gui08',   '0.8.1 GUI 移植层'),
]


def group_of(rel):
    """rel 形如 ../../../handheld/src/client/renderer/Chunk.cpp"""
    if 'thirdparty/libpng' in rel:
        return 'mpe_png'
    if '/src/' not in rel:
        return 'mpe_core'
    sub = rel.split('/src/', 1)[1]          # client/renderer/Chunk.cpp
    parts = sub.split('/')
    top = parts[0]
    if top == 'client':
        if len(parts) >= 2 and parts[1] == 'gui':
            return 'mpe_gui'
        if len(parts) >= 2 and parts[1] == 'renderer':
            return 'mpe_render'
        return 'mpe_client'
    if top == 'world':
        return 'mpe_world'
    if top == 'gui08':
        return 'mpe_gui08'
    if top == 'raknet':
        return 'mpe_raknet'
    return 'mpe_core'


HEADER_TMPL = '''# 由 gen_android_mk.py 自动生成，勿手改。
#
# MCPE 0.8.1 Android 版（GLES 1.1 固定管线）。
# 源文件清单来自 Windows 版 MinecraftWin32_GL.vcxproj，逐项映射过来。
#
# 为什么拆成多个 static library：393 个 .o 一次链接在 Windows 上会因为
# CreateProcess 命令行超长而失败（make (e=87) 参数错误）。

LOCAL_PATH := $(call my-dir)
MPE_ROOT := $(LOCAL_PATH)

# native_app_glue（NativeActivity 入口）提前 import，之后各 module 才能引用。
# 注意：import-module 会把 LOCAL_PATH 切成 native_app_glue 的目录，
# 而 LOCAL_SRC_FILES 的相对路径要以本目录为基准，所以立即恢复回来。
$(call import-module,android/native_app_glue)
LOCAL_PATH := $(MPE_ROOT)

MPE_INC := BS
    $(MPE_ROOT)/../../../handheld/src BS
    $(MPE_ROOT)/../../../handheld/src/gui08/include BS
    $(MPE_ROOT)/../../../handheld/src/gui08 BS
    $(MPE_ROOT)/../../../handheld/src/raknet BS
    $(MPE_ROOT)/../../../handheld/thirdparty BS
    $(MPE_ROOT)/../../../handheld/thirdparty/duktape BS
    $(MPE_ROOT)/../../../handheld/thirdparty/stb BS
    $(MPE_ROOT)/../../../handheld/thirdparty/libpng-1.6.40

# 注意：不要加 handheld/lib/include —— 那是 Win32 版用的旧 EGL/GLES/SLES 头，
# 会盖掉 NDK 自带的系统头。

# RakNet 裁剪开关：与 Windows 版 vcxproj 完全一致（关掉用不到的 NAT 穿透 /
# Router2 / UDP 代理），否则会缺少 UDPForwarder 等未编译文件的符号。
MPE_RAKNET_DEFS := BS
    -D_RAKNET_SUPPORT_NatPunchthroughClient=0 BS
    -D_RAKNET_SUPPORT_NatPunchthroughServer=0 BS
    -D_RAKNET_SUPPORT_NatTypeDetectionClient=0 BS
    -D_RAKNET_SUPPORT_NatTypeDetectionServer=0 BS
    -D_RAKNET_SUPPORT_Router2=0 BS
    -D_RAKNET_SUPPORT_UDPProxyClient=0 BS
    -D_RAKNET_SUPPORT_UDPProxyCoordinator=0 BS
    -D_RAKNET_SUPPORT_UDPProxyServer=0

MPE_CFLAGS   := -w -fno-strict-aliasing $(MPE_RAKNET_DEFS)
MPE_CPPFLAGS := -w -fno-strict-aliasing -frtti $(MPE_RAKNET_DEFS)

# ── libpng（ModEngine 运行时解码 png/jpg 用） ─────────────────────────────
include $(CLEAR_VARS)
LOCAL_MODULE := mpe_png
LOCAL_SRC_FILES := BS
'''

PNG_TAIL = '''
LOCAL_C_INCLUDES := $(MPE_ROOT)/../../../handheld/thirdparty/libpng-1.6.40
# intrinsics 实现（不用 .S 汇编，避开 32/64 位汇编差异）
LOCAL_CFLAGS := -w -DPNG_ARM_NEON_IMPLEMENTATION=1
LOCAL_ARM_NEON := true
include $(BUILD_STATIC_LIBRARY)

'''

GROUP_TMPL = '''# ── {name}（{desc}）────────────────────────────────────────────
include $(CLEAR_VARS)
LOCAL_MODULE := {name}
LOCAL_SRC_FILES := {bs}
{srcs}
LOCAL_C_INCLUDES := $(MPE_INC)
LOCAL_CFLAGS   := $(MPE_CFLAGS)
LOCAL_CPPFLAGS := $(MPE_CPPFLAGS)
{staticlibs}include $(BUILD_STATIC_LIBRARY)

'''

FOOTER_TMPL = '''# ── 最终 shared library ──────────────────────────────────────────────────
include $(CLEAR_VARS)

LOCAL_MODULE := minecraftpe
LOCAL_SRC_FILES := {so_src}
LOCAL_C_INCLUDES := $(MPE_INC)
LOCAL_CFLAGS   := $(MPE_CFLAGS)
LOCAL_CPPFLAGS := $(MPE_CPPFLAGS)

LOCAL_WHOLE_STATIC_LIBRARIES := {whole}

LOCAL_LDLIBS := -llog -landroid -lEGL -lGLESv1_CM -lOpenSLES -lz -lm

include $(BUILD_SHARED_LIBRARY)
'''


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    list_path, out_path = sys.argv[1], sys.argv[2]

    with open(list_path, 'r', encoding='utf-8', errors='ignore') as fh:
        raw = [ln.strip() for ln in fh if ln.strip()]

    srcs = []
    for p in raw:
        p = p.replace(BS, '/')
        name = p.rsplit('/', 1)[-1]
        if name in EXCLUDE:
            continue
        p = p.replace('../../src/', '../../../handheld/src/')
        p = p.replace('../../thirdparty/', '../../../handheld/thirdparty/')
        srcs.append(p)

    seen = set()
    ordered = []
    for p in srcs + EXTRA:
        if p in seen:
            continue
        seen.add(p)
        ordered.append(p)

    groups = {}
    so_own = None
    for p in ordered:
        if p.endswith('/' + SO_OWN_SRC):
            so_own = p
            continue
        groups.setdefault(group_of(p), []).append(p)

    for missing in ('mpe_png', 'mpe_core'):
        groups.setdefault(missing, [])

    def src_list(paths):
        body = ''.join('    %s %s%s' % (p, BS, NL) for p in paths)
        return body.rstrip(' ' + BS + NL) + NL

    out = HEADER_TMPL.replace('BS', BS)
    out += src_list(['../../../handheld/thirdparty/libpng-1.6.40/' + n
                     for n in LIBPNG_SRCS])
    out += PNG_TAIL

    for name, desc in GROUP_ORDER:
        paths = groups.get(name, [])
        if not paths:
            continue
        out += GROUP_TMPL.format(
            name=name, desc=desc, bs=BS, srcs=src_list(paths),
            staticlibs=('LOCAL_STATIC_LIBRARIES := android_native_app_glue' + NL
                        if name == 'mpe_core' else ''))

    whole = ' '.join(['mpe_png'] + [n for n, _ in GROUP_ORDER if groups.get(n)])
    out += FOOTER_TMPL.format(
        so_src=('    ' + so_own + NL) if so_own else '',
        whole=whole)

    with open(out_path, 'w', encoding='utf-8', newline='\n') as fh:
        fh.write(out)

    counts = {n: len(groups.get(n, [])) for n, _ in GROUP_ORDER}
    print('wrote %s' % out_path)
    print('  libpng: %d   total game sources: %d' % (len(LIBPNG_SRCS), len(ordered)))
    print('  groups: %s' % counts)
    print('  so-owned: %s' % so_own)
    return 0


if __name__ == '__main__':
    sys.exit(main())
