"""Extract hook signatures from the dump exe and verify uniqueness.

Wildcards: rel32 branches (E8/E9/0F8x), rip-relative disp32, abs imm64 -
exactly the bytes a patch will move. Everything else stays exact.

Usage:
  python sig_check.py 0x118BDA0 [len]     emit C++ pattern + mask for one RVA
  python sig_check.py --verify            verify Reverse/signatures.json
  python sig_check.py --emit              (re)generate signatures.json
"""
import json
import struct
import sys

import capstone

import vadump

TARGETS = {
    "condEval": 0x118BDA0,
    "poolQuery": 0xAEE050,
    "dmgCalc": 0x1F66DD0,
    "dealDamage": 0x1CC5F80,
    "dmgFormula": 0x2BC4A0,
    "atkCtxBuilder": 0x2C1050,
    "hitDispatch": 0x1F71960,
    "postEventId": 0x28B8DD0,
    "soundPost": 0x26D4C50,
    "postEventName": 0x28B8FE0,
    "propInt": 0x1922190,
    "propBool": 0x19221F0,
    "propFloat": 0x1922240,
    "gaugeAdd": 0x2122410,
    "fxNotify": 0xADD670,
    "negrEvent": 0x21038B0,
    "auraWhite": 0xF0DC60,
    "auraRed": 0xF0DCD0,
    "propLookup": 0x462700,
    "turnSet": 0x1F755C0,
    "hpDelta": 0x1F518E0,
    "stamDelta": 0x1C49F10,
    "vitSet": 0x1216940,
    "vitAdd": 0x1216880,
}
JSON_PATH = "signatures.json"


def wildcard_ranges(data, rva, length):
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    md.detail = True
    ranges = []
    off, _ = vadump.rva_to_file_offset(data, rva)
    code = data[off:off + length]
    for insn in md.disasm(code, 0x140000000 + rva):
        b = insn.bytes
        s = insn.address - (0x140000000 + rva)
        if b[0] in (0xE8, 0xE9) or (len(b) >= 2 and b[0] == 0x0F and 0x80 <= b[1] <= 0x8F):
            start = 1 if b[0] in (0xE8, 0xE9) else 2
            ranges.append((s + start, s + start + 4))
        elif len(b) >= 2 and b[0] == 0x48 and b[1] == 0xB8:  # movabs rax, imm64
            ranges.append((s + 2, s + 10))
        else:
            for op in insn.operands:
                if op.type == capstone.x86.X86_OP_MEM and op.mem.base == capstone.x86.X86_REG_RIP:
                    end = insn.address + insn.size - (0x140000000 + rva)
                    ranges.append((end - 4, end))
    return code, ranges


def build_mask(code, ranges):
    mask = bytearray(b"x" * len(code))
    for a, b in ranges:
        for i in range(a, min(b, len(mask))):
            mask[i] = ord("?")
    return bytes(mask)


def emit(data, name, rva, length):
    code, ranges = wildcard_ranges(data, rva, length)
    mask = build_mask(code, ranges)
    print(f"// {name}: RVA 0x{rva:X}")
    print("static const std::uint8_t k_sig_%s[] = {" % name)
    for i in range(0, len(code), 16):
        row = ", ".join(f"0x{b:02X}" for b in code[i:i + 16])
        mrow = "".join(chr(c) for c in mask[i:i + 16])
        print(f"    {row},  // {mrow}")
    print("};")
    print(f'static const char k_sig_{name}_mask[] = "{mask.decode()}";')
    print()
    return {"rva": rva, "pattern": list(code), "mask": mask.decode()}


def find_all(data, pattern, mask):
    pe_off = struct.unpack_from("<I", data, 0x3C)[0]
    coff = pe_off + 4
    num_sections = struct.unpack_from("<H", data, coff + 2)[0]
    opt_size = struct.unpack_from("<H", data, coff + 16)[0]
    sections = coff + 20 + opt_size
    hits = []
    for i in range(num_sections):
        base = sections + i * 40
        name = data[base:base + 8].rstrip(b"\0").decode(errors="replace")
        vsize, vaddr, rsize, raddr = struct.unpack_from("<IIII", data, base + 8)
        if name != ".text":
            continue
        blob = data[raddr:raddr + rsize]
        pos = 0
        while True:
            idx = find_first(blob, pattern, mask, pos)
            if idx < 0:
                break
            hits.append(0x140000000 + vaddr + idx)
            pos = idx + 1
    return hits


def find_first(blob, pattern, mask, start):
    mask = mask.encode() if isinstance(mask, str) else mask
    n = len(blob)
    plen = len(pattern)
    first = next(i for i, m in enumerate(mask) if m == ord("x"))
    anchor = pattern[first:first + 4]
    pos = start
    while pos <= n - plen:
        j = blob.find(anchor, pos + first)
        if j < 0:
            return -1
        base = j - first
        if base + plen <= n and all(
            m != ord("x") or blob[base + i] == pattern[i]
            for i, m in enumerate(mask)
        ):
            return base
        pos = j - first + 1
    return -1


def main():
    with open(vadump.EXE, "rb") as f:
        data = f.read()
    if len(sys.argv) > 1 and sys.argv[1] == "--verify":
        with open(JSON_PATH, encoding="utf-8") as f:
            sigs = json.load(f)
        ok = True
        for name, s in sigs.items():
            hits = find_all(data, bytes(s["pattern"]), s["mask"])
            want = 0x140000000 + s["rva"]
            good = len(hits) == 1 and hits[0] == want
            ok &= good
            print(f"{name:15s} {'OK ' if good else 'BAD'} rva=0x{s['rva']:X} "
                  f"hits={[hex(h) for h in hits]}")
        print("ALL OK" if ok else "FAILURES PRESENT")
        return
    if len(sys.argv) > 1 and sys.argv[1] == "--emit":
        out = {}
        with open(vadump.EXE, "rb") as f:
            pass
        for name, rva in TARGETS.items():
            code, ranges = wildcard_ranges(data, rva, 48)
            out[name] = {"rva": rva, "pattern": list(code),
                         "mask": build_mask(code, ranges).decode()}
        with open(JSON_PATH, "w", encoding="utf-8") as f:
            json.dump(out, f, indent=1)
        print(f"wrote {JSON_PATH}")
        for name, s in out.items():
            hits = find_all(data, bytes(s["pattern"]), s["mask"])
            print(f"{name:15s} rva=0x{s['rva']:X} hits={[hex(h) for h in hits]}")
        return
    rva = int(sys.argv[1], 16)
    length = int(sys.argv[2], 0) if len(sys.argv) > 2 else 48
    emit(data, "hook", rva, length)
    code, ranges = wildcard_ranges(data, rva, length)
    hits = find_all(data, code, build_mask(code, ranges).decode())
    print(f"verify: hits={[hex(h) for h in hits]} (want [0x{0x140000000 + rva:x}])")


if __name__ == "__main__":
    main()
