"""定位 vanilla-reset 导出与源 LMT 的第一处差异: TIML 字节级 + motion 结构级。"""
import bpy, importlib, hashlib

LMT_MOD = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt"
LMT_VAN = r"E:\MHW-arts\chunk-extracted\chunk\hm\wp\wp03\mot\w03_00\w03_00.lmt"
SCRATCH = r"E:\MHW-arts\valor-remap-pipeline\mhgu\_vanilla_reset_export.lmt"

Lmt = importlib.import_module("MHW-FreeKinetics.struct.Lmt")
mod = Lmt.parseLMT(LMT_MOD)
van = Lmt.parseLMT(LMT_VAN)
exp = Lmt.parseLMT(SCRATCH)
mod_e = {a.id: a for a in mod.ActionHeaders}
van_e = {a.id: a for a in van.ActionHeaders}
exp_e = {a.id: a for a in exp.ActionHeaders}

def dump(label, data, off, ln=48):
    seg = data[off:off+ln]
    print(label, " ".join("%02x" % b for b in seg))

# ---- TIML byte diff on entry 10 ----
for eid in (10, 1):
    a, b = mod_e[eid], exp_e[eid]
    ta, tb = a.timl.serialize(), b.timl.serialize()
    print("== entry", eid, "timl len mod/exp:", len(ta), len(tb))
    n = min(len(ta), len(tb))
    diffs = [i for i in range(n) if ta[i] != tb[i]]
    print("   first diffs:", diffs[:8], "total", len(diffs))
    if diffs:
        o = max(0, diffs[0] - 16)
        dump("   mod:", ta, o)
        dump("   exp:", tb, o)
    # structural field diff
    da, db = a.timl, b.timl
    print("   dataHeaders:", len(da.dataHeaders) if hasattr(da, "dataHeaders") else "?",
          len(db.dataHeaders) if hasattr(db, "dataHeaders") else "?")
    for ix, (xa, xb) in enumerate(zip(da.data, db.data)):
        fa = (xa.dataIx0, xa.dataIx1, int(xa.animationLength), int(xa.loopStartPoint), int(xa.loopControl))
        fb = (xb.dataIx0, xb.dataIx1, int(xb.animationLength), int(xb.loopStartPoint), int(xb.loopControl))
        if fa != fb:
            print("   data[%d] header fields mod/exp:" % ix, fa, fb)
        print("   data[%d] types: %d vs %d" % (ix, len(xa.types), len(xb.types)))
        for ty, (ya, yb) in enumerate(zip(xa.types, xb.types)):
            pa = (ya.timelineParameterHash, ya.unkn0 if hasattr(ya, "unkn0") else None, len(ya.transforms))
            pb = (yb.timelineParameterHash, yb.unkn0 if hasattr(yb, "unkn0") else None, len(yb.transforms))
            if pa != pb:
                print("     type[%d] mod/exp:" % ty, pa, pb)
            for tf, (za, zb) in enumerate(zip(ya.transforms, yb.transforms)):
                if za.datatypeHash != zb.datatypeHash or len(za.keyframes) != len(zb.keyframes):
                    print("     type[%d] tf[%d] mod:" % (ty, tf),
                          za.datatypeHash, len(za.keyframes), za.dataType,
                          "exp:", zb.datatypeHash, len(zb.keyframes), zb.dataType)
                    break
            else:
                continue
            break
        break

# ---- motion digest component diff on entry 10 ----
def comps(lmt_action):
    out = []
    for b in lmt_action.bones:
        out.append({
            "boneId": int(b.boneId), "bufferType": int(getattr(b, "bufferType", -1)),
            "fold": bool(getattr(b, "fold", False)),
            "basis": tuple(round(float(x), 4) for x in tuple(b.basis)),
            "tail": tuple(round(float(x), 4) for x in tuple(b.tail)) if hasattr(b, "tail") else None,
            "nch": None,
            "chans": [tuple((round(float(v), 4), round(float(t), 4)) for v, t in ch) for ch in b.channels()],
        })
    return out

eid = 10
ca, cb = comps(van_e[eid]), comps(exp_e[eid])
print("== entry", eid, "motion bones van/exp:", len(ca), len(cb))
for i, (x, y) in enumerate(zip(ca, cb)):
    if x["boneId"] != y["boneId"] or x["bufferType"] != y["bufferType"] or x["fold"] != y["fold"]:
        print("  bone[%d] meta van/exp:" % i, {k: x[k] for k in ("boneId", "bufferType", "fold")},
              {k: y[k] for k in ("boneId", "bufferType", "fold")})
        break
    if x["basis"] != y["basis"]:
        print("  bone[%d] boneId=%d basis van/exp:" % (i, x["boneId"]), x["basis"], y["basis"])
        break
    if x["tail"] != y["tail"]:
        print("  bone[%d] boneId=%d tail van/exp:" % (i, x["boneId"]), x["tail"], y["tail"])
        break
    if x["chans"] != y["chans"]:
        for c, (cx, cy) in enumerate(zip(x["chans"], y["chans"])):
            if cx != cy:
                print("  bone[%d] boneId=%d chan[%d] nk=%d/%d" % (i, x["boneId"], c, len(cx), len(cy)))
                for k, (kx, ky) in enumerate(zip(cx, cy)):
                    if kx != ky:
                        print("    first kf diff at", k, "van:", kx, "exp:", ky)
                        break
                break
        break
else:
    print("  per-bone scan: no component diff found (digest collision?)")
