import os, zipfile

# __file__ = handheld/src/mod/demos/derivative/_pack.py → 上溯 5 层 = 仓库根
base = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..', '..'))
print('repo base =', base)
src = os.path.join(base, 'handheld', 'src', 'mod', 'demos', 'derivative', 'main.js')
dst = os.path.join(base, 'mods', 'Derivative.zip')
with zipfile.ZipFile(dst, 'w', zipfile.ZIP_DEFLATED) as z:
    z.write(src, 'main.js')
print('packed', dst, os.path.getsize(dst), 'bytes')
