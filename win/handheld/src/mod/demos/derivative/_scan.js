var fs = require('fs'), vm = require('vm');
var s = { modLog: function () {}, console: console };
vm.createContext(s);
vm.runInContext(fs.readFileSync('main.js', 'utf8'), s);
['MAIN_FS', 'BLOOM_FS', 'FINAL_FS'].forEach(function (n) {
    var lines = s[n].split('\n');
    lines.forEach(function (L, i) {
        if (/float\s+\w+\s*=\s*[^;]*[<>=!]=/.test(L) || /bool\s+\w+\s*=/.test(L))
            console.log(n + ' L' + (i + 1) + ': ' + L.trim());
    });
});
console.log('scan done');
