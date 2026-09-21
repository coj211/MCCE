// name: 渲染API片1·着色器通路演示
// author: dev
// version: 0.2.0
// description: 渲染模组 API 通路验证：Render.enable(true) 后引擎把世界渲进
//   离屏纹理并回调 onRenderComposite(sceneTex,w,h)；本模组用裸 GL 自写 GLSL
//   把场景合成回屏幕。命令：/fx 开  /fxoff 关（关后不再自动开）
//   /mode 依次切 0原样 1反色 2像素化 3通道乱序  /fx2 /fx3 /fx4 直跳
//   /flip 翻转uv  /cam 相机。进世界约 2 秒后自动开一次。

var S = { prog: 0, vbo: 0, ok: false, mode: 1, flip: 0, tried: false, locWarned: false };
var autoOpened = false;

function log(s) { modLog("[rc1] " + s); }

function compileVS(src) {
    var s = GL.glCreateShader(GL.VERTEX_SHADER);
    GL.glShaderSource(s, src);
    GL.glCompileShader(s);
    var v = GL.glGetShaderLog(s);
    if (v != "OK") { log("vs: " + v); return 0; }
    return s;
}
function compileFS(src) {
    var s = GL.glCreateShader(GL.FRAGMENT_SHADER);
    GL.glShaderSource(s, src);
    GL.glCompileShader(s);
    var v = GL.glGetShaderLog(s);
    if (v != "OK") { log("fs: " + v); return 0; }
    return s;
}

