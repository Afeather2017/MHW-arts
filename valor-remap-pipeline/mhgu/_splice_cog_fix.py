"""Repair the appended MhBone_000.location (and object.location) tail keys in
divide_valor_sheath: the splice computed value offsets from fcurve.evaluate(),
which is clamped by LIMITS modifiers - recompute the offsets from RAW keyframes
and shift the appended keys (x >= 33.5, i.e. everything not authored by the
user's 0..33 segment) by (raw_delta - applied_delta)."""
import bpy

DST = bpy.data.actions["divide_valor_sheath"]
SRC = bpy.data.actions["plw_LongSword_100_111_Armature_remap.short"]
SEAM = 33.0
CUT = 9.0
APPLIED = {  # offsets used by the first splice pass (from evaluate())
    "location": {0: 0.3679, 1: -0.0987, 2: 0.0},
    'pose.bones["MhBone_000"].location': {0: -0.1252, 1: 0.012, 2: -0.2297},
}

def rawval(fc, fr):
    """raw key value with constant-hold semantics: last key at/before fr, else first key"""
    pts = fc.keyframe_points
    cand = None
    for k in pts:
        if k.co.x <= fr + 1e-3:
            cand = k
        else:
            break
    return (cand or pts[0]).co.y

for path in APPLIED:
    for i in range(3):
        fs = SRC.fcurves.find(path, index=i)
        fd = DST.fcurves.find(path, index=i)
        if not fs or not fd:
            print("MISSING curve %s[%d]" % (path, i))
            continue
        raw_delta = rawval(fd, SEAM) - rawval(fs, CUT)
        corr = raw_delta - APPLIED[path][i]
        n = 0
        for k in fd.keyframe_points:
            if k.co.x >= SEAM + 0.5:  # appended keys only
                k.co.y += corr
                k.handle_left.y += corr
                k.handle_right.y += corr
                n += 1
        fd.update()
        print("%s[%d]: raw_delta=%.5f applied=%.5f corr=%+.5f -> %d keys shifted" %
              (path[:42], i, raw_delta, APPLIED[path][i], corr, n))
        pops = [(round(k.co.x), round(k.co.y, 5)) for k in fd.keyframe_points
                if 31.5 <= k.co.x <= 36.5]
        print("   raw keys around seam: %s" % pops)

bpy.ops.wm.save_mainfile()
print("SAVED")
