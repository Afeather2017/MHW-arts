"""MHGU chain STEP 4b - ground travel for the GLB (Motion[*]) sources.

The RevilToolset GLB bake keeps MHGU's ground-root travel on the `reference`
bone's location (measured: Motion[102] reference locZ 0..1.916 m == the 2 m
step the source animation shows), while the LMT imports carry it on the
synthetic Root track (handled by step 2's extraction). This step gives the
GLB-sourced remaps the same treatment as 4_object_root_motion:

- extract reference.location into TRAVEL::<name> with the established axis
  conversion (obj[0] = refX, obj[1] = -refZ; GLB values are already metres),
- write the remap's object location[0]/[1] from it (first key normalised),
  leaving MhBone_000's cog channel untouched.

Idempotent. Run after 4_object_root_motion_mhgu.py.
"""
import bpy, os, sys, shutil, time

WORK = os.environ.get("MHGU_WORK_BLEND", r"E:\MHW-arts\mhgu-valor-mhw.blend")
REF_PATH = 'pose.bones["reference"].location'

bak = WORK + ".bak-" + time.strftime("%Y%m%d-%H%M%S")
shutil.copyfile(WORK, bak)
print("[backup] %s" % bak, flush=True)

bpy.ops.wm.open_mainfile(filepath=WORK)

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

def ensure_fc(a, path, index):
    fc = a.fcurves.find(path, index=index)
    return fc if fc is not None else a.fcurves.new(path, index=index)

report = []
for src in sorted([a for a in bpy.data.actions
                   if "mhgu_source" in a and not a.name.endswith("_remap")],
                  key=lambda a: a.name):
    ref_fcs = [src.fcurves.find(REF_PATH, index=i) for i in range(3)]
    if all(fc is None for fc in ref_fcs):
        continue  # LMT imports: no reference bone animation (Root track instead)
    remap = bpy.data.actions.get(src.name + "_remap")
    if remap is None:
        continue
    travel = bpy.data.actions.get("TRAVEL::" + src.name)
    if travel is None:
        travel = bpy.data.actions.new("TRAVEL::" + src.name)
        travel.use_fake_user = True
    lo, hi = remap.frame_range
    frames = set()
    for fc in ref_fcs:
        if fc:
            frames |= {k.co[0] for k in fc.keyframe_points}
    frames = sorted(f for f in frames if lo - 1e-6 <= f <= hi + 1e-6)
    # TRAVEL::<name>: obj[0]=refX, obj[1]=-refZ (blend-consistent metres)
    for i in range(3):
        old = travel.fcurves.find("location", index=i)
        if old is not None:
            travel.fcurves.remove(old)
    # obj[0]=refX, obj[1]=-refZ (glTF axis swap, same as the LMT Root track),
    # obj[2]=refY (ground root: constant 0)
    for dst_i, src_i, sign in ((0, 0, 1.0), (1, 2, -1.0), (2, 1, 1.0)):
        fc = ref_fcs[src_i]
        if fc is None:
            continue
        nf = travel.fcurves.new(data_path="location", index=dst_i)
        nf.keyframe_points.add(len(fc.keyframe_points))
        for k, sk in zip(nf.keyframe_points, fc.keyframe_points):
            k.co = (sk.co[0], sk.co[1] * sign)
        nf.update()
    # remap object curves, first-key normalised
    tfx = travel.fcurves.find("location", index=0)
    tfy = travel.fcurves.find("location", index=1)
    x0 = tfx.evaluate(lo) if tfx else 0.0
    y0 = tfy.evaluate(lo) if tfy else 0.0
    use_f = set()
    for fc in (tfx, tfy):
        if fc:
            use_f |= {k.co[0] for k in fc.keyframe_points}
    use_f = sorted(f for f in use_f if lo - 1e-6 <= f <= hi + 1e-6) or [lo]
    objx = [(f, (tfx.evaluate(f) if tfx else x0) - x0) for f in use_f]
    objy = [(f, (tfy.evaluate(f) if tfy else y0) - y0) for f in use_f]
    write_keys(ensure_fc(remap, "location", 0), objx)
    write_keys(ensure_fc(remap, "location", 1), objy)
    ymax = max((v for _, v in objy), default=0.0)
    ymin = min((v for _, v in objy), default=0.0)
    report.append((remap.name, ymin, ymax))
print("[glb-travel] processed %d GLB-sourced remaps" % len(report), flush=True)
for r in report:
    print("   %-40s objY %.3f..%.3f" % r, flush=True)

# verify: first key zero
bad = [r[0] for r in report if abs(r[1]) > 1e-6 and r[1] != report[0][1]]
# first-key check directly
bad = []
for src in [a for a in bpy.data.actions if "mhgu_source" in a and not a.name.endswith("_remap")]:
    remap = bpy.data.actions.get(src.name + "_remap")
    if remap is None or src.fcurves.find(REF_PATH) is None:
        continue
    f1 = remap.fcurves.find("location", index=1)
    if f1 and abs(f1.keyframe_points[0].co[1]) > 1e-6:
        bad.append(src.name)
print("[verify] first-key problems: %s" % (bad or "none"), flush=True)
if bad:
    print("!!! NOT SAVING"); sys.exit(1)

bpy.ops.wm.save_mainfile()
print("[done] saved:", WORK, flush=True)
