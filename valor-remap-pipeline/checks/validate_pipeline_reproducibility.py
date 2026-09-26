"""Compare a pipeline-produced blend against the verified reference blend.

Usage: build a candidate via steps 2-4 (VALOR_BLEND=candidate), then
  VALOR_BLEND="E:/path/candidate.blend" blender -b --python checks/validate_pipeline_reproducibility.py
Exit-code 0 + REPRODUCIBLE=true means the candidate matches the reference
(E:\\MHW-arts\\MHW-Valor.blend, user-verified in game 2026-09-27) on every
name-matched plw_*_remap action.
"""
import bpy, os, sys

TEST = os.environ.get("VALOR_BLEND", r"E:\MHW-arts\MHW-Valor.blend")
REF = r"E:\MHW-arts\MHW-Valor.blend"
TOL = 1e-5

def metrics(a):
    m = {"nfcurves": len(a.fcurves)}
    def sig(path, idx):
        f = a.fcurves.find(path, index=idx)
        if f is None:
            return None
        ks = f.keyframe_points
        vals = [k.co[1] for k in ks]
        return (len(ks), round(vals[0], 6), round(vals[-1], 6),
                round(min(vals), 6), round(max(vals), 6))
    M000 = 'pose.bones["MhBone_000"].location'
    for idx in (0, 1):
        m["obj_loc%d" % idx] = sig("location", idx)
    for idx in (0, 2):
        m["bone_xz%d" % idx] = sig(M000, idx)
    for idx in range(3):
        m["ik249_loc%d" % idx] = sig('pose.bones["MhBone_249"].location', idx)
    for idx in range(3):
        m["ik252_loc%d" % idx] = sig('pose.bones["MhBone_252"].location', idx)
    for bn in ("MhBone_251", "MhBone_253"):
        f = a.fcurves.find('pose.bones["%s"].location' % bn, index=0)
        m[bn] = None if f is None else round(max(abs(k.co[1]) for k in f.keyframe_points), 6)
    m["grip030_y"] = sig('pose.bones["MhBone_030"].scale', 1)
    m["grip047_y"] = sig('pose.bones["MhBone_047"].scale', 1)
    m["sock030_x"] = sig('pose.bones["MhBone_030"].location', 0)
    return m

def load(path):
    bpy.ops.wm.open_mainfile(filepath=path)
    out = {}
    for a in bpy.data.actions:
        if a.name.startswith("plw_") and a.name.endswith("_remap"):
            out[a.name] = metrics(a)
    return out

ref = load(REF)
test = load(TEST)
print("reference=%d candidate=%d remap actions" % (len(ref), len(test)), flush=True)

missing = sorted(set(ref) - set(test))
extra = sorted(set(test) - set(ref))
diffs = []
for name in sorted(set(ref) & set(test)):
    for k, v in ref[name].items():
        w = test[name][k]
        if v != w:
            if isinstance(v, tuple) and isinstance(w, tuple) and len(v) == len(w) \
               and all((x is None and y is None) or (x is not None and y is not None and abs(x - y) <= TOL)
                       for x, y in zip(v, w)):
                continue
            if isinstance(v, float) and isinstance(w, float) and abs(v - w) <= TOL:
                continue
            diffs.append((name, k, v, w))

print("missing_actions=%d extra_actions=%d value_diffs=%d" % (len(missing), len(extra), len(diffs)), flush=True)
for d in diffs[:10]:
    print("  DIFF %s %s ref=%s test=%s" % d, flush=True)
if missing:
    print("  missing:", missing[:5])
print("REPRODUCIBLE=%s" % ("false" if (missing or extra or diffs) else "true"), flush=True)
sys.exit(0 if not (missing or extra or diffs) else 1)
