"""Dump bytes at a VA from the unpacked MonsterHunterWorld dump exe.

Usage: python vadump.py <va_hex> [length]
"""
import struct
import sys

EXE = r"E:\MHW-arts\Reverse\MonsterHunterWorld_dump.exe"
BASE = 0x140000000


def rva_to_file_offset(data, rva):
    pe_off = struct.unpack_from("<I", data, 0x3C)[0]
    assert data[pe_off:pe_off + 4] == b"PE\0\0", "bad PE signature"
    coff = pe_off + 4
    num_sections = struct.unpack_from("<H", data, coff + 2)[0]
    opt_size = struct.unpack_from("<H", data, coff + 16)[0]
    sections = coff + 20 + opt_size
    for i in range(num_sections):
        base = sections + i * 40
        name = data[base:base + 8].rstrip(b"\0").decode(errors="replace")
        vsize, vaddr, rsize, raddr = struct.unpack_from("<IIII", data, base + 8)
        if vaddr <= rva < vaddr + max(vsize, rsize):
            if rva - vaddr < rsize:
                return raddr + (rva - vaddr), name
            return None, name
    return None, None


def read_va(data, va, length):
    rva = va - BASE
    off, section = rva_to_file_offset(data, rva)
    if off is None:
        return None, section
    return data[off:off + length], section


def show(data, va, length):
    blob, section = read_va(data, va, length)
    if blob is None:
        print(f"VA {va:#x}: not mapped ({section})")
        return
    print(f"VA {va:#x} (section {section}):")
    for i in range(0, len(blob), 16):
        chunk = blob[i:i + 16]
        hexs = " ".join(f"{b:02x}" for b in chunk)
        text = "".join(chr(b) if 0x20 <= b < 0x7f else "." for b in chunk)
        print(f"  {va + i:#x}  {hexs:<47}  {text}")
        for enc in ("utf-8", "shift_jis"):
            try:
                decoded = chunk.decode(enc)
                if any(ord(c) > 0x2E80 for c in decoded):
                    print(f"      [{enc}] {decoded}")
            except (UnicodeDecodeError, ValueError):
                pass


def main():
    va = int(sys.argv[1], 16)
    length = int(sys.argv[2], 0) if len(sys.argv) > 2 else 0x40
    with open(EXE, "rb") as f:
        data = f.read()
    show(data, va, length)


if __name__ == "__main__":
    main()
