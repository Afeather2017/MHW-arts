# -*- coding: utf-8 -*-
"""Build a clean base LMT: vanilla w03_00-origin as the base, overlaying the
valor conversions from the deployed LMT.

TIML transplant = CANONICAL CONTIGUOUS REBUILD: [TIML_Data 40B][types array
immediately after (align16)][per-type transform arrays][per-transform key
arrays], all pointers recomputed. FreeKinetics' marshallTIML reads the types
array sequentially at align16(timlOffset+40) - a non-contiguous (relocated)
TIML derails it (this exact bug caused the original import crash and the
in-game save-load crash). The game follows pointers, so canonical works for
both.

fcurve transplant = per-bone intervals: the bone-header array at fcurveOffset
plus each bone's key buffer [bufferOffset, +bufferSize) and lerp region as
separate intervals, with the bone's bufferOffset/lerpOffset fields patched.

Overlay = documented valor conversions. Everything else stays 100% vanilla
origin, undoing accidental built-in swaps from the old blend.

Usage: python build_clean_base_lmt.py   (re-run after every blend export)
"""
import struct
import sys

sys.path.insert(0, r"E:\MHW-arts")
import dump_timl_frames as D

VANILLA = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00-origin.lmt"
DEPLOYED = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt"
OUT = r"E:\MHW-arts\w03_00-clean-base.lmt"

OVERLAY_ENTRIES = [
    103, 140, 141, 142, 143, 306, 312, 338, 341,
    339, 340, 343,
    357, 358, 359, 360, 361, 362, 363, 364, 365, 366, 367,
]


def align16(p):
    return p + ((-p) % 16)


def read_header(buf, off):
    return struct.unpack_from("<QIIi", buf, off)  # fcurveOffset, count, frames, loop


def rebuild_timl(dep, timl):
    """Canonical contiguous rebuild of one TIML block.
    Returns (block_bytes, new_typesOffset)."""
    typesOffset, typeCount = struct.unpack_from("<QQ", dep, timl)
    hdr = bytes(dep[timl:timl + 40])
    types = []
    for ti in range(typeCount):
        toff, tcount, h, u = struct.unpack_from("<QQIi", dep, typesOffset + 24 * ti)
        if not toff:
            types.append(dict(count=tcount, hash=h, unk=u, xfs=[], toff=0))
            continue
        xfs = []
        for xi in range(tcount):
            xoff, xcount, dh, dt = struct.unpack_from("<QQIi", dep, toff + 24 * xi)
            keys = bytes(dep[xoff:xoff + 20 * xcount]) if xoff else b""
            xfs.append(dict(count=xcount, hash=dh, dt=dt, keys=keys))
        types.append(dict(count=tcount, hash=h, unk=u, xfs=xfs, toff=toff))

    types_pos = align16(40)               # their parser align16s after the 40B header!
    size = types_pos + 24 * typeCount + sum(24 * t["count"] + sum(20 * x["count"] for x in t["xfs"])
                                     for t in types)
    out = bytearray(size)
    out[0:40] = hdr
    cursor = types_pos + 24 * typeCount
    for ti, t in enumerate(types):
        struct.pack_into("<QQIi", out, types_pos + 24 * ti,
                         cursor if t["xfs"] else 0, t["count"], t["hash"], t["unk"])
        if not t["xfs"]:
            continue
        tpos = cursor
        cursor += 24 * t["count"]
        for xi, x in enumerate(t["xfs"]):
            if x["keys"]:
                struct.pack_into("<QQ", out, tpos + 24 * xi, cursor, x["count"])
                out[cursor:cursor + 20 * x["count"]] = x["keys"]
                cursor += 20 * x["count"]
            else:
                struct.pack_into("<QQ", out, tpos + 24 * xi, 0, 0)  # no key data
    struct.pack_into("<QQ", out, 0, types_pos, typeCount)
    return bytes(out), types_pos


def transplant_fcurves(van, dep, fo, fc):
    """Copy the bone-header array and each bone's key buffer/lerp region to
    van; patch the bone's bufferOffset/lerpOffset fields. Returns new fo."""
    hdr_pos = align16(len(van))
    delta = hdr_pos - fo
    van.extend(b"\x00" * (hdr_pos - len(van)))
    van += bytes(dep[fo:fo + 48 * fc])
    pos = hdr_pos
    for _ in range(fc):
        buf_size = struct.unpack_from("<i", van, pos + 12)[0]
        if buf_off := struct.unpack_from("<q", van, pos + 16)[0]:
            struct.pack_into("<q", van, pos + 16, buf_off + delta)
        if lerp := struct.unpack_from("<Q", van, pos + 40)[0]:
            struct.pack_into("<Q", van, pos + 40, lerp + delta)
        pos += 48
    return hdr_pos


def main():
    van = bytearray(open(VANILLA, "rb").read())
    dep = open(DEPLOYED, "rb").read()
    vn, va = D.parse_lmt(bytes(van))
    dn, da = D.parse_lmt(dep)

    for eid in OVERLAY_ENTRIES:
        assert eid in da, "entry %d missing from deployed" % eid
        dhead = bytearray(dep[da[eid]["offset"]:da[eid]["offset"] + 96])
        dfo, dfc = struct.unpack_from("<Qi", dep, da[eid]["offset"])

        # fcurves: per-bone interval transplant
        fo_new = transplant_fcurves(van, dep, dfo, dfc)
        dhead[0:8] = struct.pack("<Q", fo_new)

        # TIML: canonical contiguous rebuild
        timl_block, _ = rebuild_timl(dep, da[eid]["timlOffset"])
        timl_pos = align16(len(van))
        van.extend(b"\x00" * (timl_pos - len(van)))
        van += timl_block
        dhead[88:96] = struct.pack("<Q", timl_pos)

        if eid in va:
            hoff = va[eid]["offset"]
            van[hoff:hoff + 96] = dhead
        else:
            hpos = align16(len(van))
            van.extend(b"\x00" * (hpos - len(van)))
            van += dhead
            struct.pack_into("<Q", van, 16 + 8 * eid, hpos)
        print("entry %-4d: transplanted (fc@0x%X timl@0x%X %dB)" %
              (eid, fo_new, timl_pos, len(timl_block)))

    # trailing pad: FreeKinetics' bone-key parser overreads past the last entry
    van += bytes(262144)

    with open(OUT, "wb") as h:
        h.write(van)
    print("written:", OUT, len(van), "bytes (incl. 256KB tail pad)")

    # ---- verification ----
    buf = open(OUT, "rb").read()
    n, actions = D.parse_lmt(buf)
    assert n == vn, "entry count changed"
    for eid, act in actions.items():
        if not act["timlOffset"]:
            continue
        try:
            D.parse_timl(buf, act["timlOffset"])
        except Exception as ex:
            print("PARSE FAIL entry", eid, ex)
            raise
    for eid in OVERLAY_ENTRIES:
        assert actions[eid]["timlOffset"], "entry %d lost TIML" % eid
    print("verify: entries=%d all TIML parse OK" % n)


if __name__ == "__main__":
    main()
