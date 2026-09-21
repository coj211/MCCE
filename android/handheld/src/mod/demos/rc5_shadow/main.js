// name: 光影包 v0.2 · 合成级 + 实时阴影(rc5)
// author: dev
// version: 0.2.0
// description: 在 rc4 光影(v0.1,曝光/ACES色调映射/饱和度/空气雾/菲涅尔水反/暗角)
//   基础上加入实时太阳阴影:模组自建 D24 阴影深度FBO,用 Render.world() 正交渲
//   地形到阴影图,合成 shader 由主深度重建世界坐标→光空间投影→PCF 软阴影。
//   玩家移动或太阳角度变化才重渲阴影(站定零开销)。纯全屏合成,不换核心渲染;
//   GLES 构建不编译离屏,不受影响。
//   命令: /sh 开 /shoff 关 /mode 0原样|1完整|2调色+雾|3仅水
//         /diag 0合成|1mask|2refl|3深度|4阴影图
//         /shad x(阴影强度 0..1) /sun az el(太阳方位/仰角,度)
//         /scan 扫水面 /wy x 手设水面 /exp x /sat x /con x /tone x
//         /fog x /fogcol r g b /tint x /wave x /str x /vig x /flip /cam
//   进世界约 2 秒自动开一次。

var S = { prog: 0, vbo: 0, ok: false, tried: false, diag: 0, flip: 0, wy: -999,
    shFbo: 0, shColor: 0, shTex: 0, shReady: false, shDirty: false,
    shCx: 0, shCy: 0, shCz: 0, shRev: false, shFlipY: false, shDep: false,
    shErr: 0, shRetried: false, underWater: false,
    torLoc: [], torchN: 0, torchR: 8.0, torDirty: true, torScan: 0, torchList: [],
    lightVP: null, lightValid: false, lightCx: 0, lightCy: 0, lightCz: 0,
    shUnsupported: false, invVP: null, camC: [0, 0, 0], u_camLoc: -1 };
var cfg = {
    exposure: 0.88, sat: 1.12, con: 1.10, tone: 2,   // 调色(默认整体略暗)
    fog: 0.30, fogR: 0.78, fogG: 0.84, fogB: 0.92,   // 空气雾(淡蓝白)
    wave: 1.0, tint: 0.15, str: 0.55, vig: 0.12,     // 水(波纹量/水色/反射强度)/暗角
    vanilla: false, wtest: false,                      // /water 原版水视图 /wtest 闪屏测试
    shad: 0.45, sunAz: 35, sunEl: 52                 // 阴影:正常灰影强度/太阳方位角/仰角(度)
};
var near = 0.05, far = 256.0;
var VER = "rc5.14-sky";   // 版本标识(便于确认加载的是新包)
var userOff = false, autoTick = 0;
var lastScan = 0;
// ── 阴影状态 ──
var SHADOW_SIZE = 2048;   // 阴影图分辨率(0.0625m/纹素 @覆盖128m,高分辨率减抖动)
var SH = 64.0;            // 正交半宽(覆盖玩家周围 ±64m)
var SH_N = 8.0, SH_F = 240.0; // 光空间近/远(相机悬在太阳侧,范围放宽防裁切)
var SH_TEX = (2 * SH) / SHADOW_SIZE; // 单纹素世界尺寸(用于光中心对齐防抖)
var dayNight = { last: -1, day: 1.0 }; // 缓存昼夜因子(-1 强制刷新)

function log(s) { modLog("[rc5] " + s); }
function msg(s)  { if (typeof player !== "undefined" && player) player.sendMessage("[光影] " + s); }

// ── column-major mat4 helpers (engine/glGetFloatv convention) ──
function m4mul(a, b) {
    var o = new Array(16);
    for (var c = 0; c < 4; c++)
        for (var r = 0; r < 4; r++) {
            var s = 0;
            for (var k = 0; k < 4; k++) s += a[k * 4 + r] * b[c * 4 + k];
            o[c * 4 + r] = s;
        }
    return o;
}
function m4det3(s) {
    var a = s[0][0], b = s[0][1], c = s[0][2],
        d = s[1][0], e = s[1][1], f = s[1][2],
        g = s[2][0], h = s[2][1], i = s[2][2];
    return a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
}
function m4inv(m) {
    // m: column-major float[16]; returns inverse or null when singular.
    function M(r, c) { return m[c * 4 + r]; }
    function cof(r, c) {
        var sub = [];
        for (var i = 0; i < 4; i++) {
            if (i === r) continue;
            var row = [];
            for (var j = 0; j < 4; j++) {
                if (j === c) continue;
                row.push(M(i, j));
            }
            sub.push(row);
        }
        var d = m4det3(sub);
        return ((r + c) % 2 === 0) ? d : -d;
    }
    var det = 0;
    for (var j = 0; j < 4; j++) det += M(0, j) * cof(0, j);
    if (Math.abs(det) < 1e-12) return null;
    var o = new Array(16);
    for (var rr = 0; rr < 4; rr++)
        for (var cc = 0; cc < 4; cc++)
            o[cc * 4 + rr] = cof(cc, rr) / det;
    return o;
}

// ── 阴影: 太阳方向 → 光相机角度 ──
// The engine's scripted camera builds its view by (OpenGL post-multiply,
// so the LAST rotate is applied to the vertex FIRST):
//   stack: M=I; glRotate(pitch,1,0,0) -> M=Rx; glRotate(yaw+180,0,-1,0)
//          -> M = Rx(pitch) * Ry(-(yaw+180));  clip = M * v
// Given a unit view-forward f this returns [yawDeg, pitchDeg] so that
// M * f = (0,0,-1) (the camera looks along f).
function dirToCamAngles(fx, fy, fz) {
    // Ry(-a) with a=yaw+180 must send f's XZ part onto (0,-1) plane.
    var a = Math.atan2(-fx, -fz) * 180 / Math.PI; // a = yaw+180
    var yaw = a - 180;
    var pitch = -Math.asin(fy) * 180 / Math.PI;
    return [yaw, pitch];
}

// 昼夜因子 0..1。与引擎亮度(br=2cos(2π·td)+0.5, td=frac-0.25)一致:
// frac=t/19200,正午 frac≈0.25 最亮、午夜≈0.75 全黑;深夜留 0.06 基值。
function dayFactor() {
    var t;
    try { t = level.getTime() % 19200; } catch (e) { t = 0; }
    var frac = t / 19200;
    var d = 2 * Math.sin(2 * Math.PI * frac) + 0.5;
    if (d < 0) d = 0;
    if (d > 1) d = 1;
    return 0.06 + 0.94 * d;
}

