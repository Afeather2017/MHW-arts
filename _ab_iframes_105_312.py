# -*- coding: utf-8 -*-
"""A/B: same b26 i-frame window on vanilla entry 105; strip b26 from 312 (keep all else)."""
import bpy
import shutil
import time

MASTER = r"E:\MHW-arts\MHW-Valor.blend"
BAK = MASTER + ".bak-" + time.strftime("%Y%m%d-%H%M%S") + "-pre-105ab"
MFLAG, MTRIG = "FreeHKTiml.m08FD20A6", "FreeHKTiml.m99897C43"
OFF = 60.0
IFRAME_HIGH = 1024.0

shutil.copy2(MASTER, BAK)
print("backup:", BAK, flush=True)
bpy.ops.wm.open_mainfile(filepath=MASTER)
tree = bpy.data.node_groups["w03_00"]

def entry_node(num):
    for n in tree.nodes:
        if n.bl_idname == "LMTEntryNode" and n.entryNum == num:
            return n
    raise SystemExit("entry %d not found" % num)

def pmc_of(en):
    dn = en.inputs["TIML Data"].links[0].from_node
    for l in dn.inputs["TIML Animation"].links:
        a = getattr(l.from_node, "input_action", None)
        if a and a.freehk.timelineParamRaw == "m48D6114F":
            return a
    raise SystemExit("no PlMotionCommon on entry")

def rebuild(action, path, index, keys):
    for fc in list(action.fcurves):
        if fc.data_path == path and fc.array_index == index:
            action.fcurves.remove(fc)
    fc = action.fcurves.new(data_path=path, index=index)
    fc.keyframe_points.add(len(keys))
    for i, (fr, v) in enumerate(keys):
        kf = fc.keyframe_points[i]
        kf.co = (float(fr), float(v))
        kf.interpolation = 'CONSTANT'
    fc.update()

# canary
e339 = entry_node(339)
assert any(getattr(getattr(l2.from_node, "input_action", None), "freehk", None) is not None
           and l2.from_node.input_action.freehk.timelineParamRaw == "m38FF82EA"
           for l in e339.inputs["TIML Data"].links
           for l2 in l.from_node.inputs["TIML Animation"].links), "canary failed"

# --- entry 105: add b26 window, keep own baseline ---
e105 = entry_node(105)
mot = e105.inputs["LMT Animation"].links[0].from_node.input_action
print("entry105 motion=%s frameCount=%s" % (mot.name, mot.freehk.frameCount), flush=True)
pmc105 = pmc_of(e105)
lo = [fc for fc in pmc105.fcurves if fc.data_path == MFLAG and fc.array_index == 0]
hi = [fc for fc in pmc105.fcurves if fc.data_path == MFLAG and fc.array_index == 1]
assert lo and hi, "105 mFlag channels missing"
lo0 = lo[0].keyframe_points[0].co[1]
hi0 = hi[0].keyframe_points[0].co[1]
print("105 mFlag f0 = %g<<16 | %g (0x%08X)" % (hi0, lo0, int(hi0) * 65536 + int(lo0)), flush=True)
rebuild(pmc105, MFLAG, 0, [(0.0, lo0), (OFF, lo0)])
rebuild(pmc105, MFLAG, 1, [(0.0, hi0 + IFRAME_HIGH), (OFF, hi0)])
trig = [fc for fc in pmc105.fcurves if fc.data_path == MTRIG]
has2048 = any(abs(kf.co[1] - 2048) < 0.01 for fc in trig for kf in fc.keyframe_points)
if not has2048:
    rebuild(pmc105, MTRIG, 0, [(0.0, 2048.0)])
    rebuild(pmc105, MTRIG, 1, [(0.0, 0.0)])
    print("105: added mTrigger 2048@f0", flush=True)
else:
    print("105: mTrigger 2048 already present", flush=True)

# --- entry 312: strip b26 only ---
pmc312 = bpy.data.actions["TIML::nTimelineParam::PlMotionCommon.095"]
rebuild(pmc312, MFLAG, 0, [(0.0, 6.0)])
rebuild(pmc312, MFLAG, 1, [(0.0, 576.0)])
print("312 PMC.095 mFlag -> flat 0x02400006 (b26 removed), trigger untouched", flush=True)

bpy.ops.wm.save_mainfile()
print("saved.", flush=True)
