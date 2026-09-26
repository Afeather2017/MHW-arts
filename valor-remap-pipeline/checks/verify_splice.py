"""Verify splice_actions.py output: seam continuity + expected doubled travel."""
import bpy

OUT = "plw_LongSword_100_001_Loop_Armature_remap_plus"
a = bpy.data.actions.get(OUT)
assert a is not None, "combined action missing"
lo, hi = a.frame_range
print("[check] '%s' range=%.0f..%.0f fcurves=%d" % (OUT, lo, hi, len(a.fcurves)))
assert abs(lo) < 1e-4 and abs(hi - 160) < 1e-4, "range wrong"

ok = True
for i in (0, 1):
    f = a.fcurves.find("location", index=i)
    v_before = f.evaluate(79.5)
    v_seam = f.evaluate(80)
    v_after = f.evaluate(80.5)
    v_end = f.evaluate(160)
    dup = sum(1 for k in f.keyframe_points if abs(k.co[0] - 80) < 1e-4)
    print("  obj location[%d] f79.5=%.4f f80=%.4f f80.5=%.4f f160=%.4f keys@80=%d" % (
        i, v_before, v_seam, v_after, v_end, dup))
    if abs(v_before - v_seam) > 1e-3 or abs(v_after - v_seam) > 1e-3:
        ok = False; print("  !!! seam jump")
    if abs(v_end - 2 * v_seam) > 1e-3:
        ok = False; print("  !!! end != 2x seam (offset math wrong)")

f249 = a.fcurves.find('pose.bones["MhBone_249"].location', index=0)
vb, vs, va = f249.evaluate(79.5), f249.evaluate(80), f249.evaluate(80.5)
dup249 = sum(1 for k in f249.keyframe_points if abs(k.co[0] - 80) < 1e-4)
print("  249 loc[0] f79.5=%.4f f80=%.4f f80.5=%.4f keys@80=%d (total=%d)" % (
    vb, vs, va, dup249, len(f249.keyframe_points)))
if abs(vb - vs) > 1e-3 or abs(va - vs) > 1e-3: ok = False; print("  !!! IK seam jump")
if dup249 != 1: ok = False; print("  !!! seam key not deduped to 1")

g = a.fcurves.find('pose.bones["MhBone_030"].scale', index=1)
gk = [(round(k.co[0]), k.co[1]) for k in g.keyframe_points]
print("  030 scale[1] keys:", gk)
if not (len(gk) == 2 and all(abs(v - 2.0) < 1e-6 for _, v in gk)):
    ok = False; print("  !!! grip keys wrong")

print("SPLICE_CHECK=%s" % ("PASS" if ok else "FAIL"))