// 阴影目标对象(第一次进入渲染回调时创建)。失败会清理对象,允许下帧重试。
function ensureShadowTargets() {
    if (S.shReady) return true;
    if (S.shFbo) { // 上次残留(创建一半/失败): 先释放再重建
        try {
            GL.glDeleteFramebuffers(S.shFbo);
            if (S.shColor) GL.glDeleteTextures(S.shColor);
            if (S.shTex) GL.glDeleteTextures(S.shTex);
        } catch (e) {}
        S.shFbo = 0; S.shColor = 0; S.shTex = 0; S.shReady = false;
    }
    S.shFbo = GL.glGenFramebuffers();
    S.shColor = GL.glGenTextures();
    S.shTex = GL.glGenTextures();
    GL.glBindTexture(GL.TEXTURE_2D, S.shColor);
    GL.glTexParameteri(GL.TEXTURE_2D, GL.TEXTURE_MIN_FILTER, GL.NEAREST);
    GL.glTexParameteri(GL.TEXTURE_2D, GL.TEXTURE_MAG_FILTER, GL.NEAREST);
    GL.glTexParameteri(GL.TEXTURE_2D, GL.TEXTURE_WRAP_S, GL.CLAMP_TO_EDGE);
    GL.glTexParameteri(GL.TEXTURE_2D, GL.TEXTURE_WRAP_T, GL.CLAMP_TO_EDGE);
    GL.glTexImage2D(GL.TEXTURE_2D, 0, GL.RGBA8, SHADOW_SIZE, SHADOW_SIZE, 0, GL.RGBA, GL.UNSIGNED_BYTE, null);
    GL.glBindTexture(GL.TEXTURE_2D, S.shTex);
    GL.glTexParameteri(GL.TEXTURE_2D, GL.TEXTURE_MIN_FILTER, GL.NEAREST);
    GL.glTexParameteri(GL.TEXTURE_2D, GL.TEXTURE_MAG_FILTER, GL.NEAREST);
    GL.glTexParameteri(GL.TEXTURE_2D, GL.TEXTURE_WRAP_S, GL.CLAMP_TO_EDGE);
    GL.glTexParameteri(GL.TEXTURE_2D, GL.TEXTURE_WRAP_T, GL.CLAMP_TO_EDGE);
    GL.glTexImage2D(GL.TEXTURE_2D, 0, GL.DEPTH_COMPONENT24, SHADOW_SIZE, SHADOW_SIZE, 0, GL.DEPTH_COMPONENT, GL.UNSIGNED_INT, null);
    GL.glBindFramebuffer(GL.FRAMEBUFFER, S.shFbo);
    GL.glFramebufferTexture2D(GL.FRAMEBUFFER, GL.COLOR_ATTACHMENT0, GL.TEXTURE_2D, S.shColor, 0);
    GL.glFramebufferTexture2D(GL.FRAMEBUFFER, GL.DEPTH_ATTACHMENT, GL.TEXTURE_2D, S.shTex, 0);
    var st = GL.glCheckFramebufferStatus(GL.FRAMEBUFFER);
    GL.glBindFramebuffer(GL.FRAMEBUFFER, 0);
    GL.glBindTexture(GL.TEXTURE_2D, 0);
    if (st != GL.FRAMEBUFFER_COMPLETE) {
        log("shadow FBO incomplete: " + st);
        try {
            GL.glDeleteFramebuffers(S.shFbo);
            GL.glDeleteTextures(S.shColor);
            GL.glDeleteTextures(S.shTex);
        } catch (e2) {}
        S.shFbo = 0; S.shColor = 0; S.shTex = 0;
        return false;
    }
    S.shReady = true;
    log("shadow fbo=" + S.shFbo + " tex=" + S.shTex + " @ " + SHADOW_SIZE);
    return true;
}

// 需要重渲阴影: 首次 / 玩家移动超阈值 / 方块被改 / 光照参数变化。
// 节流: 距上次尝试不足 150ms 直接跳过,防止某状态下每帧重渲整场景导致卡顿。
function shadowNeedsRedraw() {
    if (!S.shFbo) return true;
    if (S.shDirty) return true;
    var now = Date.now();
    if (now - (S.shLastTry || 0) < 150) return false;
    var cam = Render.getCamera();
    var dx = cam[0] - S.shCx, dy = cam[1] - S.shCy, dz = cam[2] - S.shCz;
    return dx * dx + dy * dy + dz * dz > 0.36; // > 0.6 m
}

// 从当前太阳方向把 0/1 层地形渲进阴影深度图(光空间正交)。
function renderShadowMap() {
    if (S.shUnsupported) return;
    if (typeof Render.world !== "function" ||
        typeof Render.getWorldMatrices !== "function") {
        S.shUnsupported = true;
        return;
    }
    if (!ensureShadowTargets()) {
        S.shUnsupported = true; // FBO 建不起来就彻底关阴影,别再每帧试
        S.lightValid = false;
        return;
    }
    try {
        var cam = Render.getCamera();
        // 光中心 snap 到世界纹素网格:阴影图锚定世界,走路/转头不再纹素步进闪烁。
        var px = Math.round(cam[0] / SH_TEX) * SH_TEX;
        var py = Math.round(cam[1] / SH_TEX) * SH_TEX;
        var pz = Math.round(cam[2] / SH_TEX) * SH_TEX;
        var azr = cfg.sunAz * Math.PI / 180, elr = cfg.sunEl * Math.PI / 180;
        // 太阳方向(从地表指向太阳);光从太阳照来 => 观察方向朝地面 = -dir。
        var sdx = Math.cos(elr) * Math.sin(azr);
        var sdy = Math.sin(elr);
        var sdz = Math.cos(elr) * Math.cos(azr);
        var fx = -sdx, fy = -sdy, fz = -sdz;      // 观察前向(=光传播方向)
        var ang = dirToCamAngles(fx, fy, fz);
        // 相机悬在玩家上方太阳一侧;平行投影下沿 -f 平移只影响深度范围。
        var cx = px - fx * 96.0, cy = py - fy * 96.0, cz = pz - fz * 96.0;

        GL.glBindFramebuffer(GL.FRAMEBUFFER, S.shFbo);
        GL.glViewport(0, 0, SHADOW_SIZE, SHADOW_SIZE);
        GL.glClearColor(1, 1, 1, 1);
        GL.glClear(GL.COLOR_BUFFER_BIT | GL.DEPTH_BUFFER_BIT);
        GL.glEnable(GL.DEPTH_TEST);
        GL.glDepthMask(true);
        GL.glDisable(GL.BLEND);
        GL.glColorMask(false, false, false, false); // depth-only shadow pass
        var ok = Render.world([cx, cy, cz, ang[0], ang[1], 70, SH_N, SH_F, SH]);
        GL.glColorMask(true, true, true, true);
        GL.glDepthMask(true);
        GL.glDisable(GL.DEPTH_TEST);
        GL.glBindFramebuffer(GL.FRAMEBUFFER, 0);
        if (ok) {
            var wm = Render.getWorldMatrices();
            if (wm && wm.proj && wm.view) {
                S.lightVP = m4mul(wm.proj, wm.view);
                S.lightCx = cx; S.lightCy = cy; S.lightCz = cz;
                S.shCx = px; S.shCy = py; S.shCz = pz;
                S.lightValid = true;
                S.shErr = 0;
                S.shRetried = false;
                log("shadow 渲好: chunks=" + (wm.chunks == null ? "?" : wm.chunks)
                    + " 光矩阵0/5/10=" + wm.proj[0].toFixed(3) + "/" + wm.proj[5].toFixed(3) + "/" + wm.proj[10].toFixed(3)
                    + " cam=(" + px.toFixed(0) + "," + py.toFixed(0) + "," + pz.toFixed(0) + ")"
                    + " | " + dbgReproj());
            } else {
                log("world 成功但矩阵缺失,阴影暂不可用");
            }
        } else {
            log("Render.world 调用失败(返回 false)");
        }
        S.shDirty = false;
        S.shLastTry = Date.now();
    } catch (err) {
        S.shLastTry = Date.now();
        GL.glColorMask(true, true, true, true);
        GL.glDepthMask(true);
        GL.glBindFramebuffer(GL.FRAMEBUFFER, 0);
        S.shErr++;
        if (S.shErr > 8) {
            S.lightValid = false;
            log("shadow 渲染连续失败(已停止重试,重启光影后恢复): " + err);
            S.shLastTry = Date.now(); // 压住 150ms 节流,避免每帧空转
        } else {
            log("shadow 渲染失败(" + S.shErr + "): " + err);
            S.shDirty = true; // 下轮节流后继续重试,避免永久失去阴影
        }
    }
}

