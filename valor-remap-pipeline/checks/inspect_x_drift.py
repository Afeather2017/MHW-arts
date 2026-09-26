"""Investigate +X drift: do native MHW actions close their object travel at 0,
and do the Rise sources / migrated remaps?
"""
import bpy

bpy.ops.wm.open_mainfile(filepath=r"E:\MHW-arts\MHW-Valor.blend")

def loc_last_first(a, path, idx, rng):
    f = a.fcurves.find(path, index=idx)
    if f is None:
        return None
    lo, hi = rng
    return (f.evaluate(lo), f.evaluate(hi))

natives = [a for a in bpy.data.actions if a.name.startswith("LMT::")]
remaps = [a for a in bpy.data.actions if a.name.startswith("plw_") and a.name.endswith("_remap")]
srcs = [a for a in bpy.data.actions if a.name.startswith("plw_") and not a.name.endswith("_remap")]

print("=== native closure survey (object loc[0]/[1] last value) ===")
n_open0 = n_open1 = 0
worst0 = worst1 = 0.0
for a in natives:
    lo, hi = a.frame_range
    v0 = loc_last_first(a, "location", 0, (lo, hi))
    v1 = loc_last_first(a, "location", 1, (lo, hi))
    if v0 and abs(v0[1]) > 1e-3:
        n_open0 += 1; worst0 = max(worst0, abs(v0[1]))
    if v1 and abs(v1[1]) > 1e-3:
        n_open1 += 1; worst1 = max(worst1, abs(v1[1]))
print("natives=%d  open@X=%d (worst %.4f)  open@Y=%d (worst %.4f)" % (
    len(natives), n_open0, worst0, n_open1, worst1))

print("=== remap end-value survey (object loc[0] / loc[1] last) ===")
stats = {0: [], 1: []}
open_cnt = {0: 0, 1: 0}
for a in remaps:
    lo, hi = a.frame_range
    for i in (0, 1):
        v = loc_last_first(a, "location", i, (lo, hi))
        if v is None:
            continue
        stats[i].append(v[1])
        if abs(v[1]) > 1e-3:
            open_cnt[i] += 1
for i in (0, 1):
    vals = stats[i]
    if vals:
        print("  loc[%d]: open=%d/%d  last:min=%.4f max=%.4f mean=%+.4f" % (
            i, open_cnt[i], len(vals), min(vals), max(vals), sum(vals) / len(vals)))

print("=== source (Rise Cog) closure survey: X and Z last-first ===")
diffs = []
n_open = 0
for a in srcs:
    f = a.fcurves.find('pose.bones["Cog"].location', index=0)
    if f is None:
        continue
    lo, hi = a.frame_range
    d = f.evaluate(hi) - f.evaluate(lo)
    diffs.append(d)
    if abs(d) > 1e-3:
        n_open += 1
if diffs:
    print("  Cog X: open=%d/%d  (last-first): min=%.4f max=%.4f mean=%+.4f" % (
        n_open, len(diffs), min(diffs), max(diffs), sum(diffs) / len(diffs)))

print("=== detail: 112 ===")
for tag, name in (("SOURCE", "plw_LongSword_100_112_Armature"),
                  ("REMAP  ", "plw_LongSword_100_112_Armature_remap"),
                  ("NATIVE ", "LMT::w03_00::001")):
    a = bpy.data.actions.get(name)
    if a is None:
        print("  %s %s: MISSING" % (tag, name)); continue
    lo, hi = a.frame_range
    out = ["%s %s range=%.0f..%.0f" % (tag, name, lo, hi)]
    for path, idxs in (("location", (0, 1, 2)), ('pose.bones["Cog"].location', (0, 1, 2)),
                       ('pose.bones["MhBone_000"].location', (0, 1, 2))):
        for i in idxs:
            f = a.fcurves.find(path, index=i)
            if f:
                out.append("%s[%d] first=%.4f last=%.4f min=%.4f max=%.4f" % (
                    path.split('"')[-1].split('"]')[0] if "bones" in path else path,
                    i, f.evaluate(lo), f.evaluate(hi),
                    min(k.co[1] for k in f.keyframe_points),
                    max(k.co[1] for k in f.keyframe_points)))
    print("  " + " | ".join(out))
print("DONE")
