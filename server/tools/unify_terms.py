# -*- coding: utf-8 -*-
"""统一术语：服务端 → 服务器。

约定（用户 2026-09-12 定）：
  * 那个独立程序（MinecraftPE-Sever.exe）一律叫 **服务器**，不再叫"服务端"；
  * 玩家那一端（MinecraftWin32_GL.exe）一律叫 **客户端**。

"服务端"是中文词，只出现在注释与玩家可见文案里，不会出现在任何标识符中，
所以整词替换是安全的（代码符号不受影响）。
"""
import io
import os

FILES = [
    # MinecraftPE-Sever 自身的文档、源码与工具
    "README.md",
    "src/main_server.cpp",
    "src/ServerApp.cpp",
    "src/ServerApp.h",
    "src/ServerConfig.cpp",
    "src/ServerConfig.h",
    "tools/gen_vcxproj.py",
    "tools/make_smoke_mod.py",
    "tools/patch_modengine_headless.py",
    # 引擎里为服务器新增/改动的注释与文案
    "../MinecraftPE-Win/handheld/src/client/Minecraft.cpp",
    "../MinecraftPE-Win/handheld/src/client/Minecraft.h",
    "../MinecraftPE-Win/handheld/src/network/ServerSideNetworkHandler.cpp",
    "../MinecraftPE-Win/handheld/src/network/ServerSideNetworkHandler.h",
    "../MinecraftPE-Win/handheld/src/mod/ModEngine.cpp",
    "../MinecraftPE-Win/handheld/src/mod/ModEngine.h",
    "../MinecraftPE-Win/handheld/src/client/gui/screens/ChatInputScreen.cpp",
    "../MinecraftPE-Win/handheld/src/client/renderer/gles.h",
]


def main():
    base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # MinecraftPE-Sever
    total = 0
    for rel in FILES:
        p = os.path.join(base, rel)
        if not os.path.isfile(p):
            print("  [跳过] 不存在: %s" % rel)
            continue
        s = io.open(p, encoding="utf-8").read()
        n = s.count(u"服务端")
        if not n:
            continue
        s = s.replace(u"服务端", u"服务器")
        io.open(p, "w", encoding="utf-8", newline="").write(s)
        total += n
        print("  %3d 处  %s" % (n, rel))
    print("合计替换 %d 处" % total)


if __name__ == "__main__":
    main()
