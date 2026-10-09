# -*- coding: utf-8 -*-
# 只读: dump 树中指定区域所有节点 + 全部 LMT/TIML data 节点的连线情况 (找孤儿射击节点)
import bpy, sys
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
X0, X1 = float(argv[0]), float(argv[1])
Y0, Y1 = float(argv[2]), float(argv[3])
tree = next(t for t in bpy.data.node_groups if t.bl_idname == 'FreeHKNodeTree')
def out(s): print(s, flush=True)
out(f"FILE: {bpy.data.filepath}  region x[{X0},{X1}] y[{Y0},{Y1}]")
cnt = 0
for n in tree.nodes:
    x, y = n.location.x, n.location.y
    if X0 <= x <= X1 and Y0 <= y <= Y1:
        cnt += 1
        a = getattr(n, "input_action", None)
        an = a.name if a else ""
        en = getattr(n, "entryNum", None)
        links_in = [f"{l.from_node.name}:{l.to_socket.name}" for l in tree.links if l.to_node == n]
        links_out = [f"{l.to_node.name}:{l.from_socket.name}" for l in tree.links if l.from_node == n]
        out(f"{n.bl_idname:22s} '{n.name}' @({x:.0f},{y:.0f}) entry={en} act='{an}'")
        if links_in: out(f"    IN : {links_in}")
        if links_out: out(f"    OUT: {links_out}")
out(f"total nodes in region: {cnt}")
# 所有 data 节点及其下游 entry
out("--- ALL TIML DataNodes and their consumers ---")
for n in tree.nodes:
    if n.bl_idname == 'TIMLDataNode':
        outs = [l.to_node for l in tree.links if l.from_node == n]
        tag = ",".join(f"E{getattr(o,'entryNum','?')}" if o.bl_idname == 'LMTEntryNode' else o.name for o in outs)
        if not outs:
            tag = "<ORPHAN>"
        nt = sum(1 for l in tree.links if l.to_node == n and l.to_socket.name == 'TIML Animation')
        out(f"  '{n.name}' @({n.location.x:.0f},{n.location.y:.0f}) nTIM={nt} -> {tag} props={ {k:v for k,v in n.items() if not str(k).startswith('_')} }")
