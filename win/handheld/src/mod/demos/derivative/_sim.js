// Derivative 卡帧定位: 用 GL/Render/player/level 全桩跑 onRenderComposite,
// 捕获任何"实参为 undefined"的原生调用点(即真机上 TypeError number required 的来源)。
var fs = require('fs'), vm = require('vm');

// ---- GL 桩: 每个 gl* 返回类型合理值, 参数含 undefined 时打印调用点 ----
var g = {};
var idColor = 0x100, idFbo = 0x200, idBuf = 0x300, idShader = 0, idProg = 0;
var GL_C = {
    VERTEX_SHADER: 0x8B31, FRAGMENT_SHADER: 0x8B30, TEXTURE_2D: 0x0DE1,
    TEXTURE0: 0x84C0, TEXTURE1: 0x84C1, TEXTURE2: 0x84C2, TEXTURE3: 0x84C3, TEXTURE4: 0x84C4,
    NEAREST: 0x2600, LINEAR: 0x2601, CLAMP_TO_EDGE: 0x812F, RGBA8: 0x8058, RGBA: 0x1908,
    RGB: 0x1907, UNSIGNED_BYTE: 0x1401, UNSIGNED_INT: 0x1405, FLOAT: 0x1406,
    DEPTH_COMPONENT: 0x1902, DEPTH_COMPONENT24: 0x81A6,
    FRAMEBUFFER: 0x8D40, RENDERBUFFER: 0x8D41, COLOR_ATTACHMENT0: 0x8CE0, DEPTH_ATTACHMENT: 0x8D00,
    FRAMEBUFFER_COMPLETE: 0x8CD5, FRAMEBUFFER_BINDING: 0x8CA6,
    ARRAY_BUFFER: 0x8892, STATIC_DRAW: 0x88E4, TRIANGLES: 0x0004,
    DEPTH_TEST: 0x0B71, BLEND: 0x0BE2, CULL_FACE: 0x0B44,
    SRC_ALPHA: 0x0302, ONE_MINUS_SRC_ALPHA: 0x0303, ONE: 1, ZERO: 0,
    TEXTURE_MIN_FILTER: 0x2801, TEXTURE_MAG_FILTER: 0x2800, TEXTURE_WRAP_S: 0x2802, TEXTURE_WRAP_T: 0x2803,
    COLOR_BUFFER_BIT: 0x4000, DEPTH_BUFFER_BIT: 0x100, TRUE: 1, FALSE: 0,
    LESS: 0x0201, LEQUAL: 0x0203, GEQUAL: 0x0206, GREATER: 0x0204, ALWAYS: 0x0207
};
var retFor = {
    glGetUniformLocation: function () { return 0; },        // 让所有 uniform 走"已找到"分支
    glGetShaderLog: function () { return 'OK'; },
    glGetProgramLog: function () { return 'OK'; },
    glCheckFramebufferStatus: function () { return GL_C.FRAMEBUFFER_COMPLETE; },
    glGetError: function () { return 0; },
    glCreateShader: function () { return ++idShader; },
    glCreateProgram: function () { return ++idProg; },
    glGenTextures: function () { return ++idColor; },
    glGenFramebuffers: function () { return ++idFbo; },
    glGenBuffers: function () { return ++idBuf; },
    glGenRenderbuffers: function () { return ++idBuf; }
};
var GL = new Proxy({}, {
    get: function (t, name) {
        if (name in GL_C) return GL_C[name];
        if (name in retFor) return retFor[name];
        return function () {
            for (var i = 0; i < arguments.length; i++) {
                if (arguments[i] === undefined) {
                    console.error('>>> GL.' + name + ' arg#' + i + ' UNDEFINED @\n' + new Error().stack.split('\n').slice(1, 4).join('\n'));
                    process.exit(1);
                }
            }
        };
    }
});
// 一些 getter 型常量函数
GL.glGetIntegerv = function () {};

