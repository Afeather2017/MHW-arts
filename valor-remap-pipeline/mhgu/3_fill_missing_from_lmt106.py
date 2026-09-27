"""MHGU chain STEP 3 - fill bones the MHGU skeleton cannot animate from native 106.

MHGU animates only 19 body bones; MHW's rig also carries fingers, toes,
sockets and helper bones the remap never produces channels for. Per project
decision 2026-09-27, every MhBone that has NO fcurves in a remapped action
gets its curves copied VERBATIM from native LMT::w03_00::106 (a calm
left-hand reference move): finger curl, toe, waist 013, sockets 030/047,
helpers 101-104, IK 254 quat.

Skipped on purpose:
- MhBone_247..253: regenerated from the pose by 3_rebake_ik.py (root-pipeline
  step 3); writing 106 constants here would be overwritten anyway.
- MhBone_000: owned by the remap (cog location) + object migration.

Idempotent. Run:
  /e/MHW-arts/tools/blender-4.3.2-windows-x64/blender.exe -b --python mhgu/3_fill_missing_from_lmt106.py
"""
import bpy, os, sys, shutil, time, re

WORK = os.environ.get("MHGU_WORK_BLEND", r"E:\MHW-arts\mhgu-valor-mhw.blend")
REF = "LMT::w03_00::106"
IK_REBAKE = {"MhBone_24%d" % i for i in (7, 8)} | {"MhBone_25%d" % i for i in (0, 1, 2, 3)}

bak = WORK + ".bak-" + time.strftime("%Y%m%d-%H%M%S")
shutil.copyfile(WORK, bak)
print("[backup] %s" % bak, flush=True)

bpy.ops.wm.open_mainfile(filepath=WORK)
ref = bpy.data.actions.get(REF)
if ref is None:
    print("!!! FAIL: %s missing" % REF); sys.exit(1)

ref_curves = {}
for fc in ref.fcurves:
    m = re.match(r'pose\.bones\["([^"]+)"\]', fc.data_path)
    if not m:
        continue
    ref_curves.setdefault(m.group(1), []).append(fc)
ref_bones = sorted(ref_curves)
print("[ref] %s carries %d bones" % (REF, len(ref_bones)), flush=True)

remaps = sorted([a for a in bpy.data.actions
                 if a.name.endswith("_remap") and "mhgu_source" in bpy.data.actions.get(a.name[:-len("_remap")], "")],
                key=lambda a: a.name)
print("[load] remaps=%d" % len(remaps), flush=True)

def copy_keys(src_fc, dst_action, hi):
    """Copy source keys clamped to the remap's own frame span. Native 106
    curves can span its full 0..121 length; copying them verbatim stretched
    short MHGU actions and poisoned downstream IK/verify sampling."""
    fc = dst_action.fcurves.find(src_fc.data_path, index=src_fc.array_index)
    if fc is not None:
        return False
    fc = dst_action.fcurves.new(data_path=src_fc.data_path, index=src_fc.array_index)
    kp = fc.keyframe_points
    kept = [k for k in src_fc.keyframe_points if k.co[0] <= hi + 1e-6]
    if not kept:
        kept_val = src_fc.evaluate(0)
        kept = None
        kp.add(1)
        kp[0].co = (0.0, kept_val)
        kp[0].interpolation = 'BEZIER'
        kp[0].handle_left_type = 'AUTO_CLAMPED'
        kp[0].handle_right_type = 'AUTO_CLAMPED'
    else:
        kp.add(len(kept))
        for k, sk in zip(kp, kept):
            k.co = sk.co
            k.interpolation = sk.interpolation
            k.handle_left_type = sk.handle_left_type
            k.handle_right_type = sk.handle_right_type
    fc.update()
    return True

report = {"actions": 0, "fcurves": 0, "per_action": []}
for a in remaps:
    have = set()
    for fc in a.fcurves:
        m = re.match(r'pose\.bones\["([^"]+)"\]', fc.data_path)
        if m:
            have.add(m.group(1))
    fillable = [b for b in ref_bones if b not in have and b not in IK_REBAKE and b != "MhBone_000"]
    hi = a.frame_range[1]
    n = 0
    for b in fillable:
        for fc in ref_curves[b]:
            n += int(copy_keys(fc, a, hi))
    report["actions"] += 1
    report["fcurves"] += n
    report["per_action"].append((a.name, fillable))
print("[fill] done: actions=%d fcurves-added=%d" % (report["actions"], report["fcurves"]), flush=True)
for name, fl in report["per_action"][:3]:
    print("   %s: +%d bones %s" % (name, len(fl), fl), flush=True)

# ---- verify: coverage == native 106 minus IK rebake set minus root ----
bad = []
want = {b for b in ref_bones if b not in IK_REBAKE and b != "MhBone_000"}
for a in remaps:
    have = set()
    for fc in a.fcurves:
        m = re.match(r'pose\.bones\["([^"]+)"\]', fc.data_path)
        if m:
            have.add(m.group(1))
    miss = want - have
    if miss:
        bad.append((a.name, sorted(miss)))
print("[verify] actions missing filled bones: %d %s" % (len(bad), bad[:3]), flush=True)
if bad:
    print("!!! NOT SAVING"); sys.exit(1)

bpy.ops.wm.save_mainfile()
print("[done] saved:", WORK, flush=True)
