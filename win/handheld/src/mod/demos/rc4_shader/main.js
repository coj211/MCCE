// name: 光影包 v0.1 · 合成级(通透明亮)
// author: dev
// version: 0.1.0
// description: 成品光影(合成级,对标 Complementary Reimagined 观感):
//   离屏 scene+深度+水mask+平面水反 上做 曝光/白平衡/ACES风格色调映射/
//   饱和度对比/空气透视雾/菲涅尔水反/轻微暗角;昼夜随 level.getTime() 联动。
//   纯全屏合成,不换核心渲染(手机 GLES 构建不编译离屏,不受影响)。
//   命令: /sh 开  /shoff 关  /mode 0原样|1完整|2调色+雾|3仅水
//         /diag 0合成|1mask|2refl|3深度   /scan 扫水面  /wy x 手设水面
//         /exp x /sat x /con x /tone 0|1|2  /fog x  /fogcol r g b
//         /tint x /wave x /str x /vig x /flip /cam
//   进世界约 2 秒自动开一次。

var S = { prog: 0, vbo: 0, ok: false, tried: false, diag: 0, flip: 0, wy: -999 };
var cfg = {
    exposure: 1.00, sat: 1.12, con: 1.10, tone: 2,   // 调色
    fog: 0.30, fogR: 0.78, fogG: 0.84, fogB: 0.92,   // 空气雾(淡蓝白)
    wave: 1.0, tint: 0.10, str: 0.85, vig: 0.12      // 水/暗角
};
var near = 0.05, far = 256.0;
var userOff = false, autoTick = 0;
var lastScan = 0;

function log(s) { modLog("[rc4] " + s); }
function msg(s)  { if (typeof player !== "undefined" && player) player.sendMessage("[光影] " + s); }

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
        "uniform float u_near, u_far;",
        "uniform float u_time, u_flip, u_diag;",
        "uniform float u_exposure, u_sat, u_con, u_tone;",
        "uniform float u_fog, u_fogR, u_fogG, u_fogB;",
        "uniform float u_wave, u_tint, u_str, u_vig;",
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
        "void main(){",
        "  vec4 m = texture(u_mask, vUv);",
        "  float isWater = m.r + m.g + m.b;",
        // 诊断视图
        "  if (u_diag > 0.5 && u_diag < 1.5) { fragColor = m; return; }",
        "  if (u_diag > 1.5 && u_diag < 2.5) { fragColor = texture(u_refl, vUv); return; }",
        "  if (u_diag > 2.5) { float d = linDepth(vUv); float g = clamp(1.0 - d / u_far, 0.0, 1.0); fragColor = vec4(vec3(g), 1.0); return; }",
        //
        "  vec3 col = texture(u_scene, vUv).rgb;",
        "  float lin = linDepth(vUv);",
        // ── 水反(菲涅尔式: 水面越接近地平线/掠射,反射越强) ──
        "  if (isWater > 0.09) {",
        "    vec2 ruv = vec2(vUv.x, mix(vUv.y, 1.0 - vUv.y, u_flip));",
        "    ruv.x += sin(vUv.y * 33.0 + u_time * 2.1) * 0.0012 * u_wave;",
        "    ruv.y += (sin(vUv.x * 50.0 + u_time * 1.6) * 0.0022 + sin(vUv.x * 19.0 - u_time * 0.9) * 0.0028) * u_wave;",
        "    ruv = clamp(ruv, 0.0, 1.0);",
        "    vec3 refl = texture(u_refl, ruv).rgb;",
        // 屏幕纵坐标近似入射角: 水在画面越靠上=越远=越掠射
        "    float fres = pow(1.0 - clamp(vUv.y, 0.0, 1.0), 1.4);",
        "    float k = clamp(u_str * (0.12 + 0.88 * fres), 0.0, 1.0);",
        "    col = mix(col, refl, k);",
        // 水体自身颜色(近处/直射成分)
        "    vec3 waterCol = vec3(0.02, 0.16, 0.30);",
        "    col = mix(col, waterCol, u_tint * (0.35 + 0.65 * (1.0 - fres)));",
        "  }",
        // ── 空气透视雾(远处向淡蓝白雾色融,天空色近似雾色故天空不变灰) ──
        "  float fd = clamp(lin / u_far, 0.0, 1.0);",
        "  vec3 fogC = vec3(u_fogR, u_fogG, u_fogB);",
        "  col = mix(col, fogC, (fd * fd) * u_fog);",
        // ── 曝光/白平衡/色调映射/饱和/对比 ──
        "  col *= u_exposure;",
        "  col = aces(col, u_tone * 0.5);",           // 0.5 档先温和
        "  if (u_tone > 1.5) col = aces(col, 1.0);",  // 2 档全量
        "  float l = dot(col, vec3(0.2126, 0.7152, 0.0722));",
        "  col = mix(vec3(l), col, u_sat);",
        "  col = (col - 0.5) * u_con + 0.5;",
        "  col = clamp(col, 0.0, 1.0);",
        // ── 轻微暗角 ──
        "  float d2 = length(vUv - 0.5) * 1.15;",
        "  col *= 1.0 - u_vig * smoothstep(0.45, 0.9, d2);",
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
        str:   GL.glGetUniformLocation(p, "u_str"),
        vig:   GL.glGetUniformLocation(p, "u_vig")
    };
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
    u1(L.time, (new Date()).getTime() / 1000.0);
    u1(L.flip, S.flip); u1(L.diag, S.diag);
    u1(L.exposure, cfg.exposure); u1(L.sat, cfg.sat); u1(L.con, cfg.con); u1(L.tone, cfg.tone);
    u1(L.fog, cfg.fog); u1(L.fogR, cfg.fogR); u1(L.fogG, cfg.fogG); u1(L.fogB, cfg.fogB);
    u1(L.wave, cfg.wave); u1(L.tint, cfg.tint); u1(L.str, cfg.str); u1(L.vig, cfg.vig);

    GL.glBindBuffer(GL.ARRAY_BUFFER, S.vbo);
    GL.glEnableVertexAttribArray(0);
    GL.glVertexAttribPointer(0, 2, GL.FLOAT, false, 0, 0);
    GL.glDrawArrays(GL.TRIANGLES, 0, 6);
    GL.glDisableVertexAttribArray(0);
    GL.glBindBuffer(GL.ARRAY_BUFFER, 0);
}

