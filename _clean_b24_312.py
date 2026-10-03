# -*- coding: utf-8 -*-
"""312: drop the ineffective b24 bit - mFlag = b26 [0,59] on zero baseline."""
import bpy
MASTER = r"E:\MHW-arts\MHW-Valor.blend"
bpy.ops.wm.open_mainfile(filepath=MASTER)
tree = bpy.data.node_groups["w03_00"]
en = next(n for n in tree.nodes if n.bl_idname == "LMTEntryNode" and n.entryNum == 312)
dn = en.inputs["TIML Data"].links[0].from_node
pmc = next(getattr(l.from_node, "input_action", None)
           for l in dn.inputs["TIML Animation"].links
           if getattr(l.from_node, "input_action", None) is not None
           and l.from_node.input_action.freehk.timelineParamRaw == "m48D6114F")
def rebuild(index, keys):
    for fc in list(pmc.fcurves):
        if fc.data_path == "FreeHKTiml.m08FD20A6" and fc.array_index == index:
            pmc.fcurves.remove(fc)
    fc = pmc.fcurves.new(data_path="FreeHKTiml.m08FD20A6", index=index)
    fc.keyframe_points.add(len(keys))
    for i, (fr, v) in enumerate(keys):
        kf = fc.keyframe_points[i]
        kf.co = (float(fr), float(v))
        kf.interpolation = 'CONSTANT'
    fc.update()
rebuild(0, [(0.0, 0.0), (60.0, 0.0)])
rebuild(1, [(0.0, 1024.0), (60.0, 0.0)])
print("312 PMC mFlag -> b26 [0,59] on zero baseline", flush=True)
bpy.ops.wm.save_mainfile()
print("saved.", flush=True)
