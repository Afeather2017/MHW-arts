"""Dump full key frame lists of the offset channels + total key audit (read-only)."""
import bpy

DST = bpy.data.actions["divide_valor_sheath"]
SRC = bpy.data.actions["plw_LongSword_100_111_Armature_remap.short"]

tot = 0
for f in DST.fcurves:
    tot += len(f.keyframe_points)
print("DST total keys now: %d (pre-splice was 4352)" % tot)

COG = 'pose.bones["MhBone_000"].location'
for p in ("location", COG):
    for i in range(3):
        fd = DST.fcurves.find(p, index=i)
        fs = SRC.fcurves.find(p, index=i)
        print("--- DST %s[%d]: %d keys" % (p[:44], i, len(fd.keyframe_points) if fd else -1))
        if fd:
            xs = [round(k.co.x, 2) for k in fd.keyframe_points]
            if len(xs) <= 70:
                print("    frames:", xs)
            else:
                print("    frames head:", xs[:6], "tail:", xs[-6:])
            print("    ys around 33..40:", [(round(k.co.x), round(k.co.y, 4)) for k in fd.keyframe_points if 32.5 <= k.co.x <= 40])
        print("    SRC %s[%d]: %d keys frames[0:6]=%s [-4:]=%s" % (
            p[:30], i, len(fs.keyframe_points) if fs else -1,
            [round(k.co.x, 1) for k in list(fs.keyframe_points)[:6]] if fs else [],
            [round(k.co.x, 1) for k in list(fs.keyframe_points)[-4:]] if fs else []))

# sanity: how many dst curves end near 82 vs 33
from collections import Counter
ends = Counter()
for f in DST.fcurves:
    ends[round(max(k.co.x for k in f.keyframe_points))] += 1
print("DST max-frame histogram:", dict(sorted(ends.items())))
print("### DONE")
