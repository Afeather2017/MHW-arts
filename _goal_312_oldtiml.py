# -*- coding: utf-8 -*-
"""Goal config on the OLD 312 TIML: b26 iframe window [0,59] + trigger 2048 + PIN open at f100."""
import bpy
import shutil
import time

MASTER = r"E:\MHW-arts\MHW-Valor.blend"
BAK = MASTER + ".bak-" + time.strftime("%Y%m%d-%H%M%S") + "-pre-312-goal"
MFLAG, MTRIG = "FreeHKTiml.m08FD20A6", "FreeHKTiml.m99897C43"

shutil.copy2(MASTER, BAK)
print("backup:", BAK, flush=True)
bpy.ops.wm.open_mainfile(filepath=MASTER)
tree = bpy.data.node_groups["w03_00"]

def entry_node(num):
    for n in tree.nodes:
        if n.bl_idname == "LMTEntryNode" and n.entryNum == num:
            return n
    raise SystemExit("entry %d not found" % num)

en = entry_node(312)
dn = en.inputs["TIML Data"].links[0].from_node
pmc = pin = None
for l in dn.inputs["TIML Animation"].links:
    a = getattr(l.from_node, "input_action", None)
    if a and a.freehk.timelineParamRaw == "m48D6114F":
        pmc = a
    elif a and a.freehk.timelineParamRaw == "m6FCCD10E":
        pin = a
assert pmc and pin, "old PMC/PIN not linked"

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
    print("  %s %s[%d] <- %s" % (action.name.rsplit(".",1)[-1], path[-8:], index,
                                 [(int(f), int(v)) for f, v in keys]), flush=True)

# PMC: baseline 0x01000000 (native b24) + b26 window [0,59]
lo0 = [fc for fc in pmc.fcurves if fc.data_path == MFLAG and fc.array_index == 0][0].keyframe_points[0].co[1]
hi0 = [fc for fc in pmc.fcurves if fc.data_path == MFLAG and fc.array_index == 1][0].keyframe_points[0].co[1]
print("PMC baseline: 0x%08X" % (int(hi0) * 65536 + int(lo0)), flush=True)
assert int(hi0) == 256, "unexpected baseline high %g" % hi0
rebuild(pmc, MFLAG, 0, [(0.0, lo0), (60.0, lo0)])
rebuild(pmc, MFLAG, 1, [(0.0, hi0 + 1024.0), (60.0, hi0)])
rebuild(pmc, MTRIG, 0, [(0.0, 2048.0)])
rebuild(pmc, MTRIG, 1, [(0.0, 0.0)])

# PIN: everything to f100 open (native final values)
for fc in list(pin.fcurves):
    kfs = sorted(fc.keyframe_points, key=lambda k: k.co[0])
    final = kfs[-1].co[1]
    rebuild(pin, fc.data_path, fc.array_index, [(0.0, 0.0), (100.0, final)])

bpy.ops.wm.save_mainfile()
print("saved.", flush=True)
