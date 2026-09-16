"""Disassemble MonsterHunterWorld.exe at an RVA using only stdlib + capstone.

Usage: python disasm_rva.py <rva_hex> [length]
"""
import struct
import sys

EXE = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\MONSTERHUNTERWORLD.EXE"


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


def main():
    rva = int(sys.argv[1], 16)
    length = int(sys.argv[2], 0) if len(sys.argv) > 2 else 160
    with open(EXE, "rb") as f:
        data = f.read()
    off, section = rva_to_file_offset(data, rva)
    if off is None:
        print(f"RVA {rva:#x} not mapped in a raw section ({section})")
        return
    print(f"RVA {rva:#x} -> file offset {off:#x} (section {section})")
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = False
    for insn in md.disasm(data[off:off + length], 0x140000000 + rva):
        print(f"  {insn.address:#x}  {insn.mnemonic} {insn.op_str}")


if __name__ == "__main__":
    main()
