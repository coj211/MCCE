# -*- coding: utf-8 -*-
"""一次性补丁：把 ModEngine 里"只在有画面时有意义"的入口在 STANDALONE_SERVER 下掏空。

背景：
  引擎的核心逻辑（Level/GameMode/Mob/Item/Tile）到处调用 ModEngine 的钩子，
  所以独立服务器必须编 ModEngine.cpp。但 ModEngine 里有相当一部分 API 是
  贴图/UI/皮肤/声音 —— 它们依赖被服务器排除掉的模块（Textures / Tesselator /
  ScriptedScreen / SoundEngine / Gui），那些符号在服务器根本不存在。

为什么是"掏空函数体"而不是"函数开头早退"：
  只 return 的话，函数体照样参与编译，链接器仍然要求 Textures::loadAndBindTexture
  之类的符号。必须整段条件编译掉，符号才不会被引用。

为什么保留函数名和注册：
  注册代码（registerBindings）不用动，服务器 mod 调用 ui.*/Assets.* 时静默无效，
  不会崩、也不会因为函数不存在而编译失败。

模式：
    T foo(...) {
    #ifndef STANDALONE_SERVER
        ...原实现...
    #else
        <占位返回>
    #endif
    }

另外三处非线性改动（loadEnabledMods 的贴图还原、tick 的 ScriptedScreen、
registerBindings 里的 registerGlJsBindings）也在本脚本里按精确文本替换。

跑完即删（改动以 diff 为准）。

⚠ 已经应用过一次，**不要重复运行**：它靠"函数名首次出现"定位定义，而前向声明
（`static duk_ret_t foo(duk_context*);`）会被误当作定义，从而插到下一个函数上。
下面的幂等保护是后补的，仅用于防止再次踩坑。
"""
import io
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
TARGET = os.path.join(os.path.dirname(os.path.dirname(HERE)),
                      "MinecraftPE-Win", "handheld", "src", "mod", "ModEngine.cpp")

# (函数名, 服务器占位返回)
FUNCS = [
    # 聊天 / UI 绘制 / 屏幕 / 声音：服务器没有画面与音频输出
    ("jsPlayerSendMessage",  "(void)ctx; return 0;   // 服务器：聊天广播见 ServerSideNetworkHandler（待接入）"),
    ("jsUiDrawText",         "(void)ctx; return 0;"),
    ("jsUiDrawShadow",       "(void)ctx; return 0;"),
    ("jsUiFillRect",         "(void)ctx; return 0;"),
    ("jsUiDrawImage",        "(void)ctx; return 0;"),
    ("jsUiGetWidth",         "(void)ctx; return 0;"),
    ("jsUiGetHeight",        "(void)ctx; return 0;"),
    ("jsUiGetScale",         "(void)ctx; return 0;"),
    ("jsUiShowLoading",      "(void)ctx; return 0;"),
    ("jsUiHideLoading",      "(void)ctx; return 0;"),
    ("jsUiOpenScreen",       "(void)ctx; return 0;"),
    ("jsUiCloseScreen",      "(void)ctx; return 0;"),
    ("jsUiOverride",         "(void)ctx; return 0;"),
    ("jsUiSetImage",         "(void)ctx; return 0;"),
    ("jsUiAddButton",        "(void)ctx; return 0;"),
    ("jsUiClear",            "(void)ctx; return 0;"),
    ("jsUiSkin",             "(void)ctx; return 0;"),
    ("jsLevelPlaySound",     "(void)ctx; return 0;"),
    ("jsLevelPlayMusic",     "(void)ctx; return 0;"),
    ("jsLevelStopMusic",     "(void)ctx; return 0;"),
    # 贴图注入 / 换图 / 玩家皮肤：都靠 Textures + GL 上传
    ("injectModBlockTexture", "return 1;   // 服务器没有贴图图集"),
    ("injectModItemIcon",     "return 1;"),
    ("replaceImageCore",      "return false;"),
    ("replaceImageScaled",    "return false;"),
    ("replaceImage",          "return false;"),
    ("applyPlayerSkinFromFile", "return false;"),
    ("applySavedPlayerSkin",    "return false;"),
    ("applyDefaultSkinTo64",    "return false;"),
    ("clearPlayerSkin",         "return;"),
    # 粒子/贴图图标：同样依赖被排除的渲染模块
    ("spawnScriptedParticle",       "return false;   // 服务器没有粒子引擎"),
    ("jsAssetsReplaceBlockIcon",    "(void)ctx; return 0;"),
]

