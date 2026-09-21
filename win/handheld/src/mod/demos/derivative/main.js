// name: Derivative
// author: dev
// version: 0.1.0
// description: Java 光影 Derivative Main d24.4.14 的合成级复刻(目标相似 70%)。
//   派生自 rc6.1-vol(体积雾/体积光/实时阴影/水反/火把光全保留)。
//   v1 新增: HDR 大气天空(太阳盘+辉光/月/星/极光, level.getTime 驱动昼夜自动太阳)
//   + 多 pass Bloom 链(提取亮部→半分辨率双方向高斯→叠加) + 调色对齐(Derivative
//   Grade: AcademyFit tonemap, 后调 1.0)。命令见 the mod develop/docs/11 §4。

var S = { prog: 0, vbo: 0, ok: false, tried: false, diag: 0, flip: 0, wy: -999,
    shFbo: 0, shColor: 0, shTex: 0, shReady: false, shDirty: false,
    shCx: 0, shCy: 0, shCz: 0, shRev: false, shFlipY: false, shDep: false,
    shErr: 0, shRetried: false, underWater: false,
    torLoc: [], torchN: 0, torchR: 8.0, torDirty: true, torScan: 0, torchList: [],
    lightVP: null, lightValid: false, lightCx: 0, lightCy: 0, lightCz: 0,
    shUnsupported: false, invVP: null, camC: [0, 0, 0], u_camLoc: -1,
    // v1: 多 pass 离屏
    fMain: 0, tMain: 0, fB0: 0, tB0: 0, fB1: 0, tB1: 0, fw: 0, fh: 0,
    pMain: 0, pBlur: 0, pFinal: 0, invProj: null, camRot: null,
    lumAz: 35, lumEl: 52, sunEl: 52, skyFlip: 0 };
var cfg = {
    exposure: 1.0, sat: 0.9, con: 1.0, tone: 1,     // 1=ACES(稳) 0=AgX; Film 氛围用外围: 饱和0.9+色温+暗角
    cas: 0.55, glow: 0.4, taa: 0,                    // A8 CAS锐化 / A10 bloom雾辉光 / A7 时域稳定(默认关=省一帧; /taa 1 开)
    fog: 0.04, fogR: 0.78, fogG: 0.84, fogB: 0.92,   // 距离雾(默认极低: 远/近方块同色干净; /fog 可调)
    wave: 1.0, tint: 0.18, str: 0.60, vig: 0.20,     // 水 / 暗角(Film 档≈VIGNETTE 3, 加强)
    mb: 0.55,                                            // A6 运动模糊(CPU 角速度→拖尾; 0 关; 静止必清晰)
    vanilla: false, wtest: false,
    shad: 0.45, vol: 2.2,                             // 阴影强度 / 体积雾+光强度(再浓: 默认2.2)
    sunAuto: 1,                                        // 自动昼夜太阳(level.getTime 驱动)
    auto: 2, tgt: 0.42, adown: 0.5, aup: 0.06,           // 曝光: 2=只压暗不抬(黑处黑, 默认) 1=全自动 0=关
    ndim: 0.38,                                             // 夜间场景压暗(夜≈白天 38% 亮度, /ndim 调)
    uwl: 0.75, caut: 0.6,                                   // C3 水下体积光 / E5 水下焦散
    cine: 0,                                             // 电影 21:9 黑边(用户判定无用; 打磨阶段再开), /cine 手动
    bloom: 0.32, blthr: 0.72,                          // bloom 强度 / 亮部提取阈
    aur: 0.45, stars: 0.6,                              // 极光 / 星星强度(夜晚)
    again: 4.0,                                         // AgX 输入增益(0.5~16, 画面亮度主旋钮)
    clouds: 1.0, cloudcov: 0.52, ssao: 0.0,              // 体积云 / 云覆盖阈 / 屏幕空间AO(0=关: 半球AO曾致视角大暗区, 排查后重做)
    cirrus: 0.5,                                            // B6 高空卷云强度(0 关)
    cloudshd: 0.85,                                        // 云影强度(地面移动影, 照原版 CloudShadow)
    sway: 1, swayS: 1,                                      // 植被摇摆 amp/speed(引擎默认关; 光影开着才摇, /sway /sways)
    gb: 1,                                                  // M1 亮度图入合成强度 0..1(默认开; 老引擎无 color2 自动降 0)
    waterday: 0,                                             // 水面白天亮度权重(0=恒夜深色样式[用户偏好]; 1=正常昼夜)
    shsky: 0.05,                                             // D3 影区天光染(默认近0: 洞内/无天空处不补天光 → 黑处黑; /shsky 可调)
    nl: 0.35,                                             // N·L 地表明暗强度(0 关, 1 拉满, 默认 0.35)
    dof: 0.1, focal: 5.0, ap: 2.8, afoc: 1,               // DoF=Film档强度0.1 + 自动对焦(看哪清哪); /dof 调 /afoc 切手动
    skyz: [0.24, 0.44, 0.92], skyh: [0.78, 0.88, 1.00], // 天顶 / 地平线色(白天, 屏幕目标色)
    wb: [0.985, 1.0, 1.03],                             // 白平衡(7200K 冷调近似, 命令 /wb r g b)
    sunb: 1.0,                                            // 太阳/辉光亮度(0.2~2 手调, 默认已压低不刺眼)
    suncol: [1.0, 0.98, 0.92]                           // 太阳盘色(高角度; 低角度向橙移)
};
var near = 0.05, far = 256.0;
var VER = "Derivative-v1";
var userOff = false, autoTick = 0;
var lastScan = 0;
var SHADOW_SIZE = 2048;
var SH = 64.0;
var SH_N = 8.0, SH_F = 240.0;
var SH_TEX = (2 * SH) / SHADOW_SIZE;

function log(s) { modLog("[deriv] " + s); }
function msg(s)  { if (typeof player !== "undefined" && player) player.sendMessage("[Derivative] " + s); }

// ── column-major mat4 helpers ──
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

// 阴影: 发光体方向 → 光相机角度(引擎 glRotate 后乘序, 勿改)
function dirToCamAngles(fx, fy, fz) {
    var a = Math.atan2(-fx, -fz) * 180 / Math.PI;
    var yaw = a - 180;
    var pitch = -Math.asin(fy) * 180 / Math.PI;
    return [yaw, pitch];
}

// 昼夜因子 0..1(与引擎亮度同相: frac=t/19200, 正午 frac≈0.25 最亮)
function dayFactor() {
    var t;
    try { t = level.getTime() % 19200; } catch (e) { t = 0; }
    var frac = t / 19200;
    var d = 2 * Math.sin(2 * Math.PI * frac) + 0.5;
    if (d < 0) d = 0;
    if (d > 1) d = 1;
    return 0.06 + 0.94 * d;
}

// ── v1: 自动昼夜太阳 ──
// 太阳高度 elDeg = 90*sin(2π·frac)(正午 +90), 白天 frac∈[0,0.5]。
// 方位 az: 白天 东(90)→南(180,正午)→西(270); 夜里月亮走"西→东"反向路径。
// S.lumAz/lumEl = 当前"上方发光体"(白天太阳 / 夜晚月亮)方向, 阴影与体积光用它。
// S.sunEl = 太阳真实高度(度, 可负) → 天空分昼夜/晨昏。
function sunTick() {
    if (!cfg.sunAuto) return;
    var t;
    try { t = level.getTime() % 19200; } catch (e) { return; }
    var fr = t / 19200;
    var elSun = 90 * Math.sin(2 * Math.PI * fr);
    var azSun;
    if (fr < 0.5) azSun = 90 + 180 * (fr / 0.5);
    else          azSun = 270 - 180 * ((fr - 0.5) / 0.5);
    azSun = ((azSun % 360) + 360) % 360;
    S.sunEl = elSun;
    if (elSun > -1) { S.lumAz = azSun; S.lumEl = Math.max(elSun, 0.5); }
    else {
        // 夜里发光体=月亮: 独立东→西路径(el 用 -elSun)
        S.lumAz = 90 + 180 * ((fr - 0.5) / 0.5);
        S.lumEl = Math.max(-elSun, 0.5);
    }
}
function applySunManual() { S.lumAz = cfg.sunAz; S.lumEl = cfg.sunEl; S.sunEl = cfg.sunEl; }

// ── 阴影目标 ──
function ensureShadowTargets() {
    if (S.shReady) return true;
    if (S.shFbo) {
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
    GL.glTexParameteri(GL.TEXTURE_2D, GL.TEXTURE_MIN_FILTER, GL.LINEAR);
    GL.glTexParameteri(GL.TEXTURE_2D, GL.TEXTURE_MAG_FILTER, GL.LINEAR);
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
        try { GL.glDeleteFramebuffers(S.shFbo); GL.glDeleteTextures(S.shColor); GL.glDeleteTextures(S.shTex); } catch (e2) {}
        S.shFbo = 0; S.shColor = 0; S.shTex = 0;
        return false;
    }
    S.shReady = true;
    log("shadow fbo=" + S.shFbo + " tex=" + S.shTex + " @ " + SHADOW_SIZE);
    return true;
}

// 重渲阴影: 首次 / 玩家移动>0.6m / 方块被改 / 发光体方向变化>0.6°(≥3s 节流)
function shadowNeedsRedraw() {
    if (!S.shFbo) return true;
    if (S.shDirty) return true;
    var now = Date.now();
    if (now - (S.shLastTry || 0) < 150) return false;
    var cam = Render.getCamera();
    var dx = cam[0] - S.shCx, dy = cam[1] - S.shCy, dz = cam[2] - S.shCz;
    if (dx * dx + dy * dy + dz * dz > 0.36) return true;
    if (S.shLastRender && now - S.shLastRender > 3000) {
        var da = Math.abs(S.lumAz - (S.shLastAz || -999)) + Math.abs(S.lumEl - (S.shLastEl || -999));
        if (da > 0.6) return true;
    }
    return false;
}

function renderShadowMap() {
    if (S.shUnsupported) return;
    if (typeof Render.world !== "function" ||
        typeof Render.getWorldMatrices !== "function") {
        S.shUnsupported = true;
        return;
    }
    if (!ensureShadowTargets()) {
        S.shUnsupported = true;
        S.lightValid = false;
        return;
    }
    try {
        var cam = Render.getCamera();
        var px = Math.round(cam[0] / SH_TEX) * SH_TEX;
        var py = Math.round(cam[1] / SH_TEX) * SH_TEX;
        var pz = Math.round(cam[2] / SH_TEX) * SH_TEX;
        var azr = S.lumAz * Math.PI / 180, elr = S.lumEl * Math.PI / 180;
        var sdx = Math.cos(elr) * Math.sin(azr);
        var sdy = Math.sin(elr);
        var sdz = Math.cos(elr) * Math.cos(azr);
        var fx = -sdx, fy = -sdy, fz = -sdz;
        var ang = dirToCamAngles(fx, fy, fz);
        var cx = px - fx * 96.0, cy = py - fy * 96.0, cz = pz - fz * 96.0;

        GL.glBindFramebuffer(GL.FRAMEBUFFER, S.shFbo);
        GL.glViewport(0, 0, SHADOW_SIZE, SHADOW_SIZE);
        GL.glClearColor(1, 1, 1, 1);
        GL.glClear(GL.COLOR_BUFFER_BIT | GL.DEPTH_BUFFER_BIT);
        GL.glEnable(GL.DEPTH_TEST);
        GL.glDepthMask(true);
        GL.glDisable(GL.BLEND);
        GL.glColorMask(false, false, false, false);
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
                S.shLastRender = Date.now();
                S.shLastAz = S.lumAz; S.shLastEl = S.lumEl;
                log("shadow 渲好: chunks=" + (wm.chunks == null ? "?" : wm.chunks)
                    + " lum az/el=" + S.lumAz.toFixed(1) + "/" + S.lumEl.toFixed(1)
                    + " | " + dbgReproj());
            } else { log("world 成功但矩阵缺失, 阴影暂不可用"); }
        } else { log("Render.world 调用失败(返回 false)"); }
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
            log("shadow 连续失败(已停, 重启光影恢复): " + err);
            S.shLastTry = Date.now();
        } else {
            log("shadow 失败(" + S.shErr + "): " + err);
            S.shDirty = true;
        }
    }
}

function dbgReproj() {
    try {
        if (!S.invVP || !S.lightVP) return "dbg: invVP/lightVP 缺失";
        var ndc = [0, 0, 0.5, 1];
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
        return "中心重建=(" + wx.toFixed(1) + "," + wy.toFixed(1) + "," + wz.toFixed(1)
            + ") 光lnd=(" + (lc[0] / lc[3]).toFixed(2) + "," + (lc[1] / lc[3]).toFixed(2) + "," + (lc[2] / lc[3]).toFixed(2)
            + ") cam=(" + S.camC[0].toFixed(0) + "," + S.camC[1].toFixed(0) + "," + S.camC[2].toFixed(0) + ")";
    } catch (e) { return "dbg err: " + e; }
}

function scanTorches() {
    if (typeof player === "undefined" || !player || typeof level === "undefined") return;
    try {
        var px = Math.floor(player.getX()), py = Math.floor(player.getY()), pz = Math.floor(player.getZ());
        var R = 8, list = [];
        for (var x = px - R; x <= px + R && list.length < 7; x++)
            for (var y = py - R; y <= py + R && list.length < 7; y++)
                for (var z = pz - R; z <= pz + R && list.length < 7; z++) {
                    if (level.getBlock(x, y, z) === 50) list.push([x + 0.5, y + 0.5, z + 0.5]);
                }
        S.staticList = list;
        S.torchR = 7.0;
    } catch (e) {}
}
function glowInfo(hid) {
    if (hid === 50) return 5.5;
    if (hid === 89) return 8;
    if (hid === 327) return 8;
    if (hid === 91) return 7;
    return 0;
}
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
        var hx = Math.floor(player.getX()), hz = Math.floor(player.getZ());
        var hy = Math.floor(player.getY() + 1.5);
        var wb = level.getBlock(hx, hy, hz);
        S.underWater = (wb === 8 || wb === 9);
    } catch (e) {}
    var L = S.staticList.concat(handPos ? [handPos] : []);
    if (L.length != S.torchN) log("光源: " + L.length + " 处");
    S.torchList = L;
    S.torchN = L.length;
}

function updateShadowFrame() {
    if (S.shUnsupported) return;
    if (typeof Render.world !== "function" ||
        typeof Render.getWorldMatrices !== "function") {
        S.shUnsupported = true;
        log("此构建缺少 Render.world/getWorldMatrices, 阴影不可用");
        return;
    }
    var P = Render.getProjectionMatrix();
    var V = Render.getModelViewMatrix();
    S.invVP = null;
    if (P && P.length >= 16 && V && V.length >= 16)
        S.invVP = m4inv(m4mul(P, V));
    if (!S.invVP) {
        S.lightValid = false;
        S.shadEff = 0;
        return;
    }
    var c = Render.getCamera();
    S.camC = [c[0], c[1], c[2]];
    S.dayFactor = dayFactor();
    S.shadEff = cfg.shad * (0.5 + 0.5 * S.dayFactor);
    if (S.shadEff <= 0.01) return;
    if (S.lightValid && !shadowNeedsRedraw()) return;
    if (!S.lightValid && !shadowNeedsRedraw() && S.shRetried) return;
    S.shRetried = true;
    renderShadowMap();
}

