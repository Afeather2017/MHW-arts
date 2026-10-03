# -*- coding: utf-8 -*-
"""Disassemble ValorLS.dll 4.2: enumerate ALL hooks from the install function
and dump detour context. Uses .pdata for function boundaries."""
import struct

import capstone

data = open(r'MHW-valor/nativePC/plugins/ValorLS.dll', 'rb').read()
PE_OFF = 0x400          # .text file offset
TEXT_RVA = 0x1000
TEXT_RAW = 0x400
TEXT_SIZE = 0x249ce

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
md.detail = False


def rva2off(rva):
    if TEXT_RVA <= rva < TEXT_RVA + TEXT_SIZE:
        return rva - TEXT_RVA + TEXT_RAW
    return None


def dis(rva, n, label=""):
    off = rva2off(rva)
    code = data[off:off + n]
    print("== %s (RVA 0x%X) ==" % (label, rva))
    for ins in md.disasm(code, 0x180000000 + rva):
        print("  %08X  %-24s %s %s" % (ins.address, ins.bytes.hex(), ins.mnemonic, ins.op_str))


# parse .pdata for function ranges (RVA 0x35000, raw 0x32600, size 0x1b54)
pdata_raw, pdata_rva, pdata_size = 0x32600, 0x35000, 0x1b54
funcs = []
for i in range(0, pdata_size, 12):
    start, end, unw = struct.unpack_from("<III", data, pdata_raw + i)
    if start == 0:
        break
    funcs.append((start, end))
funcs.sort()

target_rvas = [0x9B81, 0x4DCC, 0x4DEC, 0x7CF0, 0x775A, 0x840C, 0x1543F]


def find_func(rva):
    for s, e in funcs:
        if s <= rva < e:
            return s, e
    return None


for tr in target_rvas:
    fr = find_func(tr)
    print("ref RVA 0x%X -> func 0x%X..0x%X" % (tr, fr[0], fr[1]) if fr else "ref 0x%X: no func" % tr)
