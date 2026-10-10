# -*- coding: utf-8 -*-
"""Port the USER-VERIFIED guard recipe (0x90080000 single key, no mTrigger) to
the 0.0.2 tree: entry 105 (thrust, replaces the failed 105.txt variant) and
entry 345 (kiri1, first try on migrated motion - v7 doubt noted)."""
import bpy
import shutil
import time

B = r"E:\MHW-arts\MHW-Valor-0.0.2-140-swap-with-345.blend"
BAK = B + ".bak-" + time.strftime("%Y%m%d-%H%M%S") + "-pre-guard-port"
MFLAG, MTRIG = "FreeHKTiml.m08FD20A6", "FreeHKTiml.m99897C43"

shutil.copy2(B, BAK)
print("backup:", BAK, flush=True)
bpy.ops.wm.open_mainfile(filepath=B)
tree = bpy.data.node_groups["w03_00"]

def entry(num):
    return next(n for n in tree.nodes if n.bl_idname == "LMTEntryNode" and n.entryNum == num)

def src(nd, name):
    s = next(i for i in nd.inputs if i.name == name)
    return s.links[0].from_node if s.links else None

def main_pmc(en):
    td = src(en, "TIML Data")
    feeders = [l.to_node.entryNum for l in td.outputs["TIML Data"].links
               if l.to_node.bl_idname == "LMTEntryNode"]
    assert feeders == [en.entryNum], "entry %d TIML feeds %r" % (en.entryNum, feeders)
    pmcs = [l.from_node.input_action for l in td.inputs["TIML Animation"].links
            if getattr(l.from_node, "input_action", None) is not None
            and l.from_node.input_action.freehk.timelineParamRaw == "m48D6114F"]
    pmc = next((a for a in pmcs if any(fc.data_path == MFLAG for fc in a.fcurves)), None)
    if pmc is None:
        pmc = pmcs[0]
        print("  entry %d: PMC %r has no mFlag yet (will create)" % (en.entryNum, pmc.name), flush=True)
    users = set()
    for n in tree.nodes:
        for inp in n.inputs:
            for l in inp.links:
                if getattr(l.from_node, "input_action", None) is pmc:
                    users.add(n.name)
    assert users == {td.name}, "entry %d PMC shared: %r" % (en.entryNum, sorted(users))
    return pmc

def set_guard(pmc):
    for idx, val in ((0, 0.0), (1, 36872.0)):  # 0x90080000 -> low16=0, high16=0x9008
        for fc in list(pmc.fcurves):
            if fc.data_path == MFLAG and fc.array_index == idx:
                pmc.fcurves.remove(fc)
        fc = pmc.fcurves.new(data_path=MFLAG, index=idx)
        fc.keyframe_points.add(1)
        fc.keyframe_points[0].co = (0.0, val)
        fc.keyframe_points[0].interpolation = 'CONSTANT'
        fc.update()
    n_trig = 0
    for fc in list(pmc.fcurves):
        if fc.data_path == MTRIG:
            pmc.fcurves.remove(fc)
            n_trig += 1
    fcs = [(fc.data_path.split(".")[-1], fc.array_index,
            [int(k.co[1]) for k in fc.keyframe_points]) for fc in pmc.fcurves]
    print("  guard set, trig removed=%d, PMC now: %s" % (n_trig, fcs), flush=True)

for num in (105, 345):
    print("entry %d:" % num, flush=True)
    set_guard(main_pmc(entry(num)))

bpy.ops.wm.save_mainfile()
print("saved.", flush=True)
