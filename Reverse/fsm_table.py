"""Parse per-weapon FSM condition descriptor tables.

Base 0x143fd5590, stride 0x130 per weapon type (FUN_142165600).
Block: +0x20 = ptr to rows, +0x28 = row count. Row (0x18): {i32 key, pad, ptr, u32}.

Usage: python fsm_table.py [weapon_type ...] [key ...]
  python fsm_table.py              -> summary of all 14 blocks
  python fsm_table.py 1            -> all rows of weapon 1
  python fsm_table.py 1 0x277      -> row 0x277 of weapon 1
"""
import struct
import sys

import vadump

BASE_VA = 0x143FD5590
BLOCK = 0x130
STRIDE = 0x18


def rows(data, wtype):
    block_va = BASE_VA + wtype * BLOCK
    blob, _ = vadump.read_va(data, block_va, BLOCK)
    if blob is None:
        return None, None, None
    rows_ptr, count = struct.unpack_from("<QI", blob, 0x20)
    out = []
    if rows_ptr and count:
        rb, _ = vadump.read_va(data, rows_ptr, count * STRIDE)
        for i in range(count):
            key, ptr, val = struct.unpack_from("<i4xQQ", rb, i * STRIDE)
            out.append((key, ptr, val))
    return out, block_va, count


def describe(data, ptr):
    if not (0x140000000 <= ptr < 0x148000000):
        return ""
    nb, _ = vadump.read_va(data, ptr, 64)
    if nb is None:
        return ""
    d0, d1, d2 = struct.unpack_from("<iii", nb, 0)
    text = nb.split(b"\0", 1)[0]
    s = ""
    try:
        dec = text.decode("utf-8")
        if len(dec) > 1 and all(0x20 <= ord(c) or ord(c) > 0x2E80 for c in dec):
            s = f" \"{dec}\""
    except UnicodeDecodeError:
        pass
    return f" -> d0={d0} d1={d1} d2={d2}{s}"


def main():
    with open(vadump.EXE, "rb") as f:
        data = f.read()
    args = sys.argv[1:]
    wtypes = [int(a, 0) for a in args if not a.startswith("0x")]
    keys = {int(a, 0) for a in args if a.startswith("0x")}
    if not wtypes:
        for w in range(14):
            r, bva, cnt = rows(data, w)
            hit = [(k, p, v) for k, p, v in (r or []) if k in (0x274, 0x275, 0x276, 0x277)]
            total = len(r) if r else 0
            mark = " <== LS candidate" if hit else ""
            print(f"wtype {w:2d} @ {bva:#x}: {total:4d} rows, 0x27x hits: "
                  f"{[(hex(k), hex(v)) for k, p, v in hit]}{mark}")
        return
    for w in wtypes:
        r, bva, cnt = rows(data, w)
        print(f"wtype {w} @ {bva:#x}: {cnt} rows")
        for k, p, v in r or []:
            if keys and k not in keys:
                continue
            print(f"  key={k:#x} ptr={p:#x} val={v:#x}{describe(data, p)}")


if __name__ == "__main__":
    main()
