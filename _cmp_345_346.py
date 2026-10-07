# -*- coding: utf-8 -*-
"""Compare LMT motion+TIML structure for entries. Standalone (no Blender).
Correct key sizes from FreeKinetics LmtBuffers (bit-packed types decoded).
Scans ALL non-empty entries for invariant violations.
"""
import struct, sys
from collections import Counter

DEPLOYED = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt"
VANILLA  = r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\mot\w03_00\w03_00.lmt"
ORIGIN   = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00-origin.lmt"

TRACK_HDR = 48
KEY_SIZE = {1:12, 2:16, 3:16, 4:8, 5:4, 6:8, 7:4, 11:4, 12:4, 13:4, 14:6, 15:5}
LERPED = {4, 5, 7, 11, 12, 13, 14, 15}   # has lerpOffset
BUFFERED = {3, 4, 5, 6, 7, 11, 12, 13, 14, 15}
# frame field: (byte_offset, bit_offset_in_le_int(width), bit_width) or simple offset
SIMPLE_FRAME = {3: (12, "<I"), 4: (6, "<H"), 5: (3, "<B")}
BITPACKED = {6: 64, 7: 32, 11: 32, 12: 32, 13: 32, 14: 48, 15: 40}  # total bits; frame = low 4-8 bits
FRAME_BITS = {6: 8, 7: 4, 11: 4, 12: 4, 13: 4, 14: 4, 15: 4}

def key_frame(buf, boff, k, bt):
    ks = KEY_SIZE[bt]
    base = boff + ks * k
    if bt in SIMPLE_FRAME:
        off, fmt = SIMPLE_FRAME[bt]
        return struct.unpack_from(fmt, buf, base + off)[0]
    if bt in BITPACKED:
        n = BITPACKED[bt] // 8
        val = int.from_bytes(buf[base:base+n], "little")
        # fields taken low-first in order x,y,z,w; frame is LAST field = high bits
        return val >> (BITPACKED[bt] - FRAME_BITS[bt])
    return 1  # bt 1/2 keyless

def parse_header(buf):
    sig, version, entryCount = struct.unpack_from("<4shh", buf, 0)
    assert sig == b"LMT\x00", sig
    offsets = struct.unpack_from("<%dQ" % entryCount, buf, 16)
    return version, entryCount, offsets

def action(buf, off):
    (fcurveOffset, fcurveCount, frameCount, loopFrame) = struct.unpack_from("<QIIi", buf, off)
    flags, flags2 = buf[off+64], buf[off+67]
    (timlOffset,) = struct.unpack_from("<Q", buf, off+88)
    return dict(fcurveOffset=fcurveOffset, fcurveCount=fcurveCount, frameCount=frameCount,
                loopFrame=loopFrame, flags=flags, flags2=flags2, timlOffset=timlOffset)

def tracks(buf, a):
    out = []
    for i in range(a["fcurveCount"]):
        base = a["fcurveOffset"] + TRACK_HDR * i
        bt, usage, jt, b205, boneId, weight, bsize = struct.unpack_from("<4Bifi", buf, base)
        boff, = struct.unpack_from("<Q", buf, base+16)
        loff, = struct.unpack_from("<Q", buf, base+40)
        out.append(dict(bt=bt, usage=usage, boneId=boneId, bsize=bsize, boff=boff, loff=loff, i=i))
    return out

def parse_timl(buf, off):
    typesOffset, typeCount, ix0, ix1, animLen, loopStart, loopCtrl, label = \
        struct.unpack_from("<QQiiffiI", buf, off)
    d = dict(typesOffset=typesOffset, typeCount=typeCount, animLen=animLen,
             loopStart=loopStart, loopCtrl=loopCtrl, label=label, types=[])
    if typesOffset == 0 or typeCount == 0:
        return d
    for ti in range(typeCount):
        toff, tcount, tlHash, u0 = struct.unpack_from("<QQIi", buf, typesOffset + 24*ti)
        typ = dict(hash=tlHash, u0=u0, transforms=[])
        if toff:
            for xi in range(tcount):
                xoff, xcount, dtHash, dt = struct.unpack_from("<QQIi", buf, toff + 24*xi)
                kfs = []
                if xoff and xcount:
                    for k in range(xcount):
                        frame = struct.unpack_from("<f", buf, xoff + 20*k + 12)[0]
                        kfs.append(frame)
                typ["transforms"].append(dict(hash="unk%08X" % dtHash, n=xcount, dt=dt, kfs=kfs))
        d["types"].append(typ)
    return d

