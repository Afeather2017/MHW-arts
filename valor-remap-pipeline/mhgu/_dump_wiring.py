# 只读 dump 一个 blend 的 FreeHK 接线 (entry -> action / entry -> TIML data), 永不保存。
# 用法: BLEND="x.blend" blender -b --python _dump_wiring.py  (bpy.data.filepath 为准)
import bpy, os

def out(s):
    print(s, flush=True)

out("BLEND: " + bpy.data.filepath)

trees = [t for t in bpy.data.node_groups if t.bl_idname == 'FreeHKNodeTree']
out("TREES: " + ", ".join(f"{t.name}(nodes={len(t.nodes)},links={len(t.links)})" for t in trees))

for tree in trees:
    out("")
    out(f"=== TREE {tree.name} ===")
    kinds = {}
    for n in tree.nodes:
        kinds[n.bl_idname] = kinds.get(n.bl_idname, 0) + 1
    out("KINDS: " + str(dict(sorted(kinds.items()))))

    # entry -> [action links]  (LMT Animation)
    # entry -> [data links]    (TIML Data)
    # data -> [timl action links] (TIML Animation)
    lmt_links = {}
    data_links = {}
    for l in tree.links:
        t, ts = l.to_node, l.to_socket.name
        if t.bl_idname == 'LMTEntryNode' and ts == "LMT Animation":
            lmt_links.setdefault(t, []).append(l.from_node)
        elif t.bl_idname == 'LMTEntryNode' and ts == "TIML Data":
            data_links.setdefault(t, []).append(l.from_node)

    # TIML action nodes hanging under data nodes, keyed by data node position
    def key(n):
        return (round(n.location.x, 1), round(n.location.y, 1))
    timl_under = {}
    for l in tree.links:
        if l.to_node.bl_idname == 'TIMLDataNode' and l.to_socket.name == "TIML Animation":
            timl_under.setdefault(key(l.to_node), []).append(l.from_node)

    entries = sorted([n for n in tree.nodes if n.bl_idname == 'LMTEntryNode'],
                     key=lambda n: getattr(n, "entryNum", -1))
    for e in entries:
        en = getattr(e, "entryNum", -1)
        acts = lmt_links.get(e, [])
        if not acts:
            out(f"E{en:<5d} LMT <- <EMPTY>")
        for a in acts:
            an = getattr(a, "input_action", None)
            aname = an.name if an else "<no-action>"
            out(f"E{en:<5d} LMT <- {aname}  [node '{a.name}' @({a.location.x:.0f},{a.location.y:.0f})]")
        for d in data_links.get(e, []):
            tims = timl_under.get(key(d), [])
            tnames = ",".join((getattr(t, "input_action", None) and t.input_action.name) or t.name for t in tims)
            out(f"E{en:<5d} TIMLDATA <- data@{key(d)[0]:.0f},{key(d)[1]:.0f} nTIML={len(tims)} [{tnames[:120]}]")

    # 其他 entry 类型节点 (TIMLEntryNode / EFxEntry 之类) 一并列出输入连线
    for n in tree.nodes:
        if n.bl_idname not in ('LMTEntryNode', 'LMTActionNode', 'TIMLActionNode',
                               'TIMLDataNode', 'LMTFileNode', 'TIMLFileNode'):
            ins = []
            for i in n.inputs:
                for l in i.links:
                    ins.append(f"{i.name}<-{l.from_node.name}")
            if ins:
                out(f"OTHER {n.bl_idname} '{n.name}' @({n.location.x:.0f},{n.location.y:.0f}): " + "; ".join(ins))

    # file/output 节点
    for n in tree.nodes:
        if n.bl_idname in ('LMTFileNode', 'TIMLFileNode'):
            fp = getattr(n, "filepath", "?")
            out(f"FILENODE {n.bl_idname} '{n.name}' filepath={fp}")

out("")
out("ACTIONS used by any node in any tree:")
used = set()
for tree in trees:
    for n in tree.nodes:
        a = getattr(n, "input_action", None)
        if a:
            used.add(a.name)
out("  " + ", ".join(sorted(used)))