function compile(type, src) {
    var s = GL.glCreateShader(type);
    GL.glShaderSource(s, src);
    GL.glCompileShader(s);
    var v = GL.glGetShaderLog(s);
    if (v != "OK") { log((type == GL.VERTEX_SHADER ? "vs: " : "fs: ") + v); return 0; }
    return s;
}
function link(vs, fs) {
    var p = GL.glCreateProgram();
    GL.glAttachShader(p, vs);
    GL.glAttachShader(p, fs);
    GL.glLinkProgram(p);
    var pl = GL.glGetProgramLog(p);
    if (pl != "OK") { log("link: " + pl); return 0; }
    return p;
}
function buildQuad() {
    if (S.vbo) return S.vbo;
    var b = GL.glGenBuffers();
    GL.glBindBuffer(GL.ARRAY_BUFFER, b);
    GL.glBufferData(GL.ARRAY_BUFFER, [-1,-1, 1,-1, -1,1, 1,-1, 1,1, -1,1], GL.STATIC_DRAW);
    GL.glBindBuffer(GL.ARRAY_BUFFER, 0);
    S.vbo = b;
    return b;
}
// ═══════════════════════ pass1: 主画面(天空+HDR) ═══════════════════════
// scene 全效果(水反/阴影/体积光/雾) + HDR 大气天空替换 + ACES 后存 mainTex。
var MAIN_FS = [
    "#version 130",
    "uniform sampler2D u_scene, u_mask, u_refl, u_depth, u_shadow;",
    "uniform sampler2D u_bright;",       // gbuffer color2 亮度/AO(M1, diag5 可查)
    "uniform sampler2D u_norm;",         // gbuffer color3 面法线 n*0.5+0.5(M2, diag6 可查)
    "uniform float u_near, u_far, u_time, u_flip, u_diag;",
    "uniform float u_wy;",
    "uniform float u_exposure, u_sat, u_con, u_tone;",
    "uniform float u_again;",
    "uniform float u_fog, u_fogR, u_fogG, u_fogB;",
    "uniform float u_wave, u_tint, u_str, u_vig;",
    "uniform float u_shad, u_shadowTexel, u_srev;",
    "uniform float u_flipY, u_sdep, u_vani, u_wtest, u_under;",
    "uniform vec3 u_torches[8];",
    "uniform float u_torchN, u_torchR;",
    "uniform mat4 u_invVP;",
    "uniform mat4 u_vp;",   // D4 SSAO: 世界→clip(当前 VP, 采样点投回屏幕)
    "uniform vec3 u_cam;",
    "uniform mat4 u_lightVP;",
    "uniform vec3 u_lightC;",
    "uniform vec3 u_sunDir;",      // 发光体方向(白=日/夜=月)
    "uniform float u_sunEl;",      // 太阳真实高度(度, 可负) → 昼夜/晨昏
    "uniform vec3 u_sunC;",        // 发光色(体积光散射用)
    "uniform float u_vol;",        // 体积光强度
    // v1 天空
    "uniform mat4 u_invProj;",     // 主投影逆(列主序) → 天空射线
    "uniform mat4 u_camRot;",      // 相机旋转(世界←相机, mat4 载 3x3)
    "uniform float u_aur, u_star, u_skyflip;",
    "uniform float u_wday;",
    "uniform float u_ndim;",   // 夜压暗系数(0.38=夜暗到 38%; 白天恒 1)
    "uniform float u_uwl, u_caut;",   // C3 水下体积光 / E5 焦散强度
    "uniform float u_sunb;",   // 太阳/辉光亮度乘子(0=暗 1=默认)
    "uniform float u_clouds, u_cloudcov, u_ssao, u_nl, u_hasNorm;",
    "uniform float u_cirrus;",
    "uniform float u_cloudshd;",
    "uniform float u_gb;",
    "uniform float u_shsky;",   // D3 影区天光染强度
    "uniform vec3 u_zen, u_hor, u_sunCol;",
    "uniform vec3 u_wb;",
    "in vec2 vUv;",
    "out vec4 fragColor;",
    "float linDepth(vec2 uv){",
    "  float d = texture(u_depth, uv).r;",
    "  float z = d * 2.0 - 1.0;",
    "  return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near));",
    "}",
    "vec3 aces(vec3 x){",
    "  return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);",
    "}",
    // AgX Minimal(原版 lib/Post/AgX.glsl 照搬): 7次多项式 S 曲线 + Film 域内后调(饱和0.9/色温7200K)
    "vec3 agx(vec3 x){",
    "  const mat3 am = mat3(0.842479062253094, 0.0423282422610123, 0.0423756549057051,",
    "                       0.0784335999999992, 0.878468636469772, 0.0784336,",
    "                       0.0792237451477643, 0.0791661274605434, 0.879142973793104);",
    "  vec3 v = am * (x * 8.0);",
    "  v = clamp(log2(max(v, vec3(1e-6))), -8.0, 6.0);",
    "  v = (v + 8.0) / 14.0;",
    "  vec3 x2 = v * v;",
    "  vec3 x4 = x2 * x2;",
    "  vec3 x7 = x4 * x2 * v;",
    "  v = -17.86 * x7 + 78.01 * x4 * x2 - 126.7 * x4 * v + 92.06 * x4 - 28.72 * x2 * v + 4.361 * x2 - 0.1718 * v + vec3(0.002857);",
    "  const mat3 ai = mat3(1.19687900512017, -0.0528968517574562, -0.0529716355144438,",
    "                       -0.0980208811401368, 1.15190312990417, -0.0980434501171241,",
    "                       -0.0990297440797205, -0.0989611768448433, 1.15107367264116);",
    "  return clamp(ai * v, vec3(0.0), vec3(1.0));",
    "}",
    "vec3 tonemap(vec3 x){ return u_tone > 0.5 ? aces(x) : agx(x * u_again); }",
    "float hash13(vec3 p){",
    "  p = fract(p * 0.1031);",
    "  p += dot(p, p.yzx + 33.33);",
    "  return fract((p.x + p.y) * p.z);",
    "}",
    // 星星: 方向 cell 哈希(稀疏)
    "float stars(vec3 d){",
    "  vec3 c = floor(d * 330.0);",
    "  float h = hash13(c);",
    "  float on = step(1.0 - 0.0016, h);",
    "  float b = 0.35 + 0.65 * hash13(c + 7.13);",
    "  return on * b;",
    "}",
    // 简化极光: 绕磁轴(北天~20°)的一条扰动纬度带
    "float aurora(vec3 d, float tm){",
    "  vec3 pole = vec3(0.0, 0.36, 0.93);",
    "  float lat = dot(d, pole);",
    "  float band = exp(-pow((lat - 0.52) / 0.20, 2.0));",
    "  vec3 t = d - pole * lat;",
    "  float az = atan(t.x, t.z);",
    "  float n = 0.55 + 0.45 * sin(az * 6.0 + tm * 2.0) * sin(az * 21.0 - tm * 1.2);",
    "  band *= smoothstep(0.55, 0.92, n);",
    "  return band;",
    "}",
    // v1 大气天空(HDR): 天顶/地平线双色 + 晨昏暖 + 太阳/月盘与辉光 + 星 + 极光
    // 体积云: 便宜 3D 噪声 fbm + 太阳 Mie 相位(云透光/云边亮)
    "float cloudNoise(vec3 p){",
    "  float n = sin(p.x * 0.011 + u_time * 0.004) * sin(p.z * 0.013 - u_time * 0.003);",
    "  n += 0.60 * sin(p.x * 0.019 - p.z * 0.017 + u_time * 0.006) * sin(p.y * 0.041);",
    "  n += 0.35 * sin(p.x * 0.083 + u_time * 0.015) * sin(p.z * 0.071 - p.y * 0.023 - u_time * 0.011);",
    "  n += 0.18 * sin(p.x * 0.31 + u_time * 0.02) * sin(p.z * 0.27 - p.y * 0.17 - u_time * 0.013);",  // 高频蓬松细节
    "  n += 0.09 * sin(p.x * 1.10 + p.z * 0.71 - u_time * 0.05) * sin(p.z * 0.95 - p.y * 0.33 + u_time * 0.03);",
    "  n += 0.05 * sin(p.x * 2.30 - p.z * 1.90 + u_time * 0.09) * sin(p.x * 1.20 + p.y * 0.62 + p.z * 1.10);",
    "  return clamp(n * 0.7 + 0.5, 0.0, 1.0);",
    "}",
    "void cloudMarch(vec3 cam, vec3 dir, float dayK, float sunUp, float el,",
    "                out vec3 cbase, out float cAlpha, out vec3 clum){",
    "  cbase = vec3(0.0); cAlpha = 0.0; clum = vec3(0.0);",
    "  if (dir.y < 0.03 || u_clouds <= 0.001) return;",
    "  float y0 = 160.0;",   // 云层世界固定(与云影同层)
    "  float y1 = 310.0;",
    "  float tIn = (y0 - cam.y) / max(dir.y, 1e-4);",
    "  float tOut = (y1 - cam.y) / max(dir.y, 1e-4);",
    "  float t0 = max(tIn, 0.0);",
    "  if (t0 >= tOut) return;",
    "  float sh = (tOut - t0) / 14.0;",
    "  float sumD = 0.0;",
    "  vec3 sunD = normalize(u_sunDir);",
    "  float ct = clamp(dot(dir, sunD), 0.0, 1.0);",
    "  float g = 0.8;",
    "  float mie = (1.0 - g * g) / pow(max(1.0 + g * g - 2.0 * g * ct, 1e-4), 1.5);",
    "  for (int i = 0; i < 14; i++) {",
    "    float tt = t0 + sh * (float(i) + 0.5);",
    "    vec3 p = cam + dir * tt;",
    "    float dn = cloudNoise(p);",
    "    float dens = smoothstep(u_cloudcov, u_cloudcov + 0.14, dn);",
    "    sumD += dens * sh;",
    "  }",
    "  float ext = sumD * 0.006;",
    "  float trans = exp(-ext);",
    "  cAlpha = 1.0 - trans;",
    "  float edge = smoothstep(0.4, 1.4, sumD / max(tOut - t0, 1.0));",
    "  vec3 ccol = mix(vec3(0.70, 0.74, 0.82), vec3(0.97, 0.98, 1.0), edge);",
    "  float lit = clamp(0.34 + mie * 0.6, 0.0, 1.0);",
    "  ccol *= (0.40 + 0.65 * dayK) * (0.72 + 0.5 * lit);",
    "  float wk = (1.0 - smoothstep(0.0, 0.35, el)) * smoothstep(-0.12, 0.0, el);",  // 晨昏云染橙(原版特征)
    "  ccol = mix(ccol, ccol * vec3(1.10, 0.82, 0.55), wk);",
    "  ccol = mix(ccol, ccol * vec3(0.5, 0.55, 0.8) * 0.55, 1.0 - dayK);",
    "  cbase = ccol;",
    "  float bb = mie * cAlpha * dayK * u_clouds;",
    "  clum = mix(vec3(1.0, 0.9, 0.75), vec3(1.0), clamp(el / 0.3, 0.0, 1.0)) * bb * (0.5 + 1.1 * edge) * u_sunb;",
    "}",
    // 云影(照原版 CloudShadow): 地表沿太阳方向穿同层云 → 透光 0..1(慢漂移动影)
    "float cloudShadow(vec3 w){",
    "  vec3 sd = normalize(u_sunDir);",
    "  if (sd.y <= 0.02 || u_cloudshd <= 0.001) return 1.0;",
    "  float tA = (160.0 - w.y) / sd.y;",
    "  float tB = (310.0 - w.y) / sd.y;",
    "  if (tB < 0.0) return 1.0;",
    "  float t0 = max(tA, 0.0);",
    "  if (t0 > 420.0) return 1.0;",
    "  float sh = (tB - t0) / 6.0;",
    "  if (sh <= 0.0) return 1.0;",
    "  float od = 0.0;",
    "  for (int i = 0; i < 6; i++) {",
    "    vec3 p = w + sd * (t0 + sh * (float(i) + 0.5));",
    "    od += max(cloudNoise(p) - 0.40, 0.0);",
    "  }",
    "  float a = 1.0 - exp(-od * 0.16);",
    "  return 1.0 - a * u_cloudshd;",
    "}",
    // 天空拆分: base(屏色, 0..1 直接上屏) + lum(日/月 HDR, 交给 ACES)
    "void skySplit(vec3 cam, vec3 dir, out vec3 base, out vec3 lum){",
    "  float el = u_sunEl * 0.0174533;",
    "  float sunUp = step(0.0, el);",
    "  vec3 lumD = normalize(u_sunDir);",     // JS 已把发光体方向传对: 白=太阳 夜=月
    "  float dayK  = smoothstep(-0.03, 0.14, el);",
    "  float nightK = 1.0 - smoothstep(-0.05, 0.12, el);",
    "  float warmK = (1.0 - smoothstep(0.0, 0.30, el)) * smoothstep(-0.09, 0.0, el);",
    "  float h = clamp(dir.y, 0.0, 1.0);",
    "  vec3 zen = mix(vec3(0.008, 0.014, 0.030), u_zen, dayK);",
    "  vec3 hor = mix(vec3(0.030, 0.045, 0.085), u_hor, dayK);",
    "  vec3 sky = mix(hor, zen, pow(h, 0.55));",
    "  sky += (vec3(1.0) - sky) * 0.16 * exp(-h * 26.0);",  // 极近地平线的大气白带(照原版远山上方那层)",
    "  if (warmK > 0.001) {",
    "    vec3 sd0 = vec3(u_sunDir.x, 0.0, u_sunDir.z);",
    "    float sf = clamp(dot(normalize(vec3(dir.x, 0.0, dir.z)), normalize(sd0 + vec3(1e-6))), 0.0, 1.0);",
    "    sky = mix(sky, vec3(1.0, 0.70, 0.42), warmK * sf * 0.85);",
    "  }",
    // B6: 高空卷云(cirrus) —— 极稀薄丝状层, 白天/晨昏可见(照原版 PlanarClouds 卷云层)
    "  if (u_cirrus > 0.001 && dayK > 0.05 && dir.y > 0.15) {",
    "    float tH = (560.0 - u_cam.y) / max(dir.y, 0.02);",
    "    if (tH > 0.0 && tH < 60000.0) {",
    "      vec2 cp = vec2(dir.x * tH + u_cam.x, dir.z * tH + u_cam.z) * 0.0009 + vec2(u_time * 0.010, u_time * 0.003);",
    "      float f1 = sin(cp.x * 7.0 + cp.y * 3.0 + u_time * 0.02);",
    "      float f2 = sin(cp.y * 11.0 + u_time * 0.015 + sin(cp.x * 2.3) * 2.4);",
    "      float f3 = sin(cp.x * 23.0 - cp.y * 15.0 + u_time * 0.05);",
    "      float cir = pow(clamp(f1 * 0.5 + f2 * 0.35 + f3 * 0.15 + 0.5, 0.0, 1.0), 5.0);",
    "      float g = smoothstep(0.30, 1.0, dir.y);",
    "      vec3 cirCol = mix(vec3(1.0, 0.80, 0.62), vec3(0.96, 0.97, 1.0), clamp(el / 0.25, 0.0, 1.0));",
    "      sky += cirCol * cir * g * u_cirrus * 0.07 * (0.35 + 0.65 * dayK);",
    "    }",
    "  }",
    "  if (nightK > 0.01) {",
    "    sky += vec3(0.92, 0.96, 1.0) * stars(dir) * u_star * nightK * 0.6;",
    "    if (u_aur > 0.01) {",
    "      float ar = aurora(dir, u_time);",
    "      vec3 ac = mix(vec3(0.15, 0.95, 0.55), vec3(0.55, 0.35, 0.95), clamp(h * 1.3, 0.0, 1.0));",
    "      sky += ac * ar * u_aur * nightK * (0.14 + 0.5 * ar);",
    "    }",
    "  }",
    "  base = max(sky, vec3(0.0));",
    "  lum = vec3(0.0);",
    "  float cd = dot(dir, lumD);",
    "  float ang = 0.016;",
    "  float disc = smoothstep(cos(ang), cos(ang * 0.12), cd);",
    "  float glow = smoothstep(cos(ang * 14.0), cos(ang * 2.6), cd);",
    "  float discE = sunUp * u_sunb * (2.2 + 1.6 * dayK) + (1.0 - sunUp) * 0.7;",
    "  vec3 discC = sunUp > 0.5 ? mix(vec3(1.0, 0.55, 0.22), u_sunCol, clamp(el / 0.25, 0.0, 1.0))",
    "                            : vec3(0.95, 0.95, 0.98);",
    "  lum += discC * disc * discE;",
    "  lum += discC * glow * u_sunb * (sunUp * (0.20 + 0.55 * dayK) + (1.0 - sunUp) * 0.15);",
    "  // 体积云(自绘): 3D 噪声 raymarch, 受太阳光; 云边透光送 bloom",
    "  float cAlpha; vec3 cbase; vec3 clum;",
    "  cloudMarch(cam, dir, dayK, sunUp, el, cbase, cAlpha, clum);",
    "  base = mix(base, cbase, cAlpha);",
    "  lum += clum;",
    "}",
    // 体积光/雾(步进查阴影图)
    "// 体积雾/体积光(仿 Derivative VolumetricFog: 高度衰减密度+噪声+阴影遮挡+双瓣相位+Rayleigh 空气光)",
    "float hgP(float ct, float g){ return (1.0 - g * g) / pow(max(1.0 + g * g - 2.0 * g * ct, 1e-4), 1.5); }",
    "float corP(float ct, float g){ return 0.75 * (1.0 - g * g) / (2.0 + g * g) * (1.0 + ct * ct) / pow(max(1.0 + g * g - 2.0 * g * ct, 1e-4), 1.5); }",
    // 便宜平滑 3D 噪声
    "float n3s(vec3 p){ return sin(p.x) * sin(p.y * 1.31 + 1.7) * sin(p.z * 0.93 - 2.1); }",
    // FOG_TYPE=1 中档: 海平面(91)以下指数浓度 × 噪声扰动(原版思路, 数值本土化)
    "float fogDenAt(vec3 p){",
    "  float hF = exp(-max(p.y - 58.0, 0.0) * 0.05);",  // 海面(y≈58)以下最浓, 向上指数消
    "  vec3 wind = vec3(u_time * 0.9, 0.0, u_time * 0.55);",
    "  vec3 q = p * 0.06 + wind;",
    "  float n = 0.5 + 0.5 * sin(q.x * 1.0) * sin(q.z * 1.3 + 1.2) * sin(q.y * 0.8 + 0.5);",
    "  float n2 = 0.5 + 0.5 * sin(q.x * 3.7 - 1.0) * sin(q.z * 2.9 + 0.7);",
    "  float nF = n * 0.7 + n2 * 0.3;",
    "  return hF * (0.35 + 0.65 * nF);",
    "}",
    "float pointLit(vec3 p){",
    "  vec4 lc = u_lightVP * vec4(p - u_lightC, 1.0);",
    "  vec3 lnd = lc.xyz / lc.w;",
    "  if (max(abs(lnd.x), abs(lnd.y)) >= 1.0) return 1.0;",
    "  vec2 luv = lnd.xy * 0.5 + 0.5;",
    "  float md = lnd.z * 0.5 + 0.5;",
    "  float sd = texture(u_shadow, luv).r;",
    "  return step(md - 0.0025, sd);",
    "}",
    "void volScatter(vec3 eye, vec3 dir, float maxD, out vec3 insc, out float trans){",
    "  insc = vec3(0.0); trans = 1.0;",
    "  if (maxD < 1.0) return;",
    "  float mD = min(maxD, 64.0);",
    "  int steps = 6 + int(maxD * 0.10);",   // perf: 步数收敛(原 30 步 → ≤16), 步长仍细可打断叶隙
    "  if (steps > 16) steps = 16;",
    "  if (steps < 5) steps = 5;",
    "  float stepLen = mD / float(steps);",
    "  vec3 sunD = normalize(u_sunDir);",
    "  float ct = clamp(dot(dir, sunD), -1.0, 1.0);",
    "  float ldot01 = ct * 0.5 + 0.5;",
    // 雾散射相位: CornetteShanks(0.7)*0.45 + HG(-0.3)*0.15 + 0.1(原版 FOG_TYPE<=1)
    "  float phase = corP(ct, 0.7) * 0.45 + hgP(ct, -0.3) * 0.15 + 0.1;",
    // Rayleigh 空气光(体积光)相位: (3/16π)(1+cos²)
    "  float rayP = 0.0597 * (1.0 + ct * ct);",
    "  float fogD0 = 0.028 * u_vol;",      // 逐米消光系数(再浓一档: 40m 透射≈0.35)
    "  float airD0 = u_vol * 0.0018;",
    "  float dit = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);",
    "  vec3 sunAcc = vec3(0.0); float skyAcc = 0.0;",
    "  float tt = 1.0;",
    "  for (int i = 0; i < 30; i++) {",
    "    if (i >= steps) break;",
    "    float s = stepLen * (float(i) + dit);",
    "    vec3 p = eye + dir * s;",
    "    if (p.y > 240.0) continue;",
    "    float dens = airD0 * rayP * stepLen;",
    "    float fd = fogDenAt(p);",
    "    if (fd > 1e-4) dens += fd * fogD0 * stepLen;",
    "    float stepT = exp(-dens);",
    // 叶子/细物遮挡: 段首尾两样本平均 → 叶隙透光斑驳(树叶不再整段漏)
    "    float lit = pointLit(p + sunD * (stepLen * 0.35));",   // 单采样(perf); 细分步长仍能打碎叶隙
    "    float powder = 1.0 - exp(-dens * 6.0);",
    "    powder = powder * (1.0 - ldot01) + ldot01;",
    "    float fs = powder * tt * (1.0 - stepT);",
    "    sunAcc += vec3(lit) * fs;",
    "    skyAcc += fs * (0.45 + 0.55 * (1.0 - lit));",
    "    tt *= stepT;",
    "    if (tt < 0.008) break;",
    "  }",
    "  trans = max(tt, 0.60);",   // 远景不被雾吸收压黑 → 远近方块亮度一致(雾只由散射加法给出)
    // 散射色 = 阳光(暖白)×太阳相位 + 天光(蓝); 末乘 u_vol 统一强度
    "  vec3 sunTint = max(u_sunC, vec3(1e-4));",
    "  insc = (sunTint * sunAcc * (0.10 + 0.60 * phase) + vec3(0.60, 0.74, 1.0) * skyAcc * 0.015) * u_vol;",
    // ↑ 丁达尔(阳光直射散射)为主; 均匀天光散射近无 → 影区/洞内/远景不再泛蓝染色
    "}",
    // 阴影
    "float calcShadow(vec2 uv, float lin){",
    "  if (u_shad <= 0.001) return 1.0;",
    "  if (lin >= u_far * 0.97) return 1.0;",
    "  float d = texture(u_depth, uv).r;",
    "  float zc = d * 2.0 - 1.0;",
    "  if (u_sdep > 0.5) zc = -zc;",
    "  float sy = mix(uv.y, 1.0 - uv.y, u_flipY);",
    "  vec3 ndc = vec3(vec2(uv.x, sy) * 2.0 - 1.0, zc);",
    "  vec4 q = u_invVP * vec4(ndc, 1.0);",
    "  vec3 world = q.xyz / q.w + u_cam;",
    "  vec4 lc = u_lightVP * vec4(world - u_lightC, 1.0);",
    "  vec3 lnd = lc.xyz / lc.w;",
    "  float bm = max(abs(lnd.x), abs(lnd.y));",
    "  if (bm >= 1.0) return 1.0;",
    "  float fade = smoothstep(0.86, 1.0, bm);",
    "  vec2 luv = lnd.xy * 0.5 + 0.5;",
    "  float myD = lnd.z * 0.5 + 0.5;",
    "  float bias = 0.0020;",   // 深度偏置(防低角度斜射自阴影碎片/条纹)
    "  float lit = 0.0;",
    "  float wsum = 0.0;",
    // 5×5 PCF + 三角权重 = 柔和半影(仿原版软影; 采样半径≈2.1 纹素)
    "  for (int yy = -1; yy <= 1; yy++) {",
    "    for (int xx = -1; xx <= 1; xx++) {",
    "      float wgt = max(1.5 - float(abs(xx)) - float(abs(yy)), 0.0);",
    "      vec2 suv = clamp(luv + vec2(float(xx), float(yy)) * (u_shadowTexel * 3.0), 0.0, 1.0);",
    "      float sd = texture(u_shadow, suv).r;",
    "      float ok = (u_srev > 0.5) ? step(sd, myD + bias) : step(myD - bias, sd);",
    "      lit += ok * wgt;",
    "      wsum += wgt;",
    "    }",
    "  }",
    "  lit /= max(wsum, 1e-4);",
    "  float r9 = mix(lit, 1.0, fade);",
    "  if (!(r9 >= 0.0)) r9 = 1.0;",
    "  for (int i = 0; i < 8; i++) {",
    "    if (float(i) >= u_torchN) break;",
    "    float dd = distance(world, u_torches[i]);",
    "    float at = clamp(1.0 - dd / u_torchR, 0.0, 1.0);",
    "    r9 = mix(r9, 1.0, at * at);",
    "  }",
    "  return r9;",
    "}",
    "vec3 rebuildWorld(vec2 uv, float lin){",
    "  float d = texture(u_depth, uv).r;",
    "  float zc = d * 2.0 - 1.0;",
    "  if (u_sdep > 0.5) zc = -zc;",
    "  float sy = mix(uv.y, 1.0 - uv.y, u_flipY);",
    "  vec3 ndc = vec3(vec2(uv.x, sy) * 2.0 - 1.0, zc);",
    "  vec4 q = u_invVP * vec4(ndc, 1.0);",
    "  return q.xyz / q.w + u_cam;",
    "}",
    "vec3 torchLight(vec2 uv, float lin){",
    "  if (u_torchN <= 0.0) return vec3(0.0);",
    "  if (lin >= u_far * 0.97) return vec3(0.0);",
    "  vec3 world = rebuildWorld(uv, lin);",
    // 多火把取最亮(不叠加, 照 MC 方块光照 max 语义); r=最近能量 0..1
    "  float best = 0.0;",
    "  for (int i = 0; i < 8; i++) {",
    "    if (float(i) >= u_torchN) break;",
    "    float dd = distance(world, u_torches[i]);",
    "    float at = clamp(1.0 - dd / max(u_torchR, 0.1), 0.0, 1.0);",
    "    float g = at * at * (0.5 + 0.5 * at);",   // 中心≈1, 边缘平滑衰减
    "    if (g > best) best = g;",
    "  }",
    "  return vec3(best, 0.0, 0.0);",
    "}",
    // 屏幕空间 AO — 照原版 AmbientOcclusion 思路: 法线半球 6 方向采样, 近表面遮挡
    "float ssaoAO(vec2 uv, float lin){",
    "  if (u_ssao <= 0.001 || lin >= u_far * 0.96) return 0.0;",
    "  if (u_hasNorm < 0.5) {",
    // 老引擎 fallback: 深度邻域凹陷
    "    float px = 0.0018;",
    "    float d = lin;",
    "    float a = min(linDepth(uv + vec2(px, 0.0)), linDepth(uv - vec2(px, 0.0)));",
    "    float b = min(linDepth(uv + vec2(0.0, px)), linDepth(uv - vec2(0.0, px)));",
    "    return clamp((d - (a + b) * 0.5) * 2.5, 0.0, 1.0);",
    "  }",
    "  vec3 p = rebuildWorld(uv, lin);",
    "  vec3 n = normalize(texture(u_norm, uv).rgb * 2.0 - 1.0);",
    "  vec3 t = (abs(n.y) > 0.9) ? vec3(1.0, 0.0, 0.0) : normalize(cross(vec3(0.0, 1.0, 0.0), n));",
    "  vec3 b = cross(n, t);",
    "  float rad = clamp(lin * 0.02, 0.35, 5.0);",
    "  float occ = 0.0;",
    "  for (int i = 0; i < 6; i++) {",
    "    float aa = (float(i) * 1.0472 + 0.5236);",
    "    vec3 dir = t * cos(aa) + b * sin(aa);",
    "    vec3 q = p + (n * 0.55 + dir * 0.85) * rad;",
    "    vec4 qc = u_vp * vec4(q, 1.0);",
    "    if (qc.w > 1e-3) {",
    "      vec2 uvp = qc.xy / qc.w * 0.5 + 0.5;",
    "      if (uvp.x > 0.002 && uvp.x < 0.998 && uvp.y > 0.002 && uvp.y < 0.998) {",
    "        float ds = linDepth(uvp);",
    "        if (ds < lin - rad * 0.9 + 0.12) occ += 1.0;",
    "      }",
    "    }",
    "  }",
    "  return occ / 6.0;",
    "}",
    // 屏幕空间法线 → N·L 地表明暗(照 gbuffer 思路: 法线·太阳方向), 立体感主源
    "float surfaceNL(vec2 uv, float lin){",
    "  if (lin >= u_far * 0.9) return 0.42;",
    "  float px = 0.0016;",
    "  vec3 wc = rebuildWorld(uv, lin);",
    "  vec3 wr = rebuildWorld(uv + vec2(px, 0.0), 0.0);",
    "  vec3 wl = rebuildWorld(uv - vec2(px, 0.0), 0.0);",
    "  vec3 wu = rebuildWorld(uv + vec2(0.0, px), 0.0);",
    "  vec3 wd = rebuildWorld(uv - vec2(0.0, px), 0.0);",
    "  vec3 n = cross(wr - wl, wu - wd);",
    "  float ln = length(n);",
    "  if (ln < 1e-4) return 0.42;",
    "  float nl = dot(n / ln, normalize(u_sunDir));",
    "  return clamp(nl * 0.5 + 0.5, 0.0, 1.0);",
    "}",
    // ── 水(照原版 WaterWave.glsl): 平滑值噪声(程序化 noisetex) + 四 octave 波形 + 法线差分 ──
    "float h12(vec2 p){ return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }",
    "float vnoise(vec2 p){",
    "  vec2 i = floor(p);",
    "  vec2 f = fract(p);",
    "  vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);",
    "  float a = h12(i);",
    "  float b = h12(i + vec2(1.0, 0.0));",
    "  float c = h12(i + vec2(0.0, 1.0));",
    "  float d = h12(i + vec2(1.0, 1.0));",
    "  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);",
    "}",
    "float waterH(vec2 p){",
    "  float wt = u_time * 1.2;",
    "  p.y *= 0.8;",
    "  float w = 0.0;",
    "  w += vnoise((p + vec2(0.0, p.x - wt)) * 0.8) * 1.0;",
    "  w += vnoise((p - vec2(-wt, p.x)) * 1.6) * 0.5;",
    "  w += vnoise((p + vec2(wt * 0.6, p.x - wt)) * 2.4) * 0.2;",
    "  w += vnoise((p - vec2(wt * 0.6, p.x - wt)) * 3.6) * 0.1;",
    "  return w * 0.5;",
    "}",
    "vec2 waterN(vec2 p){",
    "  float c = waterH(p);",
    "  float l = waterH(p + vec2(0.04, 0.0));",
    "  float u2 = waterH(p + vec2(0.0, 0.04));",
    "  return vec2(c - l, c - u2);",
    "}",
    // 世界方向(相机空间 → 世界): 远平面 clip 点反投影
    "vec3 viewToWorld(vec2 uv){",
    "  vec2 n = uv * 2.0 - 1.0;",
    "  if (u_skyflip > 0.5) n.y = -n.y;",
    "  vec4 cv = u_invProj * vec4(n, 1.0, 1.0);",
    "  vec3 cd = normalize(cv.xyz / cv.w);",
    "  return normalize((u_camRot * vec4(cd, 0.0)).xyz);",
    "}",
    "void main(){",
    "  vec4 m = texture(u_mask, vUv);",
    "  float isWater = m.r + m.g + m.b;",
    "  float lin = linDepth(vUv);",
    "  bool isSky = lin >= u_far * 0.97 && isWater < 0.09;",
    "  if (u_diag > 0.5 && u_diag < 1.5) { fragColor = m; return; }",
    "  if (u_diag > 1.5 && u_diag < 2.5) { fragColor = texture(u_refl, vUv); return; }",
    "  if (u_diag > 2.5 && u_diag < 3.5) { float g = clamp(1.0 - lin / u_far, 0.0, 1.0); fragColor = vec4(vec3(g), 1.0); return; }",
    "  if (u_diag > 3.5 && u_diag < 4.5) { float sd = texture(u_shadow, vUv).r; fragColor = vec4(vec3(clamp(sd, 0.0, 1.0)), 1.0); return; }",
    "  if (u_diag > 4.5 && u_diag < 5.5) { fragColor = vec4(texture(u_bright, vUv).rgb, 1.0); return; }",   // M1 亮度/AO 图
    "  if (u_diag > 5.5) { fragColor = vec4(texture(u_norm, vUv).rgb, 1.0); return; }",                       // M2 法线图
    "  vec3 col;",
    "  if (isSky) {",
    "    vec3 skyB; vec3 skyL;",
    "    skySplit(u_cam, viewToWorld(vUv), skyB, skyL);",
    "    col = skyB + tonemap(skyL * u_exposure);",
    "  } else {",
    "    col = texture(u_scene, vUv).rgb;",
    "    if (isWater > 0.09 && u_vani < 0.5) {",
    "      if (u_wtest > 0.5) {",
    "        col = vec3(0.5 + 0.5 * sin(u_time * 20.0), 0.1, 0.5 + 0.5 * cos(u_time * 20.0));",
    "      } else {",
    // ── 水(照原版 WaterWave + WaterFog) ──
    "      vec3 wpos = rebuildWorld(vUv, lin);",
    "      float wDepth = (u_wy > -100.0) ? clamp(u_wy - wpos.y, 0.0, 64.0) : 6.0;",
    "      if (lin >= u_far * 0.9) wDepth = 18.0;",
    "      vec2 wp = wpos.xz;",
    "      vec2 wN = waterN(wp) * u_wave;",
    "      vec2 ruv = vec2(vUv.x, mix(vUv.y, 1.0 - vUv.y, u_flip));",
    "      ruv += wN * vec2(0.9, 0.6) * 0.012;",
    "      ruv = clamp(ruv, 0.0, 1.0);",
    "      vec3 refl = texture(u_refl, ruv).rgb;",
    "      float fres = smoothstep(0.05, 0.55, vUv.y);",
    "      float k = clamp(u_str * (0.10 + 0.90 * fres), 0.0, 1.0);",
    "      col = mix(col, refl, k * 0.85);",
    // 波前明暗(微弱)
    "      col *= 0.94 + 0.14 * smoothstep(0.42, 0.8, waterH(wp));",
    // WaterFog: 深度吸收(照原版 trans=exp(-(abs·8+0.03)·depth)) + 天光散射色
    "      float wdayK = smoothstep(-0.03, 0.14, u_sunEl * 0.0174533);",
    "      float wdayE = mix(0.10, 1.0, wdayK);",
    "      wdayE = mix(0.10, wdayE, u_wday);",   // u_wday=0 → 恒夜间深色清透样式(用户偏好)
    "      vec3 wfog = vec3(0.04, 0.40, 0.78) * (0.10 + 0.90 * wdayE);",
    "      vec3 wt = exp(-(vec3(0.40, 0.14, 0.08) * 8.0 + vec3(0.03)) * wDepth * 0.85);",
    "      col = col * wt + wfog * (1.0 - wt);",
    "      col = clamp(col, 0.0, 1.0);",
    "      }",
    "    }",
    "    float shd = calcShadow(vUv, lin);",
    "    if (isWater > 0.09) shd = 1.0;",
    "    float litF = mix(1.0, shd, u_shad);",
    "    if (u_cloudshd > 0.01) litF *= cloudShadow(rebuildWorld(vUv, lin));",   // 水也乘云影(云遮阳→水面暗, 与方块同感)
    // 光照链(重写): 影因子(真影×云影)一次乘入 col; 删亮度反抬(envF)与 gb 重算覆盖 —— 影实根稳定、不再随亮度/视角漂移
    "    col *= litF;",
    // D3: 影区天光染(照原版: 影不是黑死, 由天光/天空可见度给蓝紫底)
    "    float shAmt = 1.0 - litF;",
    "    if (u_shsky > 0.001 && shAmt > 0.004 && isWater < 0.09) {",
    "      float up = 0.55;",
    "      if (u_hasNorm > 0.5) { vec3 gn2 = normalize(texture(u_norm, vUv).rgb * 2.0 - 1.0); up = clamp(gn2.y * 0.5 + 0.5, 0.0, 1.0); }",
    "      float skyDay = smoothstep(-0.03, 0.14, u_sunEl * 0.0174533);",
    "      vec3 skySh = mix(u_hor, u_zen, up * 0.6 + 0.15) * (0.10 + 0.90 * skyDay);",
    "      col += skySh * shAmt * u_shsky * (0.30 + 0.70 * up);",
    "    }",
    "    if (isWater < 0.09) {",
    "      col *= 1.0 - u_ssao * ssaoAO(vUv, lin);",
    "    }",
    // NL: 陆地用 color3/深度法线; 水用波浪梯度法线 → 波纹受光明暗(不再恒一色, 像方块)
    "    float snl;",
    "    if (isWater > 0.09) {",
    "      vec3 wpos3 = rebuildWorld(vUv, lin);",
    "      vec2 wn3 = waterN(wpos3.xz) * u_wave;",
    "      vec3 wno = normalize(vec3(-wn3.x * 0.5, 1.0, -wn3.y * 0.5));",
    "      snl = clamp(dot(wno, normalize(u_sunDir)) * 0.5 + 0.5, 0.0, 1.0);",
    "    } else if (u_hasNorm > 0.5) {",
    "      vec3 gn = normalize(texture(u_norm, vUv).rgb * 2.0 - 1.0);",
    "      snl = clamp(dot(gn, normalize(u_sunDir)) * 0.5 + 0.5, 0.0, 1.0);",
    "    } else { snl = surfaceNL(vUv, lin); }",
    "    col *= 1.0 + u_nl * (snl - 0.5);",
    "    col = max(col, vec3(0.0));",
    "    if (u_torchN > 0.5 && isWater < 0.09) {",
    "      float best = torchLight(vUv, lin).r;",
    "      if (best > 0.002) {",
    // 火把照明 = 目标亮度下限(非叠加加法): 环境越暗目标越高(夜里≈白天亮度),
    // 环境已亮(白天受光面)时目标低 → max 无效 → 亮处不加亮。纹理色相保留。
    "        float lumOld = dot(col, vec3(0.2126, 0.7152, 0.0722));",
    "        float goalB = clamp(0.86 - lumOld * 1.1, 0.28, 0.86);",
    "        float goalL = best * goalB;",
    "        if (goalL > lumOld + 0.002) {",
    "          vec3 colH = (lumOld > 0.004) ? col / max(lumOld, 1e-4) : vec3(1.0);",
    "          col = colH * goalL;",
    "          col = mix(col, col * vec3(1.0, 0.80, 0.55), 0.35 * best);",   // 火把暖色
    "        }",
    "      }",
    "      col = clamp(col, 0.0, 1.0);",
    "    }",
    "    if (u_vol > 0.01 && isWater < 0.09 && lin < u_far * 0.96) {",
    "      vec3 volEye = u_cam;",
    "      vec3 volW = rebuildWorld(vUv, lin);",
    "      vec3 volI; float volT;",
    "      volScatter(volEye, normalize(volW - volEye), lin, volI, volT);",
    "      col = col * volT + volI;",
    "    }",
    "    float fd = clamp(lin / u_far, 0.0, 1.0);",
    "    vec3 fogC = vec3(u_fogR, u_fogG, u_fogB);",
    "    float fogK = (fd * fd) * u_fog;",
    "    if (u_under > 0.5) { fogK *= 0.06; fogC = vec3(0.10, 0.24, 0.32); }",
    "    col = mix(col, fogC, fogK);",
    // 昼夜压暗(白天=1 不动; 夜里 scene 乘暗 → 黑处黑; 月光面仍由 NL/月光微亮)
    // C3 水下体积光 + E5 焦散(仅玩家在水下: 水底/岩壁折射光斑 + 向上天窗散射)
    "    if (u_under > 0.5 && u_uwl + u_caut > 0.002 && lin < u_far * 0.98) {",
    "      vec3 uwp = rebuildWorld(vUv, lin);",
    "      float uwD = (u_wy > -100.0) ? clamp(u_wy - uwp.y, 0.0, 32.0) : 0.0;",
    "      if (uwD > 0.3) {",
    "        float sunDn = clamp(-normalize(u_sunDir).y, 0.0, 1.0);",
    // 焦散: 波浪折射聚焦亮纹(双频流动, 越深越弱)
    "        vec2 fc = uwp.xz * 0.6 + vec2(u_time * 0.55, u_time * 0.32);",
    "        float ca1 = pow(clamp(sin(fc.x) * sin(fc.y * 1.3 + 1.7) * 0.5 + 0.5, 0.0, 1.0), 3.0);",
    "        vec2 fg = uwp.xz * 1.8 - vec2(u_time * 1.2, u_time * 0.9);",
    "        float ca2 = pow(clamp(sin(fg.x + sin(fg.y * 0.7) * 2.0), 0.0, 1.0), 6.0);",
    "        float caut = (ca1 * 0.55 + ca2 * 0.85) * exp(-uwD * 0.18) * sunDn;",
    "        col += vec3(1.0, 0.96, 0.82) * caut * u_caut;",
    // 水下体积光: 视线朝上越接近垂直越亮(天窗), 深度衰减 + 太阳高度
    "        vec3 uwrd = normalize(uwp - u_cam);",
    "        float uwUp = pow(clamp(max(uwrd.y, 0.0), 0.0, 1.0), 5.0);",
    "        float uwBeam = exp(-uwD * 0.09) * sunDn * uwUp;",
    "        col += u_sunC * uwBeam * u_uwl * 0.55;",
    "      }",
    "    }",
    "    float dk3 = smoothstep(-0.03, 0.14, u_sunEl * 0.0174533);",
    "    col *= mix(u_ndim, 1.0, dk3);",
    "    col *= u_exposure;",
    "    col = tonemap(col);",
    "  }",
    "  float l = dot(col, vec3(0.2126, 0.7152, 0.0722));",
    "  col *= u_wb;",
    "  col = mix(vec3(l), col, u_sat);",
    "  col = (col - 0.5) * u_con + 0.5;",
    "  col = clamp(col, 0.0, 1.0);",
    "  if (!(col.x >= 0.0)) col = vec3(0.0);",
    "  fragColor = vec4(col, 1.0);",
    "}"
].join("\n");

