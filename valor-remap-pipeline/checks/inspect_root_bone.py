"""Check the Rise 'Root' bone as the real root-motion carrier + bmap entries."""
import bpy

bpy.ops.wm.open_mainfile(filepath=r"E:\MHW-arts\MHW-Valor.blend")

S = bpy.data.actions.get("plw_LongSword_100_112_Armature")
R = bpy.data.actions.get("plw_LongSword_100_112_Armature_remap")

print("=== source 112: Root / Cog / Waist_00 location channels ===")
for bone in ("Root", "Cog", "Waist_00"):
    for i in range(3):
        f = S.fcurves.find('pose.bones["%s"].location' % bone, index=i)
        if f:
            ks = f.keyframe_points
            vals = [k.co[1] for k in ks]
            print("  %-9s loc[%d] keys=%d first=%.4f last=%.4f min=%.4f max=%.4f" % (
                bone, i, len(ks), vals[0], vals[-1], min(vals), max(vals)))

print("=== time series: source Root.loc vs backup MhBone_000.loc ===")
# backup has the pre-migration bone X/Z
bpy.ops.wm.open_mainfile(filepath=r"E:\MHW-arts\MHW-Valor.backup-20260926-pre-objectloc.blend")
SB = bpy.data.actions.get("plw_LongSword_100_112_Armature")
RB = bpy.data.actions.get("plw_LongSword_100_112_Armature_remap")
slo, shi = SB.frame_range
rlo, rhi = RB.frame_range
n = 9
print("  t | src Root X Z          | remap MhBone_000 X Z")
for i in range(n):
    t = i / (n - 1)
    st = slo + t * (shi - slo)
    rt = rlo + t * (rhi - rlo)
    sv = []
    for k in (0, 2):
        f = SB.fcurves.find('pose.bones["Root"].location', index=k)
        sv.append(f.evaluate(st) if f else 0.0)
    rv = []
    for k in (0, 2):
        f = RB.fcurves.find('pose.bones["MhBone_000"].location', index=k)
        rv.append(f.evaluate(rt) if f else 0.0)
    print("  %.2f | %7.3f %7.3f | %7.3f %7.3f" % (t, sv[0], sv[1], rv[0], rv[1]))
print("DONE")
