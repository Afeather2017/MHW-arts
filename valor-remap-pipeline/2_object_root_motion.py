"""STEP 2 - move root horizontal travel to object-level curves (ONE-SHOT).

Runs on the ARP-remapped blend (plw_*_remap actions on 'm_mesh Armature').
Moves MhBone_000 location X/Z travel to object-level curves, matching the
native LMT::w03_00::* reference structure; zeroes the bone-side channels;
converts the height channel (loc Y) to Bezier; strips IK channel types no
native action carries.

*** ONE-SHOT *** consumes the bone X/Z travel. Re-running it on already
migrated data would flatten the object curves to zero - there is a guard,
do not bypass it.

Run (Git Bash):
  VALOR_BLEND="E:/MHW-arts/MHW-Valor.blend" \
    /e/MHW-arts/tools/blender-4.3.2-windows-x64/blender.exe -b --python 2_object_root_motion.py
VALOR_BLEND defaults to E:\\MHW-arts\\MHW-Valor.blend. A timestamped .bak of
the blend is written next to it before anything is modified.
"""
import bpy, os, re, sys, shutil, time, traceback

BLEND = os.environ.get("VALOR_BLEND", r"E:\MHW-arts\MHW-Valor.blend")
M000 = 'pose.bones["MhBone_000"].location'
IK_BONES = ("MhBone_247", "MhBone_248", "MhBone_249", "MhBone_250",
            "MhBone_251", "MhBone_252", "MhBone_253", "MhBone_254")

bak = BLEND + ".bak-" + time.strftime("%Y%m%d-%H%M%S")
shutil.copyfile(BLEND, bak)
print("[backup] %s" % bak, flush=True)

bpy.ops.wm.open_mainfile(filepath=BLEND)
acts = bpy.data.actions
remaps = sorted([a for a in acts if a.name.startswith("plw_") and a.name.endswith("_remap")],
                key=lambda a: a.name)
natives = [a for a in acts if a.name.startswith("LMT::")]
print("[load] remaps=%d natives=%d" % (len(remaps), len(natives)), flush=True)
if not remaps or not natives:
    print("!!! FAIL: unexpected action population"); sys.exit(1)

# ---- one-shot guard ----
def flat(f):
    return f is None or all(abs(k.co[1]) < 1e-6 for k in f.keyframe_points)

probe = remaps[0]
if probe.fcurves.find("location", index=0) is not None and \
   flat(probe.fcurves.find(M000, index=0)) and flat(probe.fcurves.find(M000, index=2)):
    print("!!! ABORT: '%s' already has object curves and flat bone X/Z - step 2 already ran." % probe.name)
    sys.exit(1)

# ---- native reference ----
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

# ---- helpers ----
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
report = {"obj_created": 0, "xz_zeroed": 0, "height_bezier": 0, "ik_removed": 0, "skipped": []}
for n, a in enumerate(remaps, 1):
    try:
        fx = a.fcurves.find(M000, index=0)
        fz = a.fcurves.find(M000, index=2)
        xs = [(k.co[0], k.co[1]) for k in fx.keyframe_points] if fx else []
        zs = [(k.co[0], k.co[1]) for k in fz.keyframe_points] if fz else []
        src = xs or zs
        if not src:
            report["skipped"].append((a.name, "no bone X/Z keys"))
            continue
        x0 = xs[0][1] if xs else 0.0
        z0 = zs[0][1] if zs else 0.0
        interp = native_obj_interp or 'BEZIER'
        write_keys(ensure_fc(a, "location", 0),
                   [(fr, v - x0) for (fr, v) in xs] or const_pts(a, 0.0), interp)
        write_keys(ensure_fc(a, "location", 1),
                   [(fr, z0 - v) for (fr, v) in zs] or const_pts(a, 0.0), interp)
        write_keys(ensure_fc(a, "location", 2), const_pts(a, 0.0), interp)
        write_keys(ensure_fc(a, "rotation_quaternion", 0), const_pts(a, 1.0), interp)
        for i in (1, 2, 3):
            write_keys(ensure_fc(a, "rotation_quaternion", i), const_pts(a, 0.0), interp)
        report["obj_created"] += 1
        if fx: write_keys(fx, const_pts(a, 0.0), interp)
        if fz: write_keys(fz, const_pts(a, 0.0), interp)
        if fx or fz: report["xz_zeroed"] += 1
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
    if n % 40 == 0:
        print("[proc] %d/%d" % (n, len(remaps)), flush=True)
print("[proc] done: %s" % report, flush=True)

# ---- verify ----
bad = []
for a in remaps:
    f0 = a.fcurves.find("location", index=0)
    f1 = a.fcurves.find("location", index=1)
    if f0 is None or f1 is None:
        bad.append((a.name, "missing obj loc")); continue
    if abs(f0.keyframe_points[0].co[1]) > 1e-6 or abs(f1.keyframe_points[0].co[1]) > 1e-6:
        bad.append((a.name, "obj loc first key != 0")); continue
    if any(abs(k.co[1]) > 1e-6 for f in (a.fcurves.find(M000, index=0), a.fcurves.find(M000, index=2))
           if f for k in f.keyframe_points):
        bad.append((a.name, "bone X/Z not zeroed")); continue
print("[verify] problems: %s" % (bad[:10] if bad else "none"), flush=True)
if bad:
    print("!!! NOT SAVING"); sys.exit(1)

bpy.ops.wm.save_mainfile()
print("[done] saved:", BLEND, flush=True)
