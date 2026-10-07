# -*- coding: utf-8 -*-
"""Deep binary diff of one LMT entry: deployed vs vanilla.
Dumps ActionHeader bytes, per-track header fields, lerp targets, TIML values.
"""
import struct, math, os, sys

DEPLOYED = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt"
VANILLA  = r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\mot\w03_00\w03_00.lmt"

KEY_SIZE = {1:12, 2:12, 3:16, 4:8, 5:4, 6:8, 7:4, 11:4, 12:4, 13:4, 14:6, 15:5}
BITPACKED = {6: (64, 8), 7: (32, 4), 11: (32, 4), 12: (32, 4), 13: (32, 4), 14: (48, 4), 15: (40, 4)}
SIMPLE_FRAME = {3: (12, "<I"), 4: (6, "<H"), 5: (3, "<B")}

def load(p):
    buf = open(p, "rb").read()
    cnt = struct.unpack_from("<h", buf, 6)[0]
    offs = struct.unpack_from("<%dQ" % cnt, buf, 16)
    return buf, offs

def f4(b, o): return struct.unpack_from("<4f", b, o)

def bad_float(v):
    return math.isnan(v) or math.isinf(v)

def dump_entry(tag, buf, off, eid):
    print("=" * 100)
    print("[%s] entry %d @ 0x%X" % (tag, eid, off))
    hdr = buf[off:off+96]
    fco, fcc, frm, lf = struct.unpack_from("<QIIi", hdr, 0)
    vec0 = f4(hdr, 32); vec2 = f4(hdr, 48)
    flags, b65, b66, flags2 = hdr[64], hdr[65], hdr[66], hdr[67]
    null3 = struct.unpack_from("<5i", hdr, 68)
    timl = struct.unpack_from("<Q", hdr, 88)[0]
    print("  fcurveOffset=0x%X count=%d frameCount=%d loopFrame=%d flags=%d b65=%d b66=%d flags2=%d" %
          (fco, fcc, frm, lf, flags, b65, b66, flags2))
    print("  Vec0=%s  Vec2=%s" % (["%.4g" % v for v in vec0], ["%.4g" % v for v in vec2]))
    print("  NULL3=%s timl=0x%X" % (null3, timl))
    for v in vec0 + vec2:
        if bad_float(v): print("  !! Vec NaN/Inf")
    rows = []
    for i in range(fcc):
        base = fco + 48*i
        bt, usage, jt, b205, boneId, weight, bsize = struct.unpack_from("<4Bifi", buf, base)
        boff, = struct.unpack_from("<Q", buf, base+16)
        basis = f4(buf, base+24)
        loff, = struct.unpack_from("<Q", buf, base+40)
        lerps = f4(buf, loff) + f4(buf, loff+16) if loff else None
        rows.append((i, bt, usage, jt, b205, boneId, weight, bsize, boff, basis, loff, lerps))
        for v in filter(bad_float, basis):
            print("  !! track%d basis NaN/Inf bone%d" % (i, boneId))
        if lerps:
            for v in filter(bad_float, lerps):
                print("  !! track%d lerp NaN/Inf bone%d" % (i, boneId))
        if loff and loff + 32 > len(buf):
            print("  !! track%d lerp OOB" % i)
    return rows

