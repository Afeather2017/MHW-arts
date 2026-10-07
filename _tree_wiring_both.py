import bpy
tree = bpy.data.node_groups.get("w03_00")
from collections import defaultdict
wire = {}
for n in tree.nodes:
    if n.bl_idname != "LMTEntryNode":
        continue
    inp = next((i for i in n.inputs if i.name == "LMT Animation"), None)
    if inp and inp.links:
        src = inp.links[0].from_node
        aname = getattr(getattr(src, "input_action", None), "name", "?")
        wire.setdefault(aname, []).append(n.entryNum)
print("=== actions feeding MULTIPLE entries:")
multi = {a: es for a, es in wire.items() if len(es) > 1}
for a, es in sorted(multi.items()):
    print("  %r -> entries %s" % (a, sorted(es)))
if not multi:
    print("  (none)")
print("=== total wired entries: %d, distinct actions: %d" % (sum(len(v) for v in wire.values()), len(wire)))
for a in ("LMT::w07::192_remap", "valor_sheath_start", "divide_short", "divide_long"):
    print("  %r -> %s" % (a, sorted(wire.get(a, []))))
