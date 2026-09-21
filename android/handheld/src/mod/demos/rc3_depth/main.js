// name: 渲染API片3·深度纹理与相机矩阵
// author: dev
// version: 0.3.0
// description: 演示 scene 深度纹理(depthTexture)与相机矩阵。模式(左下角标
//   红0 绿1 蓝2 黄3): 0=线性深度灰度 1=深度描边 2=深度雾 3=原样彩色。
//   默认每 12 秒自动循环模式演示(/auto 停, /d3off 关)。命令: /m0-/m3 /mode
//   /plog。

var S = { prog: 0, vbo: 0, ok: false, tried: false, mode: 0, autoCycle: true, warned: false };
var near = 0.05, far = 256.0;

function log(s) { modLog("[rc3] " + s); }

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
        "uniform sampler2D u_depth;",
        "uniform float u_near;",
        "uniform float u_far;",
        "uniform float u_mode;",
        "in vec2 vUv;",
        "out vec4 fragColor;",
        "float linDepth(vec2 uv){",
        "  float d = texture(u_depth, uv).r;",
        "  float zndc = d * 2.0 - 1.0;",
        "  return (2.0 * u_near * u_far) / (u_far + u_near - zndc * (u_far - u_near));",
        "}",
        "void main(){",
        "  float m = u_mode;",
        "  float lin = linDepth(vUv);",
        "  vec3 col;",
        "  if (m < 0.5) {",                                     // 0 线性深度灰度
        "    float g = clamp(1.0 - lin / u_far, 0.0, 1.0);",
        "    col = vec3(g);",
        "  } else if (m < 1.5) {",                              // 1 深度边缘描边
        "    vec2 px = vec2(1.0/854.0, 1.0/480.0);",
        "    float c0 = linDepth(vUv);",
        "    float e = abs(linDepth(vUv+vec2(-px.x,0.0))-c0)" +
        "            + abs(linDepth(vUv+vec2( px.x,0.0))-c0)" +
        "            + abs(linDepth(vUv+vec2(0.0, px.y))-c0)" +
        "            + abs(linDepth(vUv+vec2(0.0,-px.y))-c0);",
        "    col = texture(u_scene, vUv).rgb;",
        "    col = mix(col, vec3(0.0), smoothstep(0.02, 0.20, e));",
        "  } else if (m < 2.5) {",                              // 2 深度雾
        "    col = texture(u_scene, vUv).rgb;",
        "    float f = clamp(lin / (u_far * 0.8), 0.0, 1.0);",
        "    col = mix(col, vec3(0.72, 0.78, 0.82), f * f);",
        "  } else {",                                          // 3 原样
        "    col = texture(u_scene, vUv).rgb;",
        "  }",
        //  左下角标: 0=红 1=绿 2=蓝 3=黄,证明 u_mode 生效
        "  vec2 pm = vUv * vec2(854.0, 480.0);",
        "  float mk = step(6.0, pm.x) * step(6.0, pm.y) * step(pm.x, 22.0) * step(pm.y, 22.0);",
        "  vec3 mc = vec3(1.0, 0.0, 0.0);",
        "  if (m > 0.5 && m < 1.5) mc = vec3(0.0, 1.0, 0.0);",
        "  else if (m > 1.5 && m < 2.5) mc = vec3(0.0, 0.3, 1.0);",
        "  else if (m > 2.5) mc = vec3(1.0, 0.9, 0.0);",
        "  col = mix(col, mc, mk * 0.8);",
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
        depth: GL.glGetUniformLocation(p, "u_depth"),
        near:  GL.glGetUniformLocation(p, "u_near"),
        far:   GL.glGetUniformLocation(p, "u_far"),
        mode:  GL.glGetUniformLocation(p, "u_mode")
    };
    log("prog=" + p + " loc mode=" + S.loc.mode + " near=" + S.loc.near + " far=" + S.loc.far);
    return p;
}

function refreshCam() {
    // 列主序 gluPerspective: proj[10]=-(f+n)/(f-n), proj[14]=-2fn/(f-n)
    var p = Render.getProjectionMatrix();
    if (!p || p.length < 15) return;
    var n = Math.abs(p[14] / (p[10] - 1));
    var f = Math.abs(p[14] / (p[10] + 1));
    if (n > 0.001 && f > n) { near = n; far = f; }
}

