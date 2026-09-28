"""Re-run the whole 104 pipeline (merge -> rebake IK -> tag -> min2 -> export),
then binary-compare the fresh export against the currently deployed (curled-up)
w03_00.lmt and the reference files.  Headless:

  blender -b --python valor-remap-pipeline/mhgu/_rerun_and_compare.py
"""
import os
os.environ["MERGE_PAIRS"] = ("LMT::w03_00::103;;LMT::w07_sa::151_remap"
                             "|LMT::w03_00::104;;LMT::w07::192_remap")
os.environ["PRESERVE_NATIVE_IK"] = "0"

import bpy, importlib

# ---------- PHASE A: merge (executes 7_merge_into_native.py) ----------
src = open(r"E:\MHW-arts\valor-remap-pipeline\mhgu\7_merge_into_native.py", encoding="utf8").read()
exec(compile(src, "7_merge_inline", "exec"))

# ---------- PHASE B: re-bake IK (metres) ----------
mm = bpy.data.objects["m_mesh Armature"]
scn = bpy.context.scene
mg = importlib.import_module("MHW-FreeKinetics.blender.tetherOps")
act = bpy.data.actions["LMT::w03_00::104"]
mm.animation_data.action = act
mg.recalculateBodyIKBones(mm, act, [(247, 57, 'FULL'), (248, 40, 'FULL'),
                                     (249, 20, 'FULL'), (250, 16, 'FULL'), (252, 4, 'LOC')])
mg.recalculateGroundIKBones(mm, act)
mm.animation_data.action = None
print("[rebake] done", flush=True)

# ---------- PHASE D: LIMITS tags on IK fcurves ----------
TAG = {(247, "location"): 4, (247, "rotation_quaternion"): 6,
       (248, "location"): 4, (248, "rotation_quaternion"): 6,
       (249, "location"): 4, (249, "rotation_quaternion"): 14,
       (250, "location"): 4, (250, "rotation_quaternion"): 14,
       (251, "location"): 4, (251, "rotation_quaternion"): 2,
       (252, "location"): 4, (252, "rotation_quaternion"): 2,
       (253, "location"): 1, (253, "rotation_quaternion"): 2}
nt = 0
for fc in act.fcurves:
    for (bone, prop), buf in TAG.items():
        if fc.data_path == 'pose.bones["MhBone_%03d"].%s' % (bone, prop):
            for m2 in list(fc.modifiers):
                if m2.type == "LIMITS":
                    fc.modifiers.remove(m2)
            l = fc.modifiers.new("LIMITS")
            l.use_restricted_range = True
            l.mute = True
            l.frame_start = l.frame_end = 0
            l.min_x, l.min_y = buf, bone
            nt += 1
print("[tag]", nt, "tagged", flush=True)

# ---------- PHASE E: min-2-keys + save ----------
n2 = 0
for fc in act.fcurves:
    kp = fc.keyframe_points
    if len(kp) == 1:
        f0, v0 = kp[0].co[0], kp[0].co[1]
        kp.add(1)
        kp[1].co = (f0 + 1.0, v0)
        fc.update()
        n2 += 1
print("[min2]", n2, "bumped", flush=True)
bpy.ops.wm.save_mainfile()

# ---------- PHASE F: export (same code path as GUI button) ----------
scn.freehk_tether = mm
tree = bpy.data.node_groups["w03_00"]
out = [x for x in tree.nodes if x.bl_idname == "LMTFileNode"][0]
out.filepath = r"E:\MHW-arts\mhgu-export-test.lmt"
eo = importlib.import_module("MHW-FreeKinetics.operators.export_ops")
addon = bpy.context.preferences.addons.get(eo.__package__.split(".")[0])
eo.exportNodeToFile(out, addon.preferences)
print("[export] done", flush=True)

# ---------- PHASE G: binary compare ----------
sys_path = r"C:\Users\Afeather\AppData\Roaming\Blender Foundation\Blender\4.3\scripts\addons"
import sys
if sys_path not in sys.path:
    sys.path.insert(0, sys_path)
Lmt = importlib.import_module("MHW-FreeKinetics.struct.Lmt")
BASE = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00"
FILES = [
    ("FRESH-EXPORT (this run)", r"E:\MHW-arts\mhgu-export-test.lmt"),
    ("DEPLOYED   (9.28 20:00, curled)", BASE + r"\w03_00.lmt"),
    ("LUOCHA     (good ref)", BASE + r"\w03_00_luocha.lmt"),
    ("WRONGIK    (bad ref)", BASE + r"\w03_00-wrongik.lmt"),
    ("ORIGIN     (vanilla)", BASE + r"\w03_00-origin.lmt"),
]

def sig(path):
    l = Lmt.parseLMT(path)
    res = {}
    for a in l.ActionHeaders:
        if a.id in (103, 104):
            ids = sorted(set(b.boneId for b in a.bones))
            v249 = None
            v000 = None
            for b in a.bones:
                if b.boneId == 249 and b.usage == 1 and getattr(b, "keyframes", None):
                    t, k = 0, b.keyframes[0]
                    v249 = tuple(round(c, 2) for c in k.vec[:3]) if getattr(k, "vec", None) else None
                if b.boneId == 0 and b.usage == 1 and getattr(b, "keyframes", None):
                    k = b.keyframes[0]
                    v000 = tuple(round(c, 3) for c in k.vec[:3]) if getattr(k, "vec", None) else None
            res[a.id] = (len(a.bones), getattr(a, "frameCount", "?"), ids, v249, v000)
    return res

for label, path in FILES:
    print("=" * 8, label)
    try:
        r = sig(path)
    except Exception as e:
        print("   PARSE FAIL:", e)
        continue
    for eid in (103, 104):
        if eid not in r:
            print("   entry %d: MISSING" % eid)
            continue
        tracks, fc, ids, v249, v000 = r[eid]
        print("   entry %d: tracks=%-3d fc=%-5s 000=%s 013=%s 030=%s 047=%s ik=%s" % (
            eid, tracks, fc,
            0 in ids, 13 in ids, 30 in ids, 47 in ids,
            [b for b in (247, 248, 249, 250, 251, 252, 253) if b in ids]))
        print("      249loc[0]=%s  000loc[0]=%s" % (v249, v000))
print("ALL DONE", flush=True)
