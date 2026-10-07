# -*- coding: utf-8 -*-
"""Byte-level per-entry diff between GOOD (0.0.1) and BAD (current) LMT.
For each entry present in both: compare track headers (minus offsets),
track buffers, lerp blocks, and TIML trees (minus relocated pointers).
Report entries whose CONTENT differs.
"""
import struct

GOOD = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00_0.0.1.lmt"
BAD  = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt"

def load(p):
    b = open(p, "rb").read()
    n = struct.unpack_from("<h", b, 6)[0]
    return b, struct.unpack_from("<%dQ" % n, b, 16)

GB, GO = load(GOOD)
BB, BO = load(BAD)

def timl_bytes(buf, to):
    """Return (normalized_bytes) of the TIML tree with pointer fields zeroed."""
    typesOffset, tc = struct.unpack_from("<QQ", buf, to)
    span = [(to, 40)]
    end = to + 40
    for ti in range(tc):
        loc = typesOffset + 24*ti
        toff, tcount, tlHash, u0 = struct.unpack_from("<QQIi", buf, loc)
        span.append((loc, 24)); end = max(end, loc+24)
        for xi in range(tcount):
            xl = toff + 24*xi
            xoff, xcount, dtHash, dt = struct.unpack_from("<QQIi", buf, xl)
            span.append((xl, 24)); end = max(end, xl+24)
            if xoff and xcount:
                span.append((xoff, 20*xcount)); end = max(end, xoff+20*xcount)
    span.sort()
    img = bytearray()
    ptrpos = []   # positions of pointer fields within image, with their target span-index
    # rebuild: copy span with pointers zeroed, remember pointer field positions
    out = bytearray()
    pos_map = {}  # original offset -> (img_start, length)
    for (o, ln) in span:
        pos_map[o] = len(out)
        out += buf[o:o+ln]
    def z(im, off, size): im[off:off+size] = b"\0"*size
    # zero pointer fields (u64 at +0 of timl header/types rows/transform rows)
    z(out, pos_map[to], 8)
    for ti in range(tc):
        z(out, pos_map[typesOffset + 24*ti], 8)
        toff = struct.unpack_from("<Q", buf, typesOffset + 24*ti)[0]
        tcount = struct.unpack_from("<Q", buf, typesOffset + 24*ti + 8)[0]
        for xi in range(tcount):
            z(out, pos_map[toff + 24*xi], 8)
    return bytes(out)

def entry_img(buf, off):
    fco, fcc, frm, lf = struct.unpack_from("<QIIi", buf, off)
    parts = []
    # header minus pointers (fcurveOffset@0..8, timlOffset@88..96)
    hdr = bytearray(buf[off:off+96])
    hdr[0:8] = b"\0"*8
    hdr[88:96] = b"\0"*8
    parts.append(bytes(hdr))
    # tracks
    for i in range(fcc):
        tb = fco + 48*i
        th = bytearray(buf[tb:tb+48])
        th[16:24] = b"\0"*8   # bufferOffset
        th[40:48] = b"\0"*8   # lerpOffset
        parts.append(bytes(th))
        boff, = struct.unpack_from("<Q", buf, tb+16)
        bsize, = struct.unpack_from("<i", buf, tb+12)
        if boff and bsize > 0:
            parts.append(bytes(buf[boff:boff+bsize]))
        loff, = struct.unpack_from("<Q", buf, tb+40)
        if loff:
            parts.append(bytes(buf[loff:loff+32]))
    to = struct.unpack_from("<Q", buf, off+88)[0]
    if to:
        parts.append(timl_bytes(buf, to))
    return b"".join(parts)

ns = struct.unpack_from("<h", GB, 6)[0]
diffs = []
missing = []
for e in range(ns):
    if not GO[e] or not BO[e]:
        if bool(GO[e]) != bool(BO[e]):
            missing.append(e)
        continue
    a, b2 = entry_img(GB, GO[e]), entry_img(BB, BO[e])
    if a != b2:
        # find first difference offset for a hint
        d = next((i for i in range(min(len(a), len(b2))) if a[i] != b2[i]), min(len(a), len(b2)))
        diffs.append((e, len(a), len(b2), d))
print("entries present in only one file:", missing)
print("byte-different entries: %d" % len(diffs))
for e, la, lb, d in diffs:
    print("  entry %-4d imglen G=%dB B=%dB firstdiff@%d" % (e, la, lb, d))
