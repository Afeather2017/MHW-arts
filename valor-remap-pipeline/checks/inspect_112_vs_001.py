"""Is remap 112's END POSE really == native 001 pose? And what does native 112 do?"""
import bpy, math

bpy.ops.wm.open_mainfile(filepath=r"E:\MHW-arts\MHW-Valor.blend")
dst = bpy.data.objects["m_mesh Armature"]
scene = bpy.context.scene
view = bpy.context.view_layer

def pose_error(act_a, fa, act_b, fb):
    """mean/max rotation angle difference (deg) across common bones."""
    dst.animation_data.action = act_a
    scene.frame_set(fa)
    view.update()
    ma = {pb.name: pb.matrix.copy() for pb in dst.pose.bones}
    dst.animation_data.action = act_b
    scene.frame_set(fb)
    view.update()
    diffs = []
    for pb in dst.pose.bones:
        if pb.name in ma:
            d = ma[pb.name].to_quaternion().rotation_difference(pb.matrix.to_quaternion())
            diffs.append(math.degrees(abs(d.angle)))
    return (sum(diffs) / len(diffs), max(diffs), len(diffs))

R112 = bpy.data.actions["plw_LongSword_100_112_Armature_remap"]
N001 = bpy.data.actions["LMT::w03_00::001"]
N112 = bpy.data.actions.get("LMT::w03_00::112")

print("remap112 last frame (83) vs native001 frames:")
for fb in (0, 60, 100, 160):
    mean, mx, n = pose_error(R112, 83, N001, fb)
    print("  vs f%-4d mean=%.2f deg  max=%.2f deg  bones=%d" % (fb, mean, mx, n))

if N112:
    lo, hi = N112.frame_range
    f = N112.fcurves.find("location", index=0)
    g = N112.fcurves.find("location", index=1)
    print("native 112: range %.0f..%.0f  objX end=%.4f  objY end=%.4f" % (
        lo, hi, f.evaluate(hi) if f else 0.0, g.evaluate(hi) if g else 0.0))
    mean, mx, n = pose_error(N112, int(hi), N001, 60)
    print("  native112 end pose vs native001 f60: mean=%.2f max=%.2f" % (mean, mx))
print("DONE")
