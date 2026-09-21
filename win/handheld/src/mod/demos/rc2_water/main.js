// name: 渲染API片2·平面反射水反
// author: dev
// version: 0.1.0
// description: 引擎镜像相机水反通路验证：世界先渲进离屏 scene 纹理，
//   引擎用"绕水面镜像的相机"把世界(层0/1)再渲成 reflTex，层2(水)渲成
//   maskTex；回调 onRenderComposite(scene, w, h, mask, refl)。本模组用裸 GL
//   在真实水像素上合成 refl(带波纹/水色)，非水像素原样。
//   命令：/water 开  /wateroff 关  /scan 扫水面y  /wy 12.0 手设水面
//   /wave 0-4 波纹  /tint 0-0.6 水色  /flip 反射翻转  /diag 0合成 1mask 2refl

var S = { prog: 0, vbo: 0, ok: false, tried: false, flip: 0, diag: 0, wy: -999 };
var cfg = { wave: 1.0, tint: 0.10, strength: 0.85 };

function log(s) { modLog("[rc2] " + s); }

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
        "uniform float u_time;",
        "uniform float u_flip;",
        "uniform float u_wave;",
        "uniform float u_tint;",
        "uniform float u_strength;",
        "uniform float u_diag;",
        "in vec2 vUv;",
        "out vec4 fragColor;",
        "void main(){",
        "  vec2 uv = vUv;",
        "  vec4 sc = texture(u_scene, uv);",
        //  诊断视图: 1 = mask, 2 = refl
        "  if (u_diag > 1.5) { fragColor = texture(u_refl, uv); return; }",
        "  if (u_diag > 0.5) { fragColor = texture(u_mask, uv); return; }",
        //  水 mask: 层2 渲成非黑(水纹理色), 非水=黑
        "  vec4 m = texture(u_mask, uv);",
        "  float isWater = m.r + m.g + m.b;",
        "  if (isWater < 0.09) { fragColor = sc; return; }",
        //  反射采样(可翻转) + 波纹扰动
        "  vec2 ruv = vec2(uv.x, mix(uv.y, 1.0 - uv.y, u_flip));",
        "  ruv.x += (sin(uv.y * 33.0 + u_time * 2.1)) * 0.0012 * u_wave;",
        "  ruv.y += (sin(uv.x * 50.0 + u_time * 1.6) * 0.0022 + sin(uv.x * 19.0 - u_time * 0.9) * 0.0028) * u_wave;",
        "  ruv = clamp(ruv, 0.0, 1.0);",
        "  vec3 refl = texture(u_refl, ruv).rgb;",
        "  vec3 outC = mix(sc.rgb, refl, clamp(u_strength, 0.0, 1.0));",
        "  vec3 waterCol = vec3(0.05, 0.28, 0.50);",
        "  outC = mix(outC, waterCol, u_tint);",
        "  fragColor = vec4(min(outC, vec3(0.98)), 1.0);",
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
    GL.glBufferData(GL.ARRAY_BUFFER, [
        -1,-1,  1,-1,  -1,1,
         1,-1,  1,1,   -1,1
    ], GL.STATIC_DRAW);
    GL.glBindBuffer(GL.ARRAY_BUFFER, 0);

    S.prog = p;
    S.vbo = b;
    S.loc = {
        scene: GL.glGetUniformLocation(p, "u_scene"),
        mask:  GL.glGetUniformLocation(p, "u_mask"),
        refl:  GL.glGetUniformLocation(p, "u_refl"),
        time:  GL.glGetUniformLocation(p, "u_time"),
        flip:  GL.glGetUniformLocation(p, "u_flip"),
        wave:  GL.glGetUniformLocation(p, "u_wave"),
        tint:  GL.glGetUniformLocation(p, "u_tint"),
        strength: GL.glGetUniformLocation(p, "u_strength"),
        diag:  GL.glGetUniformLocation(p, "u_diag")
    };
    log("prog=" + p + " locs s/m/r=" + S.loc.scene + "/" + S.loc.mask + "/" + S.loc.refl);
    return p;
}

