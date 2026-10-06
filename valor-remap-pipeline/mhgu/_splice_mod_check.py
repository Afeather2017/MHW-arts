"""Inspect fcurve modifiers on src/dst + raw seam values of the offset channels (read-only)."""
import bpy
from collections import Counter

SRC = bpy.data.actions["plw_LongSword_100_111_Armature_remap.short"]
DST = bpy.data.actions["divide_valor_sheath"]

for tag, A in (("SRC", SRC), ("DST", DST)):
    types = Counter()
    paths = {}
    for f in A.fcurves:
        for m in f.modifiers:
            types[m.type] += 1
            paths.setdefault(m.type, set()).add(f.data_path)
    print("%s modifiers: %s" % (tag, dict(types)))
    for t, ps in paths.items():
        ps = sorted(ps)
        print("   %s on %d paths, e.g. %s" % (t, len(ps), ps[:6]))

# which dst curves have modifiers - do they overlap the seam-critical channels?
COG = 'pose.bones["MhBone_000"].location'
for p in ("location", COG):
    for i in range(3):
        f = DST.fcurves.find(p, index=i)
        fsrc = SRC.fcurves.find(p, index=i)
        print("DST %s[%d]: mods=%s" % (p[:40], i, [(m.type, m.mode) for m in f.modifiers] if f else None))
        print("SRC %s[%d]: mods=%s" % (p[:40], i, [(m.type, m.mode) for m in fsrc.modifiers] if fsrc else None))

# raw vs evaluated at seam for these channels
print("--- raw key values ---")
for tag, A in (("SRC", SRC), ("DST", DST)):
    for p in ("location", COG):
        for i in range(3):
            f = A.fcurves.find(p, index=i)
            if not f:
                continue
            raw = {}
            for k in f.keyframe_points:
                if abs(k.co.x - 33.0) < 0.01 or abs(k.co.x - 9.0) < 0.01 or abs(k.co.x - 34.0) < 0.01:
                    raw[round(k.co.x, 2)] = round(k.co.y, 5)
            print("%s %s[%d] raw keys near seam: %s" % (tag, p[:40], i, raw))
            for fr in (8.0, 9.0, 10.0, 33.0, 34.0):
                print("      evaluate(%.0f)=%.5f" % (fr, f.evaluate(fr)))

# sample of dst curves WITH modifiers: which paths, first few
cnt = 0
for f in DST.fcurves:
    if f.modifiers:
        print("DST modded curve: %s[%d] mods=%s keys=%d range=%.0f..%.0f" % (
            f.data_path[:50], f.array_index, [m.type for m in f.modifiers],
            len(f.keyframe_points),
            min(k.co.x for k in f.keyframe_points), max(k.co.x for k in f.keyframe_points)))
        cnt += 1
        if cnt >= 12:
            break
print("### DONE")
