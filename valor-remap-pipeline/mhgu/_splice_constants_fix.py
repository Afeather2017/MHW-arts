"""Plain-copy the constant-only source curves onto divide_valor_sheath's tail.
The first splice skipped source curves with no keys >= f9 (single-key constants),
freezing shared channels like IK MhBone_251/253 at the user's sheath-start pose.
For every in-recipe (boneId,usage) constant curve: overwrite the tail (>= f33)
with the source constant; create + tag the curve if dst lacks it entirely."""
import bpy, re, importlib

DST = bpy.data.actions["divide_valor_sheath"]
SRC = bpy.data.actions["plw_LongSword_100_111_Armature_remap.short"]
ARM = bpy.data.objects.get("m_mesh Armature")
SEAM, CUT = 33.0, 9.0
END = 82.0
EPS = 1e-4
USAGE = {"rotation_quaternion": 0, "location": 1, "scale": 2}
ORIGIN = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00-origin.lmt"

Lmt = importlib.import_module("MHW-FreeKinetics.struct.Lmt")
recipe = {}
for b in [x for x in Lmt.parseLMT(ORIGIN).ActionHeaders if x.id == 104][0].bones:
    if b.boneId >= 0:
        recipe[(b.boneId, b.usage % 3)] = b.bufferType
for b in [x for x in [y for y in Lmt.parseLMT(ORIGIN).ActionHeaders if y.id == 109][0].bones
          if b.boneId in (30, 47) and b.usage == 2]:
    recipe[(b.boneId, 2)] = b.bufferType

def tag(fc, bone_id, buf):
    for m in list(fc.modifiers):
        if m.type == "LIMITS":
            fc.modifiers.remove(m)
    l = fc.modifiers.new("LIMITS")
    l.use_restricted_range = True
    l.mute = True
    l.frame_start = l.frame_end = 0
    l.min_x = buf
    l.min_y = bone_id

fixed = []
for fc in SRC.fcurves:
    if not fc.keyframe_points:
        continue
    if max(k.co.x for k in fc.keyframe_points) >= CUT - EPS:
        continue  # has real tail keys - already copied by the splice
    m = re.match(r'pose\.bones\["MhBone_(\d+)"\]\.(\w+)$', fc.data_path)
    if not m:
        continue
    bone_id, prop = int(m.group(1)), m.group(2)
    usage = USAGE.get(prop)
    if usage is None or (bone_id, usage) not in recipe:
        continue  # not tracked by the native layout
    const = fc.keyframe_points[0].co.y
    fd = DST.fcurves.find(fc.data_path, index=fc.array_index)
    if fd is None:
        gname = "MhBone_%03d" % bone_id
        if gname not in [g.name for g in DST.groups]:
            DST.groups.new(gname)
        fd = DST.fcurves.new(fc.data_path, index=fc.array_index, action_group=gname)
        tag(fd, bone_id, recipe[(bone_id, usage)])
        old_tail = None
        created = True
    else:
        created = False
        old_tail = max(fd.keyframe_points, key=lambda k: k.co.x).co.y
        if abs(old_tail - const) <= 1e-4:
            continue  # already holds the source constant
    pts = [(k.co.x, k.co.y) for k in fd.keyframe_points if k.co.x < SEAM - EPS]
    pts += [(SEAM, const), (END, const)]
    if created:
        pts.insert(0, (0.0, const))
    pts.sort()
    kp = fd.keyframe_points
    kp.clear()
    kp.add(len(pts))
    for k, (x, y) in zip(kp, pts):
        k.co = (x, y)
        k.interpolation = "LINEAR"
        k.handle_left_type = k.handle_right_type = "VECTOR"
    fd.update()
    fixed.append((fc.data_path, fc.array_index, round(const, 4),
                  "created" if created else "was %.4f" % old_tail))

print("[constants] fixed %d channels:" % len(fixed))
for p, i, c, note in fixed:
    print("   %s[%d] const=%.4f (%s)" % (p[:52], i, c, note))

# audit IK tail coverage again
print("--- IK audit ---")
for b in range(247, 254):
    for prop in ("location", "rotation_quaternion"):
        fc = DST.fcurves.find('pose.bones["MhBone_%d"].%s' % (b, prop), index=0)
        if fc:
            n_tail = sum(1 for k in fc.keyframe_points if k.co.x >= SEAM + 0.5)
            print("MhBone_%d %s: tail keys=%d last=%.0f" % (
                b, prop, n_tail, max(k.co.x for k in fc.keyframe_points)))
        else:
            print("MhBone_%d %s: NO CURVE" % (b, prop))

ACT = DST
print("frameCount=%s curves=%d" % (ACT.freehk.frameCount, len(ACT.fcurves)))
bpy.ops.wm.save_mainfile()
print("SAVED")