function onRenderComposite(sceneTex, w, h, maskTex, reflTex) {
    if (!S.ok) return;
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

    draw();

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
    msg("光影已开: /mode /exp /sat /fog /tint 调参, /shoff 关");
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
}

function onChat(raw) {
    var m = raw;
    if (m == "/sh")    { start(); return; }
    if (m == "/shoff") { stop(); return; }
    if (m.indexOf("/mode") == 0) {
        var v = parseInt(m.slice(6));
        if (isNaN(v)) v = 1;
        v = ((v % 4) + 4) % 4;
        // 模式 → cfg 组合
        if (v == 0) { cfg.fog = 0; cfg.str = 0; cfg.sat = 1; cfg.con = 1; cfg.tone = 0; cfg.vig = 0; cfg.exposure = 1; cfg.tint = 0; }
        if (v == 1) { cfg.fog = 0.30; cfg.str = 0.85; cfg.sat = 1.12; cfg.con = 1.10; cfg.tone = 2; cfg.vig = 0.12; cfg.exposure = 1.00; cfg.tint = 0.10; }
        if (v == 2) { cfg.fog = 0.30; cfg.str = 0;    cfg.sat = 1.12; cfg.con = 1.10; cfg.tone = 2; cfg.vig = 0.12; cfg.exposure = 1.00; cfg.tint = 0; }
        if (v == 3) { cfg.fog = 0;    cfg.str = 0.85; cfg.sat = 1;   cfg.con = 1;    cfg.tone = 0; cfg.vig = 0;    cfg.exposure = 1.00; cfg.tint = 0.10; }
        if (!S.ok) start();
        msg("mode=" + v + " (0原样 1完整 2调色+雾 3仅水反)");
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
    if (setNum("/str ", "str", true)) return;
    if (setNum("/vig ", "vig", true)) return;
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
