"""Find rip-relative code references to a VA in the dump exe.

The offline anchor tool: string/data address -> referencing instructions ->
enclosing functions. Usage: python find_xrefs.py <va_hex> [max_show]

Note: backward-decoded mnemonics can be misleading (instruction boundaries
are guessed); the POSITIONS are exact - verify context in Ghidra.
"""
import struct
import sys

import capstone

import vadump


def section(data, name):
    pe_off = struct.unpack_from("<I", data, 0x3C)[0]
    coff = pe_off + 4
    num_sections = struct.unpack_from("<H", data, coff + 2)[0]
    opt_size = struct.unpack_from("<H", data, coff + 16)[0]
    sections = coff + 20 + opt_size
    for i in range(num_sections):
        base = sections + i * 40
        sname = data[base:base + 8].rstrip(b"\0").decode(errors="replace")
        vsize, vaddr, rsize, raddr = struct.unpack_from("<IIII", data, base + 8)
        if sname == name:
            return vaddr, raddr, rsize
    return None


def find_referring_function(data, text_vaddr, hit_end_rva):
    """Walk backwards to the nearest preceding int3 padding / known start."""
    # crude: return the enclosing function by scanning back for CC CC padding
    return None


def main():
    va = int(sys.argv[1], 16)
    max_show = int(sys.argv[2]) if len(sys.argv) > 2 else 40
    with open(vadump.EXE, "rb") as f:
        data = f.read()
    text_vaddr, text_raddr, text_rsize = section(data, ".text")
    blob = data[text_raddr:text_raddr + text_rsize]
    # disp32 may sit at any byte offset (instructions are unaligned), so scan
    # every position: end_rva(i) + disp32(i) == target_rva  <=>  hit.
    end_target = va - 0x140000000 - text_vaddr
    hits = []
    for i in range(len(blob) - 4):
        disp = int.from_bytes(blob[i:i + 4], "little", signed=True)
        if i + 4 + disp == end_target:
            hits.append(i)
    print(f"{len(hits)} rip-relative refs to {va:#x} in .text")
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    shown = 0
    for h in hits:
        if shown >= max_show:
            break
        end_va = 0x140000000 + text_vaddr + h + 4
        found = None
        for back in range(1, 16):
            start = h - back
            if start < 0:
                continue
            for insn in md.disasm(blob[start:start + 24], 0x140000000 + text_vaddr + start):
                if insn.address + insn.size == end_va:
                    found = insn
                    break
            if found:
                break
        if found:
            print(f"  {found.address:#x}  {found.mnemonic} {found.op_str}")
        else:
            print(f"  ~{end_va - 4:#x}  (check in Ghidra; disp bytes "
                  f"{blob[h:h + 4].hex()})")
        shown += 1


if __name__ == "__main__":
    main()
