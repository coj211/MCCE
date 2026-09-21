from PIL import Image

for name in ['gui.png', 'gui2.png']:
    p = r'H:/workerspace/workapace/main/MinecraftPE-Win/data/images/gui/' + name
    img = Image.open(p).convert('RGBA')
    print(name, img.size)
    px = img.load()
    for y in [18, 38, 46, 66, 86, 106]:
        c = px[100, y] if img.size[0] > 100 and img.size[1] > y else None
        if c:
            print('  y=%d (100,y) = #%02x%02x%02x a=%d' % (y, c[0], c[1], c[2], c[3]))
    # check a button-looking region at (0,66,200,20)
    mid = px[100, 76] if img.size[0] > 100 and img.size[1] > 76 else None
    if mid:
        print('  button normal mid (100,76) = #%02x%02x%02x a=%d' % (mid[0], mid[1], mid[2], mid[3]))
