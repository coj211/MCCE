// name: 玩家动作 + 变身演示
// author: dev
// version: 3.0.0
// description:
//   ① 变身：按 Z 变僵尸形 / 再按变回来；按 X 变"大个子"（演示体型跟着变）
//   ② 动作：水下连按两次前进 = 躺平游泳；按 V = 挥手
//
//   ---- 外形怎么来的 ----
//   Mob.defineMob(编号, {...}) 只是**登记一份外形**，不会生成任何生物。
//   Player.setModel(玩家, 编号) 让玩家穿上它 -> 玩家就长成那个样子。
//   体型（碰撞箱）也跟着外形里的 sizeW/sizeH 走 —— 变成大个子就真的过不了矮门。

// ---------------------------------------------------------------------------
// 外形 A：僵尸样（手臂向前平举 = 僵尸的标志动作，靠 anim 回调摆出来）
// ---------------------------------------------------------------------------
Mob.defineMob(40, {
    name: "僵尸外形",
    texture: "mob/zombie.png",      // 原版贴图；也可以写 zip 里的 "skin/xxx.png"
    sizeW: 0.6, sizeH: 1.8,         // 碰撞箱（宽, 高）—— 变身之后就是这个尺寸
    model: [
        { name: "head", tex: [0, 0],   box: [-4, -8, -4, 8, 8, 8],  pos: [0, 0, 0] },
        { name: "body", tex: [16, 16], box: [-4, 0, -2,  8, 12, 4], pos: [0, 0, 0] },
        { name: "arm0", tex: [40, 16], box: [-3, -2, -2, 4, 12, 4], pos: [-5, 2, 0] },
        { name: "arm1", tex: [40, 16], box: [-1, -2, -2, 4, 12, 4], pos: [5, 2, 0] },
        { name: "leg0", tex: [0, 16],  box: [-2, 0, -2,  4, 12, 4], pos: [-2, 12, 0] },
        { name: "leg1", tex: [0, 16],  box: [-2, 0, -2,  4, 12, 4], pos: [2, 12, 0] }
    ],
    anim: function(part, age, time) {
        // 双臂前伸（僵尸标志），走路时轻轻上下晃 —— time 是走路相位。
        // 返回 null 的部件交给引擎自带的人形动作（走路摆臂/摆腿/抬头）。
        if (part === "arm0" || part === "arm1")
            return { x: -1.5708 + Math.sin(time * 0.6662) * 0.18 };
        return null;
    }
});

// ---------------------------------------------------------------------------
// 外形 B：大个子（同一个形状，放大 + 加高碰撞箱 -> 变身之后过不了矮门）
// ---------------------------------------------------------------------------
Mob.defineMob(41, {
    name: "大个子外形",
    texture: "mob/pigzombie.png",
    sizeW: 1.2, sizeH: 3.0,
    scale: 1.6,                     // 模型放大（渲染）
    model: [
        { name: "head", tex: [0, 0],   box: [-4, -8, -4, 8, 8, 8],  pos: [0, 0, 0] },
        { name: "body", tex: [16, 16], box: [-4, 0, -2,  8, 12, 4], pos: [0, 0, 0] },
        { name: "arm0", tex: [40, 16], box: [-3, -2, -2, 4, 12, 4], pos: [-5, 2, 0] },
        { name: "arm1", tex: [40, 16], box: [-1, -2, -2, 4, 12, 4], pos: [5, 2, 0] },
        { name: "leg0", tex: [0, 16],  box: [-2, 0, -2,  4, 12, 4], pos: [-2, 12, 0] },
        { name: "leg1", tex: [0, 16],  box: [-2, 0, -2,  4, 12, 4], pos: [2, 12, 0] }
    ]
});

// ---------------------------------------------------------------------------
// 动作：躺平游泳 / 挥手
//   部件名：head / body / arm0 / arm1 / leg0 / leg1
//   返回 { x,y,z, px,py,pz }；位置（px/py/pz）是模型格，模型空间 y 向下、-z 是前方
// ---------------------------------------------------------------------------
var FLAT = -1.5708;                 // -90°：趴平朝前

