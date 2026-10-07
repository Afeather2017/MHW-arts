# Fix: entry 345's TIML Data must not stay on the stale animLength=141 node
# (332 action is 134 frames; 141 > 134 = the load-time overrun class).
# Rewire 345 TIML Data <- entry 344's TIML node (animLength=133, proven pair with
# the 332 motion). Saves the blend.
import bpy

tree = bpy.data.node_groups["w03_00"]

def entry(num):
    return next(n for n in tree.nodes if n.bl_idname == "LMTEntryNode" and n.entryNum == num)

def src(entry, name):
    s = next(i for i in entry.inputs if i.name == name)
    return s.links[0].from_node if s.links else None

e344, e345 = entry(344), entry(345)
t344 = src(e344, "TIML Data")
t345 = src(e345, "TIML Data")
print("[fix] 345 TIML: %r (animLength=%r) -> %r (animLength=%r)" % (
    t345.name if t345 else None, getattr(t345, "animLength", "?"),
    t344.name if t344 else None, getattr(t344, "animLength", "?")))
assert t344 is not None and abs(getattr(t344, "animLength", 0) - 133.0) < 0.01
sock = next(i for i in e345.inputs if i.name == "TIML Data")
for l in list(sock.links):
    tree.links.remove(l)
tree.links.new(t344.outputs[0], sock)
print("[fix] rewired")

# sanity: 345 animation socket still on the 332 action
a345 = src(e345, "LMT Animation")
print("[fix] 345 action still = %r" % getattr(getattr(a345, "input_action", None), "name", None))
assert getattr(a345.input_action, "name", "") == "LMT::w03_00::332"

bpy.ops.wm.save_mainfile()
print("[saved]")