// ---- Render / player / level / 引擎桩 ----
var I16 = [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1];
var Render = {
    _on: false, _wy: -999,
    enable: function (v) { this._on = !!v; },
    enabled: function () { return this._on; },
    sceneTexture: function () { return 11; },
    depthTexture: function () { return 12; },
    sceneReady: function () { return true; },
    waterMaskTexture: function () { return 13; },
    reflectionTexture: function () { return 14; },
    setWaterLevel: function (y) { this._wy = y; },
    waterLevel: function () { return this._wy; },
    getCamera: function () { return [30, 75, 155, 0, -8]; },
    getProjectionMatrix: function () {
        // gluPerspective(70°, 4:3, 0.05, 256) 列主序
        var f = 1.0 / Math.tan(70 * Math.PI / 360);
        return [f / 1.3333, 0, 0, 0,  0, f, 0, 0,  0, 0, (256+0.05)/(0.05-256), -1,  0, 0, (2*256*0.05)/(0.05-256), 0];
    },
    getModelViewMatrix: function () { return I16.slice(); },
    getWorldMatrices: function () { return { proj: I16.slice(), view: I16.slice(), x: 30, y: 75, z: 155, chunks: 20 }; },
    world: function () { return true; }
};
var player = {
    getHeldItemId: function () { return 0; },
    getYRot: function () { return 0; }, getXRot: function () { return -8; },
    getX: function () { return 30.4; }, getY: function () { return 75.0; }, getZ: function () { return 155.2; },
    sendMessage: function (m) { console.log('[msg] ' + m); }
};
var level = { getTime: function () { return 6000; }, getBlock: function () { return 1; } };
var modLog = function (m) { console.log('[mod] ' + m); };

var sandbox = { GL: GL, Render: Render, player: player, level: level, modLog: modLog, console: console, Date: Date, Math: Math, parseFloat: parseFloat, parseInt: parseInt, isNaN: isNaN };
vm.createContext(sandbox);
try {
    vm.runInContext(fs.readFileSync('main.js', 'utf8'), sandbox, { filename: 'main.js' });
} catch (e) { console.error('EVAL FAIL: ' + e.stack); process.exit(1); }
sandbox.modLog = modLog; // main.js 里函数引用 modLog 从上下文解析, eval 时已绑定? 实际函数体里 modLog 是自由变量→作用域链到 context global. OK.

// 模拟 start + 若干帧 onRenderComposite(时间演进/光源/resize/diag/开关全路径)
var tickBase = 4000; // 下午开始, 走向日落/夜
sandbox.start();
var diags = [0, 1, 2, 3, 4, 0, 0, 0];
for (var fr = 0; fr < 40; fr++) {
    try {
        level.getTime = function () { return (tickBase + fr * 900) % 19200; };
        // 帧内变化: 水源/手持/火把/diag/resize/bloom 开关/水下
        if (fr === 3) sandbox.S.torchList = [[30.5, 74.5, 155.5], [31.5, 75.5, 156.5]]; sandbox.S.torchN = 2;
        if (fr === 6) { sandbox.S.torchList = [[30.5, 74.5, 155.5]]; sandbox.S.torchN = 1; sandbox.cfg.wtest = true; }
        if (fr === 10) sandbox.cfg.wtest = false;
        if (fr === 14) sandbox.S.diag = diags[fr % diags.length];
        var w = (fr === 22 || fr === 30) ? 1280 : 800; // resize 触发离屏重建
        var h = (fr === 22 || fr === 30) ? 720 : 600;
        if (fr === 26) sandbox.cfg.vanilla = true;  // 原版水视图路径
        if (fr === 33) { sandbox.cfg.vanilla = false; sandbox.cfg.sunAuto = 0; sandbox.cfg.sunEl = -25; sandbox.applySunManual(); } // 夜手动
        sandbox.onRenderComposite(11, w, h, 13, 14);
        if (fr === 37) sandbox.S.diag = 0;
    } catch (e) {
        console.error('frame ' + fr + ' onRenderComposite ERROR:\n' + e.stack);
        process.exit(1);
    }
}
console.log('SIM-OK: 40 frames (day→dusk→night, resize, torch, diag, vanilla-water) without undefined args');