// ═══════════════════════ pass2/3: bloom & final ═══════════════════════
// bloom: 从 mainTex 提取亮部(阈 u_thr)并降采样 → blurH(写 B0) → blurV(写 B1)。
var BLOOM_FS = [
    "#version 130",
    "uniform sampler2D u_src;",
    "uniform float u_thr, u_dir;",   // u_dir: 1=横向 0=纵向  (提取只在横向做)
    "uniform vec2 u_texel;",
    "in vec2 vUv;",
    "out vec4 fragColor;",
    "void main(){",
    "  vec3 c = texture(u_src, vUv).rgb;",
    "  if (u_dir > 1.5) c = max(c - vec3(u_thr), vec3(0.0));",
    "  vec2 o = u_dir >= 0.5 ? vec2(u_texel.x, 0.0) : vec2(0.0, u_texel.y);",
    "  vec3 acc = c * 0.227027;",
    "  acc += texture(u_src, vUv + o).rgb * 0.1945946;",
    "  acc += texture(u_src, vUv - o).rgb * 0.1945946;",
    "  acc += texture(u_src, vUv + o * 2.0).rgb * 0.1216216;",
    "  acc += texture(u_src, vUv - o * 2.0).rgb * 0.1216216;",
    "  acc += texture(u_src, vUv + o * 3.0).rgb * 0.054054;",
    "  acc += texture(u_src, vUv - o * 3.0).rgb * 0.054054;",
    "  acc += texture(u_src, vUv + o * 4.0).rgb * 0.016216;",
    "  acc += texture(u_src, vUv - o * 4.0).rgb * 0.016216;",
    "  fragColor = vec4(max(acc, vec3(0.0)), 1.0);",
    "}"
].join("\n");

