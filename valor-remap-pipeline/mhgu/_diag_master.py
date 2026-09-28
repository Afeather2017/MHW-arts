"""Which 2 tracks differ (deployed 79 vs fresh 81) + MASTER's 104 tag state."""
import sys, importlib
sys.path.insert(0, r"C:\Users\Afeather\AppData\Roaming\Blender Foundation\Blender\4.3\scripts\addons")
Lmt = importlib.import_module("MHW-FreeKinetics.struct.Lmt")
BASE = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00"

def ids_of(path, eid=104):
    l = Lmt.parseLMT(path)
    a = [x for x in l.ActionHeaders if x.id == eid][0]
    return sorted((b.boneId, b.usage) for b in a.bones)

dep = ids_of(BASE + r"\w03_00.lmt")
frs = ids_of(r"E:\MHW-arts\mhgu-export-test.lmt")
sd, sf = set(dep), set(frs)
print("deployed tracks=%d fresh tracks=%d" % (len(dep), len(frs)))
print("only in FRESH (missing from deployed):", sorted(sf - sd))
print("only in DEPLOYED:", sorted(sd - sf))

import bpy
bpy.ops.wm.open_mainfile(filepath=r"E:\MHW-arts\MHW-Valor.blend")
for name in ("LMT::w03_00::104", "LMT::w03_00::103", "LMT::w07::192_remap"):
    a = bpy.data.actions.get(name)
    if a is None:
        print("MASTER:", name, "-> MISSING")
        continue
    ik = [fc for fc in a.fcurves
          if any(("MhBone_%03d" % n) in fc.data_path for n in (247,248,249,250,251,252,253))]
    tagged = [fc for fc in ik if any(m.type == "LIMITS" and getattr(m, "min_x", 0) for m in fc.modifiers)]
    single = sum(1 for fc in a.fcurves if len(fc.keyframe_points) == 1)
    try:
        fh_fc = a.freehk.frameCount
    except Exception:
        fh_fc = "?"
    print("MASTER: %-22s fcurves=%-4d ik=%-3d ik_tagged=%-3d single_key=%-3d freehk.frameCount=%s" % (
        name, len(a.fcurves), len(ik), len(tagged), single, fh_fc))
print("DIAG DONE", flush=True)
