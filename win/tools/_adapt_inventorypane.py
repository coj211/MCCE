# -*- coding: utf-8 -*-
"""Touch_InventoryPane.cpp: 0.8.1 ScrollingPane 字段 -> client 字段映射（正则精确，防 field_248 误伤）"""
import io, re

path = r'H:\workerspace\workapace\main\MinecraftPE-Win\handheld\src\gui08\impl_gui\pane\Touch_InventoryPane.cpp'
with io.open(path, 'r', encoding='utf-8', newline='') as f:
    src = f.read()

def sub(p, r):
    global src
    src, n = re.subn(p, r, src)
    print(f'{p} -> {n}')

# 精确字段名（后跟非字母数字）替换
sub(r'this->field_4C\.height', 'this->itemBbox.height')
sub(r'this->field_4C\.width', 'this->itemBbox.width')
sub(r'this->field_8\b', 'this->columns')
sub(r'this->field_2C\.width', 'this->bbox.width')
sub(r'this->field_2C\.height', 'this->bbox.height')
sub(r'this->field_2C\.x', 'this->bbox.x')
sub(r'this->field_2C\.y', 'this->bbox.y')
sub(r'this->field_24\b', 'this->screenScale')
sub(r'this->minecraft->field_20', 'this->minecraft->height')
sub(r'this->field_1C4', 'this->vScroll')
sub(r'this->verticalScrollbar', 'this->hScroll')
# GridItem 字段（对象访问，防误伤 this->field_xxx）
sub(r'\.field_0\b', '.id')
sub(r'\.field_C\b', '.xf')
sub(r'\.field_10\b', '.yf')
# ScrollBar 局部变量 a2
sub(r'a2\.field_8', 'a2.w')
sub(r'a2\.field_4', 'a2.y')
sub(r'a2\.field_C', 'a2.h')
sub(r'a2\.color', 'a2.alpha')

# RectangleArea field_5C 构造：改用 ScrollingPane 已有的 area（构造时建好），删除 0.8.1 的扩边
src = src.replace('''\tthis->field_5C.x = (float)a4.x - a6;
\tthis->field_5C.maxX = (float)(a4.x + a4.width) + a6;
\tthis->field_5C.y = this->field_5C.y - 6.0;
\tthis->field_5C.maxY = this->field_5C.maxY + 6.0;
''', '\t// 触摸区直接使用 ScrollingPane 构造时建的 area（client 版无 0.8.1 的 field_5C 扩边）\n')

with io.open(path, 'w', encoding='utf-8', newline='') as f:
    f.write(src)
print('done')