var FINAL_FS = [
    "#version 130",
    "uniform sampler2D u_main, u_bloom, u_exp;",
    "uniform float u_amt, u_vig, u_diag, u_auto, u_tgt, u_cine, u_aspect;",
    "uniform float u_cas, u_glow;",
    "uniform vec2 u_tp;",
    "in vec2 vUv;",
    "out vec4 fragColor;",
    "void main(){",
    "  vec3 t = texture(u_main, vUv).rgb;",
    "  if (u_diag > 0.5) { fragColor = vec4(t, 1.0); return; }",
    // A8 CAS 锐化(AMD 风格: 对比弱处锐化边缘不强; 照原版 Grade CAS_ENABLED)
    "  if (u_cas > 0.001) {",
    "    vec3 k0 = texture(u_main, vUv).rgb;",
    "    vec3 k1 = texture(u_main, vUv + vec2(u_tp.x, 0.0)).rgb;",
    "    vec3 k2 = texture(u_main, vUv - vec2(u_tp.x, 0.0)).rgb;",
    "    vec3 k3 = texture(u_main, vUv + vec2(0.0, u_tp.y)).rgb;",
    "    vec3 k4 = texture(u_main, vUv - vec2(0.0, u_tp.y)).rgb;",
    "    vec3 mnc = min(min(k0, min(k1, k2)), min(k3, k4));",
    "    vec3 mxc = max(max(k0, max(k1, k2)), max(k3, k4));",
    "    float ctr = max(mxc.r - mnc.r, max(mxc.g - mnc.g, mxc.b - mnc.b));",
    "    float amp = u_cas * (1.0 - smoothstep(0.0, 0.45, ctr));",
    "    t = clamp(k0 + (k0 - (k1 + k2 + k3 + k4) * 0.25) * amp, 0.0, 1.0);",
    "  }",
    // 自动曝光(仿原版 AUTO_EXPOSURE): 全屏亮度低时提亮, 太阳入画亮度高时压暗 → 看太阳不刺眼
    "  float f = 1.0;",
    "  if (u_auto > 0.5) {",
    "    float a = texture(u_exp, vec2(0.5)).r;",
    // u_auto=2 → 只压暗不抬(夜晚/矿洞不再被提亮, 黑处黑); =1 → 全自动(暗部也提)
    "    f = (u_auto > 1.5) ? clamp(u_tgt / max(a, 0.004), 0.25, 1.0) : clamp(u_tgt / max(a, 0.004), 0.35, 3.0);",
    "  }",
    "  t *= f;",
    "  vec3 b = texture(u_bloom, vUv).rgb;",
    "  vec3 c = t + b * u_amt;",
    // A10 bloom-fog 辉光近似: bloom 值平方的低阈泛光(中亮雾/光区带 halo)
    "  c += (b * b) * u_glow;",
    "  float d2 = length(vUv - 0.5) * 1.15;",
    "  c *= 1.0 - u_vig * smoothstep(0.45, 0.9, d2);",
    "  if (u_cine > 0.5) {",   // 照原版 CINEMATIC_EFFECT: 21:9 电影黑边
    "    c *= 1.0 - step(u_aspect * (9.0 / 21.0), abs(vUv.y - 0.5) * 2.0);",
    "  }",
    "  fragColor = vec4(clamp(c, 0.0, 1.0), 1.0);",
    "}"
].join("\n");

