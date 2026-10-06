"""Round 2: object rotation at seam, MhBone_000 X/Z profile, 030/047 scale, dst curve anatomy."""
import bpy, math

SRC = "plw_LongSword_100_111_Armature_remap.short"
DST = "divide_valor_sheath"
A = bpy.data.actions[SRC]
B = bpy.data.actions[DST]

sp, dp = {}, {}
for f in A.fcurves: sp[(f.data_path, f.array_index)] = f
for f in B.fcurves: dp[(f.data_path, f.array_index)] = f

def quat_angle(pa, pb, fra, frb):
    qa = [pa.get(i).evaluate(fra) if i in pa else (1.0 if i == 0 else 0.0) for i in range(4)]
    qb = [pb.get(i).evaluate(frb) if i in pb else (1.0 if i == 0 else 0.0) for i in range(4)]
    return 2 * math.acos(min(1.0, abs(sum(a*b for a, b in zip(qa, qb)))))

pa = {i: f for (p, i), f in sp.items() if p == "rotation_quaternion"}
pb = {i: f for (p, i), f in dp.items() if p == "rotation_quaternion"}
print("OBJ ROT: dst@33 vs src@9 angle = %.4f rad (%.1f deg)" % (quat_angle(pa, pb, 33.0, 9.0), math.degrees(quat_angle(pa, pb, 33.0, 9.0))))
print("OBJ ROT: dst@0 = %s" % [round(pb[i].evaluate(0.0), 4) for i in sorted(pb)])
print("OBJ ROT: dst@33 = %s" % [round(pb[i].evaluate(33.0), 4) for i in sorted(pb)])
print("OBJ ROT: src@9 = %s" % [round(pa[i].evaluate(9.0), 4) for i in sorted(pa)])

# MhBone_000 location profile in dst
for tag, d in (("dst", dp), ("src", sp)):
    f = {i: c for (p, i), c in d.items() if p == 'pose.bones["MhBone_000"].location'}
    if f:
        for fr in (0, 8, 16, 24, 33, 40, 58):
            if fr <= max(k.co.x for c in f.values() for k in c.keyframe_points):
                print("%s 000.loc @%-3d = %s" % (tag, fr, [round(f[i].evaluate(float(fr)), 4) for i in sorted(f)]))

# scale curves presence
for tag, d in (("dst", dp), ("src", sp)):
    scales = sorted(p for p, i in d if ".scale" in p)
    print("%s scale paths: %d %s" % (tag, len(scales), scales[:12]))

# 030/047 scale values at seam frames
for bone in ("MhBone_030", "MhBone_047"):
    for tag, d in (("dst", dp), ("src", sp)):
        f = {i: c for (p, i), c in d.items() if p == 'pose.bones["%s"].scale' % bone}
        if f:
            vals = {}
            for fr in (0, 9, 33, 58):
                vv = [round(f[i].evaluate(float(fr)), 3) for i in sorted(f)]
                vals[fr] = vv
            print("%s %s scale: %s" % (tag, bone, vals))
        else:
            print("%s %s scale: NONE" % (tag, bone))

# dst curves ending at 31 (the 16 curves, 4 keys) - which bones
early = []
for f in B.fcurves:
    mx = max(k.co.x for k in f.keyframe_points)
    if 29 < mx < 32.5:
        early.append(f.data_path)
print("dst curves ending ~31: %s" % sorted(set(early)))

# key frame positions of dst location / 000 curves (to see authored key pattern)
for p in ("location", 'pose.bones["MhBone_000"].location'):
    f = dp.get((p, 1))
    if f:
        print("dst %s[1] key frames: %s" % (p, [round(k.co.x, 2) for k in f.keyframe_points]))
    f2 = sp.get((p, 1))
    if f2:
        xs = [k.co.x for k in f2.keyframe_points]
        print("src %s[1] key frames: %d keys, first=%s last=%s" % (p, len(xs), xs[:5], xs[-3:]))
print("### DONE")
