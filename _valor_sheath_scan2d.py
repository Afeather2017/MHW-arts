# -*- coding: utf-8 -*-
"""2D pose-match scan between 195 and 196 to find the true join pair.
Metric per (tA, tB):
  sum(1-|dot|) over body-bone quats (sign-aligned),
  position distances for IK targets 247/248/252/254, COG 000, object root.
Bones 247-254 rotations excluded (runtime IK targets, orientation less visual).
"""
import math

import bpy

A = bpy.data.actions["LMT::w07::195_remap"]
B = bpy.data.actions["LMT::w07::196_remap"]
PHYS = set(range(247, 255))


def bone_of(path):
    return path.split('"')[1] if "pose.bones[" in path else None


def frames_of(action):
    return int(round(max(k.co[0] for fc in action.fcurves
                         for k in fc.keyframe_points)))


NA, NB = frames_of(A), frames_of(B)


def snapshot(action, t):
    """per-frame: {bone: quat4}, {key: loc3} for IK/COG + root."""
    q = {}
    loc = {}
    for fc in action.fcurves:
        bone = bone_of(fc.data_path)
        v = fc.evaluate(t)
        if bone and ".rotation_quaternion" in fc.data_path:
            q.setdefault(bone, [None] * 4)[fc.array_index] = v
        elif ".location" in fc.data_path:
            n = bone.replace("MhBone_", "") if bone else ""
            if (n.isdigit() and int(n) in PHYS) or bone == "MhBone_000":
                loc.setdefault(bone if bone != "MhBone_000" else "000",
                              [0.0] * 3)[fc.array_index] = v
        elif fc.data_path == "location":
            loc.setdefault("root", [0.0] * 3)[fc.array_index] = v
    return q, loc


print("[scan] snapshots A(0..%d) B(0..%d) ..." % (NA, NB), flush=True)
Aq, Al, Bq, Bl = {}, {}, {}, {}
for t in range(0, NA + 1):
    Aq[t], Al[t] = snapshot(A, t)
for t in range(0, NB + 1):
    Bq[t], Bl[t] = snapshot(B, t)
print("[scan] done", flush=True)


def pose_cost(qa, qb):
    s = 0.0
    worst = ("", 0.0)
    for bone, qa4 in qa.items():
        qb4 = qb.get(bone)
        if qb4 is None or None in qa4 or None in qb4:
            continue
        n = bone.replace("MhBone_", "")
        if n.isdigit() and int(n) in PHYS:
            continue
        d = abs(sum(a * b for a, b in zip(qa4, qb4)))
        c = 1.0 - min(1.0, d)
        s += c
        if c > worst[1]:
            worst = (bone, c)
    return s, worst


def loc_cost(la, lb):
    s = 0.0
    parts = {}
    for k, w in (("MhBone_247", 3.0), ("MhBone_248", 3.0), ("MhBone_252", 1.0),
                 ("MhBone_254", 1.0), ("000", 2.0), ("root", 2.0)):
        pa, pb = la.get(k), lb.get(k)
        if not pa or not pb:
            continue
        d = math.sqrt(sum((x - y) ** 2 for x, y in zip(pa, pb)))
        parts[k] = d
        s += w * d
    return s, parts


# coarse 2D scan (step 2) then refine around best (step 1)
coarse = []
for ta in range(0, NA + 1, 2):
    for tb in range(0, NB + 1, 2):
        pa, wa = pose_cost(Aq[ta], Bq[tb])
        pl, parts = loc_cost(Al[ta], Bl[tb])
        coarse.append((pa + 100.0 * pl, ta, tb, pa, pl, wa))
coarse.sort()
print("[scan] coarse top5:")
for c in coarse[:5]:
    print("  cost=%.3f tA=%d tB=%d pose=%.3f loc=%.4f worst=%s" %
          (c[0], c[1], c[2], c[3], c[4], c[5]), flush=True)

bta, btb = coarse[0][1], coarse[0][2]
fine = []
for ta in range(max(0, bta - 3), min(NA, bta + 3) + 1):
    for tb in range(max(0, btb - 3), min(NB, btb + 3) + 1):
        pa, wa = pose_cost(Aq[ta], Bq[tb])
        pl, parts = loc_cost(Al[ta], Bl[tb])
        fine.append((pa + 100.0 * pl, ta, tb, pa, pl, wa, parts))
fine.sort()
print("[scan] fine top8:")
for c in fine[:8]:
    print("  cost=%.3f tA=%d tB=%d pose=%.3f loc=%.4f worst=%s parts=%s" %
          (c[0], c[1], c[2], c[3], c[4], c[5],
           {k: round(v, 3) for k, v in c[6].items()}), flush=True)

# landscape along tA at best tB
btb = fine[0][2]
print("[scan] tA landscape @tB=%d:" % btb)
row = []
for ta in range(0, NA + 1):
    pa, _ = pose_cost(Aq[ta], Bq[btb])
    pl, _ = loc_cost(Al[ta], Bl[btb])
    row.append((pa + 100.0 * pl, ta))
row.sort()
print("  best10 tA:", [(t, round(c, 2)) for c, t in row[:10]], flush=True)
