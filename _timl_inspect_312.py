# -*- coding: utf-8 -*-
import bpy
MASTER = r"E:\MHW-arts\MHW-Valor.blend"
OUT = r"E:\MHW-arts\_timl_inspect_312.txt"
bpy.ops.wm.open_mainfile(filepath=MASTER)
lines = []
def p(*a): lines.append(" ".join(str(x) for x in a))
tree = bpy.data.node_groups["w03_00"]
# canary: 339 CTO still linked?
canary = False
for n in tree.nodes:
    if n.bl_idname == "LMTEntryNode" and n.entryNum == 339:
        for l in n.inputs["TIML Data"].links:
            for l2 in l.from_node.inputs["TIML Animation"].links:
                act = getattr(l2.from_node, "input_action", None)
                if act and act.freehk.timelineParamRaw == "m38FF82EA":
                    canary = True
p("canary 339 CTO in tree:", canary)
for n in tree.nodes:
    if n.bl_idname == "LMTEntryNode" and n.entryNum in (312, 306):
        p("== entry %d node=%s" % (n.entryNum, n.name))
        for link in n.inputs["LMT Animation"].links:
            act = getattr(link.from_node, "input_action", None)
            if act:
                maxf = max((kf.co[0] for fc in act.fcurves for kf in fc.keyframe_points), default=0)
                p("   motion=%s star=%s frameCount=%s curves=%d maxKF=%g" %
                  (act.name, act.freehk.starType, act.freehk.frameCount, len(act.fcurves), maxf))
        for link in n.inputs["TIML Data"].links:
            dn = link.from_node
            p("   TIMLData unkn=%d,%d animLen=%d" % (dn.unkn1, dn.unkn2, dn.animLength))
            for l2 in dn.inputs["TIML Animation"].links:
                act = getattr(l2.from_node, "input_action", None)
                if not act: continue
                p("   action=%s param=%s unkn0=%s" % (act.name, act.freehk.timelineParamRaw, act.freehk.unkn0))
                for fc in act.fcurves:
                    kfs = [(int(kf.co[0]), round(kf.co[1], 3)) for kf in fc.keyframe_points]
                    p("     fc %s [%d] n=%d %s" % (fc.data_path, fc.array_index, len(kfs), kfs))
open(OUT, "w", encoding="utf-8").write("\n".join(lines))
print("WROTE", OUT, flush=True)
