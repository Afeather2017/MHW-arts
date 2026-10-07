# Test: on the 0.0.1 base (not saved), rewire entry 345's LMT Animation to the
# SAME action that entry 344 uses (LMT::w03::332 native import). TIML stays 345's own.
# Export to the game deploy path.
import bpy, importlib

tree = bpy.data.node_groups["w03_00"]

def entry(num):
    return next(n for n in tree.nodes if n.bl_idname == "LMTEntryNode" and n.entryNum == num)

def src(entry, name):
    s = next(i for i in entry.inputs if i.name == name)
    return s.links[0].from_node if s.links else None

e344 = entry(344)
e345 = entry(345)
act344 = src(e344, "LMT Animation")
print("[t] entry344 action = %r" % getattr(getattr(act344, "input_action", None), "name", None))
a = bpy.data.actions.get(act344.input_action.name)
print("[t] that action: fcurves=%d frameRange=[%.0f,%.0f]" % (len(a.fcurves), a.frame_range[0], a.frame_range[1]))

inp = next(i for i in e345.inputs if i.name == "LMT Animation")
old = src(e345, "LMT Animation")
print("[t] entry345 old <- %r" % getattr(getattr(old, "input_action", None), "name", None))
for l in list(inp.links):
    tree.links.remove(l)
tree.links.new(act344.outputs[0], inp)
print("[t] entry345 new <- entry344's action node")

scn = bpy.context.scene
scn.freehk_tether = bpy.data.objects["m_mesh Armature"]
OUT = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt"
out = next(n for n in tree.nodes if n.bl_idname == "LMTFileNode")
out.filepath = OUT
eo = importlib.import_module("MHW-FreeKinetics.operators.export_ops")
addon = bpy.context.preferences.addons.get(eo.__package__.split(".")[0])
eo.exportNodeToFile(out, addon.preferences)
print("[export] OK")
