// =====================================================================
// 枪械运行时（由 bedrock2mod 生成时套用；@@XXX@@ 是构建期替换的占位符）
//
// 数据来源：
//   模型/骨架  <- .geo.json
//   动画表     <- .animation.json（通道单位已换算：r 弧度、p 格内、s 标量）
//   交互逻辑   <- 参照行为包里的 timeline 重新实现
//                （BP 的 luger.shoot 是 "0.0 -> @s ww2:luger_fire"，
//                  luger.reload 是 2.0s 后 replaceitem 换回满弹 —— 这里用 JS 复刻）
// =====================================================================

var MAG_SIZE      = @@MAG@@;          // 一个弹匣多少发
var FIRE_INTERVAL = @@FIRE@@;         // 射速：两次开火最小间隔（秒）
var BULLET_SPEED  = @@BSPEED@@;

var curAnim = "hold";
var animStart = 0;
var ammo = MAG_SIZE;
var lastShot = -99;
var reloading = false;
var reloadStart = 0;
var reloadDur = 1.6;

function safeLog(s) { if (typeof modLog === "function") modLog(s); }
function nowSec() { return level.getTicks() / 20.0; }

// ---------------------------------------------------------------- 动画采样
// fr = [[t, [x,y,z]], ...]（t 秒，已排序）；线性插值。
function sampleCh(fr, t) {
    var n = fr.length;
    if (n === 0) return null;
    if (t <= fr[0][0]) return fr[0][1];
    if (t >= fr[n - 1][0]) return fr[n - 1][1];
    for (var i = 0; i < n - 1; ++i) {
        var a = fr[i], b = fr[i + 1];
        if (t >= a[0] && t <= b[0]) {
            var span = b[0] - a[0];
            var u = span > 1e-6 ? (t - a[0]) / span : 0.0;
            return [a[1][0] + (b[1][0] - a[1][0]) * u,
                    a[1][1] + (b[1][1] - a[1][1]) * u,
                    a[1][2] + (b[1][2] - a[1][2]) * u];
        }
    }
    return fr[n - 1][1];
}

function setAnim(name, now) {
    if (GUN_ANIMS[name]) { curAnim = name; animStart = now; }
}

// 引擎按【骨骼名】调用这里（Item.defineItem 的 anim）。返回该骨骼这一帧的姿态。
function anim(part, age, time, firstPerson) {
    var a = GUN_ANIMS[curAnim] || GUN_ANIMS.hold;
    if (!a) return null;
    // 一次性动画播完自动回 hold
    if (!a.loop && a.len > 0 && (time - animStart) >= a.len) {
        setAnim("hold", time);
        a = GUN_ANIMS.hold;
        if (!a) return null;
    }
    var b = a.bones[part];
    if (!b) return null;
    var t = time - animStart;
    var out = {};
    if (b.r) { var r = sampleCh(b.r, t); out.x = r[0]; out.y = r[1]; out.z = r[2]; }
    if (b.p) { var p = sampleCh(b.p, t); out.px = p[0]; out.py = p[1]; out.pz = p[2]; }
    // scale：给就**替换**静态值（引擎侧语义）；0 = 隐藏这个部件
    if (b.s) { var q = sampleCh(b.s, t); out.sx = q[0]; out.sy = q[1]; out.sz = q[2]; }
    return out;
}

// ---------------------------------------------------------------- 开火
function fire() {
    var now = nowSec();
    if (reloading || ammo <= 0) return;
    if (now - lastShot < FIRE_INTERVAL) return;
    lastShot = now;
    ammo--;
    setAnim("shoot", now);

    var yaw = player.getYRot() * Math.PI / 180.0;
    var pit = player.getXRot() * Math.PI / 180.0;
    var cp = Math.cos(pit);
    // MC 约定：yaw 0 朝 +z，yaw 90 朝 -x；pitch 正 = 向下看
    var dx = -Math.sin(yaw) * cp;
    var dy = -Math.sin(pit);
    var dz =  Math.cos(yaw) * cp;
    var px = player.getX(), py = player.getY() + 1.25, pz = player.getZ();
    level.spawnProjectile(BULLET_ID,
        px + dx * 0.6, py + dy * 0.6, pz + dz * 0.6,
        dx * BULLET_SPEED, dy * BULLET_SPEED, dz * BULLET_SPEED);
    level.playSound("shot_@@NAME@@", px, py, pz, 1.0, 1.0);
    if (ammo === 0) player.sendMessage("§c弹匣空了 —— 输入 /reload 换弹");
}

// ---------------------------------------------------------------- 换弹
function reload() {
    if (reloading) return;
    if (ammo >= MAG_SIZE) { player.sendMessage("§7弹匣是满的"); return; }
    reloading = true;
    var now = nowSec();
    reloadStart = now;
    var a = GUN_ANIMS.reload;
    reloadDur = (a && a.len > 0) ? a.len : 1.6;
    setAnim("reload", now);
    level.playSound("reload_open_@@NAME@@", player.getX(), player.getY(), player.getZ(), 1.0, 1.0);
    player.sendMessage("§e>>Reloading<<");
}

function tickReload() {
    if (!reloading) return;
    if (nowSec() - reloadStart < reloadDur) return;
    reloading = false;
    ammo = MAG_SIZE;
    level.playSound("reload_close_@@NAME@@", player.getX(), player.getY(), player.getZ(), 1.0, 1.0);
    player.sendMessage("§a装填完成 §7" + ammo + "/" + MAG_SIZE);
}

// ---------------------------------------------------------------- 事件
function onTick() {
    tickReload();
    if (!reloading && player.isUseHeld()) fire();
}

function onChat(msg) {
    if (msg === "/reload") reload();
    else if (msg === "/ammo") player.sendMessage("§7" + ammo + "/" + MAG_SIZE);
    else if (msg === "/anim") player.sendMessage("§7当前动画: " + curAnim);
}

function onJoinWorld() {
    player.sendMessage("§a[ww2] @@DISP@@ 已加载 §7(id=" + GUN_ID + ")");
    safeLog("[@@NAME@@] ready: parts=@@NPARTS@@ bones=@@NBONES@@ anims=@@NANIMS@@");
}

// =====================================================================
// 动画数据（由 .animation.json 转换而来）
// 通道：r = [[t,[rx,ry,rz]]] 弧度；p = [[t,[px,py,pz]]] 格内单位；s = [[t,[sx,sy,sz]]]
// =====================================================================
var GUN_ANIMS = {
@@ANIMS@@
};