// A6: 运动模糊 —— CPU 端相机角速度(yaw/pitch 帧差) → 屏幕像素拖尾,
// 不重建世界/不依赖矩阵(静止时像素差=0, 绝不糊)。默认强度 /mb。
var MB_FS = [
    "#version 130",
    "uniform sampler2D u_src;",
    "uniform float u_mbpx, u_mbpy;",
    "uniform vec2 u_texel;",
    "in vec2 vUv;",
    "out vec4 fragColor;",
    "void main(){",
    "  vec3 c = texture(u_src, vUv).rgb;",
    "  float pxl = abs(u_mbpx) + abs(u_mbpy);",
    "  if (pxl < 0.2) { fragColor = vec4(c, 1.0); return; }",
    "  vec2 d = vec2(u_mbpx, u_mbpy) * u_texel;",
    "  vec3 acc = c;",
    "  float n = 1.0;",
    "  for (int i = 1; i <= 4; i++) {",
    "    float f = float(i) * 0.22;",
    "    acc += texture(u_src, clamp(vUv - d * f, 0.0, 1.0)).rgb;",
    "    acc += texture(u_src, clamp(vUv + d * f * 0.5, 0.0, 1.0)).rgb;",
    "    n += 2.0;",
    "  }",
    "  fragColor = vec4(acc / n, 1.0);",
    "}"
].join("\n");

// A7 TAA/时域稳定 —— 用上帧 VP 把当前像素重投影到历史帧位置, 邻域 min/max clamp
// 防鬼影; 运动大时自动回退到当前帧(不拖影)。无相机 jitter(保守版, 降闪为主)。
var TAA_FS = [
    "#version 130",
    "uniform sampler2D u_cur, u_hist, u_depth;",
    "uniform mat4 u_invVP, u_vpP;",
    "uniform vec3 u_cam, u_camP;",
    "uniform float u_sdep, u_flipY, u_first;",
    "uniform vec2 u_texel;",
    "in vec2 vUv;",
    "out vec4 fragColor;",
    "void main(){",
    "  vec3 cur = texture(u_cur, vUv).rgb;",
    "  if (u_first > 0.5) { fragColor = vec4(cur, 1.0); return; }",
    // 世界重建(与 MAIN 同公式; u_depth = 引擎场景深度)
    "  float d = texture(u_depth, vUv).r;",
    "  vec2 sy = vec2(vUv.x, mix(vUv.y, 1.0 - vUv.y, u_flipY));",
    "  float zc = d * 2.0 - 1.0;",
    "  if (u_sdep > 0.5) zc = -zc;",
    "  vec4 q = u_invVP * vec4(vec3(sy * 2.0 - 1.0, zc), 1.0);",
    "  vec3 w0 = q.xyz / q.w + u_cam;",
    // 同一点用上帧 VP 投回屏幕 → 历史采样坐标
    "  vec4 pc = u_vpP * vec4(w0 - u_camP, 1.0);",
    "  vec2 hp = pc.xy / pc.w * 0.5 + 0.5;",
    "  hp.y = mix(hp.y, 1.0 - hp.y, u_flipY);",
    "  float mv = length(hp - vUv);",
    "  float wgt = smoothstep(0.015, 0.002, mv);",
    "  if (hp.x < 0.02 || hp.x > 0.98 || hp.y < 0.02 || hp.y > 0.98) wgt = 0.0;",
    // 历史颜色 clamp 到当前 3×3 邻域 min/max → 鬼影抑制
    "  vec3 his = texture(u_hist, vUv).rgb;",
    "  vec3 k1 = texture(u_cur, clamp(vUv + vec2(u_texel.x, 0.0), 0.0, 1.0)).rgb;",
    "  vec3 k2 = texture(u_cur, clamp(vUv - vec2(u_texel.x, 0.0), 0.0, 1.0)).rgb;",
    "  vec3 k3 = texture(u_cur, clamp(vUv + vec2(0.0, u_texel.y), 0.0, 1.0)).rgb;",
    "  vec3 k4 = texture(u_cur, clamp(vUv - vec2(0.0, u_texel.y), 0.0, 1.0)).rgb;",
    "  vec3 mn = min(min(cur, min(k1, k2)), min(k3, k4));",
    "  vec3 mx = max(max(cur, max(k1, k2)), max(k3, k4));",
    "  vec3 hc = clamp(his, mn, mx);",
    "  fragColor = vec4(mix(cur, hc, wgt * 0.85), 1.0);",
    "}",
].join("\n");

// 自动曝光小 pass 1: 把 main 亮度下采样到 8×8
var LUM8_FS = [
    "#version 130",
    "uniform sampler2D u_src;",
    "in vec2 vUv;",
    "out vec4 fragColor;",
    "void main(){",
    "  vec3 c = texture(u_src, vUv).rgb;",
    "  float l = dot(c, vec3(0.2126, 0.7152, 0.0722));",
    "  fragColor = vec4(vec3(l), 1.0);",
    "}"
].join("\n");

// 自动曝光小 pass 2: 8×8 平均 → 与上一帧时域平滑 → 写 1×1(供 final 下帧反调)
var LUMAVG_FS = [
    "#version 130",
    "uniform sampler2D u_src, u_prev;",
    "uniform float u_up, u_down;",
    "in vec2 vUv;",
    "out vec4 fragColor;",
    "void main(){",
    "  float s = 0.0;",
    "  for (int i = 0; i < 8; i++) {",
    "    for (int j = 0; j < 8; j++) {",
    "      s += texture(u_src, (vec2(float(i), float(j)) + 0.5) * (1.0 / 8.0)).r;",
    "    }",
    "  }",
    "  s /= 64.0;",
    "  float p = texture(u_prev, vec2(0.5)).r;",
    "  float rate = (s > p) ? u_down : u_up;",   // 变亮→快跟随压暗(防视角大影); 变暗→慢提亮(人眼适应)
    "  float m = mix(p, s, clamp(rate, 0.01, 0.95));",
    "  fragColor = vec4(vec3(clamp(m, 0.0005, 1.0)), 1.0);",
    "}"
].join("\n");
// DoF 景深(照原版 program/Post/DoF.glsl: CalculateCoC 光圈式 + 金角旋转圆盘采样 + sqrt 均匀分布 + 抖动)
var DOF_FS = [
    "#version 130",
    "uniform sampler2D u_src, u_dpt, u_foc;",
    "uniform float u_focus, u_ap, u_dof, u_proj11, u_near, u_far, u_afoc;",
    "uniform vec2 u_px;",
    "in vec2 vUv;",
    "out vec4 fragColor;",
    "float linUV(vec2 uv){",
    "  float d = texture(u_dpt, uv).r;",
    "  float z = d * 2.0 - 1.0;",
    "  return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near));",
    "}",
    "void main(){",
    "  vec3 col = texture(u_src, vUv).rgb;",
    "  float dist = linUV(vUv);",
    "  if (dist >= u_far * 0.96) { fragColor = vec4(col, 1.0); return; }",
    // 自动对焦(照原版 FOCUS_MODE=0): 焦点=屏幕中心深度(准星所指), GPU 1×1 纹理上一帧平滑
    "  float focusDist = u_afoc > 0.5 ? clamp(texture(u_foc, vec2(0.5)).r * 128.0, 2.0, 80.0) : u_focus;",
    "  float fl = 0.5 * 0.035 * u_proj11;",
    "  float ap = fl / max(u_ap, 0.5);",
    "  float coc = (1.0 - focusDist / dist) * ap * fl / max(focusDist - fl, 1e-4);",
    "  float rad = clamp(abs(coc) * u_dof * 300000.0, 0.0, 20.0);",
    "  if (rad < 0.5) { fragColor = vec4(col, 1.0); return; }",
    "  float noise = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);",
    "  float ga = 2.399963;",
    "  vec2 rot = vec2(cos(noise * 6.28318), sin(noise * 6.28318));",
    "  vec3 acc = vec3(0.0);",
    "  for (int i = 0; i < 24; i++) {",
    "    float t = sqrt((noise + float(i)) / 24.0);",
    "    vec2 o = rot * t * (rad * u_px);",
    "    acc += texture(u_src, vUv + o).rgb;",
    "    rot = vec2(rot.x * cos(ga) - rot.y * sin(ga), rot.x * sin(ga) + rot.y * cos(ga));",
    "  }",
    "  fragColor = vec4(acc * (1.0 / 24.0), 1.0);",
    "}"
].join("\n");
// 自动对焦(照原版 centerDepthSmooth 思路): 读屏幕中心深度 → 1×1 与上帧平滑 → DoF 焦点
var FOCUS_FS = [
    "#version 130",
    "uniform sampler2D u_dpt, u_prev;",
    "uniform float u_near, u_far, u_mix;",
    "out vec4 fragColor;",
    "float linUV2(vec2 uv){",
    "  float d = texture(u_dpt, uv).r;",
    "  float z = d * 2.0 - 1.0;",
    "  return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near));",
    "}",
    "void main(){",
    "  float cf = linUV2(vec2(0.5));",
    "  if (cf >= u_far * 0.85) cf = 25.0;",   // 准星指天/远景: 焦点回退 25m, 防"全屏焦外"
    "  float p = texture(u_prev, vec2(0.5)).r;",
    "  float m = mix(p, cf, clamp(u_mix, 0.05, 0.9));",
    "  fragColor = vec4(vec3(clamp(m, 2.0, 80.0) * 0.0078125), 1.0);",   // RGBA8 存 1/128 归一(>1 会截断)
    "}"
].join("\n");
// ── 构建 3 个 program ──
function buildPrograms() {
    buildQuad();
    var vs = compile(GL.VERTEX_SHADER, [
        "#version 130",
        "in vec2 aPos;",
        "out vec2 vUv;",
        "void main(){ vUv = aPos * 0.5 + 0.5; gl_Position = vec4(aPos, 0.0, 1.0); }"
    ].join("\n"));
    if (!vs) return 0;

    var names = ["scene","mask","refl","depth","shadow","near","far","time","flip","diag",
        "exposure","sat","con","tone","fog","fogR","fogG","fogB","wave","tint","str","vig",
        "shad","shadowTexel","srev","flipY","sdep","vani","wtest","under","invVP","cam",
        "lightVP","lightC","sunDir","sunEl","sunC","vol","invProj","camRot","aur","star",
        "skyflip","zen","hor","sunCol","wb","sunb","again","clouds","cirrus","cloudcov","ssao","nl","wy","cloudshd","bright","gb","norm","hasNorm","shsky","wday","ndim","uwl","caut","vp"];
    // pass1 main
    var fsM = compile(GL.FRAGMENT_SHADER, MAIN_FS);
    if (!fsM) return 0;
    var pM = link(vs, fsM);
    if (!pM) return 0;
    S.LM = {};
    for (var i = 0; i < names.length; i++) S.LM[names[i]] = GL.glGetUniformLocation(pM, "u_" + names[i]);
    S.LM.torN = GL.glGetUniformLocation(pM, "u_torchN");
    S.LM.torR = GL.glGetUniformLocation(pM, "u_torchR");
    S.torLoc = [];
    for (var _t = 0; _t < 8; _t++) S.torLoc.push(GL.glGetUniformLocation(pM, "u_torches[" + _t + "]"));
    S.pMain = pM;

    // pass2/3 bloom(方向由 uniform 切)
    var fsB = compile(GL.FRAGMENT_SHADER, BLOOM_FS);
    if (!fsB) return 0;
    var pB = link(vs, fsB);
    if (!pB) return 0;
    S.LB = {};
    ["src","thr","dir","texel"].forEach(function (k) { S.LB[k] = GL.glGetUniformLocation(pB, "u_" + k); });
    S.pBlur = pB;

    var fsF = compile(GL.FRAGMENT_SHADER, FINAL_FS);
    if (!fsF) return 0;
    var pF = link(vs, fsF);
    if (!pF) return 0;
    S.LF = {};
    ["main","bloom","exp","amt","vig","diag","auto","tgt","cine","aspect","tp","cas","glow"].forEach(function (k) { S.LF[k] = GL.glGetUniformLocation(pF, "u_" + k); });
    S.pFinal = pF;

    // A6 motion blur pass
    var fsMB = compile(GL.FRAGMENT_SHADER, MB_FS);
    if (!fsMB) return 0;
    var pMB = link(vs, fsMB);
    if (!pMB) return 0;
    S.LMB = {};
    ["src","mbpx","mbpy","texel"].forEach(function (k) { S.LMB[k] = GL.glGetUniformLocation(pMB, "u_" + k); });
    S.pMB = pMB;

    // 自动曝光小 pass: 8×8 亮度 + 1×1 时域平滑
    var fsL8 = compile(GL.FRAGMENT_SHADER, LUM8_FS);
    if (!fsL8) return 0;
    var pL8 = link(vs, fsL8);
    if (!pL8) return 0;
    S.LL8 = {};
    S.LL8.src = GL.glGetUniformLocation(pL8, "u_src");
    S.pLum8 = pL8;

    var fsLA = compile(GL.FRAGMENT_SHADER, LUMAVG_FS);
    if (!fsLA) return 0;
    var pLA = link(vs, fsLA);
    if (!pLA) return 0;
    S.LLA = {};
    S.LLA.src = GL.glGetUniformLocation(pLA, "u_src");
    S.LLA.prev = GL.glGetUniformLocation(pLA, "u_prev");
    S.LLA.up = GL.glGetUniformLocation(pLA, "u_up");
    S.LLA.down = GL.glGetUniformLocation(pLA, "u_down");
    S.pLumAvg = pLA;

    // A7 TAA 时域稳定
    var fsT = compile(GL.FRAGMENT_SHADER, TAA_FS);
    if (!fsT) return 0;
    var pT = link(vs, fsT);
    if (!pT) return 0;
    S.LT = {};
    ["cur","hist","depth","invVP","vpP","cam","camP","sdep","flipY","first","texel"].forEach(function (k) { S.LT[k] = GL.glGetUniformLocation(pT, "u_" + k); });
    S.pTaa = pT;

    // DoF 景深
    var fsD = compile(GL.FRAGMENT_SHADER, DOF_FS);
    if (!fsD) return 0;
    var pD = link(vs, fsD);
    if (!pD) return 0;
    S.LD = {};
    ["src","dpt","foc","focus","ap","dof","proj11","near","far","px","afoc"].forEach(function (k) { S.LD[k] = GL.glGetUniformLocation(pD, "u_" + k); });
    S.pDof = pD;

    // 自动对焦
    var fsFC = compile(GL.FRAGMENT_SHADER, FOCUS_FS);
    if (!fsFC) return 0;
    var pFC = link(vs, fsFC);
    if (!pFC) return 0;
    S.LFoc = {};
    S.LFoc.dpt = GL.glGetUniformLocation(pFC, "u_dpt");
    S.LFoc.prev = GL.glGetUniformLocation(pFC, "u_prev");
    S.LFoc.near = GL.glGetUniformLocation(pFC, "u_near");
    S.LFoc.far = GL.glGetUniformLocation(pFC, "u_far");
    S.LFoc.mix = GL.glGetUniformLocation(pFC, "u_mix");
    S.pFocus = pFC;
    log("programs ok: main=" + pM + " blur=" + pB + " final=" + pF + " lum8=" + pL8 + " lumAvg=" + pLA + " dof=" + pD + " focus=" + pFC);
    return pM;
}

