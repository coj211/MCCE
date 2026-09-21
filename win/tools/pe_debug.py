# -*- coding: utf-8 -*-
"""Debug PE section table and resource RVA mapping."""
import struct

EXE = r"H:/workerspace/workapace/main/MinecraftPE-Win/MinecraftWin32_GL.exe"

def u16(b, o):
    return struct.unpack_from("<H", b, o)[0]

def u32(b, o):
    return struct.unpack_from("<I", b, o)[0]

with open(EXE, "rb") as f:
    data = f.read()

e_lfanew = u32(data, 0x3C)
pe = e_lfanew
opt = pe + 24
magic = u16(data, opt)
print("magic:", hex(magic))
ddoff = opt + (112 if magic == 0x20B else 96)
res_rva = u32(data, ddoff + 16)
res_size = u32(data, ddoff + 20)
print("resource dir rva=%s size=%s" % (hex(res_rva), res_size))

nsec = u16(data, pe + 6)
sec_off = opt + (240 if magic == 0x20B else 224)
print("number of sections:", nsec)
for i in range(nsec):
    o = sec_off + i * 40
    name = data[o : o + 8].rstrip(b"\0")
    vsize = u32(data, o + 8)
    vaddr = u32(data, o + 12)
    rawsz = u32(data, o + 16)
    rawptr = u32(data, o + 20)
    print("  %-8s vaddr=%08x vsize=%08x rawptr=%08x rawsz=%08x" % (name.decode(), vaddr, vsize, rawptr, rawsz))

# does res_rva fall in any section?
for i in range(nsec):
    o = sec_off + i * 40
    name = data[o : o + 8].rstrip(b"\0")
    vsize = u32(data, o + 8)
    vaddr = u32(data, o + 12)
    rawsz = u32(data, o + 16)
    rawptr = u32(data, o + 20)
    if vaddr <= res_rva < vaddr + max(vsize, rawsz):
        print("res_rva in section", name, "file offset", hex(rawptr + (res_rva - vaddr)))
