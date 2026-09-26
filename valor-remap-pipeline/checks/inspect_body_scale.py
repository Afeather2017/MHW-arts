"""Check body-scale channels in remap actions after the object-loc fix."""
import bpy, re

bpy.ops.wm.open_mainfile(filepath=r"E:\MHW-arts\MHW-Valor.blend")
M000 = 'pose.bones["MhBone_000"].location'
remaps = [a for a in bpy.data.actions if a.name.startswith("plw_") and a.name.endswith("_remap")]
natives = [a for a in bpy.data.actions if a.name.startswith("LMT::")]

SKIP = {"MhBone_030", "MhBone_047"}
tot = flat1 = nonflat = nonzero = 0
offenders = {}
sample = None
for a in remaps:
    for f in a.fcurves:
        m = re.match(r'pose\.bones\["(MhBone_\d+)"\]\.scale$', f.data_path)
        if not m or m.group(1) in SKIP:
            continue
        tot += 1
        vals = [k.co[1] for k in f.keypoint] if False else [k.co[1] for k in f.keyframe_points]
        if max(vals) - min(vals) > 1e-6:
            nonflat += 1
            offenders.setdefault(m.group(1), 0)
            offenders[m.group(1)] += 1
            if sample is None:
                sample = (a.name, m.group(1), f.array_index,
                          round(min(vals), 4), round(max(vals), 4))
        if max(abs(v - 1.0) for v in vals) > 1e-6:
            nonzero += 1

print("body scale fcurves: total=%d nonflat=%d not-all-1.0=%d" % (tot, nonflat, nonzero))
print("bones with animated scale:", dict(sorted(offenders.items())))
if sample:
    print("first sample:", sample)

# native grip sanity + one remap object curve sanity
a = sorted(remaps, key=lambda x: x.name)[0]
for idx in (0, 1):
    f = a.fcurves.find("location", index=idx)
    ks = f.keyframe_points
    print("%s obj location[%d]: keys=%d first=(%.2f,%.4f) last=(%.2f,%.4f)" % (
        a.name, idx, len(ks), ks[0].co[0], ks[0].co[1], ks[-1].co[0], ks[-1].co[1]))
fy = a.fcurves.find(M000, index=1)
print("bone loc[1] interp:", fy.keyframe_points[0].interpolation)
nat = sorted(natives, key=lambda x: x.name)[0]
g = [f for f in nat.fcurves if f.data_path == 'pose.bones["MhBone_030"].scale']
print("native 030 scale fcurves:", len(g))
print("DONE")
