# -*- coding: utf-8 -*-
"""全字节级 LMT 对比: 差异字节归属到 entry 区域 + 差异 entry 的 TIML 结构化对比。
用法: python _bytediff_lmt.py <a.lmt> <b.lmt> [--timl 106,140,...]"""
import struct, sys
sys.path.insert(0, r"E:\MHW-arts")
import dump_timl_frames as D

pa, pb = sys.argv[1], sys.argv[2]
timl_focus = set()
if "--timl" in sys.argv:
    timl_focus = {int(x) for x in sys.argv[sys.argv.index("--timl") + 1].split(",")}

bufA = open(pa, "rb").read()
bufB = open(pb, "rb").read()
print("sizes: %d vs %d" % (len(bufA), len(bufB)))
n, actA = D.parse_lmt(bufA)
n2, actB = D.parse_lmt(bufB)
assert n == n2

# 差异字节偏移 (8 字节步进粗查 + 精确定位)
diffs = []
mvA, mvB = memoryview(bufA), memoryview(bufB)
i = 0
L = min(len(bufA), len(bufB))
while i < L:
    if bufA[i] != bufB[i]:
        j = i
        while j < L and bufA[j] != bufB[j]:
            j += 1
        diffs.append((i, j))
        i = j
    else:
        i += 1
print("differing byte runs: %d (%d bytes)" % (len(diffs), sum(j - i for i, j in diffs)))

# 区域归属: 每个 entry 的 [entryOff, +?) / fcurve 区 / timl 区 起点
regions = []  # (start, end, label)
allA = sorted(set([a["offset"] for a in actA.values()] +
                  [a["fcurveOffset"] for a in actA.values() if a["fcurveOffset"]] +
                  [a["timlOffset"] for a in actA.values() if a["timlOffset"]]))
bounds = allA + [len(bufA)]
def bucket(off):
    for k in range(len(bounds) - 1):
        if bounds[k] <= off < bounds[k + 1]:
            return bounds[k]
    return None

def owner(start):
    # start 是某区域的起点, 找拥有它的 entry
    for eid, a in actA.items():
        if start == a["offset"]:
            return eid, "hdr"
        if start == a["fcurveOffset"]:
            return eid, "fcurves"
        if start == a["timlOffset"]:
            return eid, "timl"
    return None, "?"

per_entry = {}
for i, j in diffs:
    s = bucket(i)
    eid, kind = (owner(s) if s is not None else (None, "gap"))
    per_entry.setdefault((eid, kind), [0, i, j])
    per_entry[(eid, kind)][0] += j - i

print("")
print("per-region diff summary:")
for (eid, kind), (cnt, first, last) in sorted(per_entry.items(), key=lambda kv: (kv[0][0] is None, kv[0][0] or 0)):
    print("  entry %-5s %-8s %6d bytes differ (first@0x%x last@0x%x)" % (eid, kind, cnt, first, last))

# TIML 结构化对比 (只看 focus)
def timl_struct(buf, a):
    if not a["timlOffset"]:
        return None
    t = D.parse_timl(buf, a["timlOffset"])
    # 归一化: 只留语义内容
    types = []
    for typ in t["types"]:
        xfs = []
        for xf in typ["transforms"]:
            kfs = [(k["frame"], k["value"], k["controlL"], k["controlR"], k["transition"], k["easing"])
                   for k in xf["keyframes"]]
            xfs.append((xf["name"], xf["dataType"], tuple(kfs)))
        types.append((typ["name"], tuple(xfs)))
    return dict(animLen=t["animationLength"], loopStart=t["loopStartPoint"], loopCtrl=t["loopControl"],
                labelHash=t["labelHash"], types=types)

for eid in sorted(timl_focus):
    a, b = actA.get(eid), actB.get(eid)
    if not a or not b or not a["timlOffset"] or not b["timlOffset"]:
        print("entry %d: timl missing (A=%s B=%s)" % (eid, bool(a and a["timlOffset"]), bool(b and b["timlOffset"])))
        continue
    sa, sb = timl_struct(bufA, a), timl_struct(bufB, b)
    print("")
    print("entry %d TIML: animLen %s vs %s | loopStart %s vs %s | loopCtrl %s vs %s" % (
        eid, sa["animLen"], sb["animLen"], sa["loopStart"], sb["loopStart"], sa["loopCtrl"], sb["loopCtrl"]))
    na = {t[0]: t for t in sa["types"]}
    nb = {t[0]: t for t in sb["types"]}
    for name in sorted(set(na) | set(nb)):
        ta, tb = na.get(name), nb.get(name)
        if ta == tb:
            continue
        if ta is None or tb is None:
            print("  type %s: %s" % (name, "only-in-A" if tb is None else "only-in-B"))
            continue
        xa = {x[0]: x for x in ta[1]}
        xb = {x[0]: x for x in tb[1]}
        for xn in sorted(set(xa) | set(xb)):
            x1, x2 = xa.get(xn), xb.get(xn)
            if x1 == x2:
                continue
            if x1 is None or x2 is None:
                print("  [%s] xf %s: %s" % (name, xn, "only-in-A" if x2 is None else "only-in-B"))
                continue
            ka = {k[0]: k for k in x1[2]}
            kb = {k[0]: k for k in x2[2]}
            only_a = sorted(set(ka) - set(kb))
            only_b = sorted(set(kb) - set(ka))
            changed = [(f, ka[f], kb[f]) for f in sorted(set(ka) & set(kb)) if ka[f] != kb[f]]
            if only_a: print("  [%s] xf %s: keys only-in-A frames=%s" % (name, xn, only_a[:8]))
            if only_b: print("  [%s] xf %s: keys only-in-B frames=%s" % (name, xn, only_b[:8]))
            for f, v1, v2 in changed[:8]:
                print("  [%s] xf %s: key@f%s A=%s B=%s" % (name, xn, f, v1, v2))
