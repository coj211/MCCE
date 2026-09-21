// name: 坐姿演示
// author: dev
// version: 1.0.0
// description: 坐姿 API 演示(player.sit/stand + onKey 事件编排):
//   按 F = 原地坐下 / 站起;
//   坐着时按 空格(32) = 站起(事件系统自由编排, 非引擎写死);
//   onJoinWorld 后可用 onChat 发 "sit" / "stand" 命令触发。
// 坐姿: 渲染走 HumanoidModel 自带 riding 坐姿(腿折叠), 不能走动只能转头。

function onJoinWorld() {
    modLog("sit demo loaded — 按 F 坐下/站起, 坐着按空格起身");
    player.sendMessage("坐姿demo: 按 F 坐 / 再按 F 起, 坐着按空格起身");
}

function onKey(key, down) {
    if (!down) return;
    // F(70) = 切换坐/站
    if (key === 70) {
        if (player.isSitting()) {
            player.stand();
            player.sendMessage("站起来了");
        } else {
            player.sit();
            player.sendMessage("坐下了 (按空格起身)");
        }
    }
    // 空格(32): 坐着时起身
    if (key === 32 && player.isSitting()) {
        player.stand();
        player.sendMessage("空格起身");
    }
}

function onChat(msg) {
    var m = String(msg).trim().toLowerCase();
    if (m === "sit") { player.sit(); player.sendMessage("坐下了"); }
    else if (m === "stand") { player.stand(); player.sendMessage("站起来了"); }
}
