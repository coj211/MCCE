// 登录插件（"文员"）：进服先登录，登录前不能挖/放/打/聊天
//
// 用法（游戏内）：
//   第一次进服： /设置密码 <你的密码>     ← 自动登录
//   以后每次：   /login <你的密码>
//   想改：       /改密码 <旧密码> <新密码>
//
// 没登录之前拦住的：挖方块、放方块、攻击、聊天。
// **可以走路** —— 移动是客户端本地预测的，服务器硬拦会让角色原地漂移回弹，
// 要做"锁死在登录点"得靠服务器每 tick 拉回位置，那是另一件事。
//
// 两条要知道的限制：
//   1. 密码是**明文**存在 plugin_data/登录.txt 里的（服务器管理员看得到）。
//   2. 身份基础仍然是"玩家名"：只要猜不到你的密码，别人用你的名字也进不来；
//      但**在你还没设置过密码时**，别人用你的名字可以抢先注册掉。

var 密码文件 = "plugin_data/登录.txt";
var 密码表 = {};      // 名字 -> 密码
var 已登录 = {};      // 名字 -> true

(function () {
    var all = files.read(密码文件);
    if (!all)
        return;
    var lines = all.split("\n");
    for (var i = 0; i < lines.length; ++i) {
        var line = lines[i];
        if (!line)
            continue;
        var eq = line.indexOf("=");
        if (eq > 0)
            密码表[line.substring(0, eq)] = line.substring(eq + 1);
    }
})();

function 保存密码表() {
    var out = "";
    for (var k in 密码表)
        out += k + "=" + 密码表[k] + "\n";
    files.write(密码文件, out);
}

function 提醒(name) {
    if (密码表[name])
        server.sendTo(name, "请先登录：/login <密码>");
    else
        server.sendTo(name, "你是新玩家，请先设置密码：/设置密码 <新密码>");
}

// ---------------------------------------------------------------------------
// 事件
// ---------------------------------------------------------------------------
onPlayerJoin = function (name) {
    if (密码表[name])
        server.sendTo(name, "欢迎回来，" + name + "！用 /login <密码> 登录后就能动了");
    else
        server.sendTo(name, "欢迎，" + name + "！你是新玩家，用 /设置密码 <新密码> 注册");

    // 半分钟还没登录就再提醒一次（免得他以为是游戏卡了）
    setTimeout(function () {
        if (!已登录[name])
            提醒(name);
    }, 30000);
};

onPlayerLeave = function (name) {
    delete 已登录[name];
};

// ---- 未登录：拦住所有会造成影响的操作 ----
onBlockBreak = function (name, x, y, z) {
    if (已登录[name])
        return true;
    提醒(name);
    return false;
};

onBlockPlace = function (name, x, y, z) {
    if (已登录[name])
        return true;
    提醒(name);
    return false;
};

onAttack = function (name, targetId) {
    if (已登录[name])
        return true;
    提醒(name);
    return false;
};

onChat = function (name, msg) {
    if (已登录[name])
        return true;
    server.sendTo(name, "你还没登录，先 /login <密码>");
    return false;
};

// ---------------------------------------------------------------------------
// 命令（/ 开头的整行不走聊天拦截，所以未登录也能用下面这几条）
// ---------------------------------------------------------------------------
Commands.register("设置密码", function (sender, args) {
    if (!args)
        return "用法: /设置密码 <新密码>";
    if (密码表[sender])
        return "你已经设过密码了（忘了就用 /改密码，或让管理员删掉你的记录）";
    密码表[sender] = args;
    保存密码表();
    已登录[sender] = true;
    server.log("[登录] " + sender + " 设置了密码，已自动登录");
    server.broadcast(sender + " 注册了账号");
    return "密码已设置，你已自动登录";
});

Commands.register("login", function (sender, args) {
    if (!args)
        return "用法: /login <密码>";
    if (!密码表[sender])
        return "你还没设置密码，用 /设置密码 <新密码>";
    if (密码表[sender] !== args) {
        server.log("[登录] " + sender + " 密码错误");
        return "密码不对";
    }
    已登录[sender] = true;
    server.log("[登录] " + sender + " 登录成功");
    server.broadcast(sender + " 登录了");
    return "登录成功，玩得开心";
});

Commands.register("改密码", function (sender, args) {
    var sp = args ? args.indexOf(" ") : -1;
    if (sp < 0)
        return "用法: /改密码 <旧密码> <新密码>";
    if (密码表[sender] !== args.substring(0, sp))
        return "旧密码不对";
    密码表[sender] = args.substring(sp + 1);
    保存密码表();
    return "密码已改";
});

// 管理员：删掉某个账号（忘了密码时用）
Commands.register("删账号", function (sender, args) {
    if (!args)
        return "用法: /删账号 <玩家名>";
    if (!密码表[args])
        return args + " 没有账号";
    delete 密码表[args];
    delete 已登录[args];
    保存密码表();
    server.log("[登录] " + sender + " 删掉了 " + args + " 的账号");
    return "已删掉 " + args + " 的账号（他下次进服可以重新设置密码）";
}, { op: true });

server.log("登录插件已加载（现有账号 " + Object.keys(密码表).length + " 个）");
