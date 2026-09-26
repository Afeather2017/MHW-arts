"""Where does Rise root motion live? Check source actions' OBJECT-level curves
and correlate with the remap object curves for slot 112.
"""
import bpy

bpy.ops.wm.open_mainfile(filepath=r"E:\MHW-arts\MHW-Valor.blend")
src_ob = bpy.data.objects.get("Armature")
dst_ob = bpy.data.objects.get("m_mesh Armature")
print("src obj matrix_world:", [round(v, 4) for v in src_ob.matrix_world.to_translation()],
      "scale", [round(v, 4) for v in src_ob.matrix_world.to_scale()])
print("dst obj matrix_world:", [round(v, 4) for v in dst_ob.matrix_world.to_translation()],
      "scale", [round(v, 4) for v in dst_ob.matrix_world.to_scale()])

srcs = [a for a in bpy.data.actions if a.name.startswith("plw_") and not a.name.endswith("_remap")]
n_obj = 0
paths_seen = set()
for a in srcs:
    objf = [f for f in a.fcurves if not f.data_path.startswith("pose.bones[")]
    if objf:
        n_obj += 1
        paths_seen.update(f.data_path for f in objf)
print("sources with non-pose fcurves: %d/%d  paths=%s" % (n_obj, len(srcs), sorted(paths_seen)))

S = bpy.data.actions.get("plw_LongSword_100_112_Armature")
R = bpy.data.actions.get("plw_LongSword_100_112_Armature_remap")
print("=== source 112 object curves ===")
for i in range(3):
    f = S.fcurves.find("location", index=i)
    if f:
        ks = f.keyframe_points
        print("  loc[%d] keys=%d first=(%.2f,%.4f) last=(%.2f,%.4f) min=%.4f max=%.4f" % (
            i, len(ks), ks[0].co[0], ks[0].co[1], ks[-1].co[0], ks[-1].co[1],
            min(k.co[1] for k in ks), max(k.co[1] for k in ks)))
    else:
        print("  loc[%d] MISSING" % i)

print("=== time series: source obj loc vs remap obj loc (112) ===")
slo, shi = S.frame_range
rlo, rhi = R.frame_range
print("  src range %.0f..%.0f | remap range %.0f..%.0f" % (slo, shi, rlo, rhi))
n = 9
print("  t%% | srcX srcY srcZ | remX remY")
for i in range(n):
    t = i / (n - 1)
    sf = S.frame_range; rf = R.frame_range
    st = slo + t * (shi - slo)
    rt = rlo + t * (rhi - rlo)
    sv = [S.fcurves.find("location", index=k).evaluate(st) if S.fcurves.find("location", index=k) else 0.0 for k in range(3)]
    rv = [R.fcurves.find("location", index=k).evaluate(rt) if R.fcurves.find("location", index=k) else 0.0 for k in range(3)]
    print("  %.2f | %7.3f %7.3f %7.3f | %7.3f %7.3f" % (t, sv[0], sv[1], sv[2], rv[0], rv[1]))

# also: source MESH/armature children? check rise rig root bone existence
print("=== rise rig: bones with 000/root/center in name ===")
print([b.name for b in src_ob.data.bones if any(s in b.name.lower() for s in ("root", "000", "center", "cog", "hip", "waist"))])
print("DONE")