// 诊断: 用 CPU 按与 shader 相同公式,重建"屏幕偏下一点"像素的世界坐标
// 及其在光空间的位置,输出到日志判断是 world 错还是 lightVP 错。
function dbgReproj() {
    try {
        if (!S.invVP || !S.lightVP) return "dbg: invVP/lightVP 缺失";
        var ndc = [0, 0, 0.5, 1]; // uv(0.5,0.5) 深度中间
        var q = [0, 0, 0, 0];
        for (var r = 0; r < 4; r++) {
            var s = 0;
            for (var k = 0; k < 4; k++) s += S.invVP[k * 4 + r] * ndc[k];
            q[r] = s;
        }
        var wx = q[0] / q[3] + S.camC[0], wy = q[1] / q[3] + S.camC[1], wz = q[2] / q[3] + S.camC[2];
        var d = [wx - S.lightCx, wy - S.lightCy, wz - S.lightCz, 1];
        var lc = [0, 0, 0, 0];
        for (var r = 0; r < 4; r++) {
            var s = 0;
            for (var k = 0; k < 4; k++) s += S.lightVP[k * 4 + r] * d[k];
            lc[r] = s;
        }
        var dx = q[0], dy = q[1], dz = q[2], dw = q[3];
        return "中心重建=(" + wx.toFixed(1) + "," + wy.toFixed(1) + "," + wz.toFixed(1)
            + ") 光lnd=(" + (lc[0] / lc[3]).toFixed(2) + "," + (lc[1] / lc[3]).toFixed(2) + "," + (lc[2] / lc[3]).toFixed(2)
            + ") q.w=" + dw.toFixed(2) + " cam=(" + S.camC[0].toFixed(0) + "," + S.camC[1].toFixed(0) + "," + S.camC[2].toFixed(0)
            + ") lightC=(" + S.lightCx.toFixed(0) + "," + S.lightCy.toFixed(0) + "," + S.lightCz.toFixed(0) + ")";
    } catch (e) { return "dbg err: " + e; }
}

// 静态火把扫描(低频): 只收集火把,手持光每帧在 updateHandLight 里实时跟手
function scanTorches() {
    if (typeof player === "undefined" || !player || typeof level === "undefined") return;
    try {
        var px = Math.floor(player.getX()), py = Math.floor(player.getY()), pz = Math.floor(player.getZ());
        var R = 8, list = [];
        for (var x = px - R; x <= px + R && list.length < 7; x++)
            for (var y = py - R; y <= py + R && list.length < 7; y++)
                for (var z = pz - R; z <= pz + R && list.length < 7; z++) {
                    if (level.getBlock(x, y, z) === 50) // torch
                        list.push([x + 0.5, y + 0.5, z + 0.5]);
                }
        S.staticList = list;
        S.torchR = 7.0;
    } catch (e) {}
}

// 发光物品判定: 返回光半径(0=不发光)
function glowInfo(hid) {
    if (hid === 50) return 5.5;   // 火把
    if (hid === 89) return 8;     // 萤石
    if (hid === 327) return 8;    // 岩浆桶
    if (hid === 91) return 7;     // 南瓜灯
    return 0;
}

// 每合成帧调用: 手持光源实时跟手(无延迟),水下检测同步
function updateHandLight() {
    if (typeof player === "undefined" || !player || typeof level === "undefined") return;
    S.staticList = S.staticList || [];
    var handPos = null;
    try {
        if (typeof player.getHeldItemId === "function") {
            var gr = glowInfo(player.getHeldItemId());
            if (gr > 0) {
                var yr = player.getYRot() * Math.PI / 180, pr = player.getXRot() * Math.PI / 180;
                var fx = -Math.sin(yr) * Math.cos(pr), fy = -Math.sin(pr), fz = Math.cos(yr) * Math.cos(pr);
                var px = Math.floor(player.getX()), py = Math.floor(player.getY()), pz = Math.floor(player.getZ());
                handPos = [px + 0.5 + fx * 0.85, py + 1.45 + fy * 0.4 - 0.35, pz + 0.5 + fz * 0.85];
                S.torchR = gr;
            }
        }
        // 水下检测(头在水里→去雾更清晰)
        var hx = Math.floor(player.getX()), hz = Math.floor(player.getZ());
        var hy = Math.floor(player.getY() + 1.5);
        var wb = level.getBlock(hx, hy, hz);
        S.underWater = (wb === 8 || wb === 9);
    } catch (e) {}
    var L = S.staticList.concat(handPos ? [handPos] : []);
    if (L.length != S.torchN)
        log("光源: " + L.length + " 处");
    S.torchList = L;
    S.torchN = L.length;
}

