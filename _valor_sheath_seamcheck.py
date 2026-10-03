# -*- coding: utf-8 -*-
"""Seam diagnostics for the valor sheath build:
A@join vs B@0 angular deltas (body bones), B self-loop seam, A-side scan."""
import math

import bpy

A = bpy.data.actions["LMT::w07::195_remap"]
B = bpy.data.actions["LMT::w07::196_remap"]


def phys(bone):
    n = bone.replace("MhBone_", "")
    return n.isdigit() and 247 <= int(n) <= 254


def quat_at(action, path_prefix_bone, t):
    """{bone: [q0..q3]} evaluated at frame t."""
    out = {}
    for fc in action.fcurves:
        if ".rotation_quaternion" not in fc.data_path:
            continue
        bone = fc.data_path.split('"')[1]
        out.setdefault(bone, [None] * 4)[fc.array_index] = fc.evaluate(t)
    return out


def ang(qa, qb):
    d = sum(a * b for a, b in zip(qa, qb))
    if abs(d) < 1e-9:
        return 0.0
    return 2 * math.degrees(math.acos(max(-1.0, min(1.0, abs(d)))))


def report(tag, pairs):
    rows = sorted(((ang(qa, qb), bone) for bone, (qa, qb) in pairs.items()
                   if None not in qa and None not in qb), reverse=True)
    bad = [r for r in rows if r[0] > 3]
    print("%s: n=%d over3deg=%d" % (tag, len(rows), len(bad)))
    for a, bone in rows[:8]:
        print("  %7.2f  %s%s" % (a, bone, "  (phys)" if phys(bone) else ""))


# 1) A@50 vs B@0  (body bones only)
qb0 = quat_at(B, None, 0)
qa50 = quat_at(A, None, 50)
report("A@50 vs B@0 [body]", {b: (qa50[b], qb0[b]) for b in qa50
                              if b in qb0 and not phys(b)})

# 2) B self-loop: B@0 vs B@169 (body bones)
qb_last = quat_at(B, None, 169)
report("B@0 vs B@169 [body]", {b: (qb0[b], qb_last[b]) for b in qb0
                               if b in qb_last and not phys(b)})

# 3) A-side: best join t in 40..62 vs B@0 (sum of body-bone angles)
tot = []
for t in range(40, 63):
    qat = quat_at(A, None, t)
    s = sum(ang(qat[b], qb0[b]) for b in qat if b in qb0 and not phys(b))
    tot.append((s, t))
tot.sort()
print("A-side join scan (sum body angles vs B@0): best5 =",
      [(t, round(s, 1)) for s, t in tot[:5]])

# 4) B-side: best t' in 0..40 matching A@50
tot2 = []
qa50b = qa50
qbt = {}
for tp in range(0, 41, 2):
    qbt = quat_at(B, None, tp)
    s = sum(ang(qa50b[b], qbt[b]) for b in qa50b if b in qbt and not phys(b))
    tot2.append((s, tp))
tot2.sort()
print("B-side scan (sum body angles A@50 vs B@t): best5 =",
      [(t, round(s, 1)) for s, t in tot2[:5]])