function makeTex(w, h, filt, half) {
    var t = GL.glGenTextures();
    GL.glBindTexture(GL.TEXTURE_2D, t);
    GL.glTexParameteri(GL.TEXTURE_2D, GL.TEXTURE_MIN_FILTER, filt);
    GL.glTexParameteri(GL.TEXTURE_2D, GL.TEXTURE_MAG_FILTER, filt);
    GL.glTexParameteri(GL.TEXTURE_2D, GL.TEXTURE_WRAP_S, GL.CLAMP_TO_EDGE);
    GL.glTexParameteri(GL.TEXTURE_2D, GL.TEXTURE_WRAP_T, GL.CLAMP_TO_EDGE);
    // G3: half=true → RGBA16F HDR 中间帧(新 exe 需注册 RGBA16F/HALF_FLOAT 常量)
    var fmt = half ? GL.RGBA16F : GL.RGBA8;
    var ty = half ? GL.HALF_FLOAT : GL.UNSIGNED_BYTE;
    GL.glTexImage2D(GL.TEXTURE_2D, 0, fmt, w, h, 0, GL.RGBA, ty, null);
    GL.glBindTexture(GL.TEXTURE_2D, 0);
    return t;
}
function makeFbo(tex) {
    var f = GL.glGenFramebuffers();
    GL.glBindFramebuffer(GL.FRAMEBUFFER, f);
    GL.glFramebufferTexture2D(GL.FRAMEBUFFER, GL.COLOR_ATTACHMENT0, GL.TEXTURE_2D, tex, 0);
    var st = GL.glCheckFramebufferStatus(GL.FRAMEBUFFER);
    GL.glBindFramebuffer(GL.FRAMEBUFFER, 0);
    return st == GL.FRAMEBUFFER_COMPLETE ? f : 0;
}
function delFbo(f) { try { if (f) GL.glDeleteFramebuffers(f); } catch (e) {} }
function delTex(t) { try { if (t) GL.glDeleteTextures(t); } catch (e) {} }

// 窗口尺寸变化时重建多 pass 目标
function ensurePassTargets(w, h) {
    if (S.fMain && S.fw == w && S.fh == h) return true;
    delFbo(S.fMain); delFbo(S.fB0); delFbo(S.fB1); delFbo(S.fMB);
    delTex(S.tMain); delTex(S.tB0); delTex(S.tB1); delTex(S.tDof); delTex(S.tMB);
    S.fMain = 0; S.fB0 = 0; S.fB1 = 0; S.fDof = 0; S.fMB = 0; S.tMain = 0; S.tB0 = 0; S.tB1 = 0; S.tDof = 0; S.tMB = 0;
    var bw = Math.max(1, w >> 1), bh = Math.max(1, h >> 1);
    S.tMain = makeTex(w, h, GL.LINEAR);
    S.tB0 = makeTex(bw, bh, GL.LINEAR);
    S.tB1 = makeTex(bw, bh, GL.LINEAR);
    S.fMain = makeFbo(S.tMain);
    S.fB0 = makeFbo(S.tB0);
    S.fB1 = makeFbo(S.tB1);
    S.tDof = makeTex(w, h, GL.LINEAR);
    S.fDof = makeFbo(S.tDof);
    S.tMB = makeTex(w, h, GL.LINEAR);
    S.fMB = makeFbo(S.tMB);
    if (!S.fMain || !S.fB0 || !S.fB1 || !S.fDof || !S.fMB) {
        log("pass FBO 建失败 fMain=" + S.fMain + " fB0=" + S.fB0 + " fB1=" + S.fB1 + " fDof=" + S.fDof + " fMB=" + S.fMB);
        return false;
    }
    S.fw = w; S.fh = h;
    log("pass targets: " + w + "x" + h + " bloom " + bw + "x" + bh);
    return true;
}

function refreshCam() {
    var p = Render.getProjectionMatrix();
    if (!p || p.length < 15) return;
    var n = Math.abs(p[14] / (p[10] - 1));
    var f = Math.abs(p[14] / (p[10] + 1));
    if (n > 0.001 && f > n) { near = n; far = f; }
    S.invProj = m4inv(p);
    S.proj11 = (p.length > 5) ? p[5] : 1.0;   // proj[1][1](DoF 焦距用)
    var V = Render.getModelViewMatrix();
    if (V && V.length >= 16) {
        var rot = new Array(16);
        rot[0] = V[0]; rot[1] = V[4]; rot[2] = V[8];  rot[3] = 0;
        rot[4] = V[1]; rot[5] = V[5]; rot[6] = V[9];  rot[7] = 0;
        rot[8] = V[2]; rot[9] = V[6]; rot[10] = V[10]; rot[11] = 0;
        rot[12] = 0; rot[13] = 0; rot[14] = 0; rot[15] = 1;
        S.camRot = rot;
        // A6: 前后帧 VP(世界→clip)供运动模糊速度场
        S.vpPrev = S.vpCur || null;
        S.vpCur = m4mul(p, V);
    }
}

function drawQuad() {
    GL.glBindBuffer(GL.ARRAY_BUFFER, S.vbo);
    GL.glEnableVertexAttribArray(0);
    GL.glVertexAttribPointer(0, 2, GL.FLOAT, false, 0, 0);
    GL.glDrawArrays(GL.TRIANGLES, 0, 6);
    GL.glDisableVertexAttribArray(0);
    GL.glBindBuffer(GL.ARRAY_BUFFER, 0);
}
function toFbo(f, w, h) {
    GL.glBindFramebuffer(GL.FRAMEBUFFER, f);
    GL.glViewport(0, 0, w, h);
    GL.glClearColor(0, 0, 0, 1);
    GL.glClear(GL.COLOR_BUFFER_BIT);
}
function u1(loc, v) { if (loc >= 0) GL.glUniform1f(loc, v); }
function bindTex(unit, tex, loc) {
    GL.glActiveTexture(GL.TEXTURE0 + unit);
    GL.glBindTexture(GL.TEXTURE_2D, tex);
    if (loc >= 0) GL.glUniform1i(loc, unit);
}

// ── pass1: 主画面 → mainTex ──
function passMain(w, h, sceneTex, maskTex, reflTex, depthTex) {
    var L = S.LM;
    GL.glUseProgram(S.pMain);
    toFbo(S.fMain, w, h);
    GL.glDisable(GL.DEPTH_TEST);
    GL.glDisable(GL.BLEND);
    bindTex(0, sceneTex, L.scene);
    bindTex(1, maskTex, L.mask);
    bindTex(2, reflTex, L.refl);
    bindTex(3, depthTex, L.depth);
    bindTex(4, S.shTex, L.shadow);
    // M1: gbuffer 亮度/AO 图(diag5 查看; 老 exe 无 brightnessTexture → 绑 0)
    var bT = 0;
    if (typeof Render.brightnessTexture === "function") { try { bT = Render.brightnessTexture(); } catch (e) {} }
    bindTex(5, bT, L.bright);
    // M2: gbuffer 法线图(diag6 查看; 老 exe/引擎无 normalTexture → 绑 0)
    var nT = 0;
    if (typeof Render.normalTexture === "function") { try { nT = Render.normalTexture(); } catch (e) {} }
    bindTex(6, nT, L.norm);
    u1(L.near, near); u1(L.far, far);
    u1(L.time, (S.frame || 0) * 0.018);
    u1(L.flip, S.flip); u1(L.diag, S.diag);
    u1(L.exposure, cfg.exposure); u1(L.sat, cfg.sat); u1(L.con, cfg.con); u1(L.tone, cfg.tone);
    u1(L.fog, cfg.fog); u1(L.fogR, cfg.fogR); u1(L.fogG, cfg.fogG); u1(L.fogB, cfg.fogB);
    u1(L.wave, cfg.wave); u1(L.tint, cfg.tint); u1(L.str, cfg.str); u1(L.vig, cfg.vig);
    u1(L.vani, cfg.vanilla ? 1 : 0); u1(L.wtest, cfg.wtest ? 1 : 0);
    u1(L.shad, S.lightValid ? S.shadEff : 0);
    u1(L.shadowTexel, 1.0 / SHADOW_SIZE);
    u1(L.rev, S.shRev ? 1 : 0);
    u1(L.flipY, S.shFlipY ? 1 : 0);
    u1(L.sdep, S.shDep ? 1 : 0);
    u1(L.torN, S.torchN);
    u1(L.torR, S.torchR);
    u1(L.under, S.underWater ? 1 : 0);
    for (var ti = 0; ti < 6; ti++) {
        if (ti >= S.torchList.length) break;
        var tl = S.torLoc[ti];
        if (tl >= 0) GL.glUniform3f(tl, S.torchList[ti][0], S.torchList[ti][1], S.torchList[ti][2]);
    }
    if (S.invVP && L.invVP >= 0) GL.glUniformMatrix4fv(L.invVP, S.invVP);
    if (S.vpCur && L.vp >= 0) GL.glUniformMatrix4fv(L.vp, S.vpCur);
    if (L.cam >= 0) GL.glUniform3f(L.cam, S.camC[0], S.camC[1], S.camC[2]);
    if (S.lightValid && S.lightVP && L.lightVP >= 0) {
        GL.glUniformMatrix4fv(L.lightVP, S.lightVP);
        if (L.lightC >= 0) GL.glUniform3f(L.lightC, S.lightCx, S.lightCy, S.lightCz);
    }
    u1(L.vol, S.lightValid ? cfg.vol : 0);   // 无阴影图时体积光会采样垃圾纹理, 关掉
    var azr = S.lumAz * Math.PI / 180, elr = S.lumEl * Math.PI / 180;
    if (L.sunDir >= 0)
        GL.glUniform3f(L.sunDir, Math.cos(elr) * Math.sin(azr), Math.sin(elr), Math.cos(elr) * Math.cos(azr));
    u1(L.sunEl, S.sunEl);
    if (L.sunC >= 0) {
        var dF = S.dayFactor || dayFactor();
        var up = S.sunEl >= 0;
        var wrm = up ? (1.0 - Math.min(1.0, Math.max(0.0, Math.cos(Math.min(S.sunEl, 89) * Math.PI / 180)))) : 0;
        var col;
        if (up) {
            // 太阳光色随高度: 正午白暖 → 低角度强橙(体积雾/光散射用, 照原版 sunColor 曲率)
            col = [(0.95 + 0.55 * wrm) * dF, (0.97 - 0.10 * wrm) * dF, (0.98 - 0.62 * wrm) * dF];
        } else    col = [0.16 * dF, 0.18 * dF, 0.26 * dF];
        GL.glUniform3f(L.sunC, col[0], col[1], col[2]);
    }
    u1(L.aur, cfg.aur);
    u1(L.star, cfg.stars);
    u1(L.wday, cfg.waterday);
    u1(L.ndim, cfg.ndim);
    u1(L.uwl, cfg.uwl);
    u1(L.caut, cfg.caut);
    u1(L.skyflip, S.skyFlip);
    if (L.invProj >= 0 && S.invProj) GL.glUniformMatrix4fv(L.invProj, S.invProj);
    if (L.camRot >= 0 && S.camRot) GL.glUniformMatrix4fv(L.camRot, S.camRot);
    if (L.zen >= 0) GL.glUniform3f(L.zen, cfg.skyz[0], cfg.skyz[1], cfg.skyz[2]);
    if (L.wb >= 0) GL.glUniform3f(L.wb, cfg.wb[0], cfg.wb[1], cfg.wb[2]);
    if (L.hor >= 0) GL.glUniform3f(L.hor, cfg.skyh[0], cfg.skyh[1], cfg.skyh[2]);
    if (L.sunCol >= 0) GL.glUniform3f(L.sunCol, cfg.suncol[0], cfg.suncol[1], cfg.suncol[2]);
    u1(L.sunb, cfg.sunb);
    u1(L.again, cfg.again);
    u1(L.cirrus, cfg.cirrus);
    u1(L.clouds, cfg.clouds);
    u1(L.cloudcov, cfg.cloudcov);
    u1(L.ssao, cfg.ssao);
    u1(L.nl, cfg.nl);
    u1(L.wy, (typeof S.wy === "number") ? S.wy : -999);
    u1(L.cloudshd, cfg.cloudshd);
    u1(L.shsky, cfg.shsky);
    u1(L.hasNorm, nT > 0 ? 1 : 0);   // M2 真法线 color3 可用标记(NL 数据源切换)
    var gbV = cfg.gb;
    if (gbV > 0.001 && !(bT > 0)) gbV = 0;   // 引擎无 gbuffer color2 → 不启用
    u1(L.gb, gbV);
    drawQuad();
    GL.glBindFramebuffer(GL.FRAMEBUFFER, 0);
}

