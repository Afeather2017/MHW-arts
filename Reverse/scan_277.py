"""Scan .text for cmp r32, imm32 where imm == 0x277 (condition index 631).

Usage: python scan_277.py [context_bytes]
"""
import struct
import sys

import vadump

PATTERNS = []
for modrm in range(0xF8, 0x100):  # 81 /7 with r32: F8=eax..FF=edi
    PATTERNS.append(bytes([0x81, modrm]) + struct.pack("<I", 0x277))


def main():
    ctx = int(sys.argv[1], 0) if len(sys.argv) > 1 else 0
    with open(vadump.EXE, "rb") as f:
        data = f.read()
    # find .text range
    pe_off = struct.unpack_from("<I", data, 0x3C)[0]
    coff = pe_off + 4
    num_sections = struct.unpack_from("<H", data, coff + 2)[0]
    opt_size = struct.unpack_from("<H", data, coff + 16)[0]
    sections = coff + 20 + opt_size
    text = None
    for i in range(num_sections):
        base = sections + i * 40
        name = data[base:base + 8].rstrip(b"\0").decode(errors="replace")
        vsize, vaddr, rsize, raddr = struct.unpack_from("<IIII", data, base + 8)
        if name == ".text":
            text = (raddr, rsize)
    print(f".text file {text[0]:#x} size {text[1]:#x}")
    blob = data[text[0]:text[0] + text[1]]
    hits = []
    for pat in PATTERNS:
        start = 0
        while True:
            idx = blob.find(pat, start)
            if idx < 0:
                break
            hits.append(idx)
            start = idx + 1
    hits.sort()
    # group into regions
    prev = -100
    for h in hits:
        va = 0x140000000 + h
        gap = "  ----" if h - prev > 0x100 else ""
        if gap or ctx:
            print(f"{va:#x} (file {text[0] + h:#x}){gap}")
        elif h - prev > 0x100:
            print(f"{va:#x}")
        prev = h
    print(f"total {len(hits)} hits")


if __name__ == "__main__":
    main()
