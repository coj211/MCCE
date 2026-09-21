// name: 时间控制
// author: reasonix
// version: 1.1.0
// description: /time 调世界时间、冻结或打开时间流逝，用来看日出正午日落的天空

var TPD = 24000;      // 一个游戏日的 tick 数
var autoStep = 0;     // >0 时每帧推进这么多 tick（快进播放昼夜）
var frozenTime = -1;  // >=0 时把世界时间钉在这个 tick（冻结时间流逝）

// 时间语义（和 1.6.4 / MCPE 一致）：0 = 日出，6000 = 正午，12000 = 日落，18000 = 午夜
var NAMED = {
    sunrise:   23000,
    day:       1000,
    morning:   3000,
    noon:      6000,
    afternoon: 9000,
    sunset:    12000,
    dusk:      13000,
    night:     16000,
    midnight:  18000
};

function _setDayTime(t) {
    t = ((t % TPD) + TPD) % TPD;
    var day = Math.floor(level.getTime() / TPD);
    level.setTime(day * TPD + t);
    return t;
}

function onTick() {
    // 快进优先
    if (autoStep > 0) {
        level.setTime(level.getTime() + autoStep);
        return;
    }
    // 冻结：世界时间自己往前走一点，就把它拽回来
    if (frozenTime >= 0 && level.getTime() !== frozenTime) {
        level.setTime(frozenTime);
    }
}

var HELP = "用法: /time <0-23999> | day noon sunset night midnight sunrise | "
         + "freeze(冻结) | flow(恢复流逝) | auto [每帧tick](快进) | stop | now";

Commands.register("time", function(sender, args) {
    args = (args || "").replace(/^\s+|\s+$/g, "");
    var low = args.toLowerCase();

    if (low === "" || low === "help" || low === "?") {
        return HELP;
    }

    // 冻结时间流逝：把当前时刻钉死
    if (low === "freeze" || low === "pause" || low === "flow off" || low === "off") {
        autoStep = 0;
        frozenTime = level.getTime();
        return "时间已冻结在 " + (frozenTime % TPD) + " tick（/time flow 恢复流逝）";
    }

    // 打开（恢复）时间流逝
    if (low === "flow" || low === "flow on" || low === "on"
        || low === "resume" || low === "unfreeze" || low === "run") {
        var was = frozenTime >= 0;
        frozenTime = -1;
        return was ? "时间流逝已恢复（当前 " + (level.getTime() % TPD) + " tick）"
                   : "时间流逝本来就是开的（当前 " + (level.getTime() % TPD) + " tick）";
    }

    // 停掉快进，回到正常流逝
    if (low === "stop") {
        var wasAuto = autoStep > 0;
        autoStep = 0;
        return wasAuto ? "快进已停，时间恢复正常流逝" : "本来就没在快进";
    }

    if (low === "now") {
        var state = autoStep > 0 ? ("快进中 +" + autoStep + "/帧")
                  : (frozenTime >= 0 ? "已冻结" : "正常流逝");
        return "当前 tick=" + level.getTime() + "（当日 " + (level.getTime() % TPD) + "，"
             + state + "）";
    }

    // 快进：每帧往前推一点，几秒跑完一天，方便连续看日出→正午→日落
    if (low.indexOf("auto") === 0) {
        var parts = low.split(/\s+/);
        autoStep = parts.length > 1 ? Math.max(1, parseInt(parts[1], 10) || 40) : 40;
        frozenTime = -1;
        var secs = (TPD / (autoStep * 60)).toFixed(1);
        return "快进中: 每帧 +" + autoStep + " tick（约 " + secs + " 秒一天），/time stop 停";
    }

    // 设置到某个时刻（不改冻结状态）
    var t = NAMED[low];
    if (t === undefined) t = parseInt(low, 10);
    if (isNaN(t)) return "看不懂: " + args + "  |  " + HELP;

    t = _setDayTime(t);
    if (frozenTime >= 0) frozenTime = level.getTime();   // 冻结状态下改时间：冻结点跟着挪
    return "世界时间 = " + t + "（0=日出 6000=正午 12000=日落 18000=午夜）"
         + (frozenTime >= 0 ? " [仍处于冻结]" : "");
});
