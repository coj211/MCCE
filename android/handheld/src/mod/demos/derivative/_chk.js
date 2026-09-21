// Derivative mod 静态校验: eval main.js 取三块 GLSL, 查括号配平/行注释泄漏。
var fs = require('fs');
var src = fs.readFileSync('main.js', 'utf8');
var sandbox = { modLog: function(){}, console: console };
var vm = require('vm');
vm.createContext(sandbox);
try {
    vm.runInContext(src, sandbox, { filename: 'main.js' });
} catch (e) {
    console.error('EVAL FAIL: ' + e.message);
    process.exit(1);
}
function check(name, code) {
    if (typeof code !== 'string') { console.error(name + ': NOT A STRING'); return false; }
    var ok = true;
    // 括号配平 (忽略字符串内? GLSL 字符串里无双引号, 括号出现在字符串字面量中的可能性: 无)
    for (var ch of ['{', '(', '[']) {
        var open = code.split(ch).length - 1;
        var close = code.split({ '{': '}', '(': ')', '[': ']' }[ch]).length - 1;
        if (open !== close) { console.error(name + ': brace ' + ch + ' open=' + open + ' close-expected=' + close); ok = false; }
    }
    // 双引号泄漏(整段应无双引号)
    if (code.indexOf('"') >= 0) { console.error(name + ': stray double-quote inside GLSL'); ok = false; }
    // 每行内 /* */ 未配对
    var lines = code.split('\n');
    var inBlock = false, li = 0;
    for (var ln of lines) {
        li++;
        var c = ln.replace(/"(?:[^"\\]|\\.)*"/g, '');
        while (c.indexOf('/*') >= 0) {
            inBlock = true; c = c.replace('/*', '');
        }
        while (inBlock && c.indexOf('*/') >= 0) { inBlock = false; c = c.replace('*/', ''); }
        // 行注释后出现行未终结的 */ 之类难查, 跳过
        if (inBlock) { console.error(name + ': unclosed /* block comment from line ' + li); ok = false; break; }
        // ANGLE 严格项: 比较结果不能隐式赋 float(需 bool)
        var stripped = ln.replace(/\/\/.*$/, '');
        if (/float\s+\w+\s*=\s*[^;]*[<>=!]=/.test(stripped)) {
            console.error(name + ': line ' + li + ' assigns comparison to float: ' + ln.trim()); ok = false;
        }
    }
    console.log(name + ': ' + code.split('\n').length + ' lines, braces OK=' + ok);
    // 使用的 uniform 名必须在声明里(u_ 前缀无例外)。简化: 收集声明名(每行第一个 u_ 词) vs 全部出现。
    var declared = {};
    var allUse = {};
    lines.forEach(function (ln) {
        var seg = ln.replace(/\/\/.*$/, '');
        var ms = seg.match(/uniform\s+(?:float|int|vec2|vec3|vec4|mat4|sampler2D|sampler2DShadow)\s+([u]\w+(?:\s*,\s*[u]\w+)*)/g);
        if (ms) ms.forEach(function (one) {
            var head = one.replace(/^uniform\s+(?:float|int|vec2|vec3|vec4|mat4|sampler2D|sampler2DShadow)\s+/, '');
            head.split(',').forEach(function (w) { declared[w.trim()] = true; });
        });
    });
    lines.forEach(function (ln) {
        var mm = ln.match(/u_\w+/g);
        if (mm) mm.forEach(function (w) { allUse[w] = true; });
    });
    Object.keys(allUse).forEach(function (w) {
        if (!declared[w]) { console.error(name + ': undefined uniform ' + w); ok = false; }
    });
    return ok;
}
var okAll = true;
okAll = check('MAIN_FS', sandbox.MAIN_FS) && okAll;
okAll = check('BLOOM_FS', sandbox.BLOOM_FS) && okAll;
okAll = check('FINAL_FS', sandbox.FINAL_FS) && okAll;
console.log(okAll ? 'ALL-GLSL-STATIC-OK' : 'GLSL-STATIC-FAIL');
process.exit(okAll ? 0 : 1);
