"""Pose comparison v2: sign-corrected angles, body vs IK bones split."""
import bpy, math

bpy.ops.wm.open_mainfile(filepath=r"E:\MHW-arts\MHW-Valor.blend")
dst = bpy.data.objects["m_mesh Armature"]
scene = bpy.context.scene
view = bpy.context.view_layer

def pose_error(act_a, fa, act_b, fb):
    dst.animation_data.action = act_a
    scene.frame_set(fa)
    view.update()
    ma = {pb.name: pb.matrix.copy() for pb in dst.pose.bones}
    dst.animation_data.action = act_b
    scene.frame_set(fb)
    view.update()
    body, ik = [], []
    for pb in dst.pose.bones:
        d = ma[pb.name].to_quaternion().rotation_difference(pb.matrix.to_quaternion())
        ang = math.degrees(d.angle)
        if ang > 180.0:
            ang = 360.0 - ang
        (ik if pb.name.startswith(("MhBone_24", "MhBone_25")) else body).append(ang)
    return body, ik

R112 = bpy.data.actions["plw_LongSword_100_112_Armature_remap"]
N001 = bpy.data.actions["LMT::w03_00::001"]

print("remap112 end (f83) vs native001:")
for fb in (0, 60, 100, 160):
    body, ik = pose_error(R112, 83, N001, fb)
    print("  vs f%-4d body: mean=%.2f max=%.2f | ik: mean=%.2f max=%.2f" % (
        fb, sum(body)/len(body), max(body), sum(ik)/len(ik), max(ik)))

# baseline: how much does native001 differ from ITSELF at two frames (idle breathing)
body, ik = pose_error(N001, 0, N001, 60)
print("baseline native001 f0 vs f60: body mean=%.2f max=%.2f | ik mean=%.2f max=%.2f" % (
    sum(body)/len(body), max(body), sum(ik)/len(ik), max(ik)))

N112 = bpy.data.actions.get("LMT::w03_00::112")
if N112:
    lo, hi = N112.frame_range
    fx = N112.fcurves.find("location", index=0)
    fy = N112.fcurves.find("location", index=1)
    print("native112 range %.0f..%.0f objX end=%.4f objY end=%.4f" % (
        lo, hi, fx.evaluate(hi) if fx else 0.0, fy.evaluate(hi) if fy else 0.0))
    body, ik = pose_error(N112, int(hi), N001, 60)
    print("  native112 end vs native001 f60: body mean=%.2f max=%.2f | ik mean=%.2f max=%.2f" % (
        sum(body)/len(body), max(body), sum(ik)/len(ik), max(ik)))
else:
    print("native LMT::w03_00::112 absent")
print("DONE")
