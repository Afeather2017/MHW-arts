"""Standalone action splicer: append action B after action A into a NEW action.

Paste into Blender's Text Editor (edit the CONFIG block) and run, or headless:
    blender -b file.blend -P splice_actions.py

Why IK needs NO rework (project convention, see bones.md):
- horizontal travel lives on OBJECT-level location curves (first key = 0);
- MhBone_000 loc X/Z are flat 0, loc Y = height;
- IK bones 247-253 are baked in pose space (travel-free).
So a splice only needs (a) time-shifting B to A's end and (b) offsetting B's
object location by A's end location. IK and all bone channels just ride along
with the time shift. Set REFRESH_IK = True ONLY for actions NOT produced by
this convention (then FreeKinetics re-bakes 247-253 on the combined action).
"""
import bpy

# ============================ EDIT ME ============================
ACTION_A   = "plw_LongSword_100_001_Loop_Armature_remap"        # first action
ACTION_B   = "plw_LongSword_100_001_Loop.001_Armature_remap"    # appended after A
ACTION_OUT = ""            # "" -> "<A>+<B>"
ARMATURE   = "m_mesh Armature"
SEAM_GAP   = 0             # 0 = B's frame0 lands on A's last frame (poses match there)
REFRESH_IK = False         # True -> re-bake IK 247-253 via FreeKinetics after merging
# =================================================================

A = bpy.data.actions.get(ACTION_A)
B = bpy.data.actions.get(ACTION_B)
assert A is not None, "action A not found: " + ACTION_A
assert B is not None, "action B not found: " + ACTION_B

a_lo, a_hi = A.frame_range
b_lo, b_hi = B.frame_range
shift = (a_hi - b_lo) + SEAM_GAP

# offset = A's end object location - B's start object location (per axis)
off = []
for i in range(3):
    fa = A.fcurves.find("location", index=i)
    fb = B.fcurves.find("location", index=i)
    off.append((fa.evaluate(a_hi) if fa else 0.0) - (fb.evaluate(b_lo) if fb else 0.0))

print("[splice] A %.0f..%.0f  B %.0f..%.0f  shift=%+.0f  objOffset=(%.4f, %.4f, %.4f)" % (
    a_lo, a_hi, b_lo, b_hi, shift, off[0], off[1], off[2]))

out = bpy.data.actions.new(ACTION_OUT or (ACTION_A + "_plus"))
_groups = {}

def group_ref(name):
    if name is None:
        return None
    if name not in _groups:
        _groups[name] = out.groups.new(name)
    return _groups[name]

def read_keys(fc):
    return [(tuple(k.co), tuple(k.handle_left), tuple(k.handle_right),
             k.interpolation, k.handle_left_type, k.handle_right_type)
            for k in fc.keyframe_points]

def write_keys(fc, keys):
    kp = fc.keyframe_points
    kp.clear()
    kp.add(len(keys))
    for k, (co, hl, hr, interp, hlt, hrt) in zip(kp, keys):
        k.co = co
        k.handle_left = hl
        k.handle_right = hr
        k.interpolation = interp
        k.handle_left_type = hlt
        k.handle_right_type = hrt
    fc.update()

def index_curves(act):
    return {(f.data_path, f.array_index): f for f in act.fcurves}

ia, ib = index_curves(A), index_curves(B)
warnings = []
for key in sorted(set(ia) | set(ib), key=lambda k: (k[0], k[1])):
    path, idx = key
    fa, fb = ia.get(key), ib.get(key)
    ka = read_keys(fa) if fa else []
    is_obj_loc = (path == "location")
    dy = off[idx] if is_obj_loc else 0.0
    tb = []
    if fb:
        for (co, hl, hr, interp, hlt, hrt) in read_keys(fb):
            tb.append(((co[0] + shift, co[1] + dy),
                       (hl[0] + shift, hl[1] + dy),
                       (hr[0] + shift, hr[1] + dy),
                       interp, hlt, hrt))
    # drop B keys that exactly duplicate A's seam key (same frame AND value)
    if ka and tb:
        a_last_x = max(k[0][0] for k in ka)
        kept = []
        for t in tb:
            dup = [k for k in ka if abs(k[0][0] - t[0][0]) < 1e-4]
            if dup:
                if all(abs(k[0][1] - t[0][1]) > 1e-4 for k in dup):
                    warnings.append((path, idx, round(t[0][0], 2)))  # seam value mismatch
                continue
            if t[0][0] <= a_last_x:
                warnings.append((path, idx, round(t[0][0], 2)))  # overlapping, not duplicate
            kept.append(t)
        tb = kept
    src = fa or fb
    grp_name = src.group.name if (src and src.group) else None
    if grp_name:
        fc = out.fcurves.new(path, index=idx, action_group=grp_name)
    else:
        fc = out.fcurves.new(path, index=idx)
    write_keys(fc, ka + tb)

out.use_fake_user = True
ob = bpy.data.objects.get(ARMATURE)
if ob is not None:
    ob.animation_data_create()
    ob.animation_data.action = out

print("[splice] '%s': frames %.0f..%.0f, fcurves=%d, warnings=%d" % (
    out.name, out.frame_range[0], out.frame_range[1], len(out.fcurves), len(warnings)))
for w in warnings[:8]:
    print("  seam-warn %s[%d] @f%s" % w)
if ob is not None:
    print("[splice] assigned to '%s' - scrub to f%.0f.. to check the seam" % (ARMATURE, a_hi))

if REFRESH_IK:
    import importlib
    tether = importlib.import_module("MHW-FreeKinetics.blender.tetherOps")
    dst = bpy.data.objects[ARMATURE]
    scene = bpy.context.scene
    scene.frame_set(0)
    bpy.context.view_layer.update()
    tether.recalculateBodyIKBones(dst, out, [(247, 57, 'FULL'), (248, 40, 'FULL'),
                                             (249, 20, 'FULL'), (250, 16, 'FULL'),
                                             (252, 4, 'LOC')])
    tether.recalculateGroundIKBones(dst, out)
    print("[splice] IK re-baked on '%s'" % out.name)
