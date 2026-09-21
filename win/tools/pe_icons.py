# -*- coding: utf-8 -*-
"""Parse RT_ICON resources in the exe and compare with app.ico frames."""
import struct

EXE = r"H:/workerspace/workapace/main/MinecraftPE-Win/MinecraftWin32_GL.exe"
ICO = r"H:/workerspace/workapace/main/MinecraftPE-Win/handheld/project/win32_gl/app.ico"

def u16(b, o):
    return struct.unpack_from("<H", b, o)[0]

def u32(b, o):
    return struct.unpack_from("<I", b, o)[0]

def read_pe_resources(data):
    """Return list of (type_id, rva, size) for image resources."""
    e_lfanew = u32(data, 0x3C)
    pe = e_lfanew
    assert data[pe : pe + 4] == b"PE\0\0"
    opt = pe + 24
    magic = u16(data, opt)
    ddoff = opt + (112 if magic == 0x20B else 96)
    res_rva = u32(data, ddoff + 2 * 4)
    res_size = u32(data, ddoff + 2 * 4 + 4)
    if not res_rva:
        return []
    # map rva -> file offset using section headers
    nsec = u16(data, pe + 6)
    sec_off = opt + (240 if magic == 0x20B else 224)
    secs = []
    for i in range(nsec):
        o = sec_off + i * 40
        name = data[o : o + 8].rstrip(b"\0")
        vsize = u32(data, o + 8)
        vaddr = u32(data, o + 12)
        rawsz = u32(data, o + 16)
        rawptr = u32(data, o + 20)
        secs.append((name, vaddr, vsize, rawptr, rawsz))

    def rva2off(rva):
        for name, vaddr, vsize, rawptr, rawsz in secs:
            if vaddr <= rva < vaddr + max(vsize, rawsz):
                return rawptr + (rva - vaddr)
        print("  [rva2off FAIL]", hex(rva))
        return None

    # walk resource tree
    out = []

    def walk_dir(off, depth, path):
        if depth > 3:
            return
        n_named = u16(data, off + 12)
        n_id = u16(data, off + 14)
        entries = off + 16
        for i in range(n_named + n_id):
            eo = entries + i * 8
            name_id = u32(data, eo)
            child = u32(data, eo + 4)
            if depth == 0:
                key = name_id  # type id
                next_path = [key]
            elif depth == 1:
                key = name_id  # icon id
                next_path = path + [key]
            else:
                key = name_id
                next_path = path + [key]
            if child & 0x80000000:
                walk_dir(rva2off(child & 0x7FFFFFFF), depth + 1, next_path)
            else:
                leaf_off = rva2off(child)
                if leaf_off is None:
                    continue
                lsize = u32(data, leaf_off + 4)
                lrva = u32(data, leaf_off)
                data_off = rva2off(lrva)
                if data_off is None:
                    continue
                out.append((tuple(path + [key]), data_off, lsize))

    walk_dir(rva2off(res_rva), 0, [])
    return out

with open(EXE, "rb") as f:
    exe = f.read()

res = read_pe_resources(exe)
print("PE resource entries:", len(res))
icons = [(p, o, s) for p, o, s in res if p[0] == 3]  # RT_ICON
groups = [(p, o, s) for p, o, s in res if p[0] == 14]  # RT_GROUP_ICON
print("RT_ICON entries:", len(icons), " RT_GROUP_ICON entries:", len(groups))
for p, o, s in icons:
    payload = exe[o : o + s]
    # detect PNG (Vista+ compressed icon) vs BMP (DIB)
    is_png = payload[:8] == b"\x89PNG\r\n\x1a\n"
    if is_png:
        w = struct.unpack(">I", payload[16:20])[0]
        h = struct.unpack(">I", payload[20:24])[0]
        print("  RT_ICON id=%s size=%d PNG %dx%d" % (p[1], s, w, h))
    else:
        # BITMAPINFOHEADER at start of DIB (without file header)
        bw = u32(payload, 4)
        bh = u32(payload, 8)
        print("  RT_ICON id=%s size=%d BMP-DIB %dx%d" % (p[1], s, bw, bh // 2))

# group icon header: first RT_GROUP_ICON lists the frame layout
for p, o, s in groups:
    payload = exe[o : o + s]
    n = u16(payload, 4)
    print("  RT_GROUP_ICON id=%s frames=%d:" % (p[1], n))
    for i in range(n):
        fo = 6 + i * 14
        w = payload[fo]
        h = payload[fo + 1]
        planes = u16(payload, fo + 4)
        bpp = u16(payload, fo + 6)
        sz = u32(payload, fo + 8)
        rid = u16(payload, fo + 12)
        print("    %dx%d planes=%d bpp=%d size=%d -> RT_ICON id=%d" % (w or 256, h or 256, planes, bpp, sz, rid))
