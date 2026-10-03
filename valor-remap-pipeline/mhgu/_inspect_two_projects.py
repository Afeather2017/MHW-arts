"""Dump action inventory + LMT tree wiring for MHW-Valor-new.blend / MHW-Valor.blend.
Run: blender -b <file> --python _inspect_two_projects.py -- <tag>"""
import bpy, sys, json

tag = sys.argv[sys.argv.index("--")+1] if "--" in sys.argv else "file"
out = []
out.append("=" * 72)
out.append("FILE[%s]: %s" % (tag, bpy.data.filepath))

arms = [o.name for o in bpy.data.objects if o.type == 'ARMATURE']
out.append("armatures: %s" % arms)
out.append("objects(total)=%d actions=%d node_groups=%s" %
           (len(bpy.data.objects), len(bpy.data.actions),
            [g.name for g in bpy.data.node_groups]))

def star(a):
    try: return a.freehk.starType
    except Exception: return "?"

rows = []
for a in bpy.data.actions:
    fk = a.get("freehk", {})
    rows.append({
        "name": a.name,
        "star": star(a),
        "fc": len(a.fcurves),
        "frameCount": fk.get("frameCount") if fk else None,
        "tether": a.freehk.tetherFrame.name if getattr(a.freehk, "tetherFrame", None) else None,
        "mhgu_source": fk.get("mhgu_source") if fk else None,
        "fake": a.use_fake_user,
        "users": a.users,
    })

# LMT tree wiring
wiring = {}
tree = bpy.data.node_groups.get("w03_00")
tree_info = None
if tree:
    entries = [n for n in tree.nodes if n.bl_idname == "LMTEntryNode"]
    action_nodes = [n for n in tree.nodes if n.bl_idname == "LMTActionNode"]
    data_nodes = [n for n in tree.nodes if n.bl_idname == "LMTDataNode"]
    out.append("tree w03_00: entry_nodes=%d action_nodes=%d data_nodes=%d total=%d" %
               (len(entries), len(action_nodes), len(data_nodes), len(tree.nodes)))
    wired = 0
    for e in entries:
        inp = next((i for i in e.inputs if i.name == "LMT Animation"), None)
        act = None
        if inp and inp.links:
            src = inp.links[0].from_node
            act = getattr(src, "input_action", None)
            act = act.name if act else "(node:%s,no-action)" % src.name
            wired += 1
        wiring[str(e.entryNum)] = act
    out.append("wired entries: %d/%d" % (wired, len(entries)))
else:
    out.append("NO w03_00 node tree")

json.dump({"rows": rows, "wiring": wiring},
          open(r"E:\MHW-arts\valor-remap-pipeline\mhgu\_inspect_%s.json" % tag, "w", encoding="utf-8"),
          ensure_ascii=False, indent=1)
out.append("action rows: %d  (json -> _inspect_%s.json)" % (len(rows), tag))

# compact textual summary: group actions by naming scheme
import collections
groups = collections.Counter()
for r in rows:
    n = r["name"]
    if n.startswith("TIML::"): groups["TIML::*"] += 1
    elif "_remap" in n: groups["*_remap*"] += 1
    elif n.startswith("LMT::"): groups["LMT::other"] += 1
    elif n.isdigit(): groups["<digits>"] += 1
    else: groups["other"] += 1
out.append("action name groups: %s" % dict(groups))
print("\n".join(out))
