# Forward bisect Step A3:
# base = MHW-Valor-0.0.1.blend (not saved)
# 106 <- 192_remap + its 120/12-type TIML   (Step A config = known FINE)
# 345 <- divide_short (the action that works at 339) + the same 120/12-type TIML
import bpy, importlib

tree = bpy.data.node_groups["w03_00"]

def entry(num):
    return next(n for n in tree.nodes if n.bl_idname == "LMTEntryNode" and n.entryNum == num)

def src(entry, name):
    s = next(i for i in entry.inputs if i.name == name)
    return s.links[0].from_node if s.links else None

def rewire(entry, sock_name, new_node):
    sock = next(i for i in entry.inputs if i.name == sock_name)
    for l in list(sock.links):
        tree.links.remove(l)
    tree.links.new(new_node.outputs[0], sock)

e106, e345, e140, e339 = entry(106), entry(345), entry(140), entry(339)
act192 = src(e140, "LMT Animation")
act_div = src(e339, "LMT Animation")
print("[A3b] 192_remap node=%r  divide_short node=%r" % (
    act192.name, getattr(act_div, "input_action", None) and act_div.input_action.name))

t120 = src(e106, "TIML Data")        # 120/12-type (fine with 192_remap)
t339 = src(e339, "TIML Data")        # divide_short's own TIML (114/114, proven at 339)
rewire(e106, "LMT Animation", act192)
rewire(e106, "TIML Data", t120)
rewire(e345, "LMT Animation", act_div)
rewire(e345, "TIML Data", t339)
print("[A3b] 106 <- 192_remap + TIML120 | 345 <- divide_short + its own TIML114")

scn = bpy.context.scene
scn.freehk_tether = bpy.data.objects["m_mesh Armature"]
OUT = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt"
out = next(n for n in tree.nodes if n.bl_idname == "LMTFileNode")
out.filepath = OUT
eo = importlib.import_module("MHW-FreeKinetics.operators.export_ops")
addon = bpy.context.preferences.addons.get(eo.__package__.split(".")[0])
eo.exportNodeToFile(out, addon.preferences)
print("[export] OK")