// 每合成帧调用: 维护昼夜强度与主相机逆矩阵,必要时重渲阴影图。
function updateShadowFrame() {
    if (S.shUnsupported) return;
    if (typeof Render.world !== "function" ||
        typeof Render.getWorldMatrices !== "function") {
        S.shUnsupported = true;
        log("此构建缺少 Render.world/getWorldMatrices,阴影不可用");
        return;
    }
    var P = Render.getProjectionMatrix();
    var V = Render.getModelViewMatrix();
    S.invVP = null;
    if (P && P.length >= 16 && V && V.length >= 16)
        S.invVP = m4inv(m4mul(P, V));
    if (!S.invVP) {
        // 矩阵奇异(理论上不会): 关阴影,避免全屏错误重建。
        S.lightValid = false;
        S.shadEff = 0;
        return;
    }
    var c = Render.getCamera();
    S.camC = [c[0], c[1], c[2]];
    S.dayFactor = dayFactor();
    // 影强度不再被深夜压到看不见: 保底为 cfg.shad 的 50%,白天更强。
    S.shadEff = cfg.shad * (0.5 + 0.5 * S.dayFactor);
    if (S.shadEff <= 0.01) return;              // 阴影关:不维护阴影图
    // lightValid 未就绪时也走节流重试(150ms 间隔),确保初始一定渲出阴影图。
    if (S.lightValid && !shadowNeedsRedraw()) return;
    if (!S.lightValid && !shadowNeedsRedraw() && S.shRetried) return;
    S.shRetried = true;
    renderShadowMap();
}

// ── shader 构建(沿用 rc1-3 已验证模式) ──
function compile(type, src) {
    var s = GL.glCreateShader(type);
    GL.glShaderSource(s, src);
    GL.glCompileShader(s);
    var v = GL.glGetShaderLog(s);
    if (v != "OK") { log((type == GL.VERTEX_SHADER ? "vs: " : "fs: ") + v); return 0; }
    return s;
}

