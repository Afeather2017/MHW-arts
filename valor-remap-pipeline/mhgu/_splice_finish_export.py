"""Finish the divide_valor_sheath splice for export:
 1. freehk.frameCount 33 -> 83 (0..82), probe the property first, abort if not found
 2. ensure freehk.tetherFrame bound (坑⑬)
 3. 6_tag recipe pass on the merged action: tag every in-recipe pose fcurve with
    LIMITS (min_x=bufferType, min_y=boneFunction) from vanilla entry 104 (+109
    scale override), REMOVE out-of-recipe channels (native parity - physics
    twins 070/072-077/081-085 etc. are not tracked by the native layout)
 4. add frame-0 hold keys to curves whose first key is at the seam (explicit
    constant coverage; clears MISSING_FIRST/REFERENCE_FRAME)
Idempotent. Saves only if frameCount probe succeeded.
"""
import bpy, re, importlib, sys

ACT = bpy.data.actions["divide_valor_sheath"]
ARM = bpy.data.objects.get("m_mesh Armature")
ORIGIN = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00-origin.lmt"
USAGE = {"rotation_quaternion": 0, "location": 1, "scale": 2}

# ---- 1. frameCount probe ----
fc_prop = None
if hasattr(ACT, "freehk"):
    for cand in ("frameCount", "frame_count"):
        if hasattr(ACT.freehk, cand):
            fc_prop = "freehk." + cand
            break
if fc_prop is None:
    for key, val in ACT.items():
        if "frame" in key.lower():
            fc_prop = "custom:" + key
print("[probe] frameCount property: %s = %s" % (fc_prop, ACT.freehk.frameCount if fc_prop == "freehk.frameCount" else ACT.get(fc_prop.split(":", 1)[1]) if fc_prop and fc_prop.startswith("custom:") else "?"))
if fc_prop is None:
    print("[abort] no frameCount property found")
    sys.exit(1)

def set_fc(v):
    if fc_prop.startswith("freehk."):
        setattr(ACT.freehk, fc_prop.split(".")[1], v)
    else:
        ACT[fc_prop.split(":", 1)[1]] = v

old = ACT.freehk.frameCount if fc_prop.startswith("freehk.") else ACT[fc_prop.split(":", 1)[1]]
set_fc(83)
print("[frameCount] %s -> 83" % old)

# ---- 2. tether ----
if ARM is not None and getattr(ACT.freehk, "tetherFrame", None) is None:
    ACT.freehk.tetherFrame = ARM
    print("[tether] bound to m_mesh Armature")
else:
    print("[tether] already bound: %s" % getattr(ACT.freehk, "tetherFrame", None))

# ---- 3. recipe + tag/remove ----
Lmt = importlib.import_module("MHW-FreeKinetics.struct.Lmt")
lmt = Lmt.parseLMT(ORIGIN)
recipe = {}
for b in [x for x in lmt.ActionHeaders if x.id == 104][0].bones:
    if b.boneId >= 0:
        recipe[(b.boneId, b.usage % 3)] = b.bufferType
for b in [x for x in [y for y in Lmt.parseLMT(ORIGIN).ActionHeaders if y.id == 109][0].bones
          if b.boneId in (30, 47) and b.usage == 2]:
    recipe[(b.boneId, 2)] = b.bufferType
print("[recipe] %d (bone,usage) rows" % len(recipe))

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

tagged = removed = 0
removed_paths = set()
for fc in list(ACT.fcurves):
    if not fc.data_path.startswith("pose.bones["):
        continue  # object-level: handled by restoreRootFromObject
    m = re.match(r'pose\.bones\["MhBone_(\d+)"\]\.(\w+)$', fc.data_path)
    if not m:
        continue
    bone_id, prop = int(m.group(1)), m.group(2)
    usage = USAGE.get(prop)
    if usage is None or (bone_id, usage) not in recipe:
        removed_paths.add(fc.data_path)
        ACT.fcurves.remove(fc)
        removed += 1
        continue
    tag(fc, bone_id, recipe[(bone_id, usage)])
    tagged += 1
print("[tag] tagged=%d removed=%d" % (tagged, removed))
print("[tag] removed paths: %s" % sorted(removed_paths))

# ---- 4. frame-0 hold keys ----
added = 0
for fc in ACT.fcurves:
    if fc.keyframe_points and fc.keyframe_points[0].co.x > 0.5:
        k0 = fc.keyframe_points[0]
        fc.keyframe_points.insert(0.0, k0.co.y)
        nk = fc.keyframe_points[0]
        nk.interpolation = "LINEAR"
        nk.handle_left_type = nk.handle_right_type = "VECTOR"
        fc.update()
        added += 1
print("[f0keys] added=%d" % added)

ACT.use_fake_user = True
bpy.ops.wm.save_mainfile()
print("SAVED")