// ── pass1.5: DoF 景深(main → tDof, 照原版 DoF.glsl) ──
function passDof(w, h, depthTex, focusTex) {
    GL.glUseProgram(S.pDof);
    toFbo(S.fDof, w, h);
    bindTex(0, S.tMain, S.LD.src);
    bindTex(1, depthTex, S.LD.dpt);
    bindTex(2, focusTex || 0, S.LD.foc);
    u1(S.LD.focus, cfg.focal);
    u1(S.LD.ap, cfg.ap);
    u1(S.LD.dof, cfg.dof);
    u1(S.LD.proj11, S.proj11 || 1.0);
    u1(S.LD.near, near); u1(S.LD.far, far);
    u1(S.LD.afoc, cfg.afoc ? 1 : 0);
    GL.glUniform2f(S.LD.px, 1.0 / w, 1.0 / h);
    drawQuad();
    GL.glBindFramebuffer(GL.FRAMEBUFFER, 0);
}

// 自动对焦目标(两个 1×1 pingpong) 与 pass
function ensureFocusTargets() {
    if (S.tFoc0) return true;
    S.tFoc0 = makeTex(1, 1, GL.LINEAR);
    S.fFoc0 = makeFbo(S.tFoc0);
    S.tFoc1 = makeTex(1, 1, GL.LINEAR);
    S.fFoc1 = makeFbo(S.tFoc1);
    S.fx = 0;
    if (!S.fFoc0 || !S.fFoc1) { log("对焦目标建失败"); return false; }
    var t = 6.0 * 0.0078125;   // 焦点米→RGBA8 归一存储
    GL.glBindFramebuffer(GL.FRAMEBUFFER, S.fFoc0);
    GL.glViewport(0, 0, 1, 1);
    GL.glClearColor(t, t, t, 1); GL.glClear(GL.COLOR_BUFFER_BIT);
    GL.glBindFramebuffer(GL.FRAMEBUFFER, S.fFoc1);
    GL.glClearColor(t, t, t, 1); GL.glClear(GL.COLOR_BUFFER_BIT);
    GL.glBindFramebuffer(GL.FRAMEBUFFER, 0);
    S.focTexNow = S.tFoc0;
    log("对焦目标就绪");
    return true;
}
function passFocus(depthTex) {
    var rd = S.fx, wr = 1 - S.fx;
    GL.glUseProgram(S.pFocus);
    toFbo(wr ? S.fFoc1 : S.fFoc0, 1, 1);
    bindTex(0, depthTex, S.LFoc.dpt);
    bindTex(1, rd ? S.tFoc1 : S.tFoc0, S.LFoc.prev);
    u1(S.LFoc.near, near); u1(S.LFoc.far, far);
    u1(S.LFoc.mix, 0.55);
    drawQuad();
    GL.glBindFramebuffer(GL.FRAMEBUFFER, 0);
    S.focTexNow = wr ? S.tFoc1 : S.tFoc0;
    S.fx = wr;
}

// ── A6: 运动模糊(src → tMB) — CPU 相机角速度 → 像素拖尾 ──
function passMotion(w, h, srcTex) {
    if (!S.pMB || cfg.mb <= 0.001) return srcTex;
    var cam;
    try { cam = Render.getCamera(); } catch (e) { return srcTex; }
    var yaw = cam[3], pitch = cam[4];
    if (S.yawP === undefined) { S.yawP = yaw; S.pitP = pitch; return srcTex; }
    var dy = yaw - S.yawP;
    if (dy > 180) dy -= 360; else if (dy < -180) dy += 360;
    var dp = pitch - S.pitP;
    S.yawP = yaw; S.pitP = pitch;
    // 像素/度: 1°/帧 ≈ w*1.1% 的移动(低帧率转得更长)
    var pxH = dy * (w * 0.011) * cfg.mb;
    var pxV = dp * (h * 0.011) * cfg.mb;
    var capH = w * 0.05, capV = h * 0.05;
    if (Math.abs(pxH) < 0.2 && Math.abs(pxV) < 0.2) return srcTex;
    var L = S.LMB;
    GL.glUseProgram(S.pMB);
    toFbo(S.fMB, w, h);
    GL.glDisable(GL.DEPTH_TEST);
    GL.glDisable(GL.BLEND);
    bindTex(0, srcTex, L.src);
    u1(L.mbpx, Math.max(-capH, Math.min(capH, pxH)));
    u1(L.mbpy, Math.max(-capV, Math.min(capV, pxV)));
    GL.glUniform2f(L.texel, 1.0 / w, 1.0 / h);
    drawQuad();
    GL.glBindFramebuffer(GL.FRAMEBUFFER, 0);
    return S.tMB;
}

// ── pass2: bloom(提取+横/纵高斯, 半分辨率) ──
function passBloom(w, h, srcTex) {
    var bw = Math.max(1, w >> 1), bh = Math.max(1, h >> 1);
    var tx = 1.0 / bw, ty = 1.0 / bh;
    GL.glUseProgram(S.pBlur);
    GL.glDisable(GL.DEPTH_TEST);
    GL.glDisable(GL.BLEND);
    // 横向: 提取亮部(阈) + blurH → B0
    toFbo(S.fB0, bw, bh);
    bindTex(0, srcTex || S.tMain, S.LB.src);
    u1(S.LB.thr, cfg.blthr);
    u1(S.LB.dir, 2.0);   // >1.5: 横向且做亮部提取
    GL.glUniform2f(S.LB.texel, tx, ty);
    drawQuad();
    // 纵向: blurV → B1
    toFbo(S.fB1, bw, bh);
    bindTex(0, S.tB0, S.LB.src);
    u1(S.LB.thr, 0.0);
    u1(S.LB.dir, 0.0);
    GL.glUniform2f(S.LB.texel, tx, ty);
    drawQuad();
    GL.glBindFramebuffer(GL.FRAMEBUFFER, 0);
}

// A7 TAA pass: srcTex + 历史帧 → 时域稳定帧(返回其纹理, 并成为下次历史)
function passTaa(w, h, srcTex, depthTex) {
    if (!S.fTa || S.taW != w || S.taH != h) {
        delFbo(S.fTa); delTex(S.tTa);
        S.tTa = makeTex(w, h, GL.LINEAR);
        S.fTa = makeFbo(S.tTa);
        S.taW = w; S.taH = h;
        S.taaInit = 0;
    }
    var L = S.LT;
    GL.glUseProgram(S.pTaa);
    toFbo(S.fTa, w, h);
    bindTex(0, srcTex, L.cur);
    bindTex(1, S.tTa, L.hist);
    bindTex(2, depthTex, L.depth);
    if (S.invVP) GL.glUniformMatrix4fv(L.invVP, S.invVP);
    if (S.vpPrev) GL.glUniformMatrix4fv(L.vpP, S.vpPrev);
    if (L.cam >= 0) GL.glUniform3f(L.cam, S.camC[0], S.camC[1], S.camC[2]);
    var cp = S.taaCamPrev || [S.camC[0], S.camC[1], S.camC[2]];
    if (L.camP >= 0) GL.glUniform3f(L.camP, cp[0], cp[1], cp[2]);
    u1(L.sdep, S.shDep ? 1 : 0);
    u1(L.flipY, S.shFlipY ? 1 : 0);
    u1(L.first, S.taaInit ? 0 : 1);
    GL.glUniform2f(L.texel, 1.0 / Math.max(w, 1), 1.0 / Math.max(h, 1));
    drawQuad();
    GL.glBindFramebuffer(GL.FRAMEBUFFER, 0);
    S.taaInit = 1;
    S.taaCamPrev = [S.camC[0], S.camC[1], S.camC[2]];
    return S.tTa;
}

// ── pass3: final(main + bloom → 窗口) ──
function passFinal(w, h, mainTex) {
    GL.glUseProgram(S.pFinal);
    GL.glBindFramebuffer(GL.FRAMEBUFFER, 0);
    GL.glViewport(0, 0, w, h);
    GL.glDisable(GL.DEPTH_TEST);
    GL.glDisable(GL.BLEND);
    bindTex(0, mainTex || S.tMain, S.LF.main);
    bindTex(1, S.tB1, S.LF.bloom);
    bindTex(2, S.expTex || 0, S.LF.exp);
    u1(S.LF.amt, cfg.bloom);
    u1(S.LF.vig, cfg.vig);
    u1(S.LF.diag, S.diag);
    u1(S.LF.auto, cfg.auto);
    u1(S.LF.tgt, cfg.tgt);
    u1(S.LF.cine, cfg.cine ? 1 : 0);
    u1(S.LF.cas, cfg.cas);
    u1(S.LF.glow, cfg.glow);
    if (S.LF.tp >= 0) GL.glUniform2f(S.LF.tp, 1.0 / Math.max(w, 1), 1.0 / Math.max(h, 1));
    u1(S.LF.aspect, w / Math.max(h, 1));
    drawQuad();
    GL.glActiveTexture(GL.TEXTURE0);
    GL.glBindTexture(GL.TEXTURE_2D, 0);
}

// ── 自动曝光目标(8×8 亮度 + 两个 1×1 pingpong 时域平滑) ──
function ensureLumTargets() {
    if (S.tL8) return true;
    S.tL8 = makeTex(8, 8, GL.LINEAR);
    S.fL8 = makeFbo(S.tL8);
    S.tX0 = makeTex(1, 1, GL.LINEAR);
    S.fX0 = makeFbo(S.tX0);
    S.tX1 = makeTex(1, 1, GL.LINEAR);
    S.fX1 = makeFbo(S.tX1);
    S.xw = 0;
    if (!S.fL8 || !S.fX0 || !S.fX1) {
        log("自动曝光目标建失败(关自动曝光)");
        return false;
    }
    var t = Math.min(Math.max(cfg.tgt, 0.05), 0.9);
    GL.glBindFramebuffer(GL.FRAMEBUFFER, S.fX0);
    GL.glViewport(0, 0, 1, 1);
    GL.glClearColor(t, t, t, 1); GL.glClear(GL.COLOR_BUFFER_BIT);
    GL.glBindFramebuffer(GL.FRAMEBUFFER, S.fX1);
    GL.glClearColor(t, t, t, 1); GL.glClear(GL.COLOR_BUFFER_BIT);
    GL.glBindFramebuffer(GL.FRAMEBUFFER, 0);
    S.expTex = S.tX0;
    log("自动曝光目标就绪");
    return true;
}
function passLum8(srcTex) {
    GL.glUseProgram(S.pLum8);
    toFbo(S.fL8, 8, 8);
    bindTex(0, srcTex || S.tMain, S.LL8.src);
    drawQuad();
    GL.glBindFramebuffer(GL.FRAMEBUFFER, 0);
}
function passLumAvg() {
    var rd = S.xw, wr = 1 - S.xw;
    var fboWr = wr ? S.fX1 : S.fX0;
    GL.glUseProgram(S.pLumAvg);
    toFbo(fboWr, 1, 1);
    bindTex(0, S.tL8, S.LLA.src);
    bindTex(1, rd ? S.tX1 : S.tX0, S.LLA.prev);
    u1(S.LLA.up, cfg.aup);
    u1(S.LLA.down, cfg.adown);
    drawQuad();
    GL.glBindFramebuffer(GL.FRAMEBUFFER, 0);
    S.expTex = wr ? S.tX1 : S.tX0;
    S.xw = wr;
}

function __compFrame(sceneTex, w, h, maskTex, reflTex) {
    S.frame = (S.frame || 0) + 1;
    if (!S.ok) return;
    if (!sceneTex || !maskTex || !reflTex) return;
    updateHandLight();
    if (cfg.sunAuto) sunTick();
    else applySunManual();
    if (!S.pMain && !S.tried) {
        S.tried = true;
        var p = buildPrograms();
        if (!p) { msg("shader 编译失败, 已回退(看日志)"); Render.enable(false); return; }
    }
    if (!S.pMain) return;
    var depthTex = Render.depthTexture();
    if (!depthTex) return;
    refreshCam();
    updateShadowFrame();
    if (!ensurePassTargets(w, h)) { Render.enable(false); return; }

    passMain(w, h, sceneTex, maskTex, reflTex, depthTex);
    if (S.diag < 0.5) {
        var curTex = S.tMain;
        if (cfg.dof > 0.01) {
            var focTex = 0;
            if (cfg.afoc) { if (ensureFocusTargets()) { passFocus(depthTex); focTex = S.focTexNow; } }
            passDof(w, h, depthTex, focTex);
            curTex = S.tDof;
        }
        if (cfg.mb > 0.001 && S.pMB) curTex = passMotion(w, h, curTex);
        if (cfg.taa > 0.001 && S.pTaa && S.vpPrev) curTex = passTaa(w, h, curTex, depthTex);
        if (cfg.bloom > 0.001) passBloom(w, h, curTex);
        if (cfg.auto) {
            if (ensureLumTargets()) { passLum8(curTex); passLumAvg(); }
        }
        passFinal(w, h, curTex);
    } else {
        passFinal(w, h, S.tMain);
    }
    GL.glUseProgram(0);
}

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
    S.pMain = 0; S.tried = false;
    if (cfg.sunAuto) sunTick(); else applySunManual();
    msg("Derivative 已开(/shoff 关; /autosun /bloom /aur /skyz /skyh 等调参)");
}