Player.defineAction("swim", function(part, age, time, firstPerson) {
    var stroke = Math.sin(age * 0.22);
    var swing  = Math.sin(age * 0.12) * 0.22;

    if (firstPerson) {
        if (part === "arm0") return { x: FLAT + stroke * 0.5 };
        if (part === "arm1") return { x: FLAT - stroke * 0.5 };
        return null;
    }
    if (part === "body") return { x: FLAT, y: swing, px: 0, py: 0, pz: 0 };
    if (part === "head") return { x: FLAT, y: swing, px: 0, py: 0, pz: -20 };
    if (part === "arm0") return { x: FLAT + stroke * 0.7, y: swing, px: -5, py: 0, pz: -12 + stroke * 4 };
    if (part === "arm1") return { x: FLAT - stroke * 0.7, y: swing, px: 5,  py: 0, pz: -12 - stroke * 4 };
    if (part === "leg0") return { x: FLAT, y: swing + stroke * 0.15, px: -2, py: 0, pz: 12 };
    if (part === "leg1") return { x: FLAT, y: swing - stroke * 0.15, px: 2,  py: 0, pz: 12 };
    return null;
});

Player.defineAction("wave", function(part, age, time, firstPerson) {
    if (part !== "arm0") return null;
    var s = Math.sin(age * 0.6) * 0.5;
    if (firstPerson) return { x: -0.9, y: 0, z: 0.35 + s * 0.5 };
    return { x: -0.2, y: 0, z: 2.6 + s };
});

// ---------------------------------------------------------------------------
// 事件
// ---------------------------------------------------------------------------
var swimMode = false;
var prevForward = false;
var lastForwardMs = 0;
var DOUBLE_MS = 350;

function setSwim(p, on) {
    swimMode = on;
    Player.setAction(p.id, on ? "swim" : null);
    player.sendMessage(on ? "游泳姿态（再连按两次前进 或 上岸 收起）" : "收起游泳姿态");
}

function onJoinWorld() {
    modLog("玩家动作+变身演示 v3.1 已加载");
    player.sendMessage("Z=变僵尸 / X=变大个子 / V=挥手 / 水下连按两次前进=躺平游泳");
    // op 名单：每个世界一份（存世界目录的 ops.txt）。名单为空时第一个进来的自动是 op。
    if (level.isOp())
        player.sendMessage("你是这个世界的 op（名单: " + level.listOps().join(", ") + "）");
    else
        player.sendMessage("你不是 op —— 变身会被拒绝（把名字写进世界目录的 ops.txt 即可）");
    swimMode = false;
    prevForward = false;
    lastForwardMs = 0;
    // 进世界时把身上的外形清掉（换世界/重连后状态残留会怪）
    var me = Player.self();
    if (me) { Player.setModel(me.id, null); Player.setAction(me.id, null); }
}

function onPlayerTick(id) {
    var p = Player.get(id);
    if (!p || !p.local) return;

    var fwd = player.isForwardHeld();
    if (fwd && !prevForward) {
        var now = Date.now();
        if (now - lastForwardMs < DOUBLE_MS) {
            lastForwardMs = 0;
            if (p.isInWater) setSwim(p, !swimMode);
            else if (swimMode) setSwim(p, false);
        } else {
            lastForwardMs = now;
        }
    }
    prevForward = fwd;

    if (swimMode && !p.isInWater) setSwim(p, false);
}

function onKey(key, down) {
    if (!down) return;
    var me = Player.self();
    if (!me) return;

    // 变身要 op（演示“模组用 op 做门槛”；挥手不需要）
    if (key === 90 || key === 88) {
        if (!level.isOp()) {
            player.sendMessage("你不是 op，不能变身");
            return;
        }
    }

    if (key === 90) {                    // Z：僵尸形
        if (Player.getModel(me.id)) Player.setModel(me.id, null);
        else Player.setModel(me.id, 40);
    } else if (key === 88) {             // X：大个子
        if (Player.getModel(me.id)) Player.setModel(me.id, null);
        else Player.setModel(me.id, 41);
    } else if (key === 86) {             // V：挥手
        if (Player.getAction(me.id) === "wave") Player.setAction(me.id, null);
        else Player.setAction(me.id, "wave");
    }
}
