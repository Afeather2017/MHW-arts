"""MHGU chain STEP 6 (v2) - tag EVERY fcurve with LIMITS export metadata.

FreeKinetics' LMT writer exports ONLY fcurves carrying a LIMITS modifier:
  min_x = bufferType (quantisation algorithm), min_y = boneFunction (boneId).
Untagged fcurves are silently OMITTED - Blender plays the action fine via
data_paths, but the game gets a skeleton without body tracks and the hunter
collapses into a ball (binary-verified: bad export = 22 tracks, native = 82).

Recipe (boneId, usage) -> bufferType is read DYNAMICALLY from the vanilla
native entry (w03_00-origin.lmt entry 104), so it always matches the game's
own encoding table.  Channels the native layout does not track (scale on body
bones, _s physics twins 064-082) are REMOVED for native parity.

Run LAST, right before export.  Idempotent.
"""
import bpy, os, sys, re, shutil, time, importlib

WORK = os.environ.get("MHGU_WORK_BLEND", r"E:\MHW-arts\mhgu-valor-mhw.blend")
ORIGIN = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00-origin.lmt"
ORIGIN_ENTRY = 104
USAGE = {"rotation_quaternion": 0, "location": 1, "scale": 2}

bak = WORK + ".bak-" + time.strftime("%Y%m%d-%H%M%S")
shutil.copyfile(WORK, bak)
print("[backup] %s" % bak, flush=True)

bpy.ops.wm.open_mainfile(filepath=WORK)

# ---- recipe from the vanilla native entry ----
Lmt = importlib.import_module("MHW-FreeKinetics.struct.Lmt")
lmt = Lmt.parseLMT(ORIGIN)
a = [x for x in lmt.ActionHeaders if x.id == ORIGIN_ENTRY][0]
recipe = {}
for b in a.bones:
    if b.boneId >= 0:
        recipe[(b.boneId, b.usage % 3)] = b.bufferType
print("[recipe] %d (bone,usage) entries from origin entry %d" % (len(recipe), ORIGIN_ENTRY), flush=True)

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

remaps = sorted([x for x in bpy.data.actions
                 if x.name.endswith("_remap") and "mhgu_source" in x], key=lambda x: x.name)

# ---- action-level tether binding (2026-09-28 坑⑬) ----
# The exporter's LMTActionParser takes its tether from action.freehk.tetherFrame,
# NOT from the scene.  ARP creates fresh actions, so remaps are born unbound
# (tetherFrame=None); wiring an unbound action straight into an entry exports
# raw metre floats (no tether-inverse pass) and drops the Root tracks
# (restoreRootFromObject never runs) = the hunter collapses in game.
tethered = 0
arm = bpy.data.objects.get("m_mesh Armature")
for act in remaps:
    if arm is not None and act.freehk.tetherFrame is None:
        act.freehk.tetherFrame = arm
        tethered += 1
print("[tether] %d remaps bound to m_mesh Armature" % tethered, flush=True)

tagged = removed = 0
for act in remaps:
    for fc in list(act.fcurves):
        if not fc.data_path.startswith("pose.bones["):
            continue  # object-level curves: handled by restoreRootFromObject
        m = re.match(r'pose\.bones\["MhBone_(\d+)"\]\.(\w+)$', fc.data_path)
        if not m:
            continue
        bone_id, prop = int(m.group(1)), m.group(2)
        usage = USAGE.get(prop)
        if usage is None or (bone_id, usage) not in recipe:
            act.fcurves.remove(fc)   # native layout doesn't track this channel
            removed += 1
            continue
        tag(fc, bone_id, recipe[(bone_id, usage)])
        tagged += 1
print("[tag] %d remaps: %d fcurves tagged, %d non-native channels removed" % (len(remaps), tagged, removed), flush=True)

# ---- verify ----
bad = []
untethered = [act.name for act in remaps if act.freehk.tetherFrame is None]
if untethered:
    bad += [(n, "<action>", "tetherFrame=None") for n in untethered]
for act in remaps:
    for fc in act.fcurves:
        if not fc.data_path.startswith("pose.bones["):
            continue
        m = re.match(r'pose\.bones\["MhBone_(\d+)"\]\.(\w+)$', fc.data_path)
        if not m:
            bad.append((act.name, fc.data_path, "unrecognized path")); continue
        bone_id, usage = int(m.group(1)), USAGE[m.group(2)]
        got = [(int(md.min_x), int(md.min_y)) for md in fc.modifiers if md.type == "LIMITS"]
        if (recipe[(bone_id, usage)], bone_id) not in got:
            bad.append((act.name, fc.data_path, got))
print("[verify] metadata problems: %s" % (bad[:6] if bad else "none"), flush=True)
if bad:
    print("!!! NOT SAVING"); sys.exit(1)

bpy.ops.wm.save_mainfile()
print("[done] saved:", WORK, flush=True)
