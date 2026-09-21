#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""
从 MinecraftPE-Win 的客户端工程生成独立服务器工程 MinecraftPE-Sever.vcxproj。

为什么用生成而不是手写：
  客户端工程有 393 条 ClCompile / 195 条 ClInclude（逐条列出，非通配）。
  服务器要复用同一批引擎源码，手抄必然漂移；这里做机械翻译，随时可重生成。

翻译规则：
  1. 源文件相对路径 ..\..\xxx  ->  ..\..\MinecraftPE-Win\handheld\xxx
  2. 追加宏 STANDALONE_SERVER（引擎里已有 131 处条件编译为它铺路）
  3. 子系统 Windows -> Console（服务器是控制台程序，入口仍是 int main(void)）
  4. 产物落到 MinecraftPE-Sever/build/（不碰客户端的 Release/）
  5. 追加 MinecraftPE-Sever\src\*.cpp 服务器专属源码

用法：python tools/gen_vcxproj.py
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SEVER_ROOT = os.path.dirname(HERE)                     # MinecraftPE-Sever
MAIN_ROOT = os.path.dirname(SEVER_ROOT)                # main
CLIENT_PRJ = os.path.join(MAIN_ROOT, "MinecraftPE-Win", "handheld", "project", "win32_gl",
                          "MinecraftWin32_GL.vcxproj")
OUT_VCPROJ = os.path.join(SEVER_ROOT, "MinecraftPE-Sever.vcxproj")
SEVER_SRC = os.path.join(SEVER_ROOT, "src")

# 客户端工程在 handheld/project/win32_gl/ 下用 ..\..\ 指向 handheld\；
# 服务器工程在 main\MinecraftPE-Sever\ 下，只需一级 ..\ 到 main\ 再进 MinecraftPE-Win\handheld\
NEW_PREFIX = r"..\MinecraftPE-Win\handheld"


def rewrite_paths(text: str) -> str:
    """把源文件/包含目录里的 ..\..\ 重定位到 MinecraftPE-Win\\handheld\\。

    只在一次扫描里替换（re.sub 不会重扫替换结果），否则前缀自身又会被匹配，
    变成 ..\..\MinecraftPE-Win\handheld\MinecraftPE-Win\handheld\... 这种重复。
    """
    return re.sub(r"\.\.[\\/]+\.\.[\\/]+", lambda m: NEW_PREFIX + "\\", text)


def drop_client_entry_point(text: str) -> str:
    """去掉引擎的 main.cpp 与客户端图标资源。

    1) main.cpp 提供客户端的 int main()。服务器用 MinecraftPE-Sever\\src\\main_server.cpp
       作为入口（自己控制平台初始化与主循环），两个 main 不能同时链接进来。
    2) app.rc / app.ico 是客户端的窗口图标资源（RC1110: could not open app.rc），
       控制台服务器不需要。
    """
    text = re.sub(r'\s*<ClCompile Include="[^"]*handheld\\src\\main\.cpp" />', "", text)
    text = re.sub(r'\s*<ResourceCompile Include="[^"]*" />', "", text)
    text = text.replace("<ApplicationIcon>app.ico</ApplicationIcon>", "")
    return text


def add_engine_include_dir(text: str) -> str:
    """服务器源码要 include 引擎头文件（NinecraftApp.h 等），补上 src 根。

    两个配置（Debug/Release）各一处，count=2。
    """
    return text.replace(
        "<AdditionalIncludeDirectories>",
        "<AdditionalIncludeDirectories>$(ProjectDir)..\\MinecraftPE-Win\\handheld\\src;", 2)


def inject_server_macro(text: str) -> str:
    """在 PreprocessorDefinitions 里追加 STANDALONE_SERVER（每个配置一次）。"""
    return text.replace(
        "_RAKNET_SUPPORT_NatPunchthroughClient=0;",
        "STANDALONE_SERVER;_RAKNET_SUPPORT_NatPunchthroughClient=0;")


def set_console_subsystem(text: str) -> str:
    # 服务器是控制台程序；入口保持 mainCRTStartup（引擎里是 int main(void)）
    return text.replace("<SubSystem>Windows</SubSystem>", "<SubSystem>Console</SubSystem>")


def collect_server_sources() -> str:
    """服务器专属源码（MinecraftPE-Sever\\src\\*.cpp）。"""
    if not os.path.isdir(SEVER_SRC):
        return ""
    items = []
    for root, _dirs, files in os.walk(SEVER_SRC):
        for f in sorted(files):
            if f.lower().endswith((".cpp", ".c")):
                rel = os.path.relpath(os.path.join(root, f), SEVER_ROOT)
                items.append('    <ClCompile Include="%s" />' % rel.replace("/", "\\"))
    if not items:
        return ""
    return "  <ItemGroup>\n" + "\n".join(items) + "\n  </ItemGroup>\n"


