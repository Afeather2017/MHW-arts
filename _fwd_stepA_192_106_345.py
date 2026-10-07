# Forward bisect Step A: base = MHW-Valor-0.0.1.blend; change = wire entries
# 106 and 345 to 'LMT::w07::192_remap' (today's wiring for those two slots).
# Keep each entry's existing TIML Data node (vanilla TIMLs, length-valid).
# Blend is NOT saved; export goes straight to the game deploy path.
import bpy, importlib

BLEND_NOTE = "base=MHW-Valor-0.0.1"
tree = bpy.data.node_groups["w03_00"]
ACTION = "LMT::w07::192_remap"
act = bpy.data.actions.get(ACTION)
print("[fwd] base blend loaded; action %r present=%s (fcurves=%s)" % (
    ACTION, act is not None, len(act.fcurves) if act else "-"))
assert act is not None

DEPLOY = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt"

for entryNum in (106, 345):
    entry = next(n for n in tree.nodes if n.bl_idname == "LMTEntryNode" and n.entryNum == entryNum)
    inp = next(i for i in entry.inputs if i.name == "LMT Animation")
    old = inp.links[0].from_node if inp.links else None
    print("[fwd] entry%d old <- %r" % (entryNum,
          getattr(getattr(old, "input_action", None), "name", None)))
    new = tree.nodes.new("LMTActionNode")
    new.location = (old.location.x, old.location.y - 200) if old else (0, 0)
    new.input_action = act
    for l in list(inp.links):
        tree.links.remove(l)
    tree.links.new(new.outputs[0], inp)
    timl = next(i for i in entry.inputs if i.name == "TIML Data")
    tsrc = timl.links[0].from_node if timl.links else None
    print("[fwd] entry%d TIML stays <- %r (animLength=%r)" % (
        entryNum, tsrc.name if tsrc else None,
        getattr(tsrc, "animLength", "?") if tsrc else "?"))

scn = bpy.context.scene
scn.freehk_tether = bpy.data.objects["m_mesh Armature"]
out = [n for n in tree.nodes if n.bl_idname == "LMTFileNode"][0]
out.filepath = DEPLOY
eo = importlib.import_module("MHW-FreeKinetics.operators.export_ops")
addon = bpy.context.preferences.addons.get(eo.__package__.split(".")[0])
try:
    eo.exportNodeToFile(out, addon.preferences)
    print("[export] OK ->", DEPLOY)
except Exception as e:
    import traceback; traceback.print_exc()
    print("[export] FAILED:", type(e).__name__, e)
