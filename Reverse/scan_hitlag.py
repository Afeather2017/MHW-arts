"""Scan dump exe .text for reads of [reg+disp32] (disp in DISPS) via:
  - SSE float ops: [F3][REX?][0F][op][modrm=10,rm!=4][disp32] (+SIB variant)
  - integer mov:   [REX?][8B][modrm=10,rm!=4][disp32]   (+SIB variant)
  - lea:           [REX?][8D][modrm=10,rm!=4][disp32]   (+SIB variant)
Attribute hits to enclosing FUN_ (start list from Ghidra .c).
"""
import bisect
import re
import struct

EXE = r"E:\MHW-arts\Reverse\MonsterHunterWorld_dump.exe"
CS = r"E:\MHW-arts\Reverse\MonsterHunterWorld_dump.exe.c"
BASE = 0x140000000
DISPS = (0x1F0, 0x210, 0x1EC, 0x20C)
SSE_OPS = set((0x10, 0x58, 0x59, 0x51, 0x5C, 0x5D, 0x5E, 0x5A, 0x54, 0x2A, 0x2C, 0xE6, 0x5B))

with open(EXE, "rb") as f:
    data = f.read()
pe_off = struct.unpack_from("<I", data, 0x3C)[0]
coff = pe_off + 4
num_sections = struct.unpack_from("<H", data, coff + 2)[0]
opt_size = struct.unpack_from("<H", data, coff + 16)[0]
for i in range(num_sections):
    b = coff + 20 + opt_size + i * 40
    nm = data[b:b + 8].rstrip(b"\0").decode(errors="replace")
    vsize, vaddr, rsize, raddr = struct.unpack_from("<IIII", data, b + 8)
    if nm == ".text":
        tvaddr, traddr, tsize = vaddr, raddr, max(vsize, rsize)

funcs = set()
with open(CS, "r", encoding="utf-8", errors="replace") as f:
    for line in f:
        c = line[0]
        if c.isalpha() or c == "_":
            m = re.match(r"^[A-Za-z_][A-Za-z0-9_ \*]*?\s(FUN_[0-9a-f]{9})\(", line)
            if m:
                funcs.add(int(m.group(1)[4:], 16))
fstarts = sorted(funcs)
lo = fstarts[0]
print("text rva=%#x funcs=%d first=%x" % (tvaddr, len(fstarts), lo))

blob = data[traddr:traddr + tsize]
N = len(blob)


def is_prefix(b):
    return b == 0xF3 or 0x40 <= b <= 0x4F


def prefixes_end(p):  # walk back over up to 3 prefix bytes (F3/REX)
    q = p
    for _ in range(3):
        if q - 1 >= 0 and is_prefix(blob[q - 1]):
            q -= 1
        else:
            break
    return q


hits = {}
for disp in DISPS:
    needle = struct.pack("<I", disp)
    pos = blob.find(needle)
    while pos >= 0:
        va = None
        # no-SIB: [..prefixes..][0F][op][modrm][disp] or [..rex..][8B/8D][modrm][disp]
        modrm = blob[pos - 1]
        if (modrm & 0xC0) == 0x80 and (modrm & 7) != 4:
            op = blob[pos - 2]
            if op in SSE_OPS and blob[pos - 3] == 0x0F:
                va = BASE + tvaddr + prefixes_end(pos - 3)
            elif op in (0x8B, 0x8D):
                va = BASE + tvaddr + prefixes_end(pos - 2)
        elif (modrm & 0xC0) == 0x80 and (modrm & 7) == 4 and pos >= 3:
            sib = blob[pos - 1]
            modrm = blob[pos - 2]
            if (modrm & 0xC0) == 0x80 and (sib & 7) != 5:
                op = blob[pos - 3]
                if op in SSE_OPS and blob[pos - 4] == 0x0F:
                    va = BASE + tvaddr + prefixes_end(pos - 4)
                elif op in (0x8B, 0x8D):
                    va = BASE + tvaddr + prefixes_end(pos - 3)
        if va is not None:
            r = va - BASE
            if r >= lo:
                fva = BASE + fstarts[bisect.bisect_right(fstarts, r) - 1]
                hits.setdefault(fva, {}).setdefault(disp, []).append(va)
        pos = blob.find(needle, pos + 1)

for fva, d in sorted(hits.items()):
    a, b = len(d.get(0x1F0, [])), len(d.get(0x210, []))
    if a == 0 and b == 0:
        continue
    extra = " ".join("%x:%d" % (k, len(v)) for k, v in sorted(d.items()))
    print("FUN_%08x  lagA=%d lagB=%d  [%s]" % (fva - BASE, a, b, extra))