# 服务器不需要编译的客户端源码。
#
# 依据：引擎在 STANDALONE_SERVER 下已经把**这些对象的使用**整段条件编译掉了
# （Minecraft::update 的渲染段、_levelGenerated 的 GUI 段……），所以把它们编进
# 服务器既没用、又会拖进整套 GL 调用（glEnable2 / glBindTexture2 / drawArrayVT …，
# 这些包装只在客户端分支定义）。
#
# mod\ 不排除：引擎的核心逻辑本身就依赖 ModEngine（Level::saveLevelData、
# GameMode::destroyBlock、MobSpawner、Item 等都调它的钩子），没有它服务器
# 根本链接不起来。所以服务器必须编 ModEngine.cpp，靠 STANDALONE_SERVER
# 把其中的 GL/GUI/渲染部分剥离掉。
EXCLUDE_PATTERNS = [
    r"\\client\\gui\\",
    r"\\client\\renderer\\",
    r"\\client\\model\\",
    r"\\client\\particle\\",
    r"\\client\\sound\\",
    r"\\client\\player\\input\\",
    r"\\client\\MouseHandler\.cpp$",
    r"\\network\\ClientSideNetworkHandler\.cpp$",   # 客户端侧的网络处理器，服务器用不上
    r"\\gui08\\",
    r"\\util\\PerfRenderer\.cpp$",
    r"\\mod\\GlJsBindings\.cpp$",   # 裸 GL / Render.* 绑定：服务器没有 GL 上下文
]


def filter_sources(text: str) -> str:
    """从 ClCompile 清单里剔除客户端渲染/GUI 文件。

    注意要整段匹配：工程里有几条 ClCompile 是多行形式（带 ObjectFileName
    子元素），按行删会把开标签删掉、留下孤立的 </ClCompile>，vcxproj 就废了。
    """
    dropped = []

    def repl(m):
        path = m.group("path")
        if any(re.search(p, path) for p in EXCLUDE_PATTERNS):
            dropped.append(path)
            return ""
        return m.group(0)

    pattern = re.compile(
        r'[ \t]*<ClCompile Include="(?P<path>[^"]+)"[^>]*?(?:/>|>.*?</ClCompile>)[ \t]*\r?\n',
        re.S)

    out = pattern.sub(repl, text)
    if dropped:
        print("  已从服务器工程剔除 %d 个客户端渲染/GUI 源文件" % len(dropped))
    return out


def main() -> int:
    if not os.path.isfile(CLIENT_PRJ):
        print("找不到客户端工程: %s" % CLIENT_PRJ, file=sys.stderr)
        return 1

    with open(CLIENT_PRJ, "r", encoding="utf-8") as fp:
        text = fp.read()

    text = rewrite_paths(text)
    text = drop_client_entry_point(text)
    text = filter_sources(text)
    text = inject_server_macro(text)
    text = set_console_subsystem(text)
    text = add_engine_include_dir(text)

    # 产物目录：服务器自己一套，绝不覆盖客户端输出
    outdir_block = (
        '  <PropertyGroup Condition="\'$(Configuration)|$(Platform)\'==\'Debug|Win32\'">\n'
        '    <OutDir>$(ProjectDir)build\\Debug\\</OutDir>\n'
        '    <IntDir>$(ProjectDir)build\\obj\\Debug\\</IntDir>\n'
        '  </PropertyGroup>\n'
        '  <PropertyGroup Condition="\'$(Configuration)|$(Platform)\'==\'Release|Win32\'">\n'
        '    <OutDir>$(ProjectDir)build\\Release\\</OutDir>\n'
        '    <IntDir>$(ProjectDir)build\\obj\\Release\\</IntDir>\n'
        '  </PropertyGroup>\n'
    )
    text = text.replace('  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props" />\n',
                        '  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props" />\n' + outdir_block)

    # 输出名
    text = text.replace("<Keyword>Win32Proj</Keyword>",
                        "<Keyword>Win32Proj</Keyword>\n    <ProjectName>MinecraftPE-Sever</ProjectName>")

    # 服务器专属源码追加到最后一个 ItemGroup 之前
    extra = collect_server_sources()
    if extra:
        idx = text.rfind("</Project>")
        text = text[:idx] + extra + text[idx:]

    with open(OUT_VCPROJ, "w", encoding="utf-8") as fp:
        fp.write(text)

    n_src = len(re.findall(r"<ClCompile ", text))
    print("已生成 %s" % OUT_VCPROJ)
    print("  ClCompile=%d  ClInclude=%d  服务器专属源=%s"
          % (n_src, len(re.findall(r"<ClInclude ", text)), bool(extra)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