function onRenderComposite(sceneTex, w, h, maskTex, reflTex) {
    if (!S.ok) return;
    if (!S.prog && !S.tried) {
        S.tried = true;
        var p = build();
        if (!p) { player.sendMessage("[rc3] shader 编译失败,已回退(看日志)"); Render.enable(false); return; }
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
    if (S.loc.scene >= 0) GL.glUniform1i(S.loc.scene, 0);
    GL.glActiveTexture(GL.TEXTURE1);
    GL.glBindTexture(GL.TEXTURE_2D, depthTex);
    if (S.loc.depth >= 0) GL.glUniform1i(S.loc.depth, 1);

    if (S.loc.near >= 0) GL.glUniform1f(S.loc.near, near);
    if (S.loc.far  >= 0) GL.glUniform1f(S.loc.far, far);
    if (S.loc.mode >= 0) GL.glUniform1f(S.loc.mode, S.mode);
    else if (!S.warned) { S.warned = true; log("u_mode loc = -1(被优化?),模式切换无效"); }

    GL.glBindBuffer(GL.ARRAY_BUFFER, S.vbo);
    GL.glEnableVertexAttribArray(0);
    GL.glVertexAttribPointer(0, 2, GL.FLOAT, false, 0, 0);
    GL.glDrawArrays(GL.TRIANGLES, 0, 6);
    GL.glDisableVertexAttribArray(0);
    GL.glBindBuffer(GL.ARRAY_BUFFER, 0);

    GL.glActiveTexture(GL.TEXTURE1); GL.glBindTexture(GL.TEXTURE_2D, 0);
    GL.glActiveTexture(GL.TEXTURE0); GL.glBindTexture(GL.TEXTURE_2D, 0);
    GL.glUseProgram(0);
}

function msg(s) { player.sendMessage("[rc3] " + s); }
var MODE_NAMES = ["0 线性深度(近白远黑,天=黑)", "1 深度描边", "2 深度雾", "3 原样彩色"];

function setMode(m) {
    S.mode = ((m % 4) + 4) % 4;
    msg("mode=" + MODE_NAMES[S.mode] + " (角标:" + ["红","绿","蓝","黄"][S.mode] + ")");
}

function start() {
    if (!(typeof GL !== "undefined" && typeof Render !== "undefined")) { msg("此构建无 GL/Render API"); return; }
    userOff = false;
    Render.enable(true);
    S.ok = true;
    S.prog = 0; S.tried = false; S.warned = false;
    msg("已开,当前 " + MODE_NAMES[S.mode] + " — /mode 切换");
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
    if (autoTick < 120) return;
    if (!S.ok) { start(); return; }
    // 已开启:每 60 tick(约 3 秒)自动切下一个模式 — 不输命令也能看到全部 4 种
    if (S.autoCycle && autoTick >= 180) {
        autoTick = 120;
        S.mode = (S.mode + 1) % 4;
    }
}
var userOff = false;
var autoTick = 0;

function onChat(raw) {
    log("chat:[" + raw + "]");              // 记录命令原文,便于排查
    var m = ("" + raw).replace(/^\s+|\s+$/g, "");
    // 兼容带/不带斜杠、大小写
    var c = m;
    if (c.length > 1 && c.charAt(0) == "/") c = c.substr(1);
    c = c.toLowerCase();
    if (c == "d3")    { start(); return; }
    if (c == "d3off") { stop(); return; }
    if (c == "auto")  { S.autoCycle = !S.autoCycle; msg("自动循环=" + (S.autoCycle ? "开" : "关")); return; }
    if (c == "mode" || c == "m") { setMode(S.mode + 1); return; }
    if (c == "m0") { setMode(0); return; }
    if (c == "m1") { setMode(1); return; }
    if (c == "m2") { setMode(2); return; }
    if (c == "m3") { setMode(3); return; }
    if (c == "plog") {
        var p = Render.getProjectionMatrix();
        var mv = Render.getModelViewMatrix();
        refreshCam();
        msg("near=" + near.toFixed(3) + " far=" + far.toFixed(1) +
            " (proj10=" + (p && p.length >= 15 ? p[10].toFixed(4) : "?") +
            " proj14=" + (p && p.length >= 15 ? p[14].toFixed(4) : "?") + ")");
        if (mv && mv.length >= 15) msg("mv pos=" + mv[12].toFixed(1) + "," + mv[13].toFixed(1) + "," + mv[14].toFixed(1));
        return;
    }
}