function stop() {
    Render.enable(false);
    S.ok = false;
    userOff = true;
    msg("已关, 恢复原画面");
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
            msg("找到水面 y=" + wy + ", 水反生效");
        }
    }
    if (S.wy > -100 && autoTick - lastScan > 400) {
        lastScan = autoTick;
        var wy = scanWater(20);
        if (wy > -900 && Math.abs(wy - S.wy) > 0.5) {
            S.wy = wy;
            Render.setWaterLevel(wy);
        }
    }
    if (S.torDirty || (autoTick - S.torScan) > 10) {
        S.torScan = autoTick;
        S.torDirty = false;
        scanTorches();
    }
}

function trySway() {
    try { if (typeof Render.setSway === "function") Render.setSway(cfg.sway, cfg.swayS); } catch (e) {}
}

function onChat(raw) {
    var m = raw;
    // 世界时间(照原版 /time): time <dawn|noon|dusk|night> | time <0..19199 tick>
    // 0.6.1 一天 19200 tick: t%19200=0 日出上升, 正午≈4800, 日落≈9600, 午夜≈14400
    var tw = m.replace(/^\/?/, "").trim().toLowerCase().split(/[ \t]+/);
    if (tw[0] === "time" || tw[0] === "t") {
        var presets = { "dawn": 450, "sunrise": 450, "morning": 1800, "noon": 4800, "day": 4800,
            "dusk": 9500, "sunset": 9500, "night": 14400, "midnight": 14400 };
        var tk;
        if (tw[1] !== undefined && presets[tw[1]] !== undefined) tk = presets[tw[1]];
        else if (tw[1] !== undefined && !isNaN(parseInt(tw[1], 10))) tk = parseInt(tw[1], 10) % 19200;
        else {
            msg("time 用法: time dawn | noon | dusk | night | <tick 0..19199>(日出≈450 正午≈4800 黄昏≈9500 夜≈14400)");
            return;
        }
        try {
            level.setTime(tk);
            S.shDirty = true;   // 太阳角度突变 → 下帧重渲阴影
            msg("世界时间 = " + tk + " tick (日出≈450 正午≈4800 黄昏≈9500 夜≈14400; 昼夜自动跟随)");
        } catch (e) { msg("level.setTime 不可用: " + e); }
        return;
    }
    function v3(cmd, arr) {
        if (m.indexOf(cmd) != 0) return false;
        var p = m.slice(cmd.length).split(/[ ,]+/);
        if (p.length >= 3 && !isNaN(parseFloat(p[0])) && !isNaN(parseFloat(p[1])) && !isNaN(parseFloat(p[2]))) {
            arr[0] = parseFloat(p[0]); arr[1] = parseFloat(p[1]); arr[2] = parseFloat(p[2]);
            msg(cmd + " = " + arr[0] + " " + arr[1] + " " + arr[2]);
            return true;
        }
        return false;
    }
    function setNum(prefix, key, isF, lo, hi) {
        if (m.indexOf(prefix) != 0) return false;
        var v = isF ? parseFloat(m.slice(prefix.length)) : parseInt(m.slice(prefix.length));
        if (isNaN(v)) return false;
        if (lo !== undefined && hi !== undefined) v = Math.max(lo, Math.min(hi, v));
        cfg[key] = v;
        msg(prefix + " = " + v);
        if (!S.ok) start();
        return true;
    }
    if (m == "/sh")    { start(); return; }
    if (m == "/shoff") { stop(); return; }
    // /diag [0..6] 直接切换; 无参数循环(0=合成 1=mask 2=refl 3=深度 4=阴影图 5=亮度/AO图(M1) 6=法线图(M2))
    var dg = m.replace(/^\/?/, "").match(/^diag\s*(\d*)/);
    if (dg) {
        var NMS = ["合成", "mask(水)", "refl(反射)", "深度", "阴影图", "亮度/AO图(M1)", "法线图(M2)"];
        if (dg[1] === "" ) S.diag = (S.diag + 1) % NMS.length;
        else {
            var v = parseInt(dg[1], 10);
            if (!isNaN(v)) S.diag = Math.max(0, Math.min(NMS.length - 1, v));
        }
        msg("视图=" + NMS[S.diag]);
        return;
    }
    if (m == "/autosun") { cfg.sunAuto = cfg.sunAuto ? 0 : 1; msg("自动昼夜太阳=" + (cfg.sunAuto ? "开" : "关(/sun az el 手动)")); return; }
    if (m == "/autoexp") { cfg.auto = (cfg.auto + 1) % 3; msg("自动曝光=" + (cfg.auto == 2 ? "只压暗不抬(黑处黑)" : cfg.auto == 1 ? "全自动(提暗部)" : "关,用 /exp")); return; }
    if (m == "/cine") { cfg.cine = cfg.cine ? 0 : 1; msg("电影黑边 21:9=" + (cfg.cine ? "开" : "关")); return; }
    if (setNum("/tgt ", "tgt", true, 0.15, 0.9)) return;
    if (m == "/skyflip") { S.skyFlip = S.skyFlip ? 0 : 1; msg("天空射线 y 翻转=" + (S.skyFlip ? "开" : "关")); return; }
    if (m == "/scan") {
        var wy = scanWater(32);
        if (wy > -900) { S.wy = wy; Render.setWaterLevel(wy); msg("水面 y=" + wy); }
        else msg("没扫到水(方块8/9)");
        return;
    }
    if (m.indexOf("/wy ") == 0) {
        var v = parseFloat(m.slice(4));
        S.wy = v; Render.setWaterLevel(v); msg("水面 y=" + v); return;
    }
    if (m == "/flip") { S.flip = S.flip ? 0 : 1; msg("反射翻转=" + (S.flip ? "开" : "关")); return; }
    if (m == "/cam") {
        var c = Render.getCamera();
        msg("cam=(" + c[0].toFixed(1) + "," + c[1].toFixed(1) + "," + c[2].toFixed(1) + ") yaw=" + c[3].toFixed(1) + " pitch=" + c[4].toFixed(1));
        return;
    }
    if (m == "/shinfo") {
        msg("Derivative " + VER + ": 影强=" + cfg.shad + " 昼夜=" + (S.dayFactor == null ? "?" : S.dayFactor.toFixed(2))
            + " 生效=" + (S.lightValid ? (S.shadEff || 0).toFixed(2) : "关")
            + " 发光体az/el=" + S.lumAz.toFixed(0) + "/" + S.lumEl.toFixed(0)
            + " 太阳el=" + S.sunEl.toFixed(1) + " auto=" + cfg.sunAuto
            + " bloom=" + cfg.bloom + " thr=" + cfg.blthr + " aur=" + cfg.aur + " stars=" + cfg.stars
            + (S.lightValid ? " 图有效" : " 无图"));
        return;
    }
    if (m == "/sforce") { S.shDirty = true; msg("强制重渲阴影(下帧)"); return; }
    if (m == "/srev") { S.shRev = !S.shRev; S.shDirty = true; msg("阴影深度比较=" + (S.shRev ? "反向" : "正向")); return; }
    if (m == "/sflip") { S.shFlipY = !S.shFlipY; S.shDirty = true; msg("重建UV翻转=" + (S.shFlipY ? "开" : "关")); return; }
    if (m == "/sdep") { S.shDep = !S.shDep; S.shDirty = true; msg("深度方向反转=" + (S.shDep ? "开" : "关")); return; }
    if (m == "/shad") { cfg.shad = (cfg.shad > 0) ? 0 : 0.45; S.shDirty = true; msg("阴影 = " + (cfg.shad > 0 ? "开" : "关")); return; }
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
                cfg.sunAz = az; cfg.sunEl = el; cfg.sunAuto = 0;
                applySunManual(); S.shDirty = true;
                msg("太阳 az=" + az + " el=" + el + "(手动; /autosun 回自动)");
            }
        }
        return;
    }
    if (setNum("/aup ", "aup", true, 0.01, 0.9)) return;
    if (setNum("/adown ", "adown", true, 0.01, 0.9)) return;
    if (setNum("/tgt ", "tgt", true, 0.1, 0.7)) return;
    if (setNum("/exp ", "exposure", true)) return;
    if (setNum("/sat ", "sat", true)) return;
    if (setNum("/con ", "con", true)) return;
    // (v1 固定 Derivative Grade: AcademyFit tonemap 全量; 无 tone 档位)
    if (setNum("/fog ", "fog", true)) return;
    if (setNum("/tint ", "tint", true)) return;
    if (setNum("/wave ", "wave", true)) return;
    if (setNum("/vol ", "vol", true, 0, 6)) return;
    if (setNum("/str ", "str", true)) return;
    if (setNum("/vig ", "vig", true)) return;
    if (setNum("/bloom ", "bloom", true, 0, 3)) return;
    if (setNum("/blthr ", "blthr", true, 0, 1)) return;
    if (setNum("/aur ", "aur", true, 0, 2)) return;
    if (setNum("/stars ", "stars", true, 0, 2)) return;
    if (setNum("/sunb ", "sunb", true, 0.1, 3)) return;
    if (setNum("/again ", "again", true, 0.5, 16)) return;
    if (setNum("/tone ", "tone", true, 0, 1)) return;
    if (setNum("/ndim ", "ndim", true, 0.1, 1)) return;
    if (setNum("/uwl ", "uwl", true, 0, 2)) return;
    if (setNum("/caut ", "caut", true, 0, 2)) return;
    if (setNum("/cas ", "cas", true, 0, 1.5)) return;
    if (setNum("/glow ", "glow", true, 0, 1.5)) return;
    if (setNum("/taa ", "taa", true, 0, 1)) return;
    if (setNum("/wday ", "waterday", true, 0, 1)) return;
    if (setNum("/mb ", "mb", true, 0, 1.5)) return;
    if (setNum("/cirrus ", "cirrus", true, 0, 2)) return;
    if (setNum("/clouds ", "clouds", true, 0, 3)) return;
    if (setNum("/cloudcov ", "cloudcov", true, 0, 1)) return;
    if (setNum("/shsky ", "shsky", true, 0, 1.5)) return;
    if (setNum("/sway ", "sway", true, 0, 2)) { trySway(); return; }
    if (setNum("/sways ", "swayS", true, 0.1, 3)) { trySway(); return; }
    if (setNum("/cloudshd ", "cloudshd", true, 0, 1)) return;
    // /gb 宽容匹配: gb / gb1 / gb 1 / /gb 1
    var gbM = m.replace(/^\/?/, "").match(/^gb\s*(\d*\.?\d*)/);
    if (gbM) {
        var gv = 1;
        if (gbM[1] !== undefined && gbM[1] !== "") gv = Math.max(0, Math.min(1, parseFloat(gbM[1])));
        cfg.gb = gv;
        S.shDirty = true;
        msg("gb = " + gv + " (亮度图入合成: 影区按每像素环境光调深浅; 0=关)");
        return;
    }
    if (setNum("/gb ", "gb", true, 0, 1)) { S.shDirty = true; return; }
    if (setNum("/dof ", "dof", true, 0, 3)) return;
    if (m == "/afoc") { cfg.afoc = cfg.afoc ? 0 : 1; msg("DoF 对焦=" + (cfg.afoc ? "自动(看哪清哪)" : "手动(/focal 距离)")); return; }
    if (setNum("/focal ", "focal", true, 0.1, 100)) return;
    if (setNum("/ap ", "ap", true, 0.8, 32)) return;
    if (setNum("/ssao ", "ssao", true, 0, 2)) return;
    if (setNum("/nl ", "nl", true, 0, 1)) return;
    if (m == "/water" || m == "/wr") {
        cfg.vanilla = !cfg.vanilla;
        msg("[水] " + (cfg.vanilla ? "原版水视图 ON" : "水反+波光 ON"));
        return;
    }
    if (m == "/wtest") {
        cfg.wtest = !cfg.wtest;
        msg("[测试] 水面闪屏 " + (cfg.wtest ? "ON" : "OFF") + " frame=" + (S.frame || 0));
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
    if (v3("/skyz ", cfg.skyz)) return;
    if (v3("/wb ", cfg.wb)) return;
    if (v3("/skyh ", cfg.skyh)) return;
    if (v3("/suncol ", cfg.suncol)) return;
}

function onBreakBlock() { if (S.ok) S.shDirty = true; S.torDirty = true; }
function onPlaceBlock() { if (S.ok) S.shDirty = true; S.torDirty = true; }

// 保险丝: 回调内任何异常都不得让画面永久定格。限流日志;
// 连续 60 帧出错自动回退原画面(避免"卡死"一帧)。
function onRenderComposite(sceneTex, w, h, maskTex, reflTex) {
    // M0: gbuffer color2 通道探测(每合成回调都便宜; 老引擎/旧 exe 无此函数 → 自动回退)
    try {
        if (!S.gbChecked) {
            S.gbChecked = true;
            if (typeof Render.brightnessTexture === "function") {
                var b = Render.brightnessTexture();
                S.gbBright = b;
                log("gbuffer color2: " + (b > 0 ? "可用(id=" + b + ")" : "已注册待 M1 写通道"));
            } else log("gbuffer color2: 此构建不支持, 走屏幕空间近似(M0 回退)");
            if (typeof Render.normalTexture === "function") {
                var n2 = Render.normalTexture();
                S.gbNorm = n2;
                log("gbuffer color3(normal): " + (n2 > 0 ? "可用(id=" + n2 + ")" : "已注册但 id=0(附件未建?)"));
            } else log("gbuffer color3(normal): 此构建不支持(旧 exe, 需 M2 引擎)");
            if (typeof Render.gbufferInfo === "function") {
                try {
                    var gi = Render.gbufferInfo();
                    log("gbuffer 探测: bright=" + gi[0] + " normal=" + gi[1] + " normalProg=" + (gi[2] === 0 ? "未建" : (gi[2] === 1 ? "OK" : "编译失败")));
                } catch (e) { log("gbufferInfo 异常: " + e); }
            }
        }
    } catch (e) { log("gbuffer 探测异常: " + e); }
    // 植被摇摆: 引擎默认关(原版画面不摇), 光影启动时按 cfg 开启/调速
    if (!S.swaySet) {
        S.swaySet = true;
        try { if (typeof Render.setSway === "function") Render.setSway(cfg.sway, cfg.swayS); } catch (e) {}
    }
    try {
        __compFrame(sceneTex, w, h, maskTex, reflTex);
    } catch (err) {
        S.compErr = (S.compErr || 0) + 1;
        if (S.compErr === 1 || S.compErr % 25 === 0)
            log("onRenderComposite err#" + S.compErr + ": " + err);
        if (S.compErr > 60) {
            log("合成持续出错, 自动回退原画面(重启光影可重试)");
            Render.enable(false);
            S.ok = false;
        }
    }
}
