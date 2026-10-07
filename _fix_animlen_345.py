import bpy
P = r"E:\MHW-arts\MHW-Valor.blend"
tree = bpy.data.node_groups.get("w03_00")
fixed = []
for n in tree.nodes:
    if n.bl_idname != "LMTEntryNode" or n.entryNum != 345:
        continue
    sock = next((i for i in n.inputs if i.name == "TIML Data"), None)
    if not sock or not sock.links:
        print("entry345: no TIML Data link!"); continue
    td = sock.links[0].from_node
    print("entry345 TIML node %r animLength %r -> 141" % (td.name, td.animLength))
    assert abs(td.animLength - 61) < 1e-4, "unexpected animLength"
    td.animLength = 141
    fixed.append(td)
if fixed:
    bpy.ops.wm.save_as_mainfile(filepath=P)
    print("SAVED", P)
else:
    print("NOTHING CHANGED, not saved")
