"""SURGICAL variant of 4_object_root_motion_mhgu.py for INCREMENTAL runs.

The original is ONE-SHOT: it aborts if any remap already has object curves
(the work file's 25 baseline remaps do).  This copy processes ONLY remaps
that are still missing object-level location curves; every other line mirrors
the original (native interp conventions, first-key-zero normalization,
MhBone_000 locY bezier, native-parity IK strip, verify, save).
"""
import bpy, os, re, sys, shutil, time, traceback

WORK = os.environ.get("MHGU_WORK_BLEND", r"E:\MHW-arts\mhgu-valor-mhw.blend")
M000 = 'pose.bones["MhBone_000"].location'

bak = WORK + ".bak-" + time.strftime("%Y%m%d-%H%M%S")
shutil.copyfile(WORK, bak)
print("[backup] %s" % bak, flush=True)

bpy.ops.wm.open_mainfile(filepath=WORK)
all_remaps = sorted([a for a in bpy.data.actions
                     if a.name.endswith("_remap") and "mhgu_source" in bpy.data.actions.get(a.name[:-len("_remap")], "")],
                    key=lambda a: a.name)
remaps = [a for a in all_remaps if a.fcurves.find("location", index=0) is None]
natives = [a for a in bpy.data.actions if a.name.startswith("LMT::")]
print("[load] all-remaps=%d to-process=%d natives=%d" % (len(all_remaps), len(remaps), len(natives)), flush=True)
if not remaps:
    print("!!! nothing to process"); sys.exit(0)

# ---- native reference (interp conventions) ----
native_ik_props = {}
native_obj_interp = None
native_root_loc_interp = None
for a in natives:
    for f in a.fcurves:
        m = re.match(r'pose\.bones\["(MhBone_2\d\d)"\]\.(\w+)', f.data_path)
        if m:
            native_ik_props[(m.group(1), m.group(2))] = True
        if not f.data_path.startswith("pose.bones[") and native_obj_interp is None:
            native_obj_interp = f.keyframe_points[0].interpolation
    if native_root_loc_interp is None:
        fy = a.fcurves.find(M000, index=1)
        if fy and len(fy.keyframe_points):
            native_root_loc_interp = fy.keyframe_points[0].interpolation
print("[native] obj interp=%s root-locY interp=%s" % (native_obj_interp, native_root_loc_interp), flush=True)

def write_keys(fc, pts, interp='BEZIER', htype='AUTO_CLAMPED'):
    kp = fc.keyframe_points
    kp.clear()
    kp.add(len(pts))
    for k, (fr, v) in zip(kp, pts):
        k.co = (fr, v)
        k.interpolation = interp
        k.handle_left_type = htype
        k.handle_right_type = htype
    fc.update()

def const_pts(a, v):
    lo, hi = a.frame_range
    return [(lo, v)] + ([(hi, v)] if hi > lo else [])

def ensure_fc(a, path, index):
    fc = a.fcurves.find(path, index=index)
    return fc if fc is not None else a.fcurves.new(path, index=index)

# ---- process ----
report = {"obj_created": 0, "travel_ok": 0, "no_travel": 0, "height_bezier": 0, "ik_removed": 0, "skipped": []}
travel_report = []
for n, a in enumerate(remaps, 1):
    try:
        srcname = a.name[:-len("_remap")]
        travel = bpy.data.actions.get("TRAVEL::" + srcname)
        lo = a.frame_range[0]
        hi = a.frame_range[1]
        if travel is not None:
            fx = travel.fcurves.find("location", index=0)
            fy = travel.fcurves.find("location", index=1)
            frames = set()
            for fc in (fx, fy):
                if fc:
                    frames |= {k.co[0] for k in fc.keyframe_points}
            frames = sorted(f for f in frames if lo - 1e-6 <= f <= hi + 1e-6) or [lo]
            x0 = fx.evaluate(lo) if fx else 0.0
            y0 = fy.evaluate(lo) if fy else 0.0
            objx = [(f, (fx.evaluate(f) if fx else x0) - x0) for f in frames]
            objy = [(f, (fy.evaluate(f) if fy else y0) - y0) for f in frames]
            report["travel_ok"] += 1
            travel_report.append((a.name, round(min(v for _, v in objy), 3), round(max(v for _, v in objy), 3)))
        else:
            objx = const_pts(a, 0.0)
            objy = const_pts(a, 0.0)
            report["no_travel"] += 1
        interp = native_obj_interp or 'BEZIER'
        write_keys(ensure_fc(a, "location", 0), objx, interp)
        write_keys(ensure_fc(a, "location", 1), objy, interp)
        write_keys(ensure_fc(a, "location", 2), const_pts(a, 0.0), interp)
        write_keys(ensure_fc(a, "rotation_quaternion", 0), const_pts(a, 1.0), interp)
        for i in (1, 2, 3):
            write_keys(ensure_fc(a, "rotation_quaternion", i), const_pts(a, 0.0), interp)
        report["obj_created"] += 1
        fy = a.fcurves.find(M000, index=1)
        if fy and native_root_loc_interp == 'BEZIER':
            for k in fy.keyframe_points:
                k.interpolation = 'BEZIER'
                k.handle_left_type = 'AUTO_CLAMPED'
                k.handle_right_type = 'AUTO_CLAMPED'
            fy.update()
            report["height_bezier"] += 1
        for f in list(a.fcurves):
            m = re.match(r'pose\.bones\["(MhBone_2\d\d)"\]\.(\w+)$', f.data_path)
            if m and (m.group(1), m.group(2)) not in native_ik_props:
                a.fcurves.remove(f)
                report["ik_removed"] += 1
    except Exception:
        traceback.print_exc()
        report["skipped"].append((a.name, "exception"))
print("[proc] done: %s" % report, flush=True)
print("[travel] Y ranges (m): %s" % travel_report, flush=True)

# ---- verify (processed set only) ----
bad = []
for a in remaps:
    f0 = a.fcurves.find("location", index=0)
    f1 = a.fcurves.find("location", index=1)
    if f0 is None or f1 is None:
        bad.append((a.name, "missing obj loc")); continue
    if abs(f0.keyframe_points[0].co[1]) > 1e-6 or abs(f1.keyframe_points[0].co[1]) > 1e-6:
        bad.append((a.name, "obj loc first key != 0")); continue
    fy = a.fcurves.find(M000, index=1)
    if fy is None or not len(fy.keyframe_points):
        bad.append((a.name, "cog height channel missing")); continue
print("[verify] problems: %s" % (bad[:10] if bad else "none"), flush=True)
if bad:
    print("!!! NOT SAVING"); sys.exit(1)

bpy.ops.wm.save_mainfile()
print("[done] saved:", WORK, flush=True)