function build() {
    var vs = compile(GL.VERTEX_SHADER, [
        "#version 130",
        "in vec2 aPos;",
        "out vec2 vUv;",
        "void main(){ vUv = aPos * 0.5 + 0.5; gl_Position = vec4(aPos, 0.0, 1.0); }"
    ].join("\n"));
    if (!vs) return 0;

    var fs = compile(GL.FRAGMENT_SHADER, [
        "#version 130",
        "uniform sampler2D u_scene;",
        "uniform sampler2D u_mask;",
        "uniform sampler2D u_refl;",
        "uniform sampler2D u_depth;",
        "uniform sampler2D u_shadow;",
        "uniform float u_near, u_far;",
        "uniform float u_time, u_flip, u_diag;",
        "uniform float u_exposure, u_sat, u_con, u_tone;",
        "uniform float u_fog, u_fogR, u_fogG, u_fogB;",
        "uniform float u_wave, u_tint, u_str, u_vig;",
        "uniform float u_shad, u_shadowTexel;",
        "uniform float u_srev;",
        "uniform float u_flipY;",
        "uniform float u_sdep;",
        "uniform float u_vani;",
        "uniform float u_wtest;",
        "uniform vec3 u_torches[8];",
        "uniform float u_torchN, u_torchR;",
        "uniform float u_under;",
        "uniform mat4 u_invVP;",      // inverse(main proj * view)
        "uniform vec3 u_cam;",        // main camera world origin (chunk translate)
        "uniform mat4 u_lightVP;",    // light ortho proj * view
        "uniform vec3 u_lightC;",     // light camera world origin
        "in vec2 vUv;",
        "out vec4 fragColor;",
        // 线性深度(窗口深度 0..1 → 视空间距离)
        "float linDepth(vec2 uv){",
        "  float d = texture(u_depth, uv).r;",
        "  float z = d * 2.0 - 1.0;",
        "  return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near));",
        "}",
        // ACES 近似(拟合式),t=1 全量 / 0 关闭
        "vec3 aces(vec3 x, float t){",
        "  vec3 a = (x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14);",
        "  return mix(x, clamp(a, 0.0, 1.0), t);",
        "}",
        // 阴影因子: 0=全被挡 / 1=全亮。主深度重建世界坐标 → 光空间 → PCF。
        "float calcShadow(vec2 uv, float lin){",
        "  if (u_shad <= 0.001) return 1.0;",
        "  if (lin >= u_far * 0.97) return 1.0;",   // 天空 / 超出视距
        "  float d = texture(u_depth, uv).r;",
        "  float zc = d * 2.0 - 1.0;",
        "  if (u_sdep > 0.5) zc = -zc;   // 深度方向反转开关",
        "  float sy = mix(uv.y, 1.0 - uv.y, u_flipY);",
        "  vec3 ndc = vec3(vec2(uv.x, sy) * 2.0 - 1.0, zc);",
        "  vec4 q = u_invVP * vec4(ndc, 1.0);",
        "  vec3 world = q.xyz / q.w + u_cam;",
        "  vec4 lc = u_lightVP * vec4(world - u_lightC, 1.0);",
        "  vec3 lnd = lc.xyz / lc.w;",
        "  float bm = max(abs(lnd.x), abs(lnd.y));",
        "  if (bm >= 1.0) return 1.0;",
        "  float fade = smoothstep(0.86, 1.0, bm);",   // 阴影图边缘淡出
        "  vec2 luv = lnd.xy * 0.5 + 0.5;",
        "  float myD = lnd.z * 0.5 + 0.5;",
        "  float bias = 0.0012;",
        "  float lit = 0.0;",
        "  for (int yy = -1; yy <= 1; yy++) {",
        "    for (int xx = -1; xx <= 1; xx++) {",
        "      vec2 suv = clamp(luv + vec2(float(xx), float(yy)) * u_shadowTexel, 0.0, 1.0);",
        "      float sd = texture(u_shadow, suv).r;",
        "      if (u_srev > 0.5) lit += step(sd, myD + bias);",
        "      else lit += step(myD - bias, sd);   // 有更近遮挡 => 被挡",
        "    }",
        "  }",
        "  lit /= 9.0;",
        "  float r9 = mix(lit, 1.0, fade);",
        "  if (!(r9 >= 0.0)) r9 = 1.0;",
        // 火把等点光源: 按世界距离把阴影抬回亮(模拟火把照亮影区)
        "  for (int i = 0; i < 8; i++) {",
        "    if (float(i) >= u_torchN) break;",
        "    float dd = distance(world, u_torches[i]);",
        "    float at = clamp(1.0 - dd / u_torchR, 0.0, 1.0);",
        "    r9 = mix(r9, 1.0, at * at);",
        "  }",
        "  return r9;",
        "}",
        // 世界坐标重建(与 calcShadow 同源;供点光源照亮)
        "vec3 rebuildWorld(vec2 uv, float lin){",
        "  float d = texture(u_depth, uv).r;",
        "  float zc = d * 2.0 - 1.0;",
        "  if (u_sdep > 0.5) zc = -zc;",
        "  float sy = mix(uv.y, 1.0 - uv.y, u_flipY);",
        "  vec3 ndc = vec3(vec2(uv.x, sy) * 2.0 - 1.0, zc);",
        "  vec4 q = u_invVP * vec4(ndc, 1.0);",
        "  return q.xyz / q.w + u_cam;",
        "}",
        // 点光源暖光(火把/手持发光块): 增亮+暖调,暗处与影区也有光
        "vec3 torchLight(vec2 uv, float lin){",
        "  if (u_torchN <= 0.0) return vec3(0.0);",
        "  if (lin >= u_far * 0.97) return vec3(0.0);",
        "  vec3 world = rebuildWorld(uv, lin);",
        "  vec3 acc = vec3(0.0);",
        "  for (int i = 0; i < 8; i++) {",
        "    if (float(i) >= u_torchN) break;",
        "    float dd = distance(world, u_torches[i]);",
        "    float at = clamp(1.0 - dd / u_torchR, 0.0, 1.0);",
        "    acc += vec3(1.0, 0.7, 0.42) * (at * at) * (0.30 + 0.28 * at);",
        "  }",
        "  return acc;",
        "}",
        "void main(){",
        "  vec4 m = texture(u_mask, vUv);",
        "  float isWater = m.r + m.g + m.b;",
        // 诊断视图
        "  if (u_diag > 0.5 && u_diag < 1.5) { fragColor = m; return; }",
        "  if (u_diag > 1.5 && u_diag < 2.5) { fragColor = texture(u_refl, vUv); return; }",
        "  if (u_diag > 2.5 && u_diag < 3.5) { float d = linDepth(vUv); float g = clamp(1.0 - d / u_far, 0.0, 1.0); fragColor = vec4(vec3(g), 1.0); return; }",
        "  if (u_diag > 3.5) { float sd = texture(u_shadow, vUv).r; float g = clamp(sd * 1.0, 0.0, 1.0); fragColor = vec4(vec3(g), 1.0); return; }",
        //
        "  vec3 col = texture(u_scene, vUv).rgb;",
        "  float lin = linDepth(vUv);",
        // ── 水反(BSL/Complementary 式:近处半透明看底,远处掠射反射,波纹流动+波光) ──
        "  if (isWater > 0.09 && u_vani < 0.5) {",
        // /wtest: 整片水面红蓝强闪,验证水反块在跑+时间在动
        "    if (u_wtest > 0.5) {",
        "      col = vec3(0.5 + 0.5 * sin(u_time * 20.0), 0.1, 0.5 + 0.5 * cos(u_time * 20.0));",   // 红<->蓝快闪
        "    } else {",
        "    vec2 ruv = vec2(vUv.x, mix(vUv.y, 1.0 - vUv.y, u_flip));",
        // 波纹: 低频大浪 + 高频细纹,随时间流动
        "    ruv.x += (sin(vUv.y * 24.0 + u_time * 1.8) * 0.0025 + sin(vUv.y * 8.0 + u_time * 1.2) * 0.001) * u_wave;",
        "    ruv.y += (sin(vUv.x * 18.0 - u_time * 1.6) * 0.0012 + sin(vUv.x * 47.0 + u_time * 2.6) * 0.0006) * u_wave;",
        "    ruv = clamp(ruv, 0.0, 1.0);",
        "    vec3 refl = texture(u_refl, ruv).rgb;",
        // 菲涅尔: 近处(画面下部)反射弱能看到水底/本色,远处掠射反射渐强但封顶
        "    float fres = smoothstep(0.05, 0.55, vUv.y);",
        "    float k = clamp(u_str * (0.10 + 0.90 * fres), 0.0, 1.0);",
        "    col = mix(col, refl, k * 0.85);",   // 反射上限压缩,避免像镜子
        // 水体本色(半透明水感)
        "    vec3 waterCol = vec3(0.03, 0.20, 0.34);",
        "    float tintF = u_tint * (0.65 + 0.35 * (1.0 - fres));",  // 近处更多本色
        "    col = mix(col, waterCol, tintF);",
        // 柔和单方向波光(弯曲波前向左传播,无交叉堆叠感)
        "    float wvA = sin(vUv.x * 9.0 - u_time * 1.5 + sin(vUv.y * 4.0) * 2.2);",
        "    float wnA = (0.5 + 0.5 * wvA);",
        "    col += vec3(0.16, 0.22, 0.28) * wnA * 0.55;",
        "    col += vec3(0.35, 0.45, 0.5) * (0.5 + 0.5 * wvA) * wvA * wvA * 0.25;",
        "    col = clamp(col, 0.0, 1.0);",
        "    }",
        "  }",
        // ── 太阳阴影(灰影;火把/亮区按原亮度抵消部分阴影) ──
        "  float shd = calcShadow(vUv, lin);",
        "  if (isWater > 0.09) shd = 1.0;",
        "  float litF = mix(1.0, shd, u_shad);",      // 影区保留 (1-u_shad) 底亮度
        "  float lum0 = dot(col, vec3(0.299, 0.587, 0.114));",
        "  float envF = smoothstep(0.45, 0.9, lum0);",  // 火把照亮的亮区
        "  litF = mix(litF, 1.0, envF * 0.65);",        // 抵消最多 65% 阴影
        "  col *= litF;",
        // 点光源照亮(火把/手持发光块): 暖光增亮,给暗处与影区补光
        "  if (u_torchN > 0.5 && isWater < 0.09) {",
        "    vec3 tl = torchLight(vUv, lin);",
        "    col = col * (1.0 + tl * 0.6) + tl * 0.35;",,
        "    col = clamp(col, 0.0, 1.0);",
        "  }",
        // ── 空气透视雾(远处向淡蓝白雾色融,天空色近似雾色故天空不变灰) ──
        "  float fd = clamp(lin / u_far, 0.0, 1.0);",
        "  vec3 fogC = vec3(u_fogR, u_fogG, u_fogB);",
        "  float fogK = (fd * fd) * u_fog;",
        "  if (u_under > 0.5) { fogK *= 0.06; fogC = vec3(0.10, 0.24, 0.32); }",   // 水下: 几乎去雾,留清透深蓝
        "  col = mix(col, fogC, fogK);",
        // ── 曝光/白平衡/色调映射/饱和/对比 ──
        "  col *= u_exposure;",
        "  col = aces(col, u_tone * 0.5);",           // 0.5 档先温和
        "  if (u_tone > 1.5) col = aces(col, 1.0);",  // 2 档全量
        "  float l = dot(col, vec3(0.2126, 0.7152, 0.0722));",
        "  col = mix(vec3(l), col, u_sat);",
        "  col = (col - 0.5) * u_con + 0.5;",
        "  col = clamp(col, 0.0, 1.0);",
        // ── 天空更蓝(视距外天空像素向深蓝拉;白云亮度高,保持白色不变) ──
        "  if (lin >= u_far * 0.97) {",
        "    float skyW = smoothstep(0.65, 0.88, dot(col, vec3(0.299, 0.587, 0.114)));",
        "    col = mix(col, vec3(0.33, 0.58, 0.97), 0.55 * (1.0 - skyW));",
        "  }",
        // ── 轻微暗角 ──
        "  float d2 = length(vUv - 0.5) * 1.15;",
        "  col *= 1.0 - u_vig * smoothstep(0.45, 0.9, d2);",
        // 尾段同向补光(很弱,只给一点点光泽)
        "    if (isWater > 0.09 && u_vani < 0.5 && u_wave > 0.01) {",
        "    float wv2 = sin(vUv.x * 9.0 - u_time * 1.5 + sin(vUv.y * 4.0) * 2.2);",
        "    float wn = (0.5 + 0.5 * wv2);",
        "    col += vec3(0.10, 0.13, 0.16) * wn * (0.12 * u_wave);",
        "    col += vec3(0.3, 0.38, 0.42) * wv2 * wv2 * (0.06 * u_wave);",
        "    col = clamp(col, 0.0, 1.0);",
        "  }",
        "  if (!(col.x >= 0.0)) col = vec3(0.0, 0.0, 0.0);   // NaN 兜底: 黑而非白/花",
        "  fragColor = vec4(col, 1.0);",
        "}"
    ].join("\n"));
    if (!fs) return 0;

    var p = GL.glCreateProgram();
    GL.glAttachShader(p, vs);
    GL.glAttachShader(p, fs);
    GL.glLinkProgram(p);
    var pl = GL.glGetProgramLog(p);
    if (pl != "OK") { log("link: " + pl); return 0; }

    var b = GL.glGenBuffers();
    GL.glBindBuffer(GL.ARRAY_BUFFER, b);
    GL.glBufferData(GL.ARRAY_BUFFER, [-1,-1, 1,-1, -1,1, 1,-1, 1,1, -1,1], GL.STATIC_DRAW);
    GL.glBindBuffer(GL.ARRAY_BUFFER, 0);

    S.prog = p;
    S.vbo = b;
    S.loc = {
        scene: GL.glGetUniformLocation(p, "u_scene"),
        mask:  GL.glGetUniformLocation(p, "u_mask"),
        refl:  GL.glGetUniformLocation(p, "u_refl"),
        depth: GL.glGetUniformLocation(p, "u_depth"),
        shadow: GL.glGetUniformLocation(p, "u_shadow"),
        shadowTexel: GL.glGetUniformLocation(p, "u_shadowTexel"),
        rev:  GL.glGetUniformLocation(p, "u_srev"),
        torN: GL.glGetUniformLocation(p, "u_torchN"),
        torR: GL.glGetUniformLocation(p, "u_torchR"),
        under: GL.glGetUniformLocation(p, "u_under"),
        flipY: GL.glGetUniformLocation(p, "u_flipY"),
        sdep:  GL.glGetUniformLocation(p, "u_sdep"),
        invVP: GL.glGetUniformLocation(p, "u_invVP"),
        cam:   GL.glGetUniformLocation(p, "u_cam"),
        lightVP: GL.glGetUniformLocation(p, "u_lightVP"),
        lightC:  GL.glGetUniformLocation(p, "u_lightC"),
        shad:  GL.glGetUniformLocation(p, "u_shad"),
        near:  GL.glGetUniformLocation(p, "u_near"),
        far:   GL.glGetUniformLocation(p, "u_far"),
        time:  GL.glGetUniformLocation(p, "u_time"),
        flip:  GL.glGetUniformLocation(p, "u_flip"),
        diag:  GL.glGetUniformLocation(p, "u_diag"),
        exposure: GL.glGetUniformLocation(p, "u_exposure"),
        sat:   GL.glGetUniformLocation(p, "u_sat"),
        con:   GL.glGetUniformLocation(p, "u_con"),
        tone:  GL.glGetUniformLocation(p, "u_tone"),
        fog:   GL.glGetUniformLocation(p, "u_fog"),
        fogR:  GL.glGetUniformLocation(p, "u_fogR"),
        fogG:  GL.glGetUniformLocation(p, "u_fogG"),
        fogB:  GL.glGetUniformLocation(p, "u_fogB"),
        wave:  GL.glGetUniformLocation(p, "u_wave"),
        tint:  GL.glGetUniformLocation(p, "u_tint"),
        vani:  GL.glGetUniformLocation(p, "u_vani"),
        wtest: GL.glGetUniformLocation(p, "u_wtest"),
        str:   GL.glGetUniformLocation(p, "u_str"),
        vig:   GL.glGetUniformLocation(p, "u_vig")
    };
    // 火把数组 uniform 的 location(逐元素)
    S.torLoc = [];
    for (var _t = 0; _t < 8; _t++)
        S.torLoc.push(GL.glGetUniformLocation(p, "u_torches[" + _t + "]"));
    log("prog=" + p);
    return p;
}

