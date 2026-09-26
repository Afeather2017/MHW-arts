"""Inspect MHW-Valor.blend: remap actions, root channels, object curves, IK bones."""
import bpy, re

PATH = r"E:\MHW-arts\MHW-Valor.blend"
bpy.ops.wm.open_mainfile(filepath=PATH)

IK_BONES = {"MhBone_%d" % i for i in (247, 248, 249, 250, 251, 252, 253, 254)}

print("=== OBJECTS ===")
for ob in bpy.data.objects:
    extra = " bones=%d" % len(ob.data.bones) if ob.type == "ARMATURE" else ""
    ad = ob.animation_data.action.name if ob.animation_data and ob.animation_data.action else None
    print("  '%s' type=%s ad=%s%s" % (ob.name, ob.type, ad, extra))

acts = sorted(bpy.data.actions, key=lambda a: a.name)
print("=== ACTIONS total=%d ===" % len(acts))
remaps = [a for a in acts if a.name.endswith("_remap")]
plw    = [a for a in acts if a.name.startswith("plw_")]
native = [a for a in acts if a.name.startswith("LMT::")]
other  = [a.name for a in acts if not a.name.startswith(("plw_", "LMT::"))]
print("  remap=%d plw=%d LMT-native=%d other=%s" % (len(remaps), len(plw), len(native), other[:12]))

# naming pattern of remaps
names = [a.name for a in remaps]
print("  remap name samples:", names[:6])

M000 = 'pose.bones["MhBone_000"].location'

def fc_summary(a, path, animated_only=False):
    out = {}
    for f in a.fcurves:
        if f.data_path != path:
            continue
        vals = [k.co[1] for k in f.keyframe_points]
        if animated_only and max(vals) - min(vals) < 1e-5 and abs(vals[0]) < 1e-5:
            continue
        out[f.array_index] = (round(min(vals), 4), round(max(vals), 4), len(vals),
                              round(vals[0], 4))
    return dict(sorted(out.items()))

print("=== per-remap root/object state ===")
n_obj_loc = 0
n_m000xz_anim = 0
first_x_keys = {}
for a in remaps:
    obj_loc = [f for f in a.fcurves if not f.data_path.startswith("pose.bones[") and "location" in f.data_path]
    obj_all = sorted({f.data_path for f in a.fcurves if not f.data_path.startswith("pose.bones[")})
    if obj_loc:
        n_obj_loc += 1
    m000 = fc_summary(a, M000)
    if 0 in m000 and m000[0][1] - m000[0][0] > 1e-5:
        n_m000xz_anim += 1
        first_x_keys[a.name] = m000[0][3]
    if remaps.index(a) < 3 or a is remaps[-1]:
        print("  '%s' range=%.0f..%.0f fcurves=%d obj_paths=%s" % (a.name, a.frame_range[0], a.frame_range[1], len(a.fcurves), obj_all))
        print("     MhBone_000.loc: %s" % m000)
        # IK bone channels
        ik_hits = set()
        for f in a.fcurves:
            m = re.match(r'pose\.bones\["([^"]+)"\]', f.data_path)
            if m and m.group(1) in IK_BONES:
                ik_hits.add(m.group(1))
        print("     IK bone channels present: %s" % sorted(ik_hits))

print("=== tallies ===")
print("  remaps with object-level location fcurves: %d/%d" % (n_obj_loc, len(remaps)))
print("  remaps with animated MhBone_000 loc X: %d/%d" % (n_m000xz_anim, len(remaps)))
if first_x_keys:
    vals = sorted(first_x_keys.values())
    print("  first-key X values: min=%.4f max=%.4f nonzero=%d" % (vals[0], vals[-1], sum(1 for v in vals if abs(v) > 1e-6)))

print("=== native action structure (object channels reference) ===")
for nm in other:
    a = bpy.data.actions[nm]
    obj = [(f.data_path, f.array_index) for f in a.fcurves if not f.data_path.startswith("pose.bones[")]
    if obj:
        print("  '%s':" % nm)
        for dp, ai in sorted(set(obj)):
            f = next(x for x in a.fcurves if x.data_path == dp and x.array_index == ai)
            vals = [k.co[1] for k in f.keyframe_points]
            print("     %s[%d] keys=%d %.4f..%.4f first=%.4f" % (dp, ai, len(vals), min(vals), max(vals), vals[0]))
        m000 = fc_summary(a, M000)
        print("     MhBone_000.loc: %s" % m000)
        break

for nm in ("valor-sheath-after-being-attack",):
    a = bpy.data.actions.get(nm)
    if a:
        obj = sorted({(f.data_path, f.array_index) for f in a.fcurves if not f.data_path.startswith("pose.bones[")})
        print("  '%s': obj_channels=%s" % (nm, obj))
        print("     MhBone_000.loc: %s" % fc_summary(a, M000))

print("DONE")
