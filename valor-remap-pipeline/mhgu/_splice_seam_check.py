"""Seam analysis for divide_valor_sheath + plw_LongSword_100_111_Armature_remap.short (read-only)."""
import bpy, math

SRC = "plw_LongSword_100_111_Armature_remap.short"
DST = "divide_valor_sheath"
A = bpy.data.actions[SRC]
B = bpy.data.actions[DST]

# --- armature bone coverage ---
arm = bpy.data.objects.get("m_mesh Armature")
arm_bones = {b.name for b in arm.data.bones} if arm else set()
print("m_mesh Armature bones: %d" % len(arm_bones))

src_paths = {(f.data_path, f.array_index): f for f in A.fcurves}
dst_paths = {(f.data_path, f.array_index): f for f in B.fcurves}

def bonename(p):
    return p.split('["')[1].split('"]')[0] if '["' in p else None

src_bones = {bonename(p) for p, _ in src_paths if bonename(p)}
dst_bones = {bonename(p) for p, _ in dst_paths if bonename(p)}
print("src bones missing from m_mesh Armature:", sorted(src_bones - arm_bones))
print("src-only bones (not in dst):", sorted(src_bones - dst_bones))
print("dst-only bones (not in src):", sorted(dst_bones - src_bones))
print("dst paths missing in src:", len(set(p for p, _ in dst_paths) - set(p for p, _ in src_paths)))
for p in sorted(set(p for p, _ in dst_paths) - set(p for p, _ in src_paths)):
    print("   dst-only path:", p)

# --- source key density / constant curves ---
const_only, keys_ge9 = [], 0
for (p, i), fc in src_paths.items():
    xs = [k.co.x for k in fc.keyframe_points]
    if max(xs) < 9.0 - 1e-4:
        const_only.append((p, i, fc.keyframe_points[0].co.y))
    keys_ge9 += sum(1 for x in xs if x >= 9.0 - 1e-4)
print("src curves total=%d  constant(<f9)=%d  keys>=9 total=%d" % (len(src_paths), len(const_only), keys_ge9))
from collections import Counter
dens = Counter(len(fc.keyframe_points) for fc in A.fcurves)
print("src keys-per-curve histogram:", dict(sorted(dens.items())))
dens_b = Counter(len(fc.keyframe_points) for fc in B.fcurves)
print("dst keys-per-curve histogram:", dict(sorted(dens_b.items())))

# --- seam pose deltas ---
J = 33.0
loc_pos = {0: "locX", 1: "locY", 2: "locZ"}
def delta(fa, fb, fr_a, fr_b):
    """per-component abs diff; for quats use angle via dot."""
    n = max(fa.array_index, fb.array_index) + 1
    va = [fa.evaluate(fr_a) if fa and fa.array_index == k else None for k in range(4)]
    # safer: evaluate per index
    return None

def quat_angle(fcs_a, fcs_b, fr_a, fr_b):
    qa = [fcs_a[i].evaluate(fr_a) if i in fcs_a else (1.0 if i == 0 else 0.0) for i in range(4)]
    qb = [fcs_b[i].evaluate(fr_b) if i in fcs_b else (1.0 if i == 0 else 0.0) for i in range(4)]
    d = abs(sum(a * b for a, b in zip(qa, qb)))
    return 2 * math.acos(min(1.0, d))  # radians

# group by data_path
from collections import defaultdict
sp = defaultdict(dict)
dp = defaultdict(dict)
for (p, i), fc in src_paths.items():
    sp[p][i] = fc
for (p, i), fc in dst_paths.items():
    dp[p][i] = fc

shared = sorted(set(sp) & set(dp))
print("shared paths: %d" % len(shared))
tot8 = tot9 = 0.0
rows = []
for p in shared:
    fa, fb = sp[p], dp[p]
    if "rotation_quaternion" in p:
        d8 = quat_angle(fa, fb, 8.0, J)
        d9 = quat_angle(fa, fb, 9.0, J)
    else:
        d8 = d9 = 0.0
        for i in fb:
            if i in fa:
                d8 += abs(fa[i].evaluate(8.0) - fb[i].evaluate(J))
                d9 += abs(fa[i].evaluate(9.0) - fb[i].evaluate(J))
    rows.append((p, d8, d9))
    tot8 += d8 * d8
    tot9 += d9 * d9

print("L2 over shared paths: |dst@33 - src@8| = %.5f   |dst@33 - src@9| = %.5f" % (math.sqrt(tot8), math.sqrt(tot9)))
rows.sort(key=lambda r: -abs(r[1] - r[2]))
print("worst mismatches (path, d8, d9):")
for p, d8, d9 in rows[:15]:
    print("   %-55s d8=%.5f d9=%.5f" % (p[:55], d8, d9))

# --- root motion offset ---
off = []
for i in range(3):
    fa = sp.get("location", {}).get(i)
    fb = dp.get("location", {}).get(i)
    va = fa.evaluate(9.0) if fa else 0.0
    vb = fb.evaluate(J) if fb else 0.0
    off.append(vb - va)
print("root offset dst@33 - src@9 = (%.4f, %.4f, %.4f)" % tuple(off))
fa8 = sp.get("location", {}).get(0)
if fa8:
    print("src object loc: f8=(%.4f,%.4f,%.4f) f9=(%.4f,%.4f,%.4f) f58=(%.4f,%.4f,%.4f)" % (
        *[sp["location"][i].evaluate(8.0) for i in range(3)],
        *[sp["location"][i].evaluate(9.0) for i in range(3)],
        *[sp["location"][i].evaluate(58.0) for i in range(3)]))
if dp.get("location"):
    print("dst object loc: f33=(%.4f,%.4f,%.4f) f0=(%.4f,%.4f,%.4f)" % (
        *[dp["location"][i].evaluate(J) for i in range(3)],
        *[dp["location"][i].evaluate(0.0) for i in range(3)]))
# 000 height (locY) continuity
for tag, d, fr in (("src", sp, 9.0), ("dst", dp, J)):
    f = d.get('pose.bones["MhBone_000"].location', {})
    if f:
        print("%s MhBone_000 loc @their-seam = (%.4f, %.4f, %.4f)" % (tag, *[f[i].evaluate(fr) for i in range(3) if i in f]))
print("### DONE")