def diff_entry(eid):
    DB, offsD = load(DEPLOYED)
    VB, offsV = load(VANILLA)
    rd = dump_entry("deployed", DB, offsD[eid], eid)
    rv = dump_entry("vanilla ", VB, offsV[eid], eid)
    print("-" * 100)
    if len(rd) != len(rv):
        print("TRACK COUNT DIFFERS: %d vs %d" % (len(rd), len(rv)))
    keys = lambda r: (r[1], r[2], r[5])  # bt, usage, boneId
    vd = {keys(r): r for r in rd}
    vv = {keys(r): r for r in rv}
    for k in sorted(set(vd) | set(vv), key=lambda k: (k[2], k[1], k[0])):
        a, b = vd.get(k), vv.get(k)
        if a and not b:
            print("  ONLY-DEPLOYED track bt=%d usage=%d bone=%d" % k); continue
        if b and not a:
            print("  ONLY-VANILLA  track bt=%d usage=%d bone=%d" % k); continue
        # compare jointType, 205, weight, bsize/keycount, basis, lerps
        diffs = []
        if a[3] != b[3]: diffs.append("jointType %d!=%d" % (a[3], b[3]))
        if a[4] != b[4]: diffs.append("byte205 %d!=%d" % (a[4], b[4]))
        if abs(a[6]-b[6]) > 1e-6: diffs.append("weight %g!=%g" % (a[6], b[6]))
        ka, kb = a[7]//KEY_SIZE.get(a[1], 1), b[7]//KEY_SIZE.get(b[1], 1)
        if a[1] != b[1]: diffs.append("bufType %d!=%d" % (a[1], b[1]))
        elif ka != kb: diffs.append("keycount %d!=%d" % (ka, kb))
        if any(abs(x-y) > 1e-4 for x, y in zip(a[9], b[9])): diffs.append("basis %s!=%s" % (a[9], b[9]))
        if bool(a[11]) != bool(b[11]): diffs.append("lerp presence %s!=%s" % (bool(a[11]), bool(b[11])))
        elif a[11] and any(abs(x-y) > 1e-4 for x, y in zip(a[11], b[11])):
            diffs.append("lerpvals %s!=%s" % (
                ["%.3g" % v for v in a[11]], ["%.3g" % v for v in b[11]]))
        if diffs:
            print("  track bone=%4d usage=%d bt=%2d: %s" % (k[2], k[1], k[0], "; ".join(diffs)))

def diff_timl(eid):
    DB, offsD = load(DEPLOYED)
    VB, offsV = load(VANILLA)
    toD = struct.unpack_from("<Q", DB, offsD[eid]+88)[0]
    toV = struct.unpack_from("<Q", VB, offsV[eid]+88)[0]
    def parse(buf, off):
        typesOffset, typeCount, ix0, ix1, animLen, loopStart, loopCtrl, label = \
            struct.unpack_from("<QQiiffiI", buf, off)
        out = {}
        for ti in range(typeCount):
            toff, tcount, tlHash, u0 = struct.unpack_from("<QQIi", buf, typesOffset + 24*ti)
            for xi in range(tcount):
                xoff, xcount, dtHash, dt = struct.unpack_from("<QQIi", buf, toff + 24*xi)
                kfs = []
                if xoff and xcount:
                    for k in range(xcount):
                        v = struct.unpack_from("<I", buf, xoff + 20*k)[0]
                        fr = struct.unpack_from("<f", buf, xoff + 20*k + 12)[0]
                        cL, cR, ftt, trans, easing = struct.unpack_from("<2ff2h", buf, xoff + 20*k + 12)
                        # careful: key layout = value4 bounce4 bounce4 frame4 interp2 easing2
                        fr2 = struct.unpack_from("<f", buf, xoff + 20*k + 12)[0]
                        tr = struct.unpack_from("<2h", buf, xoff + 20*k + 16)
                        kfs.append((round(fr2, 2), v, tr))
                out[(tlHash, dtHash)] = kfs
        return out
    D, V = parse(DB, toD), parse(VB, toV)
    print("=" * 100)
    print("TIML diff entry %d (deployed vs vanilla)" % eid)
    for k in sorted(set(D) | set(V)):
        a, b = D.get(k, []), V.get(k, [])
        if a != b:
            print("  hash %08X/%08X:" % k)
            print("    deployed: %s" % a)
            print("    vanilla : %s" % b)

if __name__ == "__main__":
    eid = int(sys.argv[1]) if len(sys.argv) > 1 else 345
    diff_entry(eid)
    diff_timl(eid)
