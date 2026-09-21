from PIL import Image

img = Image.open(r'H:/workerspace/workapace/main/MinecraftPE-Win/data/images/gui/widgets_1710.png').convert('RGBA')
px = img.load()

def grid(x0, y0, x1, y1, cols=8, rows=8):
    for ry in range(rows):
        line = []
        for rx in range(cols):
            x = x0 + (x1 - x0) * rx // (cols - 1)
            y = y0 + (y1 - y0) * ry // (rows - 1)
            r, g, b, a = px[x, y]
            line.append('#%02x%02x%02x' % (r, g, b) if a > 128 else 'trans')
        print('  y=%d' % (y0 + (y1 - y0) * ry // (rows - 1)), ' '.join(line))

print('button normal (0,66,200,20):')
grid(0, 66, 200, 86)
print()
print('button hover (0,86,200,20):')
grid(0, 86, 200, 106)
print()
print('button disabled (0,46,200,20):')
grid(0, 46, 200, 66)
