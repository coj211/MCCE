# -*- coding: utf-8 -*-
# 给 mods/giant_zip.zip 里的 main.js 的 onJoinWorld() 加回一句进世界提示。
# 只改 main.js 这一个条目，其它文件（png 贴图）原样复制；保持 UTF-8 + CRLF 原编码。
import zipfile, shutil, os

SRC = r'H:\workerspace\workapace\main\MinecraftPE-Win\mods\giant_zip.zip'
BAK = SRC + '.bak'
MSG = u"输入 /giant 召唤石头人，/dragon 召唤暗黑龙，/boom 放烟花"

if not os.path.exists(BAK):
    shutil.copy2(SRC, BAK)
    print('backup ->', BAK)

zin = zipfile.ZipFile(SRC)
items = zin.infolist()
data = {i.filename: zin.read(i.filename) for i in items}
zin.close()

js = data['main.js'].decode('utf-8')
old = "function onJoinWorld() {\r\n    var d = level.getCurrentDimension();\r\n"
assert js.count(old) == 1, 'anchor count = %d' % js.count(old)
new = ("function onJoinWorld() {\r\n"
       "    player.sendMessage(\"" + MSG + "\");\r\n"
       "    var d = level.getCurrentDimension();\r\n")
js = js.replace(old, new)
data['main.js'] = js.encode('utf-8')

tmp = SRC + '.tmp'
with zipfile.ZipFile(tmp, 'w', zipfile.ZIP_DEFLATED) as zout:
    for i in items:
        zi = zipfile.ZipInfo(i.filename, date_time=i.date_time)
        zi.compress_type = i.compress_type
        zi.external_attr = i.external_attr
        zout.writestr(zi, data[i.filename])
os.replace(tmp, SRC)
print('patched ->', SRC)
