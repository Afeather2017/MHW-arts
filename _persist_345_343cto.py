# Persist into MHW-Valor.blend (SAVES):
# 1) entry 345 LMT Animation <- the node feeding entry 344 (LMT::w03_00::332)
# 2) 343's CollisionTimelineObject TIML action: CTO node key 63 -> 85 (deployed-patch parity)
import bpy, struct

tree = bpy.data.node_groups["w03_00"]

def entry(num):
    return next(n for n in tree.nodes if n.bl_idname == "LMTEntryNode" and n.entryNum == num)

def src(entry, name):
    s = next(i for i in entry.inputs if i.name == name)
    return s.links[0].from_node if s.links else None

# --- 1) 345 wiring
e344, e345 = entry(344), entry(345)
act344 = src(e344, "LMT Animation")
inp = next(i for i in e345.inputs if i.name == "LMT Animation")
old = src(e345, "LMT Animation")
print("[1] 345: %r -> %r" % (
    getattr(getattr(old, "input_action", None), "name", None),
    getattr(getattr(act344, "input_action", None), "name", None)))
for l in list(inp.links):
    tree.links.remove(l)
tree.links.new(act344.outputs[0], inp)

# --- 2) 343 CTO key 63 -> 85
e343 = entry(343)
td = src(e343, "TIML Data")
print("[2] 343 TIML Data node:", td.name if td else None)
cto_act = None
if td:
    a_inp = next((i for i in td.inputs if i.name == "TIML Animation"), None)
    for lk in (a_inp.links if a_inp else []):
        an = getattr(getattr(lk.from_node, "input_action", None), "name", "")
        if "CollisionTimelineObject" in an:
            cto_act = bpy.data.actions.get(an)
            print("[2] CTO action:", an)
            break
if cto_act is None:
    print("[2] !! CTO action not found, skipping")
else:
    patched = 0
    for fc in cto_act.fcurves:
        for k in fc.keyframe_points:
            if abs(k.co[1] - 63.0) < 0.01:
                print("   key %.0f -> 85 (fcurve %s)" % (k.co[1], fc.data_path))
                k.co[1] = 85.0
                patched += 1
    print("[2] keys patched:", patched)
    assert patched >= 1, "expected at least one node-63 key"

bpy.ops.wm.save_mainfile()
print("[saved] MHW-Valor.blend")
