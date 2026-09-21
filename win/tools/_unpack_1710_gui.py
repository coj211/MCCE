import zipfile, os

jar = r'D:/游戏/我的世界/.minecraft/versions/1.7.10-Forge_10.13.4.1614-OptiFine_E7/1.7.10-Forge_10.13.4.1614-OptiFine_E7.jar'
out = r'H:/workerspace/workapace/main/MinecraftPE-Win/data/images/gui'

z = zipfile.ZipFile(jar)

files = [
    'assets/minecraft/textures/gui/widgets.png',
    'assets/minecraft/textures/gui/title/minecraft.png',
    'assets/minecraft/textures/gui/options_background.png',
]

targets = {
    'assets/minecraft/textures/gui/widgets.png': 'widgets_1710.png',
    'assets/minecraft/textures/gui/title/minecraft.png': 'minecraft_1710.png',
    'assets/minecraft/textures/gui/options_background.png': 'options_bg_1710.png',
}

for src, dst in targets.items():
    data = z.read(src)
    with open(os.path.join(out, dst), 'wb') as f:
        f.write(data)
    print('extracted', src, '->', os.path.join(out, dst), len(data), 'bytes')

# Inspect PNG dimensions
import struct
def png_size(path):
    with open(path, 'rb') as f:
        f.read(16)
        w, h = struct.unpack('>II', f.read(8))
        return w, h

for dst in targets.values():
    p = os.path.join(out, dst)
    if os.path.exists(p):
        print(dst, png_size(p))
