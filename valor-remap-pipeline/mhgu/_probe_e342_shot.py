# -*- coding: utf-8 -*-
# 只读探针: 完整 dump 一个 entry 的 TIML data 节点下所有 tim 动作内容 (名字/属性/fcurve/键)。
# 用法: blender -b <blend> --python _probe_e342_shot.py -- 342   (永不保存)
import bpy, sys, json

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ENTRY = int(argv[0]) if argv else 342

def out(s):
    print(s, flush=True)

out("FILE: " + bpy.data.filepath)

tree = next(t for t in bpy.data.node_groups if t.bl_idname == 'FreeHKNodeTree')

# entry -> data node (TIML Data socket)
entry_nodes = {getattr(n, "entryNum", -1): n for n in tree.nodes if n.bl_idname == 'LMTEntryNode'}
e = entry_nodes.get(ENTRY)
if e is None:
    out(f"NO ENTRY {ENTRY}")
    raise SystemExit

data_node = None
lmt_action = None
for l in tree.links:
    if l.to_node == e:
        if l.to_socket.name == "TIML Data":
            data_node = l.from_node
        elif l.to_socket.name == "LMT Animation":
            a = getattr(l.from_node, "input_action", None)
            lmt_action = a.name if a else "<none>"
out(f"E{ENTRY} LMT <- {lmt_action}")
if data_node is None:
    out("NO TIML DATA LINK")
    raise SystemExit

# tims hanging under data node (link order)
tims = [l.from_node for l in tree.links
        if l.to_node == data_node and l.to_socket.name == "TIML Animation"]
out(f"DATA NODE '{data_node.name}' props={ {k: v for k, v in data_node.items() if not k.startswith('_')} }")
out(f"nTIMS={len(tims)}")

for i, t in enumerate(tims):
    a = getattr(t, "input_action", None)
    out("")
    out(f"[{i}] node='{t.name}' @({t.location.x:.0f},{t.location.y:.0f})")
    if a is None:
        out("    <no action>")
        continue
    props = {k: (round(v, 4) if isinstance(v, float) else v) for k, v in a.items() if not str(k).startswith("_")}
    out(f"    ACTION '{a.name}' fc={len(a.fcurves)} frame_start={a.frame_start} frame_end={a.frame_end} props={props}")
    for fc in a.fcurves:
        keys = [(round(k.co[0], 2), round(k.co[1], 4), round(k.handle_left[0], 2), round(k.handle_right[0], 2)) for k in fc.keyframe_points]
        out(f"    fc dp='{fc.data_path}' ix={fc.array_index} n={len(keys)} {keys[:24]}{'...' if len(keys) > 24 else ''}")
