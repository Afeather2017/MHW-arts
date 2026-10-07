import bpy, sys
BLEND = sys.argv[sys.argv.index("--")+1] if "--" in sys.argv else ""
# run as: blender -b <blend> --python this.py -- <blend path is auto-loaded>; just dump
WANT = ("LMT::w07::192_remap", "valor_sheath_start")
tree = bpy.data.node_groups.get("w03_00")
targets = {}
for n in tree.nodes:
    if n.bl_idname != "LMTEntryNode":
        continue
    inp = next((i for i in n.inputs if i.name == "LMT Animation"), None)
    if inp and inp.links:
        aname = getattr(getattr(inp.links[0].from_node, "input_action", None), "name", "")
        if aname in WANT:
            targets.setdefault(aname, []).append(n.entryNum)
for aname, entries in sorted(targets.items()):
    a = bpy.data.actions.get(aname)
    if a is None:
        print("ACTION %r MISSING" % aname); continue
    print("=" * 80)
    print("ACTION %r entries=%s fcurves=%d frameRange=[%.0f,%.0f]" % (
        aname, entries, len(a.fcurves), a.frame_range[0], a.frame_range[1]))
    from collections import Counter
    c = Counter()
    for fc in a.fcurves:
        top = fc.data_path.split(".")[0].split("[")[0].split('"')[0]
        c[(top, fc.data_path)] += 1
    for (top, dp), cnt in sorted(c.items()):
        print("  [%s] x%d  %s" % (top, cnt, dp if cnt <= 3 else ""))
    # grip curves detail (030/047)
    for fc in a.fcurves:
        dp = fc.data_path
        if ("030" in dp or "047" in dp) and "scale" in dp:
            ks = [(round(k.co[0],1), round(k.co[1],3)) for k in fc.keyframe_points]
            print("  GRIP %s[%d] keys=%s limits=%s" % (dp, fc.array_index, ks,
                  (getattr(fc, 'range', None) and "y") or "n"))