function onRenderComposite(sceneTex, w, h, maskTex, reflTex) {
    if (!S.ok) return;
    if (!sceneTex || !maskTex || !reflTex) return;
    if (!S.prog && !S.tried) {
        S.tried = true;
        var p = build();
        if (!p) { player.sendMessage("[rc2] shader 编译失败,已回退(看日志)"); Render.enable(false); return; }
    }
    if (!S.prog) return;

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

    GL.glUniform1f(S.loc.time, (new Date()).getTime() / 1000.0);
    GL.glUniform1f(S.loc.flip, S.flip);
    GL.glUniform1f(S.loc.wave, cfg.wave);
    GL.glUniform1f(S.loc.tint, cfg.tint);
    GL.glUniform1f(S.loc.strength, S.wy > -100 ? cfg.strength : 0.0);
    GL.glUniform1f(S.loc.diag, S.diag);

    GL.glBindBuffer(GL.ARRAY_BUFFER, S.vbo);
    GL.glEnableVertexAttribArray(0);
    GL.glVertexAttribPointer(0, 2, GL.FLOAT, false, 0, 0);
    GL.glDrawArrays(GL.TRIANGLES, 0, 6);
    GL.glDisableVertexAttribArray(0);
    GL.glBindBuffer(GL.ARRAY_BUFFER, 0);

    GL.glActiveTexture(GL.TEXTURE2); GL.glBindTexture(GL.TEXTURE_2D, 0);
    GL.glActiveTexture(GL.TEXTURE1); GL.glBindTexture(GL.TEXTURE_2D, 0);
    GL.glActiveTexture(GL.TEXTURE0); GL.glBindTexture(GL.TEXTURE_2D, 0);
    GL.glUseProgram(0);
}

// 扫描玩家附近水面:返回最高水方块顶部 y,没有则 -999
// (MCPE 0.6: 8=flowing water 9=stationary water)
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

function msg(s) { player.sendMessage("[rc2] " + s); }

function start() {
    if (!(typeof GL !== "undefined" && typeof Render !== "undefined")) {
        msg("此构建没有 GL/Render API");
        return;
    }
    userOff = false;
    Render.enable(true);
    S.ok = true;
    S.prog = 0; S.tried = false;
    msg("平面反射水反已开 (waterY=" + S.wy + ") 找片湖/海看");
}

function stop() {
    Render.enable(false);
    S.ok = false;
    userOff = true;    // 手动关 = 不再自动开
    msg("已关,恢复原画面");
}

function onTick() {
    if (userOff) return;
    if (typeof GL === "undefined" || typeof Render === "undefined") return;
    autoTick++;
    // 进世界约 2 秒后自动开启一次
    if (!S.ok && autoTick == 120) start();
    // 水面探测独立于开关:每 ~2 秒自动找一次,找到了就设上
    // (反射 pass 只在 waterY > -100 时才会跑)
    if (S.wy < -100 && autoTick > 0 && (autoTick % 40) == 0) {
        var wy = scanWater(16);
        if (wy > -900) {
            S.wy = wy;
            Render.setWaterLevel(wy);
            msg("找到水面 y=" + wy + ",反射已生效");
        }
    }
}
var userOff = false;
var autoOpened = false;
var autoTick = 0;

function onChat(m) {
    if (m == "/water")   { start(); return; }
    if (m == "/wateroff"){ stop(); return; }
    if (m == "/scan") {
        var wy = scanWater(32);
        if (wy > -900) { S.wy = wy; Render.setWaterLevel(wy); msg("水面 y=" + wy); }
        else msg("没扫到水(8/9)" );
        return;
    }
    if (m.indexOf("/wy ") == 0) {
        var v = parseFloat(m.slice(4));
        S.wy = v;
        Render.setWaterLevel(v);
        msg("水面 y=" + v);
        if (!S.ok) start();
        return;
    }
    if (m == "/flip") { S.flip = S.flip ? 0 : 1; msg("refl 翻转=" + (S.flip ? "开" : "关") + "(倒影方向不对就切)"); return; }
    if (m.indexOf("/wave") == 0) { var v = parseFloat(m.slice(6)); if (!isNaN(v)) { cfg.wave = v; msg("波纹=" + v); } return; }
    if (m.indexOf("/tint") == 0) { var v = parseFloat(m.slice(6)); if (!isNaN(v)) { cfg.tint = v; msg("水色=" + v); } return; }
    if (m == "/diag") { S.diag = (S.diag + 1) % 3; msg("视图=" + (S.diag == 0 ? "合成" : (S.diag == 1 ? "mask(水)" : "refl(反射)"))); return; }
    if (m == "/cam") {
        var c = Render.getCamera();
        msg("cam=(" + c[0].toFixed(1) + "," + c[1].toFixed(1) + "," + c[2].toFixed(1) + ") yaw=" + c[3].toFixed(1) + " pitch=" + c[4].toFixed(1));
    }
}
