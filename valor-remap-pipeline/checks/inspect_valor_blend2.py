"""Round 2: native object-curve reference + IK bake state + Z first keys."""
import bpy, re

PATH = r"E:\MHW-arts\MHW-Valor.blend"
bpy.ops.wm.open_mainfile(filepath=PATH)

IK_BONES = ("MhBone_247", "MhBone_248", "MhBone_249", "MhBone_250",
            "MhBone_251", "MhBone_252", "MhBone_253", "MhBone_254")
M000 = 'pose.bones["MhBone_000"].location'

def summ(a, path, idxs=None):
    out = {}
    for f in a.fcurves:
        if f.data_path != path:
            continue
        if idxs is not None and f.array_index not in idxs:
            continue
        vals = [k.co[1] for k in f.keyframe_points]
        out[f.array_index] = (round(min(vals), 4), round(max(vals), 4), len(vals), round(vals[0], 4))
    return dict(sorted(out.items()))

acts = sorted(bpy.data.actions, key=lambda a: a.name)
remaps = [a for a in acts if a.name.endswith("_remap")]
lmt = [a for a in acts if a.name.startswith("LMT::")]
print("LMT-native actions: %d, samples: %s" % (len(lmt), [a.name for a in lmt[:3]]))

print("=== native LMT action object-level channels ===")
shown = 0
for a in lmt:
    obj = sorted({(f.data_path, f.array_index) for f in a.fcurves if not f.data_path.startswith("pose.bones[")})
    bone_paths = sorted({f.data_path for f in a.fcurves if f.data_path.startswith("pose.bones[")})
    if obj and shown < 2:
        print("  '%s' obj_channels:" % a.name)
        for dp, ai in obj:
            f = next(x for x in a.fcurves if x.data_path == dp and x.array_index == ai)
            vals = [k.co[1] for k in f.keyframe_points]
            print("    %s[%d] keys=%d %.4f..%.4f first=%.4f interp=%s" % (
                dp.split(".")[-1] if "." in dp else dp, ai, len(vals), min(vals), max(vals), vals[0],
                f.keyframe_points[0].interpolation))
        print("    MhBone_000.loc: %s" % summ(a, M000))
        shown += 1
n_obj = sum(1 for a in lmt if any("location" in f.data_path and not f.data_path.startswith("pose.bones[") for f in a.fcurves))
print("  LMT actions with object location: %d/%d" % (n_obj, len(lmt)))

va = bpy.data.actions.get("valor-sheath-after-being-attack")
if va:
    obj = sorted({(f.data_path, f.array_index) for f in va.fcurves if not f.data_path.startswith("pose.bones[")})
    print("=== valor-sheath-after-being-attack ===")
    print("  obj_channels=%s" % (obj,))
    for dp, ai in obj:
        f = next(x for x in va.fcurves if x.data_path == dp and x.array_index == ai)
        vals = [k.co[1] for k in f.keyframe_points]
        print("    %s[%d] keys=%d %.4f..%.4f first=%.4f" % (dp, ai, len(vals), min(vals), max(vals), vals[0]))
    print("  MhBone_000.loc: %s" % summ(va, M000))

print("=== remap IK / socket state ===")
ik_flat = 0
sock = 0
z_nonzero_first = 0
x_nonzero_first = 0
for a in remaps:
    # IK bake check: 251/253 ground anchors should be constant at origin if baked
    f251 = a.fcurves.find('pose.bones["MhBone_251"].location', index=0)
    f253 = a.fcurves.find('pose.bones["MhBone_253"].location', index=0)
    baked = True
    for f in (f251, f253):
        if f is None:
            baked = False
            continue
        vals = [k.co[1] for k in f.keyframe_points]
        if max(abs(v) for v in vals) > 1e-5:
            baked = False
    if baked:
        ik_flat += 1
    if a.fcurves.find('pose.bones["MhBone_030"].location') and a.fcurves.find('pose.bones["MhBone_047"].location'):
        sock += 1
    fz = a.fcurves.find(M000, index=2)
    if fz and abs(fz.keyframe_points[0].co[1]) > 1e-6:
        z_nonzero_first += 1
    fx = a.fcurves.find(M000, index=0)
    if fx and abs(fx.keyframe_points[0].co[1]) > 1e-6:
        x_nonzero_first += 1

print("  remaps with 251/253 flat-at-origin (baked ground anchors): %d/%d" % (ik_flat, len(remaps)))
print("  remaps with 030+047 socket loc channels: %d/%d" % (sock, len(remaps)))
print("  remaps with nonzero FIRST KEY on MhBone_000 loc X: %d  loc Z: %d" % (x_nonzero_first, z_nonzero_first))

a = remaps[0]
print("=== sample remap IK channel values (%s) ===" % a.name)
for b in IK_BONES:
    for kind in ("location",):
        s = summ(a, 'pose.bones["%s"].%s' % (b, kind))
        if s:
            print("  %s %s: %s" % (b, kind, s))
# channel census
import collections
cat = collections.Counter()
for f in a.fcurves:
    if f.data_path.startswith("pose.bones["):
        b = f.data_path.split('"')[1]
        prop = f.data_path.split(".")[-1]
        if b in IK_BONES:
            cat["IK:" + prop] += 1
        elif b in ("MhBone_030", "MhBone_047"):
            cat["SOCKET:" + prop] += 1
        elif b == "MhBone_000":
            cat["ROOT:" + prop] += 1
        else:
            cat["body:" + prop] += 1
    else:
        cat["OBJECT"] += 1
print("  channel census:", dict(cat))

# root loc keyframes: same frames across idx? interp?
print("=== MhBone_000 loc key structure (%s) ===" % a.name)
for idx in (0, 1, 2):
    f = a.fcurves.find(M000, index=idx)
    if f:
        ks = f.keyframe_points
        print("  loc[%d] keys=%d interp=%s hL=%s hR=%s frames[0..3]=%s" % (
            idx, len(ks), ks[0].interpolation, ks[0].handle_left_type, ks[0].handle_right_type,
            [round(k.co[0], 2) for k in ks[:4]]))
print("DONE")
