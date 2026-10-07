# -*- coding: utf-8 -*-
"""Delta-aware LMT motion validator + TIML differ. Standalone.
Frame fields are per-key DELTAS: bt3 u32, bt4 u16, bt5 u8, bitpacked (6:8bit, 7/11-15:4bit).
"""
import struct, sys
from collections import Counter

DEPLOYED = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt"
VANILLA  = r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\mot\w03_00\w03_00.lmt"

TRACK_HDR = 48
KEY_SIZE = {1:12, 2:16, 3:16, 4:8, 5:4, 6:8, 7:4, 11:4, 12:4, 13:4, 14:6, 15:5}
BITPACKED = {6: (64, 8), 7: (32, 4), 11: (32, 4), 12: (32, 4), 13: (32, 4), 14: (48, 4), 15: (40, 4)}
SIMPLE_FRAME = {3: (12, "<I"), 4: (6, "<H"), 5: (3, "<B")}

def deltas(buf, boff, n, bt):
    out = []
    ks = KEY_SIZE[bt]
    for k in range(n):
        base = boff + ks*k
        if bt in SIMPLE_FRAME:
            off, fmt = SIMPLE_FRAME[bt]
            out.append(struct.unpack_from(fmt, buf, base+off)[0])
        elif bt in BITPACKED:
            bits, fbits = BITPACKED[bt]
            val = int.from_bytes(buf[base:base+bits//8], "little")
            out.append(val >> (bits - fbits))
    return out

def parse_header(buf):
    sig, version, entryCount = struct.unpack_from("<4shh", buf, 0)
    offsets = struct.unpack_from("<%dQ" % entryCount, buf, 16)
    return version, entryCount, offsets

def entry_info(buf, eid, off):
    (fcurveOffset, fcurveCount, frameCount, loopFrame) = struct.unpack_from("<QIIi", buf, off)
    (timlOffset,) = struct.unpack_from("<Q", buf, off+88)
    probs = []
    trk = []
    for i in range(fcurveCount):
        base = fcurveOffset + 48*i
        bt, usage, jt, b205, boneId, weight, bsize = struct.unpack_from("<4Bifi", buf, base)
        boff, = struct.unpack_from("<Q", buf, base+16)
        loff, = struct.unpack_from("<Q", buf, base+40)
        trk.append((i, bt, usage, boneId, bsize, boff, loff))
    for (i, bt, usage, boneId, bsize, boff, loff) in trk:
        ks = KEY_SIZE.get(bt)
        if ks is None:
            probs.append("t%d unknown bt=%d" % (i, bt)); continue
        if bsize % ks:
            probs.append("t%d bone%d u%d bt%d bsize=%d %% %d" % (i, boneId, usage, bt, bsize, ks))
        if boff + bsize > len(buf):
            probs.append("t%d OOB" % i); continue
        n = bsize // ks
        if bt in SIMPLE_FRAME or bt in BITPACKED:
            ds = deltas(buf, boff, n, bt)
            cum = 0
            for d in ds:
                cum += d
                if cum > frameCount + 1:
                    probs.append("t%d bone%d u%d bt%d: cumulative frame %d > frameCount %d" %
                                 (i, boneId, usage, bt, cum, frameCount))
                    break
            maxd = max(ds) if ds else 0
            fbits = BITPACKED[bt][1] if bt in BITPACKED else {"4": 16, "5": 8, "3": 32}.get(str(bt), 32)
            if maxd > (1 << fbits) - 1:
                probs.append("t%d bone%d u%d bt%d: delta %d overflows %d-bit field" %
                             (i, boneId, usage, bt, maxd, fbits))
    has_root = any(boneId == -1 for (_, _, _, boneId, _, _, _) in trk)
    return dict(frameCount=frameCount, loopFrame=loopFrame, n=fcurveCount,
                probs=probs, has_root=has_root, trk=trk, timlOffset=timlOffset)

def parse_timl(buf, off):
    typesOffset, typeCount, ix0, ix1, animLen, loopStart, loopCtrl, label = \
        struct.unpack_from("<QQiiffiI", buf, off)
    types = []
    if typesOffset and typeCount:
        for ti in range(typeCount):
            toff, tcount, tlHash, u0 = struct.unpack_from("<QQIi", buf, typesOffset + 24*ti)
            xfs = []
            if toff:
                for xi in range(tcount):
                    xoff, xcount, dtHash, dt = struct.unpack_from("<QQIi", buf, toff + 24*xi)
                    kfs = []
                    if xoff and xcount:
                        for k in range(xcount):
                            v = struct.unpack_from("<I", buf, xoff + 20*k)[0]
                            fr = struct.unpack_from("<f", buf, xoff + 20*k + 12)[0]
                            kfs.append((round(fr, 2), v))
                    xfs.append(("unk%08X" % dtHash, dt, kfs))
            types.append(("unk%08X" % tlHash, u0, xfs))
    return dict(animLen=animLen, loopStart=loopStart, loopCtrl=loopCtrl, types=types)

with open(DEPLOYED, "rb") as f: DB = f.read()
with open(VANILLA, "rb") as f: VB = f.read()

_, cntD, offsD = parse_header(DB)
_, cntV, offsV = parse_header(VB)

print("== vanilla entries 338-346 ==")
for eid in range(338, 347):
    if offsV[eid]:
        (fo, fc, frm, lf) = struct.unpack_from("<QIIi", VB, offsV[eid])
        (to,) = struct.unpack_from("<Q", VB, offsV[eid]+88)
        print("vanilla entry %d: frameCount=%d fcurves=%d timl=%s" % (eid, frm, fc, bool(to)))
    else:
        print("vanilla entry %d: EMPTY" % eid)

print("\n== deployed 338-346 delta validation ==")
for eid in range(338, 347):
    if not offsD[eid]:
        print("entry %d EMPTY" % eid); continue
    info = entry_info(DB, eid, offsD[eid])
    print("entry %d frameCount=%d tracks=%d root=%s probs=%d" %
          (eid, info["frameCount"], info["n"], info["has_root"], len(info["probs"])))
    for p in info["probs"][:8]:
        print("   PROBLEM:", p)

print("\n== TIML 345 vs 346 (deployed) ==")
for eid in (345, 346):
    (to,) = struct.unpack_from("<Q", DB, offsD[eid]+88)
    t = parse_timl(DB, to)
    print("--- entry %d animLen=%.3f loopStart=%.1f loopCtrl=%d types=%d" %
          (eid, t["animLen"], t["loopStart"], t["loopCtrl"], len(t["types"])))
    for tl, u0, xfs in t["types"]:
        print("  type %s u0=%d" % (tl, u0))
        for name, dt, kfs in xfs:
            print("    %s dt=%d keys=%d %s" % (name, dt, len(kfs), kfs[:10]))
