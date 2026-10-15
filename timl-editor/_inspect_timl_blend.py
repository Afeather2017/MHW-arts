# 无头检查 blend 里的 TIML action 形态: fcurve 通道布局/键数/帧范围 + 节点接线 + Keyframe 属性可用性
import bpy, os

print("=== Keyframe attrs ===")
kattr = set()
ap = bpy.data.actions.new("_probe")
fc = ap.fcurves.new(data_path="FreeHKTiml.m08FD20A6", index=0)
fc.keyframe_points.add(1)
kf = fc.keyframe_points[0]
kattr = {a for a in dir(kf) if not a.startswith("_")}
print("back" in kattr, "period" in kattr, "easing" in kattr, sorted(a for a in kattr if a in ("back","period","easing","interpolation","co","uv")))

print("=== TIML actions ===")
for a in bpy.data.actions:
    if getattr(a.freehk, "starType", "") != "TIML_Action":
        continue
    raw = a.freehk.timelineParamRaw or a.freehk.timelineParam
    cur = [f for f in a.fcurves if not f.mute]
    print("ACTION %s param=%s unkn0=%s fcurves=%d users=%d" % (
        a.name, raw, a.freehk.unkn0, len(a.fcurves), a.users))
    for f in cur:
        kfs = f.keyframe_points
        fr = ("%.0f..%.0f" % (kfs[0].co[0], kfs[-1].co[0])) if len(kfs) else "-"
        print("   [%d] %s n=%d %s" % (f.array_index, f.data_path, len(kfs), fr))

print("=== node tree wiring (TIML) ===")
for tree in bpy.data.node_groups:
    entrynodes = [n for n in tree.nodes if getattr(n, "bl_idname", "") == "LMTEntryNode"]
    if not entrynodes:
        continue
    print("TREE %s: %d entries" % (tree.name, len(entrynodes)))
    for n in entrynodes:
        timin = next((i for i in n.inputs if i.name == "TIML Data"), None)
        lmtin = next((i for i in n.inputs if i.name == "LMT Animation"), None)
        tsrc = timin.links[0].from_node if (timin and timin.links) else None
        lsrc = lmtin.links[0].from_node if (lmtin and lmtin.links) else None
        def acts(nd, depth=0):
            out = []
            if nd is None: return out
            if getattr(nd, "bl_idname", "") == "TIMLActionNode" and nd.input_action:
                out.append(nd.input_action.name)
            elif getattr(nd, "bl_idname", "") == "TIMLDataNode":
                for i in nd.inputs:
                    for l in i.links:
                        out += acts(l.from_node, depth+1)
            return out
        tnames = acts(tsrc)
        lname = getattr(lsrc, "input_action", None) and lsrc.input_action.name
        if tnames or tsrc is not None:
            print("  entry %d: LMT<- %s | TIML<- %s" % (n.entryNum, lname, tnames))
