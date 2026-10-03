# Standalone MHW LMT/TIML dumper (no Blender needed).
# Parses LMT action headers + per-action TIML blocks following FreeHyperKinetics
# struct/Lmt.py + struct/TIML.py binary layout, resolves hashes via the FHK txt tables.
import struct, os, sys

FHK = r"E:\MHW-arts\research\MHW-Free-HyperKinetics\struct"

def load_names():
    timeline, datatypes = {}, {}
    with open(os.path.join(FHK, "TIMLTimelineParam.txt"), encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line: continue
            h, name = line.split(",", 1)
            timeline[int(h)] = name
    with open(os.path.join(FHK, "TIMLDatatypes.txt"), encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line: continue
            parts = line.split(",", 2)
            datatypes[int(parts[0])] = (parts[1], int(parts[2]))
    return timeline, datatypes

TIMELINE_NAMES, DATATYPE_NAMES = load_names()

def tname(h):  return TIMELINE_NAMES.get(h, "unk%08X" % h)
def dname(h):
    if h in DATATYPE_NAMES: return DATATYPE_NAMES[h]
    return ("unk%08X" % h, 2)

VALUE_FMT = {0: ("<i", lambda v: str(v)),      # signed int
             1: ("<I", lambda v: str(v)),      # uint
             2: ("<f", lambda v: "%.4g" % v),  # float
             3: ("<4B", lambda v: str(tuple(v))),  # color
             4: ("<I", lambda v: str(v))}      # bool

def align16(pos): return pos + ((-pos) % 16)

def parse_lmt(buf):
    sig, version, entryCount, unkn = struct.unpack_from("<4shh8s", buf, 0)
    assert sig == b"LMT\x00", sig
    offsets = struct.unpack_from("<%dQ" % entryCount, buf, 16)
    actions = {}
    for ix, off in enumerate(offsets):
        if not off: continue
        (fcurveOffset, fcurveCount, frameCount, loopFrame) = struct.unpack_from("<QIIi", buf, off)
        (timlOffset,) = struct.unpack_from("<Q", buf, off + 88)
        actions[ix] = dict(offset=off, fcurveOffset=fcurveOffset, frameCount=frameCount,
                           loopFrame=loopFrame, timlOffset=timlOffset)
    return entryCount, actions

def parse_timl(buf, timlOffset):
    # TIML_Data (32 bytes) - action.timlOffset points here (no "timl" header per action)
    typesOffset, typeCount, dataIx0, dataIx1, animLen, loopStart, loopCtrl, labelHash = \
        struct.unpack_from("<QQiiffiI", buf, timlOffset)
    out = dict(typesOffset=typesOffset, typeCount=typeCount, dataIx0=dataIx0, dataIx1=dataIx1,
               animationLength=animLen, loopStartPoint=loopStart, loopControl=loopCtrl,
               labelHash=labelHash, types=[])
    if typesOffset == 0 or typeCount == 0:
        return out
    for ti in range(typeCount):
        toff, tcount, tlHash, unkn0 = struct.unpack_from("<QQIi", buf, typesOffset + 24 * ti)
        typ = dict(hash=tlHash, name=tname(tlHash), unkn0=unkn0, transforms=[])
        if toff:
            for xi in range(tcount):
                xoff, xcount, dtHash, dataType = struct.unpack_from("<QQIi", buf, toff + 24 * xi)
                xf = dict(hash=dtHash, name=dname(dtHash)[0], dataType=dataType, keyframes=[])
                if xoff and xcount:
                    vfmt, vfn = VALUE_FMT.get(dataType, VALUE_FMT[2])
                    for ki in range(xcount):
                        val = struct.unpack_from(vfmt, buf, xoff + 20 * ki)
                        cL, cR, ft, trans, easing = struct.unpack_from("<2ff2h", buf, xoff + 20 * ki + struct.calcsize(vfmt))
                        xf["keyframes"].append(dict(value=vfn(val), controlL=cL, controlR=cR,
                                                    frame=ft, transition=trans, easing=easing))
                typ["transforms"].append(xf)
        out["types"].append(typ)
    return out

def dump_entry(buf, eid, act, out):
    lines = []
    lines.append("=" * 100)
    lines.append("LMT entry %d  frameCount=%d loopFrame=%d  timlOffset=0x%X" %
                 (eid, act["frameCount"], act["loopFrame"], act["timlOffset"]))
    timl = parse_timl(buf, act["timlOffset"])
    lines.append("  TIML data: animationLength=%.3f loopStartPoint=%.3f loopControl=%d dataIx=%d,%d labelHash=%08X" %
                 (timl["animationLength"], timl["loopStartPoint"], timl["loopControl"],
                  timl["dataIx0"], timl["dataIx1"], timl["labelHash"]))
    for typ in timl["types"]:
        lines.append("  [%s]" % typ["name"])
        for xf in typ["transforms"]:
            kfs = "  ".join("f%.1f=%s" % (k["frame"], k["value"]) for k in xf["keyframes"])
            lines.append("    %-40s dataType=%d  %s" % (xf["name"], xf["dataType"], kfs))
    print("\n".join(lines), file=out)

def main():
    targets = [
        (r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt",
         [101, 140, 307, 308]),
        (r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00-rs.lmt",
         [115]),
    ]
    outdir = r"E:\MHW-arts\timl_dump"
    os.makedirs(outdir, exist_ok=True)
    scan = "--scan" in sys.argv
    for path, entries in targets:
        with open(path, "rb") as f:
            buf = f.read()
        entryCount, actions = parse_lmt(buf)
        base = os.path.basename(path)
        print("%s: entryCount=%d, actions with timl: %d" %
              (base, entryCount, sum(1 for a in actions.values() if a["timlOffset"])))
        outpath = os.path.join(outdir, base + ".timl.txt")
        with open(outpath, "w", encoding="utf-8") as out:
            if scan:
                # full-file property inventory + entries using defense-related props
                interesting = {"mNoHitTime", "mSuperArmorTime", "mHyperArmorTime", "mTurnAng",
                               "mTurnAngFixed", "mTurnSpeed", "mTurnBaseDir"}
                hits = {}
                for eid, act in sorted(actions.items()):
                    if not act["timlOffset"]: continue
                    timl = parse_timl(buf, act["timlOffset"])
                    for typ in timl["types"]:
                        for xf in typ["transforms"]:
                            key = "%s :: %s" % (typ["name"], xf["name"])
                            hits.setdefault(key, []).append(eid)
                with open(os.path.join(outdir, base + ".inventory.txt"), "w", encoding="utf-8") as inv:
                    for key in sorted(hits):
                        mark = "  <== DEFENSE" if any(i in key for i in interesting) else ""
                        print("%-90s entries: %s%s" % (key, hits[key], mark), file=inv)
                print("  wrote inventory:", os.path.join(outdir, base + ".inventory.txt"))
            for eid in entries:
                if eid not in actions:
                    print("entry %d missing in %s" % (eid, base)); continue
                dump_entry(buf, eid, actions[eid], out)
        print("  wrote", outpath)

if __name__ == "__main__":
    main()