// 从投影矩阵反推 near/far(列主序 gluPerspective)
function refreshCam() {
    var p = Render.getProjectionMatrix();
    if (!p || p.length < 15) return;
    var n = Math.abs(p[14] / (p[10] - 1));
    var f = Math.abs(p[14] / (p[10] + 1));
    if (n > 0.001 && f > n) { near = n; far = f; }
}

function draw() {
    var L = S.loc;
    function u1(loc, v) { if (loc >= 0) GL.glUniform1f(loc, v); }
    u1(L.near, near); u1(L.far, far);
    u1(L.time, (S.frame || 0) * 0.018);   // 帧计数时钟(慢速波动): 帧数递增 => 时间必动
    u1(L.flip, S.flip); u1(L.diag, S.diag);
    u1(L.exposure, cfg.exposure); u1(L.sat, cfg.sat); u1(L.con, cfg.con); u1(L.tone, cfg.tone);
    u1(L.fog, cfg.fog); u1(L.fogR, cfg.fogR); u1(L.fogG, cfg.fogG); u1(L.fogB, cfg.fogB);
    u1(L.wave, cfg.wave); u1(L.tint, cfg.tint); u1(L.str, cfg.str); u1(L.vig, cfg.vig);
    u1(L.vani, cfg.vanilla ? 1 : 0);
    u1(L.wtest, cfg.wtest ? 1 : 0);
    // ── 阴影 uniforms ──
    u1(L.shad, S.lightValid ? S.shadEff : 0);
    u1(L.shadowTexel, 1.0 / SHADOW_SIZE);
    u1(L.rev, S.shRev ? 1 : 0);
    u1(L.flipY, S.shFlipY ? 1 : 0);
    u1(L.sdep, S.shDep ? 1 : 0);
    // 火把点光源(照亮阴影): 位置数组
    u1(L.torN, S.torchN);
    u1(L.torR, S.torchR);
    u1(L.under, S.underWater ? 1 : 0);
    for (var ti = 0; ti < 6; ti++) {
        if (ti >= S.torchList.length) break;
        var tl = S.torLoc[ti];
        if (tl >= 0)
            GL.glUniform3f(tl, S.torchList[ti][0], S.torchList[ti][1], S.torchList[ti][2]);
    }
    if (S.invVP && L.invVP >= 0) GL.glUniformMatrix4fv(L.invVP, S.invVP);
    if (L.cam >= 0) GL.glUniform3f(L.cam, S.camC[0], S.camC[1], S.camC[2]);
    if (S.lightValid && S.lightVP && L.lightVP >= 0) {
        GL.glUniformMatrix4fv(L.lightVP, S.lightVP);
        if (L.lightC >= 0) GL.glUniform3f(L.lightC, S.lightCx, S.lightCy, S.lightCz);
    }

    GL.glBindBuffer(GL.ARRAY_BUFFER, S.vbo);
    GL.glEnableVertexAttribArray(0);
    GL.glVertexAttribPointer(0, 2, GL.FLOAT, false, 0, 0);
    GL.glDrawArrays(GL.TRIANGLES, 0, 6);
    GL.glDisableVertexAttribArray(0);
    GL.glBindBuffer(GL.ARRAY_BUFFER, 0);
}