# 三处非线性改动：精确文本 -> 替换
TEXT_PATCHES = [
    (
        "\tif (minecraft() && minecraft()->textures) {\n"
        "\t\tfor (size_t i = 0; i < _overriddenTextures.size(); ++i)\n"
        "\t\t\tminecraft()->textures->unloadTexture(_overriddenTextures[i]);\n"
        "\t\t_overriddenTextures.clear();\n"
        "\t}\n",
        "#ifndef STANDALONE_SERVER\n"
        "\t// 还原资源包式贴图覆盖（服务器没有贴图，直接跳过）\n"
        "\tif (minecraft() && minecraft()->textures) {\n"
        "\t\tfor (size_t i = 0; i < _overriddenTextures.size(); ++i)\n"
        "\t\t\tminecraft()->textures->unloadTexture(_overriddenTextures[i]);\n"
        "\t\t_overriddenTextures.clear();\n"
        "\t}\n"
        "#endif\n",
    ),
    (
        "\tif (_pendingScreen) {\n"
        "\t\tScriptedScreen* s = _pendingScreen;\n"
        "\t\t_pendingScreen = NULL;\n"
        "\t\tif (minecraft())\n"
        "\t\t\tminecraft()->setScreen(s);\n"
        "\t}\n",
        "#ifndef STANDALONE_SERVER\n"
        "\t// 模组请求打开的自定义界面（服务器没有界面可开）\n"
        "\tif (_pendingScreen) {\n"
        "\t\tScriptedScreen* s = _pendingScreen;\n"
        "\t\t_pendingScreen = NULL;\n"
        "\t\tif (minecraft())\n"
        "\t\t\tminecraft()->setScreen(s);\n"
        "\t}\n"
        "#endif\n",
    ),
    (
        "\tregisterGlJsBindings(ctx);",
        "#ifndef STANDALONE_SERVER\n"
        "\t// 裸 GL / Render.* 绑定（服务器没有 GL 上下文）\n"
        "\tregisterGlJsBindings(ctx);\n"
        "#endif",
    ),
]


def main():
    src = io.open(TARGET, encoding="utf-8").read()
    lines = src.splitlines(True)

    # ---- 1) 三处精确文本替换 ----
    n_text = 0
    for old, new in TEXT_PATCHES:
        if new in src:          # 已经打过这个补丁，跳过（否则会一层层嵌套）
            continue
        if old in src:
            src = src.replace(old, new, 1)
            n_text += 1
    lines = src.splitlines(True)

    # ---- 2) 掏空函数体 ----
    def find_def(name):
        """找函数定义。要跳过前向声明（以 ';' 结尾），否则会把下一个函数的
        函数体当成它的。"""
        pat = re.compile(r'^[A-Za-z_].*\b' + re.escape(name) + r'\s*\(')
        for i, ln in enumerate(lines):
            if not pat.match(ln):
                continue
            if ln.rstrip().endswith(';'):
                continue                       # 前向声明，跳过
            for j in range(i, min(i + 8, len(lines))):
                if lines[j].rstrip().endswith('{'):
                    return i, j
            return i, i
        return None

    targets = []
    for name, ph in FUNCS:
        pos = find_def(name)
        if not pos:
            print("  [跳过] 找不到定义: %s" % name)
            continue
        start, brace = pos
        # 找函数结束（行首 '}'）
        end = None
        for k in range(brace + 1, len(lines)):
            if lines[k].rstrip() == '}':
                end = k
                break
        if end is None:
            print("  [跳过] 找不到函数结尾: %s" % name)
            continue
        if '#ifndef STANDALONE_SERVER' in lines[brace + 1]:
            print("  [已是] %s" % name)
            continue
        targets.append((name, brace, end, ph))

    # 从后往前插入，避免行号漂移
    targets.sort(key=lambda t: t[1], reverse=True)
    for name, brace, end, ph in targets:
        lines.insert(end, "#else\n\t%s\n#endif\n" % ph)
        lines.insert(brace + 1, "#ifndef STANDALONE_SERVER\n")

    io.open(TARGET, "w", encoding="utf-8", newline="").write("".join(lines))
    print("ModEngine.cpp: 文本补丁 %d 处，函数掏空 %d 个" % (n_text, len(targets)))
    for t in targets:
        print("   - %s" % t[0])


if __name__ == "__main__":
    main()
