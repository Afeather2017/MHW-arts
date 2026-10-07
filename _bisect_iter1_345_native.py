# Bisect iteration 1: rewire entry 345 to the 0.0.1 wiring (action + TIML from entry 333),
# export straight to the game deploy path. Blend is NOT saved.
import bpy, importlib, sys

tree = bpy.data.node_groups["w03_00"]
e333 = next(n for n in tree.nodes if n.bl_idname == "LMTEntryNode" and n.entryNum == 333)
e345 = next(n for n in tree.nodes if n.bl_idname == "LMTEntryNode" and n.entryNum == 345)

def source(entry, sock_name):
    s = next(i for i in entry.inputs if i.name == sock_name)
    if s.links:
        return s.links[0].from_node
    return None

act333 = source(e333, "LMT Animation")
timl333 = source(e333, "TIML Data")
print("[bisect] entry333 sources: action=%r timl=%r" % (
    getattr(getattr(act333, "input_action", None), "name", None),
    timl333.name if timl333 else None))

# rewire 345: LMT Animation -> entry333's action node; TIML Data -> entry333's TIML node
for sock_name, new_src in (("LMT Animation", act333), ("TIML Data", timl333)):
    sock = next(i for i in e345.inputs if i.name == sock_name)
    old = sock.links[0].from_node if sock.links else None
    print("[bisect] entry345 %s: %r -> %r" % (sock_name,
          getattr(getattr(old, "input_action", None), "name", old.name if old else None),
          getattr(getattr(new_src, "input_action", None), "name", new_src.name)))
    for l in list(sock.links):
        tree.links.remove(l)
    tree.links.new(new_src.outputs[0], sock)

# export (GUI button code path)
scn = bpy.context.scene
scn.freehk_tether = bpy.data.objects["m_mesh Armature"]
OUT = r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00.lmt"
out = [n for n in tree.nodes if n.bl_idname == "LMTFileNode"][0]
out.filepath = OUT
eo = importlib.import_module("MHW-FreeKinetics.operators.export_ops")
addon = bpy.context.preferences.addons.get(eo.__package__.split(".")[0])
try:
    eo.exportNodeToFile(out, addon.preferences)
    print("[export] OK ->", OUT)
except Exception as e:
    import traceback; traceback.print_exc()
    print("[export] FAILED:", type(e).__name__, e)
