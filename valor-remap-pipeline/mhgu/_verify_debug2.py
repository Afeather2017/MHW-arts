"""修好的验证: TIML = 掩掉块首 offset 字段后比 md5; motion = 逐组件找第一处差异。"""
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

def timl_md5_masked(timl):
    raw = bytearray(timl.serialize())
    raw[0:4] = b"\x00\x00\x00\x00"
    return hashlib.md5(bytes(raw)).hexdigest()

ok = bad = 0
badlist = []
for i, a in mod_e.items():
    if not a.timlOffset:
        continue
    if i not in exp_e or not exp_e[i].timlOffset:
        badlist.append((i, "missing")); bad += 1; continue
    if timl_md5_masked(a.timl) == timl_md5_masked(exp_e[i].timl):
        ok += 1
    else:
        ra, rb = a.timl.serialize(), exp_e[i].timl.serialize()
        n = min(len(ra), len(rb))
        d = [k for k in range(n) if ra[k] != rb[k]]
        badlist.append((i, "diff bytes=%d len=%d/%d first=%s" % (len(d), len(ra), len(rb), d[:6])))
        bad += 1
print("TIML masked-md5 ok=%d bad=%d %s" % (ok, bad, badlist[:10]))

def comps(lmt_action):
    out = []
    for b in lmt_action.bones:
        out.append((int(b.boneId), int(getattr(b, "bufferType", -1)), bool(getattr(b, "fold", False)),
                    tuple(round(float(x), 5) for x in tuple(b.basis)),
                    tuple(round(float(x), 5) for x in tuple(b.tail)) if hasattr(b, "tail") else None,
                    tuple(tuple((round(float(v), 5), round(float(t), 5)) for v, t in ch) for ch in b.channels())))
    return out

mok = mbad = 0
mbadlist = []
for i, dg in ((i, None) for i in van_e if van_e[i].fcurveOffset):
    if i not in exp_e or not exp_e[i].fcurveOffset:
        mbadlist.append((i, "missing")); mbad += 1; continue
    if comps(van_e[i]) == comps(exp_e[i]):
        mok += 1
    else:
        ca, cb = comps(van_e[i]), comps(exp_e[i])
        detail = "nbones=%d/%d" % (len(ca), len(cb))
        for bi, (x, y) in enumerate(zip(ca, cb)):
            if x[:3] != y[:3]:
                detail += " bone[%d] meta %s/%s" % (bi, x[:3], y[:3]); break
            if x[3] != y[3]:
                detail += " bone[%d](%d) basis %s/%s" % (bi, x[0], x[3], y[3]); break
            if x[4] != y[4]:
                detail += " bone[%d](%d) tail %s/%s" % (bi, x[0], x[4], y[4]); break
            if x[5] != y[5]:
                for c, (cx, cy) in enumerate(zip(x[5], y[5])):
                    if cx != cy:
                        kfd = next((k for k, (kx, ky) in enumerate(zip(cx, cy)) if kx != ky), None)
                        detail += " bone[%d](%d) ch[%d] nk=%d/%d kf[%s] %s/%s" % (
                            bi, x[0], c, len(cx), len(cy), kfd,
                            cx[kfd] if kfd is not None and kfd < len(cx) else None,
                            cy[kfd] if kfd is not None and kfd < len(cy) else None)
                        break
                else:
                    detail += " bone[%d](%d) ch count %d/%d" % (bi, x[0], len(x[5]), len(y[5]))
                break
        mbadlist.append((i, detail)); mbad += 1
print("MOTION comps ok=%d bad=%d" % (mok, mbad))
for x in mbadlist[:15]:
    print("   ", x)
