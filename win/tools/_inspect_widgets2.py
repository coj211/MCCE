from PIL import Image

img = Image.open(r'H:/workerspace/workapace/main/MinecraftPE-Win/data/images/gui/widgets_1710.png').convert('RGBA')
px = img.load()

def sample(x, y):
    r, g, b, a = px[x, y]
    return '#%02x%02x%02x a=%d' % (r, g, b, a)

# Row profile: distinct colors along a horizontal line
def row_profile(y, x0=0, x1=256, step=4):
    seen = []
    for x in range(x0, x1, step):
        c = px[x, y]
        if c not in [s[1] for s in seen]:
            seen.append((x, c))
    return [(x, '#%02x%02x%02x a=%d' % (c[0], c[1], c[2], c[3])) for x, c in seen[:12]]

for y in [0, 18, 38, 46, 66, 86, 106, 126, 146, 166, 186, 206, 226, 246]:
    print('y=%3d' % y, row_profile(y)[:6])

print()
print('corner (0,0):', sample(0, 0))
print('button normal mid (100,66):', sample(100, 66))
print('button hover mid (100,86):', sample(100, 86))
print('button disabled mid (100,46):', sample(100, 46))
print('textfield normal mid (100,18):', sample(100, 18))
print('textfield focus mid (100,38):', sample(100, 38))