def check_entry(buf, eid, off, verbose=False):
    """Return (summary_lines, problems)."""
    lines, probs = [], []
    a = action(buf, off)
    lines.append("entry %d @0x%X frameCount=%d loopFrame=%d fcurves=%d flags=%d flags2=%d timl=0x%X" %
                 (eid, off, a["frameCount"], a["loopFrame"], a["fcurveCount"], a["flags"], a["flags2"], a["timlOffset"]))
    if a["fcurveCount"] == 0:
        lines.append("  (no motion tracks)")
    trk = tracks(buf, a)
    usage_ctr = Counter()
    maxframe = -1
    for t in trk:
        tag = "track%d[bone=%d usage=%d bt=%d]" % (t["i"], t["boneId"], t["usage"], t["bt"])
        usage_ctr[(t["usage"], t["boneId"])] += 1
        ks = KEY_SIZE.get(t["bt"])
        if ks is None:
            probs.append("%s UNKNOWN bufferType" % tag); continue
        if t["bsize"] % ks != 0:
            probs.append("%s bufferSize=%d NOT divisible by keySize %d" % (tag, t["bsize"], ks))
        if t["bt"] in LERPED and t["loff"] == 0:
            probs.append("%s lerped type but lerpOffset=0" % tag)
        if t["bt"] not in LERPED and t["loff"] != 0:
            probs.append("%s non-lerped type but lerpOffset=0x%X" % (tag, t["loff"]))
        if t["boff"]:
            if t["boff"] + t["bsize"] > len(buf):
                probs.append("%s buffer OOB 0x%X+%d" % (tag, t["boff"], t["bsize"]))
                continue
            if t["bt"] in SIMPLE_FRAME or t["bt"] in BITPACKED:
                n = t["bsize"] // ks
                last = None
                for k in range(n):
                    fr = key_frame(buf, t["boff"], k, t["bt"])
                    if last is not None and fr < last:
                        probs.append("%s UNORDERED frames (f%d after f%d)" % (tag, fr, last))
                    last = fr
                if last is not None:
                    maxframe = max(maxframe, last)
    if a["frameCount"] > 0 and maxframe > a["frameCount"]:
        probs.append("max keyframe %d exceeds frameCount %d" % (maxframe, a["frameCount"]))
    if a["loopFrame"] >= 0 and a["loopFrame"] > a["frameCount"]:
        probs.append("loopFrame %d > frameCount %d" % (a["loopFrame"], a["frameCount"]))
    if a["timlOffset"]:
        timl = parse_timl(buf, a["timlOffset"])
        lines.append("  TIML animLen=%.3f loopStart=%.3f loopCtrl=%d types=%d" %
                     (timl["animLen"], timl["loopStart"], timl["loopCtrl"], timl["typeCount"]))
        if timl["animLen"] <= 0 and timl["typeCount"] > 0:
            probs.append("TIML animLen=%.3f with %d types" % (timl["animLen"], timl["typeCount"]))
        for typ in timl["types"]:
            if verbose:
                lines.append("    type %08X u0=%d: %s" % (typ["hash"], typ["u0"],
                            ", ".join("%s n=%d" % (t["hash"], t["n"]) for t in typ["transforms"])))
            for t in typ["transforms"]:
                if t["kfs"]:
                    if any(t["kfs"][i] < t["kfs"][i-1] - 1e-4 for i in range(1, len(t["kfs"]))):
                        probs.append("TIML %s/%s UNORDERED keys %s" % (typ["name"], t["hash"], t["kfs"]))
                    if timl["animLen"] and max(t["kfs"]) > timl["animLen"] + 0.011:
                        probs.append("TIML %s/%s key f%.2f beyond animLen %.3f" %
                                     (typ["name"], t["hash"], max(t["kfs"]), timl["animLen"]))
    else:
        lines.append("  TIML: none")
    dup = {k: v for k, v in usage_ctr.items() if v > 3}
    if dup:
        probs.append("duplicate (usage,boneId)>3 channels: %s" % dup)
    return lines, probs

def scan_all(path, only=None):
    with open(path, "rb") as f:
        buf = f.read()
    ver, cnt, offs = parse_header(buf)
    name = path.replace("\\", "/").split("/")[-1]
    print("### %s version=%d entries=%d size=%d" % (name, ver, cnt, len(buf)))
    bad = 0
    for eid in range(cnt):
        if only is not None and eid not in only:
            continue
        if not offs[eid]:
            continue
        lines, probs = check_entry(buf, eid, offs[eid])
        if probs:
            bad += 1
            print("\n".join(lines))
            for p in probs:
                print("  PROBLEM: %s" % p)
    print("  => %d entries with problems" % bad)

if __name__ == "__main__":
    focus = set(range(338, 347))
    for path in (DEPLOYED, VANILLA, ORIGIN):
        scan_all(path, focus)
        print()
    print("=" * 30 + " FULL SCAN of deployed file " + "=" * 30)
    scan_all(DEPLOYED, None)
