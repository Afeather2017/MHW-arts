"""Revert the root-motion welding on divide_valor_sheath's appended tail (frames >= 33):
plain-copy semantics for the two world-coordinate channel families (object.location,
MhBone_000.location) - source values as-is, no offsets. Rotation/IK channels never had
offsets and stay untouched. Also audits that the IK bone tail keys exist."""
import bpy

DST = bpy.data.actions["divide_valor_sheath"]
SRC = bpy.data.actions["plw_LongSword_100_111_Armature_remap.short"]
SEAM, CUT, SHIFT = 33.0, 9.0, 24.0
EPS = 1e-4

def copy_plain(path):
    for i in range(3):
        fd = DST.fcurves.find(path, index=i)
        fs = SRC.fcurves.find(path, index=i)
        if not fd or not fs:
            print("MISSING %s[%d]" % (path, i))
            continue
        keep = [(k.co.x, k.co.y, tuple(k.handle_left), tuple(k.handle_right),
                 k.interpolation, k.handle_left_type, k.handle_right_type)
                for k in fd.keyframe_points if k.co.x < SEAM - EPS]
        src_keys = [(k.co.x, k.co.y, tuple(k.handle_left), tuple(k.handle_right),
                     k.interpolation, k.handle_left_type, k.handle_right_type)
                    for k in fs.keyframe_points]
        new = list(keep)
        if not any(abs(x - CUT) < 1e-3 for x, *_ in src_keys):
            v = fs.evaluate(CUT)
            new.append((SEAM, v, (SEAM, v), (SEAM, v), "LINEAR", "VECTOR", "VECTOR"))
        for (x, y, hl, hr, interp, hlt, hrt) in src_keys:
            if x >= CUT - EPS:
                new.append((x + SHIFT, y, (hl[0] + SHIFT, hl[1]), (hr[0] + SHIFT, hr[1]),
                            interp, hlt, hrt))
        new.sort(key=lambda d: d[0])
        kp = fd.keyframe_points
        kp.clear()
        kp.add(len(new))
        for k, (x, y, hl, hr, interp, hlt, hrt) in zip(kp, new):
            k.co = (x, y)
            k.handle_left = hl
            k.handle_right = hr
            k.interpolation = interp
            k.handle_left_type = hlt
            k.handle_right_type = hrt
        fd.update()
        show = [(round(k.co.x), round(k.co.y, 4)) for k in fd.keyframe_points
                if 31.5 <= k.co.x <= 34.5 or k.co.x >= 81.5]
        print("%s[%d]: n=%d tail-seam %s" % (path[:44], i, len(fd.keyframe_points), show))

for p in ("location", 'pose.bones["MhBone_000"].location'):
    copy_plain(p)

# audit: IK bones tail presence (must be untouched by this patch)
print("--- IK audit ---")
for b in range(247, 254):
    for prop in ("location", "rotation_quaternion"):
        fc = DST.fcurves.find('pose.bones["MhBone_%d"].%s' % (b, prop), index=0)
        if fc:
            n_tail = sum(1 for k in fc.keyframe_points if k.co.x >= SEAM + 0.5)
            mx = max(k.co.x for k in fc.keyframe_points)
            print("MhBone_%d %s: tail keys=%d last=%.0f" % (b, prop, n_tail, mx))
        else:
            print("MhBone_%d %s: NO CURVE" % (b, prop))

bpy.ops.wm.save_mainfile()
print("SAVED")
