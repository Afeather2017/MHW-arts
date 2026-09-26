"""Dump LMT::w03_00::106 socket channels (030/047) as the left-hand grip reference."""
import bpy

bpy.ops.wm.open_mainfile(filepath=r"E:\MHW-arts\MHW-Valor.blend")
a = bpy.data.actions.get("LMT::w03_00::106")
if not a:
    print("!!! no LMT::w03_00::106"); raise SystemExit
print("action '%s' range=%s fcurves=%d" % (a.name, tuple(round(v) for v in a.frame_range), len(a.fcurves)))
for bone in ("MhBone_030", "MhBone_047"):
    for prop in ("location", "rotation_quaternion", "rotation_euler", "scale"):
        for i in range(4):
            fc = a.fcurves.find('pose.bones["%s"].%s' % (bone, prop), index=i)
            if fc is None:
                continue
            ks = fc.keyframe_points
            vals = [k.co[1] for k in ks]
            uniq = sorted(set(round(v, 4) for v in vals))
            print("%s %s[%d] keys=%d frames=%s uniq_vals=%s interp=%s hL=%s" % (
                bone, prop, i, len(ks),
                [round(k.co[0]) for k in ks][:8],
                uniq[:4], ks[0].interpolation, ks[0].handle_left_type))
print("DONE")