function onRenderComposite(sceneTex, w, h, maskTex, reflTex) {
    S.frame = (S.frame || 0) + 1;
    if (!S.ok) return;
    updateHandLight();   // 手持光源每帧跟手,水下检测同步
    if (!sceneTex || !maskTex || !reflTex) return;
    if (!S.prog && !S.tried) {
        S.tried = true;
        var p = build();
        if (!p) { msg("shader 编译失败,已回退(看日志)"); Render.enable(false); return; }
    }
    if (!S.prog) return;
    var depthTex = Render.depthTexture();
    if (!depthTex) return;
    refreshCam();
    // 阴影维护(必要时把 0/1 层地形渲进自建 D24 阴影图)
    updateShadowFrame();

    GL.glViewport(0, 0, w, h);
    GL.glDisable(GL.DEPTH_TEST);
    GL.glDisable(GL.BLEND);
    GL.glUseProgram(S.prog);

    GL.glActiveTexture(GL.TEXTURE0);
    GL.glBindTexture(GL.TEXTURE_2D, sceneTex);
    GL.glUniform1i(S.loc.scene, 0);
    GL.glActiveTexture(GL.TEXTURE1);
    GL.glBindTexture(GL.TEXTURE_2D, maskTex);
    GL.glUniform1i(S.loc.mask, 1);
    GL.glActiveTexture(GL.TEXTURE2);
    GL.glBindTexture(GL.TEXTURE_2D, reflTex);
    GL.glUniform1i(S.loc.refl, 2);
    GL.glActiveTexture(GL.TEXTURE3);
    GL.glBindTexture(GL.TEXTURE_2D, depthTex);
    GL.glUniform1i(S.loc.depth, 3);
    if (S.lightValid && S.shTex && S.loc.shadow >= 0) {
        GL.glActiveTexture(GL.TEXTURE0 + 4);
        GL.glBindTexture(GL.TEXTURE_2D, S.shTex);
        GL.glUniform1i(S.loc.shadow, 4);
    }

    draw();

    if (S.lightValid && S.shTex) {
        GL.glActiveTexture(GL.TEXTURE0 + 4);
        GL.glBindTexture(GL.TEXTURE_2D, 0);
    }
    GL.glActiveTexture(GL.TEXTURE3); GL.glBindTexture(GL.TEXTURE_2D, 0);
    GL.glActiveTexture(GL.TEXTURE2); GL.glBindTexture(GL.TEXTURE_2D, 0);
    GL.glActiveTexture(GL.TEXTURE1); GL.glBindTexture(GL.TEXTURE_2D, 0);
    GL.glActiveTexture(GL.TEXTURE0); GL.glBindTexture(GL.TEXTURE_2D, 0);
    GL.glUseProgram(0);
}

// 扫描玩家附近水面(方块8流动/9静水),返回最高水方块顶部 y
function scanWater(radius) {
    radius = radius || 24;
    var cx = Math.floor(player.getX());
    var cy = Math.floor(player.getY());
    var cz = Math.floor(player.getZ());
    var found = -999;
    var R = radius;
    var step = R > 20 ? 2 : 1;
    for (var y = cy + 10; y > cy - 24; y--) {
        for (var dx = -R; dx <= R; dx += step) {
            for (var dz = -R; dz <= R; dz += step) {
                if (dx * dx + dz * dz > R * R) continue;
                var id = level.getBlock(cx + dx, y, cz + dz);
                if (id == 8 || id == 9) {
                    if (y + 1 > found) found = y + 1;
                }
            }
        }
        if (found > -900) break;
    }
    return found;
}

function start() {
    if (!(typeof GL !== "undefined" && typeof Render !== "undefined")) {
        msg("此构建没有 GL/Render API");
        return;
    }
    userOff = false;
    Render.enable(true);
    S.ok = true;
    S.prog = 0; S.tried = false;
    msg("光影已开: /mode /exp /sat /fog /tint /shad /sun 调参, /shoff 关");
}

function stop() {
    Render.enable(false);
    S.ok = false;
    userOff = true;
    msg("已关,恢复原画面");
}

function onTick() {
    if (userOff) return;
    if (typeof GL === "undefined" || typeof Render === "undefined") return;
    autoTick++;
    if (!S.ok && autoTick == 120) start();
    if (S.wy < -100 && autoTick > 0 && (autoTick % 40) == 0) {
        var wy = scanWater(16);
        if (wy > -900) {
            S.wy = wy;
            Render.setWaterLevel(wy);
            msg("找到水面 y=" + wy + ",水反生效");
        }
    }
    // 水面可能变化(走了/涨水): 已找到后每 400 tick 复查一次
    if (S.wy > -100 && autoTick - lastScan > 400) {
        lastScan = autoTick;
        var wy = scanWater(20);
        if (wy > -900 && Math.abs(wy - S.wy) > 0.5) {
            S.wy = wy;
            Render.setWaterLevel(wy);
        }
    }
    // 光源扫描(火把+手持发光): 方块改动立即重扫,平时每 10 tick(0.5s)刷新
    if (S.torDirty || (autoTick - S.torScan) > 10) {
        S.torScan = autoTick;
        S.torDirty = false;
        scanTorches();
    }
}

