// 示例插件（"文员"）：称号系统（存文件，重启不丢）+ 几条管理命令
//
// 用法：服务器控制台敲 `plugins reload` 生效
//   游戏内：
//     /称号 大佬          → 之后你说话显示 [大佬] 名字: ...
//     /称号 空            → 清掉称号
//     /在线               → 看在线玩家
//     /封禁 某人 开挂      → 踢不掉，但下次他就进不来了（需要管理员）
//     /解封 某人           → 解除封禁（需要管理员）
//
// 插件能用的全部东西见 MinecraftPE-Sever/README.md 第 5 节。
// 这里用到：files（存称号）、ops（判权限）、bans（封禁）、setInterval（定时存盘）。

var 称号文件 = "plugin_data/称号.txt";
var 称号 = {};

// ---- 启动时把称号从文件读回来（不然重启服就全没了）----
(function () {
    var all = files.read(称号文件);
    if (!all)
        return;
    var lines = all.split("\n");
    for (var i = 0; i < lines.length; ++i) {
        var line = lines[i];
        if (!line)
            continue;
        var eq = line.indexOf("=");
        if (eq > 0)
            称号[line.substring(0, eq)] = line.substring(eq + 1);
    }
})();

function 保存称号() {
    var out = "";
    for (var k in 称号)
        out += k + "=" + 称号[k] + "\n";
    files.write(称号文件, out);
}

function 是管理员(name) {
    var list = ops.list();
    for (var i = 0; i < list.length; ++i)
        if (list[i] === name)
            return true;
    return false;
}

// 每 5 分钟自动存一次（其实每次改都存了，这是保险）
setInterval(function () {
    保存称号();
}, 5 * 60 * 1000);

// ---------------------------------------------------------------------------
// 事件
// ---------------------------------------------------------------------------
onPlayerJoin = function (name) {
    server.log("玩家进服: " + name);
};

onPlayerLeave = function (name) {
    server.log("玩家离开: " + name);
};

// ---------------------------------------------------------------------------
// 命令
// ---------------------------------------------------------------------------
Commands.register("称号", function (sender, args) {
    if (!args)
        return "用法: /称号 <文字>（清空：/称号 空）";
    if (args === "空" || args === "clear") {
        delete 称号[sender];
        保存称号();
        return "已清空你的称号";
    }
    称号[sender] = args;
    保存称号();
    return "已把你的称号设为 [" + args + "]";
});

Commands.register("在线", function (sender, args) {
    var ps = server.players();
    var names = [];
    for (var i = 0; i < ps.length; ++i)
        names.push(ps[i].name);
    return "在线 " + ps.length + " 人：" + names.join(", ");
});

Commands.register("封禁", function (sender, args) {
    if (!args)
        return "用法: /封禁 <玩家名> [原因]";
    var sp = args.indexOf(" ");
    var name = (sp < 0) ? args : args.substring(0, sp);
    var reason = (sp < 0) ? "" : args.substring(sp + 1);
    bans.add(name, reason);
    server.broadcast(name + " 被 " + sender + " 封禁了");
    return "已封禁 " + name + "（他下次就进不来了）";
}, { op: true });        // ← 第三个参数：只有管理员能用这条命令

Commands.register("解封", function (sender, args) {
    if (!args)
        return "用法: /解封 <玩家名>";
    return bans.remove(args) ? ("已解除 " + args + " 的封禁") : (args + " 不在封禁名单里");
}, { op: true });

Commands.register("名单", function (sender, args) {
    return "封禁: [" + bans.list().join(", ") + "]  管理员: [" + ops.list().join(", ") + "]";
}, { op: true });

// ---------------------------------------------------------------------------
// 聊天格式化：给名字加称号前缀
// ---------------------------------------------------------------------------
chat.setFormat(function (name, msg) {
    return (称号[name] ? "[" + 称号[name] + "] " : "") + name + ": " + msg;
});

server.log("称号插件已加载（" + Object.keys(称号).length + " 条称号记录）");
