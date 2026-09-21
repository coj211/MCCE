from PIL import Image

img = Image.open(r'H:/workerspace/workapace/main/MinecraftPE-Win/data/images/gui/widgets_1710.png').convert('RGBA')
w, h = img.size
print('size', w, h)

# Find rows (y bands) that contain non-transparent pixels in the left 200px
px = img.load()
bands = []
y = 0
while y < h:
    # check if row y has any non-transparent pixel in x 0..200
    def row_has_alpha(row, x0=0, x1=200):
        for x in range(x0, min(x1, w)):
            if px[x, row][3] > 0:
                return True
        return False
    if row_has_alpha(y):
        y0 = y
        while y < h and row_has_alpha(y):
            y += 1
        bands.append((y0, y))
    else:
        y += 1

print('opaque horizontal bands in x0..200:')
for b in bands:
    print('  y', b[0], '-', b[1], 'height', b[1]-b[0])

# Also check vertical extent of the right side (x 200..256) briefly
def col_has_alpha(x):
    for y in range(h):
        if px[x, y][3] > 0:
            return True
    return False
cols = [x for x in range(0, 256) if col_has_alpha(x)]
print('non-empty x range:', min(cols), '-', max(cols))