function onChat(raw) {
    var m = raw;
    if (m == "/sh")    { start(); return; }
    if (m == "/shoff") { stop(); return; }
    if (m.indexOf("/mode") == 0) {
        var v = parseInt(m.slice(6));
        if (isNaN(v)) v = 1;
        v = ((v % 4) + 4) % 4;
        // 模式 → cfg 组合(阴影随 mode 关/开)
        if (v == 0) { cfg.fog = 0; cfg.str = 0; cfg.sat = 1; cfg.con = 1; cfg.tone = 0; cfg.vig = 0; cfg.exposure = 1; cfg.tint = 0; cfg.shad = 0; }
        if (v == 1) { cfg.fog = 0.30; cfg.str = 0.55; cfg.sat = 1.12; cfg.con = 1.10; cfg.tone = 2; cfg.vig = 0.12; cfg.exposure = 1.00; cfg.tint = 0.15; cfg.shad = 0.45; }
        if (v == 2) { cfg.fog = 0.30; cfg.str = 0;    cfg.sat = 1.12; cfg.con = 1.10; cfg.tone = 2; cfg.vig = 0.12; cfg.exposure = 1.00; cfg.tint = 0; cfg.shad = 0; }
        if (v == 3) { cfg.fog = 0;    cfg.str = 0.85; cfg.sat = 1;   cfg.con = 1;    cfg.tone = 0; cfg.vig = 0;    cfg.exposure = 1.00; cfg.tint = 0.10; cfg.shad = 0; }
        if (!S.ok) start();
        S.shDirty = true;
        msg("mode=" + v + " (0原样 1完整 2调色+雾 3仅水反)");
        return;
    }
    if (m == "/diag") {
        S.diag = (S.diag + 1) % 5;
        msg("视图=" + (S.diag == 0 ? "合成" : (S.diag == 1 ? "mask(水)" : (S.diag == 2 ? "refl(反射)" : (S.diag == 3 ? "深度" : "阴影图")))));
        return;
    }
    if (m == "/diag") {
        S.diag = (S.diag + 1) % 4;
        msg("视图=" + (S.diag == 0 ? "合成" : (S.diag == 1 ? "mask(水)" : (S.diag == 2 ? "refl(反射)" : "深度"))));
        return;
    }
    if (m == "/scan") {
        var wy = scanWater(32);
        if (wy > -900) { S.wy = wy; Render.setWaterLevel(wy); msg("水面 y=" + wy); }
        else msg("没扫到水(方块8/9)");
        return;
    }
    if (m.indexOf("/wy ") == 0) {
        var v = parseFloat(m.slice(4));
        S.wy = v;
        Render.setWaterLevel(v);
        msg("水面 y=" + v);
        return;
    }
    if (m == "/flip") { S.flip = S.flip ? 0 : 1; msg("反射翻转=" + (S.flip ? "开" : "关") + "(倒影方向不对就切)"); return; }
    if (m == "/cam") {
        var c = Render.getCamera();
        msg("cam=(" + c[0].toFixed(1) + "," + c[1].toFixed(1) + "," + c[2].toFixed(1) + ") yaw=" + c[3].toFixed(1) + " pitch=" + c[4].toFixed(1));
        return;
    }
    // ── 阴影命令 ──
    if (m == "/shinfo") {
        msg("光影 " + VER + ": 强度=" + cfg.shad + " 昼夜=" + (S.dayFactor == null ? "?" : S.dayFactor.toFixed(2))
            + " 生效=" + (S.lightValid ? (S.shadEff || 0).toFixed(2) : "关")
            + " 太阳az/el=" + cfg.sunAz + "/" + cfg.sunEl
            + " wave=" + cfg.wave + " str=" + cfg.str + " tint=" + cfg.tint
            + " 开关[uv翻/深翻/比较翻]=" + (S.shFlipY ? 1 : 0) + "/" + (S.shDep ? 1 : 0) + "/" + (S.shRev ? 1 : 0)
            + (S.lightValid ? " 图有效" : " 无图"));
        return;
    }
    if (m == "/sforce") { S.shDirty = true; msg("强制重渲阴影(下帧)"); return; }
    if (m == "/srev") {
        S.shRev = !S.shRev;
        S.shDirty = true;
        msg("阴影深度比较=" + (S.shRev ? "反向" : "正向") + ",已重渲");
        return;
    }
    if (m == "/sflip") {
        S.shFlipY = !S.shFlipY;
        S.shDirty = true;
        msg("重建UV翻转=" + (S.shFlipY ? "开" : "关") + ",已重渲");
        return;
    }
    if (m == "/sdep") {
        S.shDep = !S.shDep;
        S.shDirty = true;
        msg("深度方向反转=" + (S.shDep ? "开" : "关") + ",已重渲");
        return;
    }
    if (m == "/shad") {
        cfg.shad = (cfg.shad > 0) ? 0 : 0.45;
        S.shDirty = true;
        msg("阴影 = " + (cfg.shad > 0 ? "正常灰影(强度" + cfg.shad + ")" : "关"));
        return;
    }
    if (m.indexOf("/shad ") == 0) {
        var v = parseFloat(m.slice(6));
        if (!isNaN(v)) { cfg.shad = Math.max(0, Math.min(1, v)); S.shDirty = true; msg("阴影强度 = " + cfg.shad); }
        return;
    }
    if (m.indexOf("/sun ") == 0) {
        var p = m.slice(5).split(/[ ,]+/);
        if (p.length >= 2) {
            var az = parseFloat(p[0]), el = parseFloat(p[1]);
            if (!isNaN(az) && !isNaN(el)) {
                cfg.sunAz = az; cfg.sunEl = Math.max(3, Math.min(89, el));
                S.shDirty = true;
                msg("太阳 az=" + cfg.sunAz + " el=" + cfg.sunEl + "(方位/仰角度)");
            }
        }
        return;
    }
    function setNum(prefix, key, isF) {
        if (m.indexOf(prefix) != 0) return false;
        var v = isF ? parseFloat(m.slice(prefix.length)) : parseInt(m.slice(prefix.length));
        if (isNaN(v)) return false;
        cfg[key] = v;
        msg(prefix + " = " + v);
        if (!S.ok) start();
        return true;
    }
    if (setNum("/exp ", "exposure", true)) return;
    if (setNum("/sat ", "sat", true)) return;
    if (setNum("/con ", "con", true)) return;
    if (setNum("/tone ", "tone", true)) return;
    if (setNum("/fog ", "fog", true)) return;
    if (setNum("/tint ", "tint", true)) return;
    if (setNum("/wave ", "wave", true)) return;
    if (m == "/water" || m == "/wr") {
        cfg.vanilla = !cfg.vanilla;
        msg("[水] " + (cfg.vanilla ? "原版水视图 ON(水反/波光已关,看原版水)" : "水反+波光 ON"));
        return;
    }
    if (setNum("/str ", "str", true)) return;
    if (m == "/wtest") {
        cfg.wtest = !cfg.wtest;
        msg("[测试] 水面闪屏测试 " + (cfg.wtest ? "ON(水面应红蓝快闪)" : "OFF") + " timeLoc=" + (S.loc ? S.loc.time : "?") + " frame=" + (S.frame || 0));
        return;
    }
    if (m.indexOf("/fogcol ") == 0) {
        var p = m.slice(8).split(" ");
        if (p.length >= 3) {
            cfg.fogR = parseFloat(p[0]); cfg.fogG = parseFloat(p[1]); cfg.fogB = parseFloat(p[2]);
            msg("雾色 = " + cfg.fogR + "," + cfg.fogG + "," + cfg.fogB);
            if (!S.ok) start();
        }
        return;
    }
}

// 世界方块被挖/被放 → 下帧重渲阴影图 + 重扫火把。
function onBreakBlock() { if (S.ok) S.shDirty = true; S.torDirty = true; }
function onPlaceBlock() { if (S.ok) S.shDirty = true; S.torDirty = true; }
