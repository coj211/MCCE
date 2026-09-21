import zipfile, os

jar = r'D:/游戏/我的世界/.minecraft/versions/1.7.10-Forge_10.13.4.1614-OptiFine_E7/1.7.10-Forge_10.13.4.1614-OptiFine_E7.jar'
out = r'H:/workerspace/workapace/main/MinecraftPE-Win/tools/_unpack1710'

z = zipfile.ZipFile(jar)
names = z.namelist()
print('total entries:', len(names))

# GUI textures
for n in names:
    if n.startswith('assets/minecraft/textures/gui') and n.endswith('.png'):
        print(n)
