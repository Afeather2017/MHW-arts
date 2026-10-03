# -*- coding: utf-8 -*-
"""Headless inspect of MHW-Valor.blend for the valor-sheath build:
actions, freehk metadata, 195/196 keyframe structure, w03_00 node tree."""
import bpy

def frange(a):
    try:
        f0, f1 = a.frame_range
        return round(f0, 1), round(f1, 1)
    except Exception:
        return (None, None)

print("== ACTIONS (LMT/TIML) ==")
for a in sorted(bpy.data.actions, key=lambda x: x.name):
    st = getattr(a.freehk, "starType", None) if hasattr(a, "freehk") else None
    if st not in ("LMT_Action", "TIML_Action", None, ""):
        continue
    f0, f1 = frange(a)
    tag = ""
    if "195" in a.name or "196" in a.name or "::003" in a.name or a.name in ("195_remap", "196_remap"):
        tag = "  <<<<"
    if st == "TIML_Action" or "TIML" in a.name.upper():
        tag = tag or "  (TIML)"
    print("%-40s star=%-12s fc=%-6s tether=%-5s range=%s..%s fc=%d%s" % (
        a.name, st, getattr(a.freehk, "frameCount", None),
        "yes" if getattr(a.freehk, "tetherFrame", None) else "no",
        f0, f1, len(a.fcurves), tag))

print("\n== 195/196 detail ==")
for nm in ("LMT::w07::195_remap", "LMT::w07::196_remap", "LMT::w03_00::003"):
    a = bpy.data.actions.get(nm)
    if a is None:
        # try variants
        cands = [x.name for x in bpy.data.actions if nm.split("::")[-1] in x.name]
        print("%s: MISSING (cands: %s)" % (nm, cands[:6]))
        continue
    f0, f1 = frange(a)
    print("%s: range %s..%s  freehk.frameCount=%s  groups=%d  fcurves=%d" % (
        nm, f0, f1, getattr(a.freehk, "frameCount", None), len(a.action_groups), len(a.fcurves)))
    # keyframe frame histogram near the join point
    allframes = set()
    paths = {}
    for fc in a.fcurves:
        for kp in fc.keyframe_points:
            allframes.add(round(kp.co[0]))
        paths[fc.data_path.split('"')[0] if not fc.data_path.startswith("pose") else fc.data_path.split(".")[0]] = \
            paths.get(fc.data_path.split(".")[0], 0) + 1
    near = sorted(f for f in allframes if f <= 5 or f >= 48)
    print("  keyframes near join (<=5 or >=48): %s" % near[:40])
    print("  path prefixes: %s" % dict(list(paths.items())[:8]))

print("\n== NODE TREE w03_00 ==")
tree = bpy.data.node_groups.get("w03_00")
if tree:
    for n in tree.nodes:
        if n.bl_idname == "LMTEntryNode":
            act = tim = None
            for i in n.inputs:
                if i.name == "LMT Animation" and i.links:
                    src = i.links[0].from_node
                    act = getattr(src, "input_action", None)
                    act = act.name if act else src.name
                if i.name == "TIML Data" and i.links:
                    src = i.links[0].from_node
                    tim = src.name
            print("ENTRY %-4d loop=%-5s act=%-40s timl=%s" % (
                n.entryNum, getattr(n, "loopFrame", "?"), act, tim))
        elif n.bl_idname == "LMTActionNode":
            ia = getattr(n, "input_action", None)
            if ia is not None and ("195" in ia.name or "196" in ia.name):
                print("ACTIONNODE %s -> %s (linked: %s)" % (
                    n.name, ia.name, [l.to_node.name for o in n.outputs for l in o.links]))
        elif n.bl_idname not in ("NodeFrame",):
            pass
print("\n== scene freehk_tether ==")
print(bpy.context.scene.get("freehk_tether", None))
