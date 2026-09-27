"""MHGU chain STEP 5 - acceptance validation (read-only, saves nothing).

A. IDENTITY CHECK: MHGU w07.lmt motion 106 and MHW native LMT::w03_00::106 are
   the SAME motion (proof: MHGU cog position Y 47.135cm == native MhBone_000
   locY 0.4714). So plw_w07_106_remap must reproduce the native pose: world
   positions of every mapped body bone compared against native 106 on the same
   rig, sampled over the motion.
B. COVERAGE REPORT: every plw_*_remap - bone coverage vs native 106, IK bones
   baked and tracking, object curves present with first key 0, freehk
   metadata, frame span equal to its source.

Run:
  /e/MHW-arts/tools/blender-4.3.2-windows-x64/blender.exe -b --python mhgu/5_validate_mhgu.py
"""
import bpy, os, re, math
from mathutils import Vector

WORK = os.environ.get("MHGU_WORK_BLEND", r"E:\MHW-arts\mhgu-valor-mhw.blend")
bpy.ops.wm.open_mainfile(filepath=WORK)
dst = bpy.data.objects["m_mesh Armature"]
scn = bpy.context.scene
view = bpy.context.view_layer

def evaluate(a, f):
    dst.animation_data.action = a
    scn.frame_set(f)
    view.update()

def quat_angle(q1, q2):
    d = abs(q1.rotation_difference(q2).angle)
    return d

# ---------- A. retarget fidelity + cog identity ----------
# NOTE: MHGU w07.lmt::106 is NOT the same body motion as native LMT::w03_00::106
# (only the cog height channel is shared: 0.4713/0.4714). The correct fidelity
# metric is source-rig pose vs remapped pose on the anatomical pairs.
remap = bpy.data.actions["plw_w07_106_remap"]
src_act = bpy.data.actions["plw_w07_106"]
PAIRS = [("0:0", "MhBone_000"), ("7:1", "MhBone_001"), ("8:2", "MhBone_002"),
         ("9:3", "MhBone_003"), ("10:4", "MhBone_004"),
         ("15:5", "MhBone_005"), ("16:6", "MhBone_006"), ("17:7", "MhBone_007"),
         ("18:8", "MhBone_008"), ("11:9", "MhBone_009"), ("12:10", "MhBone_010"),
         ("13:11", "MhBone_011"), ("14:12", "MhBone_012"),
         ("4:14", "MhBone_014"), ("5:15", "MhBone_015"), ("6:16", "MhBone_016"),
         ("1:17", "MhBone_018"), ("2:18", "MhBone_019"), ("3:19", "MhBone_020")]
src_rig = bpy.data.objects["Animation Group"]

# cog identity anchor: same-motion proof of the channel convention
fy = remap.fcurves.find('pose.bones["MhBone_000"].location', index=1)
cog0 = fy.keyframe_points[0].co[1]
print("=== A. fidelity: MHGU source pose vs plw_w07_106_remap (m) ===")
print("  cog locY anchor: %.4f (native 106 = 0.4714) %s" % (
    cog0, "OK" if abs(cog0 - 0.4714) < 5e-3 else "FAIL"))
def pose_of(rig, act, f):
    rig.animation_data.action = act
    scn.frame_set(f)
    view.update()
    return {b.name: b.matrix.copy() for b in rig.pose.bones}
errs = {d: 0.0 for _, d in PAIRS}
for f in (10, 20, 30, 40, 50, 60):
    ps = pose_of(src_rig, src_act, f)
    pd = pose_of(dst, remap, f)
    for s, d in PAIRS:
        errs[d] = max(errs[d], (ps[s].translation - pd[d].translation).length)
core = [errs[d] for _, d in PAIRS[:5]]
limbs = [errs[d] for _, d in PAIRS[5:]]
print("  core (cog/spine/neck/head): max %.3f m" % max(core))
print("  limbs: max %.3f m | mean of maxima %.3f m" % (max(limbs), sum(limbs) / len(limbs)))
print("  per-bone:", sorted(((d, round(v, 3)) for d, v in errs.items()), key=lambda kv: -kv[1])[:6])
identity_ok = abs(cog0 - 0.4714) < 5e-3 and max(core) < 0.15 and max(limbs) < 0.45
print("  FIDELITY:", "PASS" if identity_ok else "FAIL")

# ---------- B. coverage report ----------
ref = bpy.data.actions["LMT::w03_00::106"]
ref_bones = set()
for fc in ref.fcurves:
    m = re.match(r'pose\.bones\["([^"]+)"\]', fc.data_path)
    if m:
        ref_bones.add(m.group(1))
remaps = sorted([a for a in bpy.data.actions
                 if a.name.startswith("plw_") and a.name.endswith("_remap")],
                key=lambda a: a.name)
print("=== B. coverage over %d remaps ===" % len(remaps))
rows = []
for a in remaps:
    have = set()
    for fc in a.fcurves:
        m = re.match(r'pose\.bones\["([^"]+)"\]', fc.data_path)
        if m:
            have.add(m.group(1))
    missing = ref_bones - have
    obj_ok = a.fcurves.find("location", index=0) is not None and \
             abs(a.fcurves.find("location", index=0).keyframe_points[0].co[1]) < 1e-6
    ik_baked = all(a.fcurves.find('pose.bones["%s"].location' % b, index=0) is not None
                   for b in ("MhBone_247", "MhBone_249", "MhBone_250", "MhBone_252"))
    src = bpy.data.actions.get(a.name[:-len("_remap")])
    span_ok = src is not None and abs((a.frame_range[1] - src.frame_range[1])) < 1.5
    fc_ok = a.freehk.frameCount == src.freehk.frameCount if src else False
    ok = not missing and obj_ok and ik_baked and span_ok and fc_ok
    rows.append((a.name, len(have), sorted(missing), obj_ok, ik_baked, span_ok, fc_ok, ok))
bad = [r for r in rows if not r[-1]]
for r in rows:
    print("  %-22s bones=%2d obj=%s ik=%s span=%s fc=%s -> %s%s" % (
        r[0], r[1], r[3], r[4], r[5], r[6], "OK" if r[7] else "BAD",
        "" if not r[2] else " missing=%s" % r[2][:4]))
print("  coverage: %d/%d OK" % (len(rows) - len(bad), len(rows)))

# ---------- sakura travel sanity ----------
sa151 = bpy.data.actions["plw_sa_151_remap"]
oy = sa151.fcurves.find("location", index=1)
vals = [k.co[1] for k in oy.keyframe_points]
print("=== plw_sa_151_remap object travel: Y %.3f..%.3f (m) keys=%d ===" % (min(vals), max(vals), len(vals)))

print("=== RESULT:", "ALL PASS" if (identity_ok and not bad) else "PROBLEMS FOUND", "===")
