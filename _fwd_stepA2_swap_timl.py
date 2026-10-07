# Forward bisect Step A2 (clean rewrite):
# base = MHW-Valor-0.0.1.blend (not saved); wire entries 106/345 <- 192_remap;
# SWAP their TIML Data nodes (106 gets the vanilla-345 TIML, 345 gets the 106 TIML).
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

e106, e345, e140 = entry(106), entry(345), entry(140)
act_node_192 = src(e140, "LMT Animation")          # existing 192_remap action node
assert act_node_192 is not None and act_node_192.bl_idname == "LMTActionNode"
print("[A2] 192_remap node:", act_node_192.name)

t106 = src(e106, "TIML Data")
t345 = src(e345, "TIML Data")
print("[A2] before: TIML106 animLength=%r / TIML345 animLength=%r" % (
    getattr(t106, "animLength", "?"), getattr(t345, "animLength", "?")))

rewire(e106, "LMT Animation", act_node_192)
rewire(e345, "LMT Animation", act_node_192)
rewire(e106, "TIML Data", t345)     # swap
rewire(e345, "TIML Data", t106)     # swap
print("[A2] after : TIML106 animLength=%r / TIML345 animLength=%r" % (
    getattr(t345, "animLength", "?"), getattr(t106, "animLength", "?")))

scn = bpy.context.scene
scn.freehk_tether = bpy.data.objects["m_mesh Armature"]
OUT = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt"
out = next(n for n in tree.nodes if n.bl_idname == "LMTFileNode")
out.filepath = OUT
eo = importlib.import_module("MHW-FreeKinetics.operators.export_ops")
addon = bpy.context.preferences.addons.get(eo.__package__.split(".")[0])
eo.exportNodeToFile(out, addon.preferences)
print("[export] OK")
