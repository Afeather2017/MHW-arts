# -*- coding: utf-8 -*-
"""Inspect TIML wiring for entries 140/339/340/343 in MHW-Valor.blend (read-only)."""
import bpy

MASTER = r"E:\MHW-arts\MHW-Valor.blend"
OUT = r"E:\MHW-arts\_timl_inspect.txt"
TARGETS = (140, 339, 340, 343)

bpy.ops.wm.open_mainfile(filepath=MASTER)
try:
    bpy.ops.preferences.addon_enable(module="MHW-FreeKinetics")
except Exception as e:
    print("addon_enable:", e)

lines = []
def p(*a):
    lines.append(" ".join(str(x) for x in a))

trees = [ng for ng in bpy.data.node_groups if getattr(ng, 'bl_idname', '') == 'FreeHKNodeTree']
p("trees:", [ng.name for ng in trees])
for ng in trees:
    for fn in [n for n in ng.nodes if n.bl_idname == 'LMTFileNode']:
        p("LMTFileNode:", fn.name, "path:", fn.filepath, "entryCount:", fn.entryCount, "inject:", fn.inject)
        for link in fn.inputs["LMT Entry"].links:
            en = link.from_node
            if en.bl_idname != 'LMTEntryNode':
                p("  non-entry child:", en.bl_idname, en.name)
                continue
            if en.entryNum not in TARGETS:
                continue
            p("== entry %d node=%s loopFrame=%s flags=%s/%s" %
              (en.entryNum, en.name, en.loopFrame, list(en.byteflag), list(en.byteflag2)))
            for link in en.inputs["LMT Animation"].links:
                an = link.from_node
                act = getattr(an, "input_action", None)
                if act:
                    maxf = max((kf.co[0] for fc in act.fcurves for kf in fc.keyframe_points), default=0)
                    p("   motion action=%s star=%s frameCount=%s curves=%d maxKeyFrame=%g" %
                      (act.name, act.freehk.starType, act.freehk.frameCount, len(act.fcurves), maxf))
                else:
                    p("   motion node %s: NO ACTION" % an.name)
            for link in en.inputs["TIML Data"].links:
                dn = link.from_node
                p("   TIMLDataNode=%s unkn=%d,%d animLength=%d loopStart=%d loop=%s" %
                  (dn.name, dn.unkn1, dn.unkn2, dn.animLength, dn.loopStartPoint, dn.loopControl))
                for l2 in dn.inputs["TIML Animation"].links:
                    tan = l2.from_node
                    act = getattr(tan, "input_action", None)
                    if not act:
                        p("     TIMLActionNode=%s NO ACTION" % tan.name)
                        continue
                    p("     TIMLActionNode=%s action=%s param=%s star=%s unkn0=%s" %
                      (tan.name, act.name, act.freehk.timelineParamRaw, act.freehk.starType, act.freehk.unkn0))
                    for fc in act.fcurves:
                        kfs = [(int(kf.co[0]), round(kf.co[1], 3)) for kf in fc.keyframe_points]
                        p("       fc %s [%d] n=%d %s" % (fc.data_path, fc.array_index, len(kfs), kfs))

open(OUT, "w", encoding="utf-8").write("\n".join(lines))
print("WROTE", OUT, flush=True)
