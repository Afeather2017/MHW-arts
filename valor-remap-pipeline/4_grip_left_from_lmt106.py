"""STEP 4 - default every remap action to the LEFT-HAND grip from LMT::w03_00::106.

Per project decision (2026-09-27): MHRS-sourced actions are all left-hand-bound;
socket transform/state comes from native LMT::w03_00::106:
- MhBone_030 (sword socket, active): 106 verbatim - single key @f0,
  loc=(0.1008,-0.0404,0.5808), quat=(0.0365,0.0045,-0.0115,0.9993),
  scale=(1,2,1) = drawn/left. (Rise L_Weapon_00 carries the same offset -
  both engines share the LS left-hand grip convention.)
- MhBone_047 (right socket, inactive): constants from 106 frame-0 loc/quat,
  scale=(1,1,1). 106's own 047 curves ARE animated (that move has a two-hand
  moment) and are deliberately NOT replayed.

Idempotent (rewrites all 20 socket fcurves as constants). Run:
  VALOR_BLEND="E:/MHW-arts/MHW-Valor.blend" \
    /e/MHW-arts/tools/blender-4.3.2-windows-x64/blender.exe -b --python 4_grip_left_from_lmt106.py
"""
import bpy, os, sys, shutil, time

BLEND = os.environ.get("VALOR_BLEND", r"E:\MHW-arts\MHW-Valor.blend")
REF = "LMT::w03_00::106"
BONES = ("MhBone_030", "MhBone_047")
PROPS = (("location", 3), ("rotation_quaternion", 4), ("scale", 3))

bak = BLEND + ".bak-" + time.strftime("%Y%m%d-%H%M%S")
shutil.copyfile(BLEND, bak)
print("[backup] %s" % bak, flush=True)

bpy.ops.wm.open_mainfile(filepath=BLEND)
ref = bpy.data.actions.get(REF)
if ref is None:
    print("!!! FAIL: reference action %s missing" % REF); sys.exit(1)

def first_val(a, bone, prop, idx):
    fc = a.fcurves.find('pose.bones["%s"].%s' % (bone, prop), index=idx)
    if fc is None or not len(fc.keyframe_points):
        return None
    return fc.keyframe_points[0].co[1]

ref_vals = {}
missing = []
for b in BONES:
    for prop, n in PROPS:
        for i in range(n):
            v = first_val(ref, b, prop, i)
            if v is None:
                missing.append((b, prop, i))
            ref_vals[(b, prop, i)] = v
if missing:
    print("!!! FAIL: reference missing channels: %s" % missing); sys.exit(1)
print("[ref] 030 loc=%s quat=%s scale=%s" % (
    tuple(round(ref_vals[("MhBone_030", "location", i)], 4) for i in range(3)),
    tuple(round(ref_vals[("MhBone_030", "rotation_quaternion", i)], 4) for i in range(4)),
    tuple(round(ref_vals[("MhBone_030", "scale", i)], 2) for i in range(3))), flush=True)
assert abs(ref_vals[("MhBone_030", "scale", 1)] - 2.0) < 1e-6, "030.scaleY != 2 (not left-hand drawn)"
assert abs(ref_vals[("MhBone_047", "scale", 1)] - 1.0) < 1e-6, "047.scaleY != 1"

remaps = sorted([a for a in bpy.data.actions
                 if a.name.startswith("plw_") and a.name.endswith("_remap")],
                key=lambda a: a.name)
print("[load] remaps=%d" % len(remaps), flush=True)

def write_const(fc, frame, value):
    kp = fc.keyframe_points
    kp.clear()
    kp.add(1)
    k = kp[0]
    k.co = (frame, value)
    k.interpolation = 'BEZIER'
    k.handle_left_type = 'AUTO_CLAMPED'
    k.handle_right_type = 'AUTO_CLAMPED'
    fc.update()

created = 0
for n, a in enumerate(remaps, 1):
    for (b, prop, i), v in sorted(ref_vals.items()):
        path = 'pose.bones["%s"].%s' % (b, prop)
        fc = a.fcurves.find(path, index=i)
        if fc is None:
            fc = a.fcurves.new(path, index=i)
            created += 1
        write_const(fc, 0, v)
    if n % 40 == 0:
        print("[grip] %d/%d" % (n, len(remaps)), flush=True)
print("[grip] done, fcurves created=%d (expect 0 on re-runs)" % created, flush=True)

bad = []
for a in remaps:
    s30 = a.fcurves.find('pose.bones["MhBone_030"].scale', index=1)
    s47 = a.fcurves.find('pose.bones["MhBone_047"].scale', index=1)
    l30 = a.fcurves.find('pose.bones["MhBone_030"].location', index=0)
    ok = (s30 and len(s30.keyframe_points) == 1 and abs(s30.keyframe_points[0].co[1] - 2.0) < 1e-6
          and s47 and len(s47.keyframe_points) == 1 and abs(s47.keyframe_points[0].co[1] - 1.0) < 1e-6
          and l30 and abs(l30.keyframe_points[0].co[1] - ref_vals[("MhBone_030", "location", 0)]) < 1e-6
          and all(len(a.fcurves.find('pose.bones["%s"].%s' % (b, p), index=i).keyframe_points) == 1
                  for (b, p, i) in ref_vals))
    if not ok:
        bad.append(a.name)
print("[verify] bad=%d %s" % (len(bad), bad[:5]), flush=True)
if bad:
    print("!!! NOT SAVING"); sys.exit(1)

bpy.ops.wm.save_mainfile()
print("[done] saved:", BLEND, flush=True)