function build() {
    var vs = compileVS([
        "#version 130",
        "in vec2 aPos;",
        "out vec2 vUv;",
        "void main(){ vUv = aPos * 0.5 + 0.5; gl_Position = vec4(aPos, 0.0, 1.0); }"
    ].join("\n"));
    if (!vs) return 0;
    var fs = compileFS([
        "#version 130",
        "uniform sampler2D u_scene;",
        "uniform float u_time;",
        "uniform float u_flip;",
        "uniform float u_mode;",
        "in vec2 vUv;",
        "out vec4 fragColor;",
        "void main(){",
        "  vec2 uv = vUv;",
        "  if (u_flip > 0.5) uv.y = 1.0 - uv.y;",
        "  vec4 c = texture(u_scene, uv);",
        "  float m = u_mode;",
        "  if (m < 0.5) {",                       // 0 原样 + 四角标记(证明此模式在跑)
        "    vec3 out0 = c.rgb;",
        "    vec2 p = vUv * vec2(854.0, 480.0);",
        "    float corner = step(30.0, p.x) * step(30.0, p.y) * step(p.x, 60.0) * step(p.y, 60.0);",
        "    out0 = mix(out0, vec3(1.0, 0.0, 0.0), corner * 0.7);",
        "    fragColor = vec4(out0, 1.0); return;",
        "  }",
        "  if (m < 1.5) {",                       // 1 反色+扫描线
        "    vec3 inv = 1.0 - c.rgb;",
        "    float scan = 0.92 + 0.08 * sin(vUv.y * 900.0 + u_time * 4.0);",
        "    fragColor = vec4(inv * scan, 1.0); return;",
        "  }",
        "  if (m < 2.5) {",                       // 2 强像素化(大块+描边)
        "    vec2 px = vec2(1.0 / 128.0, 1.0 / 128.0);",
        "    vec2 cuv = (floor(vUv / px) + 0.5) * px;",
        "    vec4 p = texture(u_scene, cuv);",
        "    vec2 g = abs(fract(vUv / px) - 0.5);",
        "    float edge = step(0.46, max(g.x, g.y));",
        "    vec3 out2 = mix(p.rgb * 1.15, vec3(0.05), edge * 0.9);",
        "    fragColor = vec4(out2, 1.0); return;",
        "  }",
        "  fragColor = vec4(c.g, c.r, c.b, 1.0);", // 3 红蓝互换
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
        time:  GL.glGetUniformLocation(p, "u_time"),
        flip:  GL.glGetUniformLocation(p, "u_flip"),
        mode:  GL.glGetUniformLocation(p, "u_mode")
    };
    return p;
}

function onRenderComposite(sceneTex, w, h) {
    if (!S.ok) return;
    if (!sceneTex) return;

    if (!S.prog && !S.tried) {
        S.tried = true;
        var p = build();
        if (!p) { player.sendMessage("[rc1] shader 编译失败,已回退原画面(看日志)"); Render.enable(false); return; }
        log("prog=" + p + " vbo=" + S.vbo + " locs scene=" + S.loc.scene + " time=" + S.loc.time +
            " flip=" + S.loc.flip + " mode=" + S.loc.mode);
    }
    if (!S.prog) return;

    if (!S.locWarned && S.loc.mode < 0) {
        S.locWarned = true;
        log("u_mode location = -1 (uniform 被优化?),切换模式可能无效");
    }

    GL.glViewport(0, 0, w, h);
    GL.glDisable(GL.DEPTH_TEST);
    GL.glDisable(GL.BLEND);
    GL.glUseProgram(S.prog);

    GL.glActiveTexture(GL.TEXTURE0);
    GL.glBindTexture(GL.TEXTURE_2D, sceneTex);
    GL.glUniform1i(S.loc.scene, 0);
    GL.glUniform1f(S.loc.time, (new Date()).getTime() / 1000.0);
    GL.glUniform1f(S.loc.flip, S.flip);
    GL.glUniform1f(S.loc.mode, S.mode);

    GL.glBindBuffer(GL.ARRAY_BUFFER, S.vbo);
    GL.glEnableVertexAttribArray(0);
    GL.glVertexAttribPointer(0, 2, GL.FLOAT, false, 0, 0);
    GL.glDrawArrays(GL.TRIANGLES, 0, 6);
    GL.glDisableVertexAttribArray(0);
    GL.glBindBuffer(GL.ARRAY_BUFFER, 0);
    GL.glBindTexture(GL.TEXTURE_2D, 0);
    GL.glUseProgram(0);
}

function msg(s) { player.sendMessage(s); }
var MODE_NAMES = ["0 原样(看红角标+天空方向)", "1 反色+扫描线", "2 像素化", "3 红蓝互换"];

function start() {
    if (!(typeof GL !== "undefined" && typeof Render !== "undefined")) {
        msg("[rc1] 此构建没有 GL/Render API,无法演示");
        return;
    }
    Render.enable(true);
    S.ok = true;
    S.prog = 0; S.tried = false; S.locWarned = false;
    msg("[rc1] 已开 mode=" + MODE_NAMES[S.mode]);
}

function stop() {
    Render.enable(false);
    S.ok = false;
    autoOpened = true;      // 用户手动关 = 不再自动开
    msg("[rc1] 已关,恢复原画面");
}

function setMode(m) {
    S.mode = ((m % 4) + 4) % 4;
    if (!S.ok) { start(); return; }
    msg("[rc1] mode=" + MODE_NAMES[S.mode]);
}

function onTick() {
    if (autoOpened || S.ok) return;
    if (typeof GL === "undefined" || typeof Render === "undefined") return;
    if (++autoTick < 120) return;
    autoOpened = true;
    start();
}
var autoTick = 0;

function onChat(msg) {
    if (msg == "/fx")   { start(); return; }
    if (msg == "/fxoff"){ stop(); return; }
    if (msg == "/mode") { setMode(S.mode + 1); return; }
    if (msg == "/fx0")  { setMode(0); return; }
    if (msg == "/fx1")  { setMode(1); return; }
    if (msg == "/fx2")  { setMode(2); return; }
    if (msg == "/fx3")  { setMode(3); return; }
    if (msg == "/flip") { S.flip = S.flip ? 0 : 1; msg("[rc1] flip=" + (S.flip ? "翻" : "不翻") + "(0=原方向 1=翻转;mode0 看天空在哪)"); return; }
    if (msg == "/cam") {
        if (typeof Render.getCamera !== "function") return;
        var c = Render.getCamera();
        msg("[rc1] cam=(" + c[0].toFixed(1) + "," + c[1].toFixed(1) + "," + c[2].toFixed(1) +
            ") yaw=" + c[3].toFixed(1) + " pitch=" + c[4].toFixed(1));
    }
}
