# -*- coding: utf-8 -*-
"""生成"服务器 mod 冒烟测试"包：mods/smoke_test.zip + mods/modlist.json。

用途：验证独立服务器是否**真的加载并执行**了 JS mod（而不只是把 zip 下发给了客户端）。
服务器启动后去看 mod 日志：
    %LOCALAPPDATA%\\Temp\\mcpe_mod.log
应当出现 [smoke] 开头的三行（加载 / onJoinWorld / 定期心跳）。

用法：python tools/make_smoke_mod.py
"""
import io
import json
import os
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
SEVER_ROOT = os.path.dirname(HERE)
MODS_DIR = os.path.join(SEVER_ROOT, "mods")

MAIN_JS = u"""// name: ServerSmokeTest
// author: reasonix
// version: 1.0.0
// description: 验证独立服务器真的加载并执行了 JS mod（含服务器命令）

modLog("[smoke] ===== SERVER MOD LOADED =====");
modLog("[smoke] side: isServer=" + isServer + " isClient=" + isClient);
if (isClient) {
    // 只有客户端进程会走到这里（服务器没有画面）
    modLog("[smoke] client-side half ran");
}

var __ticks = 0;

function onJoinWorld() {
    modLog("[smoke] onJoinWorld fired (server side)");
}

function onTick() {
    __ticks++;
    if (__ticks % 100 === 0)
        modLog("[smoke] server tick " + __ticks);
}

// 服务器命令：客户端输入 /smoke [文字]，由服务器执行并把返回值回给玩家。
Commands.register("smoke", function (sender, args) {
    modLog("[smoke] /smoke by " + sender + " args='" + args + "'");
    return "服务器收到 /smoke（来自 " + sender + "），参数: " + (args || "(无)");
});
"""


def main():
    if not os.path.isdir(MODS_DIR):
        os.makedirs(MODS_DIR)

    mod_name = "smoke_test.zip"
    zip_path = os.path.join(MODS_DIR, mod_name)
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("main.js", MAIN_JS.encode("utf-8"))

    # modlist.json：服务器启用列表（服务器权威 —— 客户端连上来会被对齐到这份列表）
    list_path = os.path.join(MODS_DIR, "modlist.json")
    io.open(list_path, "w", encoding="utf-8").write(
        json.dumps({"enabled": [mod_name]}, indent=2) + "\n")

    print("已生成:")
    print("  %s (%d bytes)" % (zip_path, os.path.getsize(zip_path)))
    print("  %s" % list_path)


if __name__ == "__main__":
    main()
