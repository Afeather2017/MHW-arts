"""STEP 5 - close object travel for remap actions that END at the idle stance.

MUST run AFTER 3_rebake_ik.py (the stance detection reads the baked IK bones).

Why: Rise root motion (the 'Root' bone) carries a non-zero net displacement on
chain-authored motions (e.g. slot 112 ends (+0.15, +0.41) away from its start).
MHW natives keep the actor at the stance origin for such moves, and the user's
acceptance is "last frame == LMT::w03_00::001". For actions whose END POSE
matches the native idle stance (IK-target rotation distance vs native 001,
mean < 5 deg), the object location[0]/[1] curves get a linear ramp subtracted
so the END value becomes 0 (first is already 0). Real traveling actions are
left open, like natives (92/154 natives end displaced, up to metres).

Run (Git Bash):
  VALOR_BLEND="E:/MHW-arts/MHW-Valor.blend" \
    /e/MHW-arts/tools/blender-4.3.2-windows-x64/blender.exe -b --python 5_close_stance_loops.py
"""
import bpy, os, sys, shutil, time, math

BLEND = os.environ.get("VALOR_BLEND", r"E:\MHW-arts\MHW-Valor.blend")
REF_NAME = "LMT::w03_00::001"
REF_FRAME = 60
IK_BONES = ("MhBone_247", "MhBone_248", "MhBone_249", "MhBone_250", "MhBone_252")
IK_MEAN_MAX = 5.0  # degrees

bak = BLEND + ".bak-" + time.strftime("%Y%m%d-%H%M%S")
shutil.copyfile(BLEND, bak)
print("[backup] %s" % bak, flush=True)

bpy.ops.wm.open_mainfile(filepath=BLEND)
dst = bpy.data.objects.get("m_mesh Armature")
scene = bpy.context.scene
view = bpy.context.view_layer
ref = bpy.data.actions.get(REF_NAME)
if dst is None or ref is None:
    print("!!! FAIL: armature/reference missing"); sys.exit(1)

remaps = sorted([a for a in bpy.data.actions
                 if a.name.startswith("plw_") and a.name.endswith("_remap")],
                key=lambda a: a.name)
print("[load] remaps=%d ref='%s'" % (len(remaps), REF_NAME), flush=True)

def pose_metrics(act, fa):
    """sign-corrected rotation distance of this action's pose at fa vs ref pose."""
    dst.animation_data_create()
    dst.animation_data.action = act
    scene.frame_set(fa)
    view.update()
    ma = {pb.name: pb.matrix.copy() for pb in dst.pose.bones}
    dst.animation_data.action = ref
    scene.frame_set(REF_FRAME)
    view.update()
    ik, body = [], []
    for pb in dst.pose.bones:
        d = ma[pb.name].to_quaternion().rotation_difference(pb.matrix.to_quaternion())
        ang = math.degrees(d.angle)
        if ang > 180.0:
            ang = 360.0 - ang
        (ik if pb.name in IK_BONES else body).append(ang)
    dst.animation_data.action = ref
    return sum(ik) / len(ik), sum(body) / len(body), max(ik), max(body)

# ---- 1. classify ----
rows = []
for a in remaps:
    lo, hi = a.frame_range
    ikm, bm, ikx, bx = pose_metrics(a, int(round(hi)))
    rows.append((a.name, ikm, bm, ikx, bx, ikm < IK_MEAN_MAX))
for a in remaps:  # restore a sane state later; classification done
    break
rows.sort(key=lambda r: r[1])
print("[classify] end-pose vs %s f%d (ik-mean deg; <%.0f -> close):" % (REF_NAME, REF_FRAME, IK_MEAN_MAX), flush=True)
for name, ikm, bm, ikx, bx, will in rows:
    print("   %-52s ik=%6.2f (max %5.2f) body=%5.2f -> %s" % (
        name, ikm, ikx, bm, "CLOSE" if will else "keep"), flush=True)

# ---- 2. close ----
def close_fcurve(f, lo, hi):
    span = hi - lo
    v_end = f.evaluate(hi)
    if abs(v_end) < 1e-4 or span <= 0:
        return False
    for k in f.keyframe_points:
        t = (k.co[0] - lo) / span
        k.co[1] -= t * v_end
        hl, hr = k.handle_left, k.handle_right
        k.handle_left = (hl[0], hl[1] - ((hl[0] - lo) / span) * v_end)
        k.handle_right = (hr[0], hr[1] - ((hr[0] - lo) / span) * v_end)
    f.update()
    return True

closed, already = [], []
for name, ikm, bm, ikx, bx, will in rows:
    if not will:
        continue
    a = bpy.data.actions[name]
    lo, hi = a.frame_range
    did = []
    for idx in (0, 1):
        f = a.fcurves.find("location", index=idx)
        if f is None:
            continue
        if close_fcurve(f, lo, hi):
            did.append(idx)
        else:
            already.append((name, idx))
    if did:
        ex = a.fcurves.find("location", index=0)
        ey = a.fcurves.find("location", index=1)
        closed.append((name, ex.keyframe_points[-1].co[1], ey.keyframe_points[-1].co[1]))
print("[close] closed=%d already-zero=%d" % (len(closed), len(already)), flush=True)
for name, ex, ey in closed:
    print("   %-52s end -> (%+.4f, %+.4f)" % (name, ex, ey), flush=True)

# ---- 3. verify ----
bad = []
for name, _, _ in closed:
    a = bpy.data.actions[name]
    lo, hi = a.frame_range
    for idx in (0, 1):
        f = a.fcurves.find("location", index=idx)
        if f is None:
            bad.append((name, idx, "missing")); continue
        if abs(f.evaluate(lo)) > 1e-4 or abs(f.evaluate(hi)) > 1e-4:
            bad.append((name, idx, "ends not zero"))
print("[verify] bad=%d %s" % (len(bad), bad[:6]), flush=True)
if bad:
    print("!!! NOT SAVING"); sys.exit(1)

bpy.ops.wm.save_mainfile()
print("[done] saved:", BLEND, flush=True)
